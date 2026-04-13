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

#ifndef __HWMODEL_HW_STATISTICS_HH__
#define __HWMODEL_HW_STATISTICS_HH__

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

#include "params/HWStatistics.hh"
#include "salam/LLVMRead/debug_flags.hh"
#include "sim/sim_object.hh"

using namespace gem5;

enum class SalamMemTargetClass : uint8_t
{
    Local = 0,
    Global,
    SPM,
    Stream,
    NumTargetClasses
};

enum class SalamAccessKind : uint8_t
{
    Read = 0,
    Write,
    NumAccessKinds
};

enum class SalamFUClass : uint8_t
{
    IntAdder = 0,
    IntMultiplier,
    IntShifter,
    IntBitwise,
    FpSpAdder,
    FpDpAdder,
    FpSpMultiplier,
    FpSpDivider,
    FpDpMultiplier,
    FpDpDivider,
    Register,
    NumFuClasses
};

static constexpr size_t kNumTargetClasses =
    static_cast<size_t>(SalamMemTargetClass::NumTargetClasses);
static constexpr size_t kNumAccessKinds =
    static_cast<size_t>(SalamAccessKind::NumAccessKinds);
static constexpr size_t kNumFuClasses =
    static_cast<size_t>(SalamFUClass::NumFuClasses);

// Things here are output only once at end of simulation
struct HW_Params
{
    int run_end;

    void
    reset()
    {
        run_end = 0;
    }
};

// These are outputs that are stored each cycle
struct HW_Cycle_Stats
{
    int cycle;

    // Exact queue depths sampled once per owner tick.
    int reservationDepth;
    int readQueueDepth;
    int writeQueueDepth;
    int computeQueueDepth;

    // Exact event counters for this owner tick.
    int internalLoadCompletions;
    int loadsIssued;
    int loadsCommitted;
    int storesIssued;
    int storesCommitted;
    int computeLaunchAttempts;
    int computeLaunchesAccepted;
    int computeCommitted;
    int callsIssued;
    int callsCommitted;

    // Exact owner-level boolean cycle conditions.
    bool hadLoadRawHazard;
    bool hadFuCapacityDeny;
    bool hadCallWait;
    bool hadThresholdBlock;
    bool hadLockstepBlock;
    bool hadIssueBackpressure;
    bool hadAllPortsStalled;
    bool hadPortRetry;

    bool hadOutstandingMemory;
    bool hadOutstandingCompute;
    bool hadReadyMemory;
    bool hadReadyCompute;

    bool reservationNonEmpty;
    bool readQueueNonEmpty;
    bool writeQueueNonEmpty;
    bool computeQueueNonEmpty;

    int reservationDepthStart;
    int reservationDepthEnd;
    int reservationDepthPeak;

    int readQueueDepthStart;
    int readQueueDepthEnd;
    int readQueueDepthPeak;

    int writeQueueDepthStart;
    int writeQueueDepthEnd;
    int writeQueueDepthPeak;

    int computeQueueDepthStart;
    int computeQueueDepthEnd;
    int computeQueueDepthPeak;

    bool hadReadIssueBackpressure;
    bool hadWriteIssueBackpressure;
    bool hadReadRetry;
    bool hadWriteRetry;

    bool hadComputeAndMemoryOutstanding;
    bool hadComputeAndMemoryReady;

    // Issue-attempt counters: counted when a request enters the interface
    // path (after sendPacket), not on downstream acceptance.  Totals are
    // accurate; per-cycle timing is slightly optimistic for stalled sends.
    uint64_t memOps[kNumTargetClasses][kNumAccessKinds];
    uint64_t memBytes[kNumTargetClasses][kNumAccessKinds];

    uint64_t fuBusySlots[kNumFuClasses];
    uint64_t fuPeakBusySlots[kNumFuClasses];
    uint64_t fuAccepted[kNumFuClasses];
    uint64_t fuDenied[kNumFuClasses];

    void
    reset()
    {
        cycle = 0;

        reservationDepth = 0;
        readQueueDepth = 0;
        writeQueueDepth = 0;
        computeQueueDepth = 0;

        internalLoadCompletions = 0;
        loadsIssued = 0;
        loadsCommitted = 0;
        storesIssued = 0;
        storesCommitted = 0;
        computeLaunchAttempts = 0;
        computeLaunchesAccepted = 0;
        computeCommitted = 0;
        callsIssued = 0;
        callsCommitted = 0;

        hadLoadRawHazard = false;
        hadFuCapacityDeny = false;
        hadCallWait = false;
        hadThresholdBlock = false;
        hadLockstepBlock = false;
        hadIssueBackpressure = false;
        hadAllPortsStalled = false;
        hadPortRetry = false;

        hadOutstandingMemory = false;
        hadOutstandingCompute = false;
        hadReadyMemory = false;
        hadReadyCompute = false;

        reservationNonEmpty = false;
        readQueueNonEmpty = false;
        writeQueueNonEmpty = false;
        computeQueueNonEmpty = false;

        reservationDepthStart = 0;
        reservationDepthEnd = 0;
        reservationDepthPeak = 0;

        readQueueDepthStart = 0;
        readQueueDepthEnd = 0;
        readQueueDepthPeak = 0;

        writeQueueDepthStart = 0;
        writeQueueDepthEnd = 0;
        writeQueueDepthPeak = 0;

        computeQueueDepthStart = 0;
        computeQueueDepthEnd = 0;
        computeQueueDepthPeak = 0;

        hadReadIssueBackpressure = false;
        hadWriteIssueBackpressure = false;
        hadReadRetry = false;
        hadWriteRetry = false;

        hadComputeAndMemoryOutstanding = false;
        hadComputeAndMemoryReady = false;

        std::memset(memOps, 0, sizeof(memOps));
        std::memset(memBytes, 0, sizeof(memBytes));
        std::memset(fuBusySlots, 0, sizeof(fuBusySlots));
        std::memset(fuPeakBusySlots, 0, sizeof(fuPeakBusySlots));
        std::memset(fuAccepted, 0, sizeof(fuAccepted));
        std::memset(fuDenied, 0, sizeof(fuDenied));
    }
};

struct HW_Stats_Summary
{
    uint64_t cyclesRecorded = 0;

    // Exact event totals.
    uint64_t totalInternalLoadCompletions = 0;
    uint64_t totalLoadsIssued = 0;
    uint64_t totalLoadsCommitted = 0;
    uint64_t totalStoresIssued = 0;
    uint64_t totalStoresCommitted = 0;
    uint64_t totalComputeAttempts = 0;
    uint64_t totalComputeLaunchesAccepted = 0;
    uint64_t totalComputeCommitted = 0;
    uint64_t totalCallsIssued = 0;
    uint64_t totalCallsCommitted = 0;

    // Queue-depth aggregates.
    uint64_t reservationDepthSum = 0;
    uint64_t readQueueDepthSum = 0;
    uint64_t writeQueueDepthSum = 0;
    uint64_t computeQueueDepthSum = 0;

    uint64_t maxReservationDepth = 0;
    uint64_t maxReadQueueDepth = 0;
    uint64_t maxWriteQueueDepth = 0;
    uint64_t maxComputeQueueDepth = 0;

    // Exact owner-level boolean cycle totals.
    uint64_t loadRawHazardCycles = 0;
    uint64_t fuCapacityDenyCycles = 0;
    uint64_t callWaitCycles = 0;
    uint64_t thresholdBlockedCycles = 0;
    uint64_t lockstepBlockedCycles = 0;
    uint64_t issueBackpressureCycles = 0;
    uint64_t allPortsStalledCycles = 0;
    uint64_t portRetryCycles = 0;

    uint64_t outstandingMemoryCycles = 0;
    uint64_t outstandingComputeCycles = 0;
    uint64_t readyMemoryCycles = 0;
    uint64_t readyComputeCycles = 0;

    uint64_t reservationNonEmptyCycles = 0;
    uint64_t readQueueNonEmptyCycles = 0;
    uint64_t writeQueueNonEmptyCycles = 0;
    uint64_t computeQueueNonEmptyCycles = 0;

    uint64_t reservationDepthEndSum = 0;
    uint64_t readQueueDepthEndSum = 0;
    uint64_t writeQueueDepthEndSum = 0;
    uint64_t computeQueueDepthEndSum = 0;

    uint64_t reservationDepthPeakSum = 0;
    uint64_t readQueueDepthPeakSum = 0;
    uint64_t writeQueueDepthPeakSum = 0;
    uint64_t computeQueueDepthPeakSum = 0;

    uint64_t maxReservationDepthPeak = 0;
    uint64_t maxReadQueueDepthPeak = 0;
    uint64_t maxWriteQueueDepthPeak = 0;
    uint64_t maxComputeQueueDepthPeak = 0;

    uint64_t readIssueBackpressureCycles = 0;
    uint64_t writeIssueBackpressureCycles = 0;
    uint64_t readRetryCycles = 0;
    uint64_t writeRetryCycles = 0;

    uint64_t computeAndMemoryOutstandingCycles = 0;
    uint64_t computeAndMemoryReadyCycles = 0;

    uint64_t totalMemOps[kNumTargetClasses][kNumAccessKinds] = {};
    uint64_t totalMemBytes[kNumTargetClasses][kNumAccessKinds] = {};

    uint64_t fuBusySlotSum[kNumFuClasses] = {};
    uint64_t fuPeakBusySlots[kNumFuClasses] = {};
    uint64_t fuAccepted[kNumFuClasses] = {};
    uint64_t fuDenied[kNumFuClasses] = {};
    uint64_t fuActiveCycles[kNumFuClasses] = {};
};

class HWStatistics : public SimObject
{
  private:
    HW_Params hw_params;
    std::vector<std::vector<HW_Cycle_Stats>> hw_buffer_list;

    bool cycle_tracking = false;
    bool dbg;
    int statBufferSize;
    int statBufferPreDefine;

    int current_buffer_index = 0;

  public:
    HWStatistics();
    HWStatistics(const HWStatisticsParams &params);

    bool
    use_cycle_tracking()
    {
        return cycle_tracking;
    }

    void print(bool dump_cycles = false);
    HW_Stats_Summary summarize() const;
    void recordCycle(const HW_Cycle_Stats &stats);
    void updateBuffer();
    void resetRun();
};

#endif //__HWMODEL_HW_STATISTICS_HH__
