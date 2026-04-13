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

#include "hw_statistics.hh"

HWStatistics::HWStatistics(const HWStatisticsParams &params)
    : SimObject(params)
{
    statBufferSize = params.stat_buffer_size;
    statBufferPreDefine = params.stat_buffer_predefine;
    dbg = params.debug;
    cycle_tracking = params.cycle_tracking;

    hw_buffer_list.reserve(statBufferPreDefine);

    for (int i = 0; i < statBufferPreDefine; i++) {
        std::vector<HW_Cycle_Stats> hw_cycle_buffer;
        hw_cycle_buffer.reserve(statBufferSize);
        hw_buffer_list.push_back(std::move(hw_cycle_buffer));
    }

    current_buffer_index = 0;
}

void
HWStatistics::recordCycle(const HW_Cycle_Stats &stats)
{
    if (!cycle_tracking) {
        return;
    }

    if (dbg) {
        DPRINTF(SALAM_Debug, "Recording Cycle Statistics\n");
    }

    if (hw_buffer_list.empty()) {
        std::vector<HW_Cycle_Stats> hw_cycle_buffer;
        hw_cycle_buffer.reserve(statBufferSize);
        hw_buffer_list.push_back(std::move(hw_cycle_buffer));
        current_buffer_index = 0;
    }

    auto &buffer = hw_buffer_list.at(current_buffer_index);
    buffer.push_back(stats);
    updateBuffer();
}

void
HWStatistics::updateBuffer()
{
    if (dbg) {
        DPRINTF(SALAM_Debug, "Checking Buffer[%i][%i]\n", current_buffer_index,
                hw_buffer_list.at(current_buffer_index).size());
    }

    if (hw_buffer_list.at(current_buffer_index).size() < statBufferSize) {
        return;
    }

    current_buffer_index++;

    if (current_buffer_index == (int)hw_buffer_list.size()) {
        if (dbg) {
            DPRINTF(SALAM_Debug, "Creating New Buffer Window\n");
        }

        std::vector<HW_Cycle_Stats> hw_cycle_buffer;
        hw_cycle_buffer.reserve(statBufferSize);
        hw_buffer_list.push_back(std::move(hw_cycle_buffer));
    } else if (dbg) {
        DPRINTF(SALAM_Debug, "Next Buffer Window\n");
    }
}

void
HWStatistics::resetRun()
{
    current_buffer_index = 0;

    for (auto &buffer : hw_buffer_list) {
        buffer.clear();
    }
}

HW_Stats_Summary
HWStatistics::summarize() const
{
    HW_Stats_Summary summary;

    for (const auto &buffer : hw_buffer_list) {
        for (const auto &cycle : buffer) {
            summary.cyclesRecorded++;

            summary.totalInternalLoadCompletions +=
                cycle.internalLoadCompletions;
            summary.totalLoadsIssued += cycle.loadsIssued;
            summary.totalLoadsCommitted += cycle.loadsCommitted;
            summary.totalStoresIssued += cycle.storesIssued;
            summary.totalStoresCommitted += cycle.storesCommitted;
            summary.totalComputeAttempts += cycle.computeLaunchAttempts;
            summary.totalComputeLaunchesAccepted +=
                cycle.computeLaunchesAccepted;
            summary.totalComputeCommitted += cycle.computeCommitted;
            summary.totalCallsIssued += cycle.callsIssued;
            summary.totalCallsCommitted += cycle.callsCommitted;

            summary.reservationDepthSum += cycle.reservationDepth;
            summary.readQueueDepthSum += cycle.readQueueDepth;
            summary.writeQueueDepthSum += cycle.writeQueueDepth;
            summary.computeQueueDepthSum += cycle.computeQueueDepth;

            summary.maxReservationDepth = std::max<uint64_t>(
                summary.maxReservationDepth, cycle.reservationDepth);
            summary.maxReadQueueDepth = std::max<uint64_t>(
                summary.maxReadQueueDepth, cycle.readQueueDepth);
            summary.maxWriteQueueDepth = std::max<uint64_t>(
                summary.maxWriteQueueDepth, cycle.writeQueueDepth);
            summary.maxComputeQueueDepth = std::max<uint64_t>(
                summary.maxComputeQueueDepth, cycle.computeQueueDepth);

            summary.loadRawHazardCycles += cycle.hadLoadRawHazard ? 1 : 0;
            summary.fuCapacityDenyCycles += cycle.hadFuCapacityDeny ? 1 : 0;
            summary.callWaitCycles += cycle.hadCallWait ? 1 : 0;
            summary.thresholdBlockedCycles += cycle.hadThresholdBlock ? 1 : 0;
            summary.lockstepBlockedCycles += cycle.hadLockstepBlock ? 1 : 0;
            summary.issueBackpressureCycles +=
                cycle.hadIssueBackpressure ? 1 : 0;
            summary.allPortsStalledCycles += cycle.hadAllPortsStalled ? 1 : 0;
            summary.portRetryCycles += cycle.hadPortRetry ? 1 : 0;

            summary.outstandingMemoryCycles +=
                cycle.hadOutstandingMemory ? 1 : 0;
            summary.outstandingComputeCycles +=
                cycle.hadOutstandingCompute ? 1 : 0;
            summary.readyMemoryCycles += cycle.hadReadyMemory ? 1 : 0;
            summary.readyComputeCycles += cycle.hadReadyCompute ? 1 : 0;

            summary.reservationNonEmptyCycles +=
                cycle.reservationNonEmpty ? 1 : 0;
            summary.readQueueNonEmptyCycles += cycle.readQueueNonEmpty ? 1 : 0;
            summary.writeQueueNonEmptyCycles +=
                cycle.writeQueueNonEmpty ? 1 : 0;
            summary.computeQueueNonEmptyCycles +=
                cycle.computeQueueNonEmpty ? 1 : 0;

            summary.reservationDepthEndSum += cycle.reservationDepthEnd;
            summary.readQueueDepthEndSum += cycle.readQueueDepthEnd;
            summary.writeQueueDepthEndSum += cycle.writeQueueDepthEnd;
            summary.computeQueueDepthEndSum += cycle.computeQueueDepthEnd;

            summary.reservationDepthPeakSum += cycle.reservationDepthPeak;
            summary.readQueueDepthPeakSum += cycle.readQueueDepthPeak;
            summary.writeQueueDepthPeakSum += cycle.writeQueueDepthPeak;
            summary.computeQueueDepthPeakSum += cycle.computeQueueDepthPeak;

            summary.maxReservationDepthPeak = std::max<uint64_t>(
                summary.maxReservationDepthPeak, cycle.reservationDepthPeak);
            summary.maxReadQueueDepthPeak = std::max<uint64_t>(
                summary.maxReadQueueDepthPeak, cycle.readQueueDepthPeak);
            summary.maxWriteQueueDepthPeak = std::max<uint64_t>(
                summary.maxWriteQueueDepthPeak, cycle.writeQueueDepthPeak);
            summary.maxComputeQueueDepthPeak = std::max<uint64_t>(
                summary.maxComputeQueueDepthPeak, cycle.computeQueueDepthPeak);

            summary.readIssueBackpressureCycles +=
                cycle.hadReadIssueBackpressure ? 1 : 0;
            summary.writeIssueBackpressureCycles +=
                cycle.hadWriteIssueBackpressure ? 1 : 0;
            summary.readRetryCycles += cycle.hadReadRetry ? 1 : 0;
            summary.writeRetryCycles += cycle.hadWriteRetry ? 1 : 0;

            summary.computeAndMemoryOutstandingCycles +=
                cycle.hadComputeAndMemoryOutstanding ? 1 : 0;
            summary.computeAndMemoryReadyCycles +=
                cycle.hadComputeAndMemoryReady ? 1 : 0;

            for (size_t t = 0; t < kNumTargetClasses; ++t) {
                for (size_t a = 0; a < kNumAccessKinds; ++a) {
                    summary.totalMemOps[t][a] += cycle.memOps[t][a];
                    summary.totalMemBytes[t][a] += cycle.memBytes[t][a];
                }
            }

            for (size_t f = 0; f < kNumFuClasses; ++f) {
                summary.fuBusySlotSum[f] += cycle.fuBusySlots[f];
                summary.fuPeakBusySlots[f] = std::max(
                    summary.fuPeakBusySlots[f], cycle.fuPeakBusySlots[f]);
                summary.fuAccepted[f] += cycle.fuAccepted[f];
                summary.fuDenied[f] += cycle.fuDenied[f];
                if (cycle.fuBusySlots[f] > 0) {
                    summary.fuActiveCycles[f]++;
                }
            }
        }
    }

    return summary;
}

void
HWStatistics::print(bool dump_cycles)
{
    if (dbg) {
        DPRINTF(SALAM_Debug, " Buffers: %i\n", (current_buffer_index + 1));
    }

    auto summary = summarize();

    std::cout << "Cycles Recorded: " << summary.cyclesRecorded << std::endl;

    if (summary.cyclesRecorded > 0) {
        std::cout << "Average Reservation Depth: "
                  << static_cast<double>(summary.reservationDepthSum) /
                         summary.cyclesRecorded
                  << std::endl;
        std::cout << "Average Read Queue Depth: "
                  << static_cast<double>(summary.readQueueDepthSum) /
                         summary.cyclesRecorded
                  << std::endl;
        std::cout << "Average Write Queue Depth: "
                  << static_cast<double>(summary.writeQueueDepthSum) /
                         summary.cyclesRecorded
                  << std::endl;
        std::cout << "Average Compute Queue Depth: "
                  << static_cast<double>(summary.computeQueueDepthSum) /
                         summary.cyclesRecorded
                  << std::endl;
    }

    std::cout << "Max Reservation Depth: " << summary.maxReservationDepth
              << std::endl;
    std::cout << "Max Read Queue Depth: " << summary.maxReadQueueDepth
              << std::endl;
    std::cout << "Max Write Queue Depth: " << summary.maxWriteQueueDepth
              << std::endl;
    std::cout << "Max Compute Queue Depth: " << summary.maxComputeQueueDepth
              << std::endl;

    std::cout << "Total Internal Load Completions: "
              << summary.totalInternalLoadCompletions << std::endl;
    std::cout << "Total Loads Issued: " << summary.totalLoadsIssued
              << std::endl;
    std::cout << "Total Loads Committed: " << summary.totalLoadsCommitted
              << std::endl;
    std::cout << "Total Stores Issued: " << summary.totalStoresIssued
              << std::endl;
    std::cout << "Total Stores Committed: " << summary.totalStoresCommitted
              << std::endl;
    std::cout << "Total Compute Launch Attempts: "
              << summary.totalComputeAttempts << std::endl;
    std::cout << "Total Compute Launches Accepted: "
              << summary.totalComputeLaunchesAccepted << std::endl;
    std::cout << "Total Compute Committed: " << summary.totalComputeCommitted
              << std::endl;
    std::cout << "Total Calls Issued: " << summary.totalCallsIssued
              << std::endl;
    std::cout << "Total Calls Committed: " << summary.totalCallsCommitted
              << std::endl;

    std::cout << "Load RAW Hazard Cycles: " << summary.loadRawHazardCycles
              << std::endl;
    std::cout << "FU Capacity Deny Cycles: " << summary.fuCapacityDenyCycles
              << std::endl;
    std::cout << "Call Wait Cycles: " << summary.callWaitCycles << std::endl;
    std::cout << "Threshold Blocked Cycles: " << summary.thresholdBlockedCycles
              << std::endl;
    std::cout << "Lockstep Blocked Cycles: " << summary.lockstepBlockedCycles
              << std::endl;
    std::cout << "Issue Backpressure Cycles: "
              << summary.issueBackpressureCycles << std::endl;
    std::cout << "All Ports Stalled Cycles: " << summary.allPortsStalledCycles
              << std::endl;
    std::cout << "Port Retry Cycles: " << summary.portRetryCycles << std::endl;

    std::cout << "Outstanding Memory Cycles: "
              << summary.outstandingMemoryCycles << std::endl;
    std::cout << "Outstanding Compute Cycles: "
              << summary.outstandingComputeCycles << std::endl;
    std::cout << "Ready Memory Cycles: " << summary.readyMemoryCycles
              << std::endl;
    std::cout << "Ready Compute Cycles: " << summary.readyComputeCycles
              << std::endl;

    std::cout << "Reservation Non-Empty Cycles: "
              << summary.reservationNonEmptyCycles << std::endl;
    std::cout << "Read Queue Non-Empty Cycles: "
              << summary.readQueueNonEmptyCycles << std::endl;
    std::cout << "Write Queue Non-Empty Cycles: "
              << summary.writeQueueNonEmptyCycles << std::endl;
    std::cout << "Compute Queue Non-Empty Cycles: "
              << summary.computeQueueNonEmptyCycles << std::endl;

    if (!dump_cycles) {
        return;
    }

    for (const auto &buffer : hw_buffer_list) {
        for (const auto &cycle : buffer) {
            std::cout << "Cycle: " << cycle.cycle
                      << " ResDepth: " << cycle.reservationDepth
                      << " RdDepth: " << cycle.readQueueDepth
                      << " WrDepth: " << cycle.writeQueueDepth
                      << " CmpDepth: " << cycle.computeQueueDepth
                      << " IntLdCom: " << cycle.internalLoadCompletions
                      << " LdIss: " << cycle.loadsIssued
                      << " LdCom: " << cycle.loadsCommitted
                      << " StIss: " << cycle.storesIssued
                      << " StCom: " << cycle.storesCommitted
                      << " CmpTry: " << cycle.computeLaunchAttempts
                      << " CmpAcc: " << cycle.computeLaunchesAccepted
                      << " CmpCom: " << cycle.computeCommitted
                      << " CallIss: " << cycle.callsIssued
                      << " CallCom: " << cycle.callsCommitted
                      << " LoadRAW: " << cycle.hadLoadRawHazard
                      << " FUDeny: " << cycle.hadFuCapacityDeny
                      << " CallWait: " << cycle.hadCallWait
                      << " ThreshBlk: " << cycle.hadThresholdBlock
                      << " LockBlk: " << cycle.hadLockstepBlock
                      << " MemBP: " << cycle.hadIssueBackpressure
                      << " AllPorts: " << cycle.hadAllPortsStalled
                      << " Retry: " << cycle.hadPortRetry
                      << " OutMem: " << cycle.hadOutstandingMemory
                      << " OutCmp: " << cycle.hadOutstandingCompute
                      << " ReadyMem: " << cycle.hadReadyMemory
                      << " ReadyCmp: " << cycle.hadReadyCompute
                      << " ResNonEmpty: " << cycle.reservationNonEmpty
                      << " RdNonEmpty: " << cycle.readQueueNonEmpty
                      << " WrNonEmpty: " << cycle.writeQueueNonEmpty
                      << " CmpNonEmpty: " << cycle.computeQueueNonEmpty
                      << std::endl;
        }
    }
}
