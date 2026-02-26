/*
 * Copyright (c) 2025 Akanksha Chaudhari, Matt Sinclair
 * All rights reserved.
 *
 * This file contains modifications and/or code derived from:
 * gem5-SALAM: https://github.com/TeCSAR-UNCC/gem5-SALAM
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "salam/scratchpad_memory.hh"

#include <cstdio>
#include <cstdlib>
#include <iomanip>

#include "base/trace.hh"
#include "debug/Drain.hh"
#include "mem/packet.hh"
#include "mem/packet_access.hh"
#include "sim/system.hh"

using namespace std;

/*****************************************************************************
 * Scratchpad Scratchpad Device for Accelerators using CommMemInterface
 * Acts as a simple memory for external devices
 * Enables specialization of access for parent device
 ****************************************************************************/
#include "debug/MemoryAccess.hh"

ScratchpadMemory::ScratchpadMemory(const ScratchpadMemoryParams &p)
    : AbstractMemory(p),
      readyMode(p.ready_mode),
      readOnInvalid(p.read_on_invalid),
      writeOnValid(p.write_on_valid),
      resetOnScratchpadRead(p.reset_on_scratchpad_read),
      initial(true),
      port(name() + ".port", *this),
      tickEngine(p.tick_engine),
      accessLatencyCycles(p.access_latency_cycles),
      bytesPerCycle(p.bytes_per_cycle),
      maxReqsPerCycle(p.max_reqs_per_cycle)
{
    fatal_if(!tickEngine, "ScratchpadMemory '%s' requires tick_engine\n",
             name());

    // Bind this memory as the cycle-ticked owner of the engine.
    tickEngine->setOwner(this);

    ready = new bool[range.size()];
    if (readyMode) {
        for (auto i = 0; i < range.size(); i++) {
            ready[i] = false;
        }
    }
    // Ensure base port state exists (idx 0).
    ensurePortState(0);
}

void
ScratchpadMemory::ensurePortState(PortID idx)
{
    if (idx >= reqQueues.size()) {
        reqQueues.resize(idx + 1);
        respQueues.resize(idx + 1);
        inFlight.resize(idx + 1);
        retryResp.resize(idx + 1, false);
    }
}

void
ScratchpadMemory::serviceResponses(PortID idx)
{
    if (retryResp[idx]) {
        return;
    }

    while (!respQueues[idx].empty() &&
           respQueues[idx].front().remaining == Cycles(0)) {
        PacketPtr pkt = respQueues[idx].front().pkt;

        bool ok = false;
        if (idx == 0) {
            ok = port.sendTimingResp(pkt);
        } else {
            ok = spm_ports[idx - 1]->sendTimingResp(pkt);
        }

        retryResp[idx] = !ok;
        if (retryResp[idx]) {
            return;
        }

        respQueues[idx].pop_front();
    }
}

static inline Cycles
ceilDivToCycles(size_t bytes, unsigned bytesPerCycle)
{
    const unsigned bpc = std::max(1u, bytesPerCycle);
    const size_t cyc = (bytes + bpc - 1) / bpc;
    // Always make forward progress
    // (0-byte reqs are weird but avoid deadlock)
    return Cycles(std::max<size_t>(1, cyc));
}

void
ScratchpadMemory::startInFlight(PortID idx)
{
    // Caller ensures: !inFlight[idx].active && !reqQueues[idx].empty()
    PendingReq pr = reqQueues[idx].front();
    reqQueues[idx].pop_front();

    InFlightReq &inf = inFlight[idx];
    inf.pkt = pr.pkt;
    inf.validateAccess = pr.validateAccess;
    inf.needsResponse = pr.pkt->needsResponse();
    // Clear any accumulated interconnect delay
    // (we model service in cycles here)
    pr.pkt->headerDelay = pr.pkt->payloadDelay = 0;
    inf.remainingXfer = ceilDivToCycles(pr.pkt->getSize(), bytesPerCycle);
    inf.active = true;
}

void
ScratchpadMemory::completeInFlight(PortID idx)
{
    InFlightReq &inf = inFlight[idx];
    assert(inf.active && inf.pkt);

    PacketPtr pkt = inf.pkt;
    const bool needs_resp = inf.needsResponse;

    // Perform the actual memory access at completion time.
    scratchpadAccess(pkt, inf.validateAccess);

    if (needs_resp) {
        // scratchpadAccess should have created the response already.
        assert(pkt->isResponse() || pkt->isError());
        // Queue response with a per-cycle countdown.
        // Note: allow 0 cycles to
        // mean "eligible to send immediately".
        const Cycles lat = accessLatencyCycles;
        respQueues[idx].emplace_back(pkt, lat);
        // If latency is 0, attempt to send now
        // (otherwise it would wait until
        // the next tickCycle() due to phase ordering)
        if (lat == Cycles(0)) {
            serviceResponses(idx);
        }
    } else {
        pendingDelete.push_back(pkt);
    }

    // Clear slot
    inf = InFlightReq{};
}

bool
ScratchpadMemory::tickCycle()
{
    const unsigned max_issue = std::max(1u, maxReqsPerCycle);

    // Decrement response countdowns and attempt to send ready responses
    for (PortID idx = 0; idx < respQueues.size(); ++idx) {
        for (auto &e : respQueues[idx]) {
            if (e.remaining > Cycles(0)) {
                e.remaining = e.remaining - Cycles(1);
            }
        }
        serviceResponses(idx);
    }

    // Advance/complete in-flight transfers and launch new ones
    for (PortID idx = 0; idx < reqQueues.size(); ++idx) {
        ensurePortState(idx);
        unsigned issued = 0;

        // If something is currently in flight, advance it by 1 cycle
        if (inFlight[idx].active) {
            if (inFlight[idx].remainingXfer > Cycles(0)) {
                inFlight[idx].remainingXfer =
                    inFlight[idx].remainingXfer - Cycles(1);
            }
            if (inFlight[idx].remainingXfer == Cycles(0)) {
                completeInFlight(idx);
                issued++;
            }
        }

        // If idle, we may start (and possibly complete) new requests
        while (!inFlight[idx].active && !reqQueues[idx].empty() &&
               issued < max_issue) {
            startInFlight(idx);

            // Consume 1 cycle immediately for the newly-started request
            if (inFlight[idx].remainingXfer > Cycles(0)) {
                inFlight[idx].remainingXfer =
                    inFlight[idx].remainingXfer - Cycles(1);
            }
            if (inFlight[idx].remainingXfer == Cycles(0)) {
                completeInFlight(idx);
                issued++;
            } else {
                // Now busy; no further starts until it completes
                break;
            }
        }
    }

    // Delete no-response packets that completed service
    while (!pendingDelete.empty()) {
        delete pendingDelete.front();
        pendingDelete.pop_front();
    }

    // Decide whether to keep ticking
    bool keep = false;
    for (PortID idx = 0; idx < reqQueues.size(); ++idx) {
        if (!reqQueues[idx].empty()) {
            keep = true;
        }
        if (inFlight[idx].active) {
            keep = true;
        }
    }
    for (PortID idx = 0; idx < respQueues.size(); ++idx) {
        if (!respQueues[idx].empty() && !retryResp[idx]) {
            keep = true;
        }
        for (const auto &e : respQueues[idx]) {
            if (e.remaining > Cycles(0)) {
                keep = true;
            }
        }
    }

    // If the only remaining condition is retryResp[*],
    // stop and wait for recvRespRetry()
    if (!keep) {
        bool onlyRetryBlock = false;
        for (bool r : retryResp) {
            if (r) {
                onlyRetryBlock = true;
            }
        }
        if (!onlyRetryBlock && drainState() == DrainState::Draining) {
            DPRINTF(Drain, "Draining of ScratchpadMemory complete\n");
            signalDrainDone();
        }
        return false;
    }
    return true;
}

bool
ScratchpadMemory::isReady(Addr ad, size_t size, bool read)
{
    if (!readyMode) {
        return true;
    } else if (read) {
        // We are reading. We can read if readOnInvalid or
        // if all segments are valid.
        if (readOnInvalid) {
            return true;
        }
        Addr start_offset = ad - range.start();
        Addr end_offset = start_offset + size;
        for (auto i = start_offset; i < end_offset; i++) {
            if (!ready[i]) {
                return false;
            }
        }
    } else {
        // We are writing. We can write if writeOnValid or
        // if all segments are invalid.
        if (writeOnValid) {
            return true;
        }
        Addr start_offset = ad - range.start();
        Addr end_offset = start_offset + size;
        for (auto i = start_offset; i < end_offset; i++) {
            if (ready[i]) {
                return false;
            }
        }
    }
    return true;
}

void
ScratchpadMemory::setAllReady(bool r)
{
    if (readyMode && !initial) {
        for (auto i = 0; i < range.size(); i++) {
            ready[i] = r;
        }
    }
    initial = true;
}

static inline void
tracePacket(System *sys, const char *label, PacketPtr pkt)
{
    int size = pkt->getSize();
    if (size == 1 || size == 2 || size == 4 || size == 8) {
        DPRINTF(MemoryAccess,
                "%s from %s of size %i on address %#x data "
                "%#x %c\n",
                label, sys->getRequestorName(pkt->req->requestorId()), size,
                pkt->getAddr(), pkt->getUintX(ByteOrder::little),
                pkt->req->isUncacheable() ? 'U' : 'C');
        return;
    }
    DPRINTF(MemoryAccess, "%s from %s of size %i on address %#x %c\n", label,
            sys->getRequestorName(pkt->req->requestorId()), size,
            pkt->getAddr(), pkt->req->isUncacheable() ? 'U' : 'C');
    DDUMP(MemoryAccess, pkt->getConstPtr<uint8_t>(), pkt->getSize());
}

#if TRACING_ON
#define TRACE_PACKET(A) tracePacket(system(), A, pkt)
#else
#define TRACE_PACKET(A)
#endif

void
ScratchpadMemory::scratchpadAccess(PacketPtr pkt, bool validateAccess)
{
    initial = false;
    if (pkt->cacheResponding()) {
        DPRINTF(MemoryAccess, "Cache responding to %#llx: not responding\n",
                pkt->getAddr());
        return;
    }

    if (pkt->cmd == MemCmd::CleanEvict || pkt->cmd == MemCmd::WritebackClean) {
        DPRINTF(MemoryAccess, "CleanEvict  on 0x%x: not responding\n",
                pkt->getAddr());
        return;
    }

    assert(AddrRange(pkt->getAddr(), pkt->getAddr() + pkt->getSize())
               .isSubset(range));

    uint8_t *hostAddr = pmemAddr + pkt->getAddr() - range.start();

    if (pkt->cmd == MemCmd::SwapReq) {
        if (pkt->isAtomicOp()) {
            if (pmemAddr) {
                pkt->setData(hostAddr);
                (*(pkt->getAtomicOp()))(hostAddr);
            }
        } else {
            std::vector<uint8_t> overwrite_val(pkt->getSize());
            uint64_t condition_val64;
            uint32_t condition_val32;

            panic_if(!pmemAddr, "Swap only works if there is real memory "
                                "(i.e. null=False)");

            bool overwrite_mem = true;
            // keep a copy of our possible write value, and copy what is at the
            // memory address into the packet
            pkt->writeData(&overwrite_val[0]);
            pkt->setData(hostAddr);

            if (pkt->req->isCondSwap()) {
                if (pkt->getSize() == sizeof(uint64_t)) {
                    condition_val64 = pkt->req->getExtraData();
                    overwrite_mem = !std::memcmp(&condition_val64, hostAddr,
                                                 sizeof(uint64_t));
                } else if (pkt->getSize() == sizeof(uint32_t)) {
                    condition_val32 = (uint32_t)pkt->req->getExtraData();
                    overwrite_mem = !std::memcmp(&condition_val32, hostAddr,
                                                 sizeof(uint32_t));
                } else {
                    panic("Invalid size for conditional read/write\n");
                }
            }

            if (overwrite_mem) {
                std::memcpy(hostAddr, &overwrite_val[0], pkt->getSize());
            }

            assert(!pkt->req->isInstFetch());
            TRACE_PACKET("Read/Write");
            stats.numOther[pkt->req->requestorId()]++;
        }
    } else if (pkt->isRead()) {
        assert(!pkt->isWrite());
        if (pkt->isLLSC()) {
            assert(!pkt->fromCache());
            // if the packet is not coming from a cache then we have
            // to do the LL/SC tracking here
            trackLoadLocked(pkt);
        }
        if (validateAccess) {
            if (!isReady(pkt->getAddr(), pkt->getSize(), true)) {
                panic("Scratchpad read at address: 0x%lx is invalid! "
                      "Sector has not been written yet!\n",
                      pkt->getAddr());
            }
            if (resetOnScratchpadRead) {
                Addr start_offset = pkt->getAddr() - range.start();
                Addr end_offset = start_offset + pkt->getSize();
                for (auto i = start_offset; i < end_offset; i++) {
                    ready[i] = false;
                }
            }
        }
        if (pmemAddr) {
            pkt->setData(hostAddr);
        }
        TRACE_PACKET(pkt->req->isInstFetch() ? "IFetch" : "Read");
        stats.numReads[pkt->req->requestorId()]++;
        stats.bytesRead[pkt->req->requestorId()] += pkt->getSize();
        if (pkt->req->isInstFetch()) {
            stats.bytesInstRead[pkt->req->requestorId()] += pkt->getSize();
        }
    } else if (pkt->isInvalidate() || pkt->isClean()) {
        assert(!pkt->isWrite());
        // in a fastmem system invalidating and/or cleaning packets
        // can be seen due to cache maintenance requests

        // no need to do anything
    } else if (pkt->isWrite()) {
        if (writeOK(pkt)) {
            if (pmemAddr) {
                pkt->writeData(hostAddr);
                DPRINTF(MemoryAccess, "%s wrote %i bytes to address %x\n",
                        __func__, pkt->getSize(), pkt->getAddr());
            }
            assert(!pkt->req->isInstFetch());
            TRACE_PACKET("Write");
            stats.numWrites[pkt->req->requestorId()]++;
            stats.bytesWritten[pkt->req->requestorId()] += pkt->getSize();
        }
        if (validateAccess) {
            if (!isReady(pkt->getAddr(), pkt->getSize(), false)) {
                panic("Scratchpad write at address: 0x%lx is invalid! "
                      "Sector has not been cleared yet!\n",
                      pkt->getAddr());
            }
        }
        // Set ready bits on external writes
        if (readyMode) {
            Addr start_offset = pkt->getAddr() - range.start();
            Addr end_offset = start_offset + pkt->getSize();
            for (auto i = start_offset; i < end_offset; i++) {
                ready[i] = true;
            }
        }
    } else {
        panic("Unexpected packet %s", pkt->print());
    }

    if (pkt->needsResponse()) {
        pkt->makeResponse();
    }
}

void
ScratchpadMemory::init()
{
    if (port.isConnected()) {
        port.sendRangeChange();
    }
    for (auto p : spm_ports) {
        if (p && p->isConnected()) {
            p->sendRangeChange();
        }
    }
    initial = true;
}

Tick
ScratchpadMemory::recvAtomic(PacketPtr pkt, bool validateAccess)
{
    panic_if(pkt->cacheResponding(), "Should not see packets where cache "
                                     "is responding");

    scratchpadAccess(pkt, validateAccess);
    // Atomic latency: convert accessLatencyCycles
    // to ticks using engine clock.
    const Cycles lat =
        (accessLatencyCycles == Cycles(0)) ? Cycles(0) : accessLatencyCycles;
    return tickEngine->clockPeriod() * lat;
}

Tick
ScratchpadMemory::recvAtomicBackdoor(PacketPtr pkt, MemBackdoorPtr &_backdoor)
{
    Tick latency = recvAtomic(pkt);

    if (backdoor.ptr()) {
        _backdoor = &backdoor;
    }
    return latency;
}

void
ScratchpadMemory::recvFunctional(PacketPtr pkt)
{
    pkt->pushLabel(name());

    functionalAccess(pkt);

    bool done = false;

    for (auto &q : reqQueues) {
        for (auto &e : q) {
            if (done) {
                break;
            }
            done = pkt->trySatisfyFunctional(e.pkt);
        }
        if (done) {
            break;
        }
    }
    if (!done) {
        for (auto &inf : inFlight) {
            if (done) {
                break;
            }
            if (inf.active && inf.pkt) {
                done = pkt->trySatisfyFunctional(inf.pkt);
            }
        }
    }
    if (!done) {
        for (auto &q : respQueues) {
            for (auto &e : q) {
                if (done) {
                    break;
                }
                done = pkt->trySatisfyFunctional(e.pkt);
            }
            if (done) {
                break;
            }
        }
    }

    pkt->popLabel();
}

bool
ScratchpadMemory::recvTimingReq(PacketPtr pkt, PortID recvPort,
                                bool validateAccess)
{
    panic_if(pkt->cacheResponding(), "Should not see packets where cache "
                                     "is responding");

    panic_if(!(pkt->isRead() || pkt->isWrite()),
             "Should only see reads and writes at scratchpad memory, "
             "saw %s to %#llx\n",
             pkt->cmdString(), pkt->getAddr());

    const PortID idx = recvPort + 1;
    ensurePortState(idx);

    // We queue and service later (cycle model)
    pkt->headerDelay = pkt->payloadDelay = 0;
    reqQueues[idx].emplace_back(pkt, validateAccess);
    tickEngine->start();

    return true;
}

void
ScratchpadMemory::recvRespRetry(PortID id)
{
    const PortID idx = id + 1;
    ensurePortState(idx);

    assert(retryResp[idx]);
    retryResp[idx] = false;

    // Attempt to send anything ready immediately;
    // if still blocked, we'll wait
    serviceResponses(idx);

    tickEngine->start();
}

Port &
ScratchpadMemory::getPort(const std::string &if_name, PortID idx)
{
    if (if_name == "port") {
        return port;
    } else if (if_name == "spm_ports") {
        if (idx >= spm_ports.size()) {
            spm_ports.resize((idx + 1), nullptr);
        }
        ensurePortState(idx + 1);
        if (spm_ports[idx] == nullptr) {
            const std::string portName =
                name() + ".spm_ports[" + std::to_string(idx) + "]";
            spm_ports[idx] = new SPMPort(portName, this, idx);
        }
        return *spm_ports[idx];
    }
    return AbstractMemory::getPort(if_name, idx);
}

DrainState
ScratchpadMemory::drain()
{
    for (const auto &q : reqQueues) {
        if (!q.empty()) {
            DPRINTF(
                Drain,
                "ScratchpadMemory has pending requests, waiting to drain\n");
            return DrainState::Draining;
        }
    }
    for (const auto &inf : inFlight) {
        if (inf.active) {
            DPRINTF(Drain, "ScratchpadMemory has an in-flight request, "
                           "waiting to drain\n");
            return DrainState::Draining;
        }
    }
    for (const auto &q : respQueues) {
        if (!q.empty()) {
            DPRINTF(
                Drain,
                "ScratchpadMemory has pending responses, waiting to drain\n");
            return DrainState::Draining;
        }
    }
    for (auto r : retryResp) {
        if (r) {
            DPRINTF(Drain, "ScratchpadMemory awaiting response retry, "
                           "waiting to drain\n");
            return DrainState::Draining;
        }
    }
    return DrainState::Drained;
}

ScratchpadMemory::MemoryPort::MemoryPort(const std::string &_name,
                                         ScratchpadMemory &_memory)
    : ResponsePort(_name), memory(_memory)
{}

AddrRangeList
ScratchpadMemory::MemoryPort::getAddrRanges() const
{
    AddrRangeList ranges;
    ranges.push_back(memory.getAddrRange());
    return ranges;
}

Tick
ScratchpadMemory::MemoryPort::recvAtomic(PacketPtr pkt)
{
    return memory.recvAtomic(pkt);
}

Tick
ScratchpadMemory::MemoryPort::recvAtomicBackdoor(PacketPtr pkt,
                                                 MemBackdoorPtr &_backdoor)
{
    return memory.recvAtomicBackdoor(pkt, _backdoor);
}

void
ScratchpadMemory::MemoryPort::recvFunctional(PacketPtr pkt)
{
    memory.recvFunctional(pkt);
}

bool
ScratchpadMemory::MemoryPort::recvTimingReq(PacketPtr pkt)
{
    return memory.recvTimingReq(pkt, id);
}

void
ScratchpadMemory::MemoryPort::recvRespRetry()
{
    memory.recvRespRetry(id);
}
