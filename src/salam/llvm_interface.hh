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

#ifndef __SALAM_LLVM_INTERFACE_HH__
#define __SALAM_LLVM_INTERFACE_HH__

// C++ Includes
#include <algorithm>
#include <chrono>
#include <ctime>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <queue>
#include <ratio>
#include <type_traits>
#include <typeinfo>

// LLVM Includes
#include <llvm-c/Core.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Transforms/Utils/Cloning.h>

// SALAM Includes
#include "params/LLVMInterface.hh"
#include "salam/HWModeling/hw_interface.hh"
#include "salam/LLVMRead/basic_block.hh"
#include "salam/LLVMRead/debug_flags.hh"
#include "salam/LLVMRead/function.hh"
#include "salam/LLVMRead/operand.hh"
#include "salam/acc_compute_unit.hh"

class LLVMInterface : public AccComputeUnit
{
  private:
    std::string filename;
    std::string topName;
    uint32_t scheduling_threshold;
    int cycle;
    // Debug / sanity metric only
    // Counts compute-domain runtime cycles where there was active work
    // in-flight, but no forward progress this cycle
    int stalls;

    // Simulated accelerator metrics
    uint64_t dynInstsIssued = 0;
    uint64_t dynInstsCommitted = 0;
    uint64_t dynLoadsIssued = 0;
    uint64_t dynLoadsCommitted = 0;
    uint64_t dynStoresIssued = 0;
    uint64_t dynStoresCommitted = 0;

    // Compute launch accounting:
    //   Attempts  = scheduler tried to launch a compute op
    //   Launched  = launch accepted (immediate commit or in-flight)
    //   Committed = compute op retired
    uint64_t dynComputeLaunchAttempts = 0;
    uint64_t dynComputeLaunched = 0;
    uint64_t dynComputeCommitted = 0;

    uint64_t dynCallsIssued = 0;
    uint64_t dynCallsCommitted = 0;
    uint64_t dynInternalLoadCompletions = 0;

    enum class CycleCause : uint8_t
    {
        UsefulCompute = 0,
        UsefulMemory,
        UsefulControl,
        DependencyStall,
        FuCapacityStall,
        ComputeLatencyWait,
        MemoryServiceWait,
        MemoryIssueBackpressure,
        ComputeAndMemoryOutstandingWait,
        SchedulingBlocked,
        Idle
    };

    struct CycleSignals
    {
        bool anyReservationWork = false;
        bool anyOutstandingMemory = false;
        bool anyOutstandingCompute = false;

        bool anyReadyMemory = false;
        bool anyReadyCompute = false;

        bool dependencyBlocked = false;
        bool fuDenied = false;
        bool lockstepBlocked = false;
        bool thresholdBlocked = false;
        bool callBlocked = false;
        bool loadRawHazard = false;

        bool unissuedMemoryReq = false;

        bool reservationNonEmpty = false;
        bool readQueueNonEmpty = false;
        bool writeQueueNonEmpty = false;
        bool computeQueueNonEmpty = false;

        void
        reset()
        {
            *this = CycleSignals{};
        }
    };

    CycleSignals cycleSignals;

    // Disjoint cycle breakdown. These must sum to runtime cycles
    uint64_t usefulComputeCycles = 0;
    uint64_t usefulMemoryCycles = 0;
    uint64_t usefulControlCycles = 0;
    uint64_t dependencyStallCycles = 0;
    uint64_t fuCapacityStallCycles = 0;
    uint64_t computeLatencyWaitCycles = 0;
    uint64_t memoryServiceWaitCycles = 0;
    uint64_t memoryIssueBackpressureCycles = 0;
    uint64_t unissuedMemoryReqCycles = 0;
    uint64_t computeAndMemoryOutstandingWaitCycles = 0;
    uint64_t schedulingBlockedCycles = 0;
    uint64_t idleCycles = 0;

    // Overlapping diagnostic cycle counters
    uint64_t loadRawHazardCycles = 0;
    uint64_t callWaitCycles = 0;
    uint64_t thresholdBlockedCycles = 0;
    uint64_t lockstepBlockedCycles = 0;
    uint64_t memoryBackpressureCycles = 0;
    uint64_t allPortsStalledCycles = 0;
    uint64_t portRetryCycles = 0;
    uint64_t reservationNonEmptyCycles = 0;
    uint64_t readQueueNonEmptyCycles = 0;
    uint64_t writeQueueNonEmptyCycles = 0;
    uint64_t computeQueueNonEmptyCycles = 0;

    uint64_t invocationCount = 0;

    uint64_t aggCycles = 0;
    uint64_t aggDynInstsIssued = 0;
    uint64_t aggDynInstsCommitted = 0;
    uint64_t aggDynLoadsIssued = 0;
    uint64_t aggDynLoadsCommitted = 0;
    uint64_t aggDynStoresIssued = 0;
    uint64_t aggDynStoresCommitted = 0;
    uint64_t aggDynComputeLaunchAttempts = 0;
    uint64_t aggDynComputeLaunched = 0;
    uint64_t aggDynComputeCommitted = 0;
    uint64_t aggDynCallsIssued = 0;
    uint64_t aggDynCallsCommitted = 0;
    uint64_t aggInternalLoadCompletions = 0;
    uint64_t aggExternalLoadCompletions = 0;

    uint64_t aggUsefulComputeCycles = 0;
    uint64_t aggUsefulMemoryCycles = 0;
    uint64_t aggUsefulControlCycles = 0;
    uint64_t aggDependencyStallCycles = 0;
    uint64_t aggFuCapacityStallCycles = 0;
    uint64_t aggComputeLatencyWaitCycles = 0;
    uint64_t aggMemoryServiceWaitCycles = 0;
    uint64_t aggMemoryIssueBackpressureCycles = 0;
    uint64_t aggUnissuedMemoryReqCycles = 0;
    uint64_t aggComputeAndMemoryOutstandingWaitCycles = 0;
    uint64_t aggSchedulingBlockedCycles = 0;
    uint64_t aggIdleCycles = 0;

    bool windowStatsEnable;
    uint32_t windowSize;

    struct WindowStats
    {
        uint64_t cycles = 0;
        uint64_t usefulCompute = 0;
        uint64_t usefulMemory = 0;
        uint64_t usefulControl = 0;
        uint64_t depStall = 0;
        uint64_t fuStall = 0;
        uint64_t cmpWait = 0;
        uint64_t memWait = 0;
        uint64_t memBpWait = 0;
        uint64_t bothWait = 0;
        uint64_t schedBlocked = 0;
        uint64_t idle = 0;
    };

    WindowStats curWindow;
    uint64_t windowIndex = 0;

    bool running;
    bool loadOpScheduled;
    bool storeOpScheduled;
    bool compOpScheduled;
    bool lockstep;
    bool dbg;
    std::chrono::duration<float> setupTime;
    std::chrono::duration<float> simTotal;
    std::chrono::duration<float> simTime;
    std::chrono::duration<float> schedulingTime;
    std::chrono::duration<float> queueProcessTime;
    std::chrono::duration<float> computeTime;
    std::chrono::duration<float> hwTime;
    std::chrono::high_resolution_clock::time_point simStop;
    std::chrono::high_resolution_clock::time_point setupStop;
    std::chrono::high_resolution_clock::time_point timeStart;

    class ActiveFunction
    {
        friend class LLVMInterface;

      private:
        LLVMInterface *owner;
        HWInterface *hw;
        std::shared_ptr<SALAM::Function> func;
        std::shared_ptr<SALAM::Instruction> caller;
        std::list<std::shared_ptr<SALAM::Instruction>> reservation;
        std::map<uint64_t, std::shared_ptr<SALAM::Instruction>> readQueue;
        std::map<MemoryRequest *, uint64_t> readQueueMap;
        std::map<uint64_t, std::shared_ptr<SALAM::Instruction>> writeQueue;
        std::map<MemoryRequest *, uint64_t> writeQueueMap;
        std::map<uint64_t, std::shared_ptr<SALAM::Instruction>> computeQueue;
        std::shared_ptr<SALAM::BasicBlock> previousBB;
        uint32_t scheduling_threshold;
        bool returned = false;
        bool lockstep;
        bool dbg;

        inline bool
        uidActive(uint64_t id)
        {
            return computeUIDActive(id) || readUIDActive(id) ||
                   writeUIDActive(id);
        }

        std::map<Addr, std::shared_ptr<SALAM::Instruction>> activeWrites;
        inline void
        trackWrite(Addr writeAddr,
                   std::shared_ptr<SALAM::Instruction> writeInst)
        {
            activeWrites.insert({writeAddr, writeInst});
        }
        inline void
        untrackWrite(uint64_t writeAddr)
        {
            auto it = activeWrites.find(writeAddr);
            if (it != activeWrites.end()) {
                activeWrites.erase(it);
            }
        }
        inline bool
        writeActive(uint64_t writeAddr)
        {
            return (activeWrites.find(writeAddr) != activeWrites.end());
        }

        inline std::shared_ptr<SALAM::Instruction>
        getActiveWrite(uint64_t writeAddr)
        {
            return activeWrites.find(writeAddr)->second;
        }
        inline bool
        writeUIDActive(uint64_t uid)
        {
            return (writeQueue.find(uid) != writeQueue.end());
        }
        inline bool
        readUIDActive(uint64_t uid)
        {
            return (readQueue.find(uid) != readQueue.end());
        }
        inline bool
        computeUIDActive(uint64_t uid)
        {
            return (computeQueue.find(uid) != computeQueue.end());
        }
        inline bool
        hasUnissuedMemRequests() const
        {
            for (const auto &it : readQueueMap) {
                if (!it.first->hasBeenIssued()) {
                    return true;
                }
            }
            for (const auto &it : writeQueueMap) {
                if (!it.first->hasBeenIssued()) {
                    return true;
                }
            }
            return false;
        }

        int
        reservationDepth() const
        {
            return reservation.size();
        }
        int
        readDepth() const
        {
            return readQueue.size();
        }
        int
        writeDepth() const
        {
            return writeQueue.size();
        }
        int
        computeDepth() const
        {
            return computeQueue.size();
        }

      public:
        ActiveFunction(LLVMInterface *_owner,
                       std::shared_ptr<SALAM::Function> _func,
                       std::shared_ptr<SALAM::Instruction> _caller)
            : owner(_owner), func(_func), caller(_caller), previousBB(nullptr)
        {
            scheduling_threshold = owner->getSchedulingThreshold();
            lockstep = (owner->getLockstepStatus());
            dbg = owner->debug();
        }
        void readCommit(MemoryRequest *req);
        void writeCommit(MemoryRequest *req);
        void findDynamicDeps(std::shared_ptr<SALAM::Instruction> inst);
        void scheduleBB(std::shared_ptr<SALAM::BasicBlock> bb);
        void processQueues();
        void launch();
        inline bool
        queuesClear()
        {
            return readQueue.empty() && writeQueue.empty() &&
                   computeQueue.empty();
        }
        inline bool
        lockstepReady()
        {
            return !lockstep || queuesClear();
        }
        inline bool
        canReturn()
        {
            return queuesClear() && !reservation.empty() &&
                   reservation.front()->isReturn();
        }
        void launchRead(std::shared_ptr<SALAM::Instruction> readInst);
        void launchWrite(std::shared_ptr<SALAM::Instruction> writeInst);
        bool
        hasReturned()
        {
            return returned;
        }
    };

    std::list<ActiveFunction> activeFunctions;
    std::map<MemoryRequest *, ActiveFunction *> globalReadQueue;
    std::map<MemoryRequest *, ActiveFunction *> globalWriteQueue;

    // One owner-level cycle record per LLVMInterface tick
    HW_Cycle_Stats tick_hw_cycle_stats;

    // Async memory callbacks can arrive outside the queue-processing flow
    // Hold exact commit events here until the current owner tick is finalized
    int pendingLoadsCommitted = 0;
    int pendingStoresCommitted = 0;

    inline void
    foldPendingMemStatsIntoTickCycle()
    {
        tick_hw_cycle_stats.loadsCommitted += pendingLoadsCommitted;
        tick_hw_cycle_stats.storesCommitted += pendingStoresCommitted;
    }

    inline void
    clearPendingMemStats()
    {
        pendingLoadsCommitted = 0;
        pendingStoresCommitted = 0;
    }

    std::vector<std::shared_ptr<SALAM::Function>> functions;
    std::vector<std::shared_ptr<SALAM::Value>> values;

  protected:
    virtual bool
    debug()
    {
        return comm->debug();
    }

  public:
    PARAMS(LLVMInterface);
    LLVMInterface(const LLVMInterfaceParams &p);
    void tick();
    void constructStaticGraph();
    void startup();
    void initialize();
    void finalize();
    void debug(uint64_t flags);
    bool
    getLockstepStatus()
    {
        return lockstep;
    }
    void readCommit(MemoryRequest *req);
    void writeCommit(MemoryRequest *req);
    void dumpModule(llvm::Module *m);
    void printResults();
    void emitSummaryLine() const;
    void snapshotQueueDepthStart();
    void snapshotQueueDepthEnd();
    void sampleQueueDepthPeaks();
    int totalReservationDepth() const;
    int totalReadDepth() const;
    int totalWriteDepth() const;
    int totalComputeDepth() const;
    void captureCommInterfaceCycleStats();
    void captureFuCycleStats();
    void captureComputeMemoryOverlapStats();
    void printTrafficSummary(const HW_Stats_Summary &summary) const;
    void printResourceSummary(const HW_Stats_Summary &summary) const;
    void printOverlapSummary(const HW_Stats_Summary &summary) const;
    void printQueueSummary(const HW_Stats_Summary &summary) const;
    void updateWindowStats(CycleCause cause);
    void emitWindowSummary() const;
    void resetWindowStats();
    void rollUpCurrentInvocationIntoAggregate();
    bool hasCurrentInvocationData() const;
    uint64_t disjointCycleBreakdownTotal() const;
    void printDisjointCycleBreakdown() const;
    void launchFunction(std::shared_ptr<SALAM::Function> callee,
                        std::shared_ptr<SALAM::Instruction> caller);
    void launchTopFunction();
    void endFunction(ActiveFunction *afunc);
    void launchRead(MemoryRequest *memReq, ActiveFunction *func);
    void launchWrite(MemoryRequest *memReq, ActiveFunction *func);
    std::shared_ptr<SALAM::Instruction>
    createInstruction(llvm::Instruction *inst, uint64_t id);
    void dumpQueues();
    uint32_t
    getSchedulingThreshold()
    {
        return scheduling_threshold;
    }
    void
    addSchedulingTime(std::chrono::duration<float> timeDelta)
    {
        schedulingTime = schedulingTime + timeDelta;
    }
    void
    addQueueTime(std::chrono::duration<float> timeDelta)
    {
        queueProcessTime = queueProcessTime + timeDelta;
    }
    void
    addComputeTime(std::chrono::duration<float> timeDelta)
    {
        computeTime = computeTime + timeDelta;
    }
    void
    addHWTime(std::chrono::duration<float> timeDelta)
    {
        hwTime = hwTime + timeDelta;
    }
    // uint64_t
    // executedNodesDebug() const
    // {
    //     return dynInstsCommitted;
    // }
};

#endif //__SALAM_LLVM_INTERFACE_HH__
