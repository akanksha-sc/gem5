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

#ifndef __SALAM_SCRATCHPAD_MEMORY_HH__
#define __SALAM_SCRATCHPAD_MEMORY_HH__

#include <deque>
#include <vector>

#include "mem/abstract_mem.hh"
#include "mem/port.hh"
#include "salam/tick_engine.hh"

using namespace gem5;
using namespace memory;

class ScratchpadRequestPort;

class ScratchpadResponsePort : public ResponsePort
{
    friend class ScratchpadRequestPort;

  private:
  public:
    ScratchpadResponsePort(const std::string &_name, SimObject *_owner,
                           PortID id = InvalidPortID)
        : ResponsePort(_name, id)
    {}

  protected:
    virtual bool
    canAccess(Addr add, size_t len, bool read)
    {
        return true;
    }
    Tick
    recvAtomic(PacketPtr pkt) override
    {
        return 0;
    }
    Tick
    recvAtomicBackdoor(PacketPtr pkt, MemBackdoorPtr &_backdoor) override
    {
        return 0;
    }
    void
    recvFunctional(PacketPtr pkt) override
    {}
    bool
    recvTimingReq(PacketPtr pkt) override
    {
        return false;
    }
    void
    recvRespRetry() override
    {}
    AddrRangeList
    getAddrRanges() const override
    {
        AddrRangeList range;
        return range;
    }
    virtual void
    setReadyStatus(bool r)
    {}
};

class ScratchpadRequestPort : public RequestPort
{
  private:
    ScratchpadResponsePort *_spmresp;

  protected:
    //
  public:
    ScratchpadRequestPort(const std::string &_name, SimObject *_owner,
                          PortID id = InvalidPortID)
        : RequestPort(_name, id)
    {}
    void
    setReadyStatus(bool r)
    {
        _spmresp->setReadyStatus(r);
    }
    bool
    canAccess(Addr add, size_t len, bool read)
    {
        return _spmresp->canAccess(add, len, read);
    }
    void
    bind(Port &peer) override
    {
        auto *spmresp = dynamic_cast<ScratchpadResponsePort *>(&peer);
        if (spmresp) {
            _spmresp = spmresp;
        }
        RequestPort::bind(peer);
    }
    void
    unbind() override
    {
        _spmresp = nullptr;
        RequestPort::unbind();
    }
};

#include "params/ScratchpadMemory.hh"

class ScratchpadMemory : public AbstractMemory, public SALAM::CycleTicked
{
  protected:
    bool readyMode;
    bool readOnInvalid;
    bool writeOnValid;
    bool resetOnScratchpadRead;
    bool initial;
    bool *ready;

  public:
    PARAMS(ScratchpadMemory);
    ScratchpadMemory(const ScratchpadMemoryParams &p);
    bool isReady(Addr ad, size_t size, bool read);
    void scratchpadAccess(PacketPtr pkt, bool validateAccess = false);
    void setAllReady(bool r);

  private:
    class MemoryPort : public ResponsePort
    {
      private:
        ScratchpadMemory &memory;

      public:
        MemoryPort(const std::string &_name, ScratchpadMemory &_memory);

      protected:
        Tick recvAtomic(PacketPtr pkt) override;
        Tick recvAtomicBackdoor(PacketPtr pkt,
                                MemBackdoorPtr &_backdoor) override;
        void recvFunctional(PacketPtr pkt) override;
        bool recvTimingReq(PacketPtr pkt) override;
        void recvRespRetry() override;
        AddrRangeList getAddrRanges() const override;
    };

    MemoryPort port;

    class SPMPort : public ScratchpadResponsePort
    {
      private:
        ScratchpadMemory *memory;

      public:
        SPMPort(const std::string &_name, ScratchpadMemory *_memory,
                PortID id = InvalidPortID)
            : ScratchpadResponsePort(_name, _memory, id), memory(_memory)
        {}

      protected:
        bool
        canAccess(Addr add, size_t len, bool read) override
        {
            return memory->isReady(add, len, read);
        }
        Tick
        recvAtomic(PacketPtr pkt) override
        {
            return memory->recvAtomic(pkt, true);
        };
        Tick
        recvAtomicBackdoor(PacketPtr pkt, MemBackdoorPtr &_backdoor) override
        {
            return memory->recvAtomicBackdoor(pkt, _backdoor);
        };
        void
        recvFunctional(PacketPtr pkt) override
        {
            memory->recvFunctional(pkt);
        };
        bool
        recvTimingReq(PacketPtr pkt) override
        {
            return memory->recvTimingReq(pkt, id, true);
        };
        void
        recvRespRetry() override
        {
            memory->recvRespRetry(id);
        };
        AddrRangeList
        getAddrRanges() const override
        {
            AddrRangeList ranges;
            ranges.push_back(memory->getAddrRange());
            return ranges;
        }
        void
        setReadyStatus(bool r) override
        {
            memory->setAllReady(r);
        }
    };

    std::vector<SPMPort *> spm_ports;

    /**
     * Cycle tick engine used to drive internal scratchpad timing.
     */
    SALAMTickEngine *tickEngine;

    /**
     * Cycle-based timing and throughput parameters.
     */
    const Cycles accessLatencyCycles;
    const unsigned bytesPerCycle;
    const unsigned maxReqsPerCycle;

    struct PendingReq
    {
        PacketPtr pkt;
        bool validateAccess;

        PendingReq(PacketPtr _pkt, bool _validate)
            : pkt(_pkt), validateAccess(_validate)
        {}
    };

    // multi-cycle service of a single request via per-port in-flight slot
    struct InFlightReq
    {
        PacketPtr pkt = nullptr;
        bool validateAccess = false;
        bool needsResponse = false;
        Cycles remainingXfer = Cycles(0);
        bool active = false;
    };

    struct DeferredResp
    {
        PacketPtr pkt;
        Cycles remaining;

        DeferredResp(PacketPtr _pkt, Cycles _remaining)
            : pkt(_pkt), remaining(_remaining)
        {}
    };

    /**
     * Per-port request and response queues
     * (index 0 is .port, 1..N are spm_ports)
     */
    std::vector<std::deque<PendingReq>> reqQueues;
    std::vector<InFlightReq> inFlight;
    std::vector<std::deque<DeferredResp>> respQueues;

    /**
     * Per-port response retry tracking.
     */
    std::vector<bool> retryResp;

    /**
     * Packets with no response are deleted after service.
     */
    std::deque<PacketPtr> pendingDelete;

    void ensurePortState(PortID idx);
    void serviceResponses(PortID idx);
    void startInFlight(PortID idx);
    void completeInFlight(PortID idx);

  public:
    // SALAM: Per-cycle callback driven by SALAMTickEngine.
    bool tickCycle() override;

    DrainState drain() override;

    Port &getPort(const std::string &if_name,
                  PortID idx = InvalidPortID) override;
    void init() override;

  protected:
    Tick recvAtomic(PacketPtr pkt, bool validateAccess = false);
    Tick recvAtomicBackdoor(PacketPtr pkt, MemBackdoorPtr &_backdoor);
    void recvFunctional(PacketPtr pkt);
    bool recvTimingReq(PacketPtr pkt, PortID recvPort,
                       bool validateAccess = false);
    void recvRespRetry(PortID id);
};

#endif //__SALAM_SCRATCHPAD_MEMORY_HH__
