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

#include "salam/register_bank.hh"

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/user.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>

#include "base/logging.hh"
#include "base/trace.hh"
#include "debug/Drain.hh"
#include "mem/packet.hh"
#include "mem/packet_access.hh"
#include "sim/system.hh"

using namespace std;

#include "debug/MemoryAccess.hh"

RegisterBank::RegisterBank(const RegisterBankParams &p)
    : AbstractMemory(p),
      port(name() + ".reg_port", this),
      load(name() + ".load_port", this),
      tickEngine(p.tick_engine),
      readLatencyCycles(p.read_latency_cycles),
      writeVisibilityCycles(p.write_visibility_cycles),
      deltaPending(false),
      deltaRemaining(0),
      retryResp(false)
{
    fatal_if(!tickEngine, "RegisterBank %s requires tick_engine\n", name());

    // Bind this register bank as the cycle-ticked owner of the engine.
    tickEngine->setOwner(this);

    // Setup of the delta memory container
    int shm_fd = -1;
    int map_flags = MAP_ANON | MAP_PRIVATE;

    deltaAddr = (uint8_t *)mmap(NULL, range.size(), PROT_READ | PROT_WRITE,
                                map_flags, shm_fd, 0);
    if (deltaAddr == (uint8_t *)MAP_FAILED) {
        perror("mmap");
        fatal("Could not mmap %d bytes for range %s!\n", range.size(),
              range.to_string());
    }
}

Tick
RegisterBank::getAccessLatency(PacketPtr pkt) const
{
    const Cycles c =
        pkt->isWrite() ? writeVisibilityCycles : readLatencyCycles;
    if (c == Cycles(0)) {
        // gem5-SALAM: timing reads are immediate; writes still pay visibility.
        return pkt->isWrite() ? tickEngine->clockPeriod() : 0;
    }
    return tickEngine->clockPeriod() * c;
}

void
RegisterBank::serviceResponses()
{
    if (retryResp) {
        return;
    }

    while (!respQueue.empty() && respQueue.front().remaining == Cycles(0)) {
        retryResp = !port.sendTimingResp(respQueue.front().pkt);
        if (retryResp) {
            return;
        }
        respQueue.pop_front();
    }
}

static inline bool
hasCountdown(const std::deque<RegisterBank::DeferredResp> &q)
{
    for (const auto &e : q) {
        if (e.remaining > Cycles(0)) {
            return true;
        }
    }
    return false;
}

bool
RegisterBank::tickCycle()
{
    // Handle pending delta visibility update
    if (deltaPending) {
        if (deltaRemaining > Cycles(0)) {
            deltaRemaining = deltaRemaining - Cycles(1);
        }
        if (deltaRemaining == Cycles(0)) {
            std::memcpy(pmemAddr, deltaAddr, range.size());
            deltaPending = false;
        }
    }

    // Decrement countdown for queued responses.
    for (auto &e : respQueue) {
        if (e.remaining > Cycles(0)) {
            e.remaining = e.remaining - Cycles(1);
        }
    }

    serviceResponses();
    // Decide if ticking can make progress without an external retry
    // - If retryResp set, can't send the head response until recvRespRetry
    // - If we still have countdowns, keep ticking to decrement them
    const bool countdowns = hasCountdown(respQueue);
    const bool canAttemptSend = (!respQueue.empty() && !retryResp);
    const bool keepTicking = deltaPending || countdowns || canAttemptSend;

    if (!keepTicking) {
        // Fully idle (or only waiting on retry);
        // if fully idle and draining, signal done.
        const bool fullyIdle =
            !deltaPending && respQueue.empty() && !retryResp;
        if (fullyIdle && drainState() == DrainState::Draining) {
            DPRINTF(Drain, "Draining of RegisterBank complete\n");
            signalDrainDone();
        }
        return false;
    }

    return true;
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
RegisterBank::registerAccess(PacketPtr pkt)
{
    assert(AddrRange(pkt->getAddr(), pkt->getAddr() + (pkt->getSize()))
               .isSubset(range));

    if (pkt->isRead()) {
        assert(!pkt->isWrite());
        uint8_t *hostAddr = pmemAddr + pkt->getAddr() - range.start();
        if (pmemAddr) {
            pkt->setData(hostAddr);
        }
        stats.numReads[pkt->req->requestorId()]++;
        stats.bytesRead[pkt->req->requestorId()] += pkt->getSize();
    } else if (pkt->isWrite()) {
        uint8_t *hostAddr = deltaAddr + pkt->getAddr() - range.start();
        if (writeOK(pkt)) {
            if (deltaAddr) {
                pkt->writeData(hostAddr);
                DPRINTF(MemoryAccess, "%s wrote %i bytes to address %x\n",
                        __func__, pkt->getSize(), pkt->getAddr());
            }
            assert(!pkt->req->isInstFetch());
            TRACE_PACKET("Write");
            stats.numWrites[pkt->req->requestorId()]++;
            stats.bytesWritten[pkt->req->requestorId()] += pkt->getSize();

            // Schedule write visibility via cycle engine
            // (do not reschedule if pending)
            if (!deltaPending) {
                deltaPending = true;
                deltaRemaining = (writeVisibilityCycles == Cycles(0))
                                     ? Cycles(1)
                                     : writeVisibilityCycles;
                tickEngine->start();
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
RegisterBank::init()
{
    // allow unconnected memories as this is used in several ruby
    // systems at the moment
    if (port.isConnected()) {
        port.sendRangeChange();
    }
    if (load.isConnected()) {
        load.sendRangeChange();
    }
}

Tick
RegisterBank::recvAtomic(PacketPtr pkt)
{
    panic_if(pkt->cacheResponding(), "Should not see packets where cache "
                                     "is responding");
    registerAccess(pkt);
    return getAccessLatency(pkt);
}

Tick
RegisterBank::recvAtomicBackdoor(PacketPtr pkt, MemBackdoorPtr &_backdoor)
{
    Tick latency = recvAtomic(pkt);

    if (backdoor.ptr()) {
        _backdoor = &backdoor;
    }
    return latency;
}

void
RegisterBank::recvFunctional(PacketPtr pkt)
{
    pkt->pushLabel(name());

    functionalAccess(pkt);

    bool done = false;
    auto p = respQueue.begin();
    // potentially update queued response packets as well
    while (!done && p != respQueue.end()) {
        done = pkt->trySatisfyFunctional(p->pkt);
        ++p;
    }

    pkt->popLabel();
}

bool
RegisterBank::recvTimingReq(PacketPtr pkt)
{
    panic_if(pkt->cacheResponding(), "Should not see packets where cache "
                                     "is responding");

    panic_if(!(pkt->isRead() || pkt->isWrite()),
             "Should only see reads and writes at register bank, "
             "saw %s to %#llx\n",
             pkt->cmdString(), pkt->getAddr());

    const bool needsResponse = pkt->needsResponse();

    // Snapshot data / stage writes now (response is produced later).
    registerAccess(pkt);

    if (needsResponse) {
        const Cycles lat = pkt->isWrite() ? (writeVisibilityCycles == Cycles(0)
                                                 ? Cycles(1)
                                                 : writeVisibilityCycles)
                                          : readLatencyCycles;

        // HLS pipeline registers: reads are combinational (0-cycle).
        // Writes latch at the next clock edge via the tick engine.
        if (pkt->isRead() && lat == Cycles(0)) {
            retryResp = !port.sendTimingResp(pkt);
            if (!retryResp) {
                return true;
            }
            respQueue.emplace_back(pkt, Cycles(0));
            return true;
        }

        respQueue.emplace_back(pkt, lat);
        tickEngine->start();
    }

    return true;
}

void
RegisterBank::recvRespRetry()
{
    assert(retryResp);
    retryResp = false;

    serviceResponses();

    const bool workLeft = deltaPending || !respQueue.empty();
    if (!workLeft && !retryResp) {
        // Corner-case: retry cleared last response immediately;
        // no more ticks may occur
        if (drainState() == DrainState::Draining) {
            DPRINTF(Drain, "Draining of RegisterBank complete\n");
            signalDrainDone();
        }
        return;
    }

    if (workLeft && !retryResp) {
        tickEngine->start();
    }
}

Port &
RegisterBank::getPort(const std::string &if_name, PortID idx)
{
    if (if_name == "reg_port") {
        return port;
    } else if (if_name == "load_port") {
        return load;
    }
    return AbstractMemory::getPort(if_name, idx);
}

DrainState
RegisterBank::drain()
{
    if (!respQueue.empty() || retryResp || deltaPending) {
        DPRINTF(Drain, "RegisterBank has in-flight work, waiting to drain\n");
        return DrainState::Draining;
    }
    return DrainState::Drained;
}
