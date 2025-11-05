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
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

#include "hwacc/LLVMRead/src/debug_flags.hh"
#include "params/HWStatistics.hh"
#include "sim/sim_object.hh"

using namespace gem5;


// Things here are output only once at end of simulation
struct HW_Params
{
    int run_end;

    void reset() {
        run_end = 0;
    }

};

// These are outputs that are stored each cycle
struct HW_Cycle_Stats
{
    int cycle;

    int resInFlight;

    int loadInFlight;
    int loadInternal;
    int loadActive;
    int loadRawStall;

    int storeInFlight;
    int storeActive;

    int compInFlight;
    int compLaunched;
    int compActive;
    int compStructStall;
    int compCommited;

    int compCommitThisCycle;
    int memCommitThisCycle;
    int ctrlCommitThisCycle;
    int anyCommit;

    // memory-side cycle flags
    int memRetry;
    int memNoPort;
    int memInFlightFlag;

    // stall breakdown by cause (disjoint)
    int stallMemWait;   // mem in-flight>0, comp in-flight==0
    int stallCompWait;  // comp in-flight>0, mem in-flight==0
    int stallBothWait;  // both mem and comp in-flight>0
    int stallDepSched;  // no in-flight; reservation>0
    int stallIdle;      // no in-flight; reservation==0

    void reset() {
        cycle = 0;

        resInFlight = 0;

        loadInFlight = 0;
        loadInternal = 0;
        loadActive = 0;
        loadRawStall = 0;

        storeInFlight = 0;
        storeActive = 0;

        compInFlight = 0;
        compLaunched = 0;
        compActive = 0;
        compStructStall = 0;
        compCommited = 0;

        compCommitThisCycle = 0;
        memCommitThisCycle  = 0;
        ctrlCommitThisCycle  = 0;
        anyCommit = 0;

        memRetry = 0;
        memNoPort = 0;
        memInFlightFlag = 0;

        stallMemWait = 0;
        stallCompWait = 0;
        stallBothWait = 0;
        stallDepSched = 0;
        stallIdle = 0;
    }
};

class HWStatistics : public SimObject
{
    private:
        HW_Params hw_params;
        HW_Cycle_Stats current_cycle_stats;
        std::vector<std::vector<HW_Cycle_Stats>> hw_buffer_list;

        // Make Into SimObjects to pass from config.yml
        bool cycle_tracking = false;
        bool dbg;
        int statBufferSize;
        int statBufferPreDefine;

        // Class Only
        int current_buffer_index = 0;

        // Latency accumulators
        uint64_t ldLatSum = 0, ldLatMax = 0, ldDone = 0;
        uint64_t stLatSum = 0, stLatMax = 0, stDone = 0;
        uint64_t compLatSum = 0, compLatMax = 0, compDone = 0;

        uint64_t totalCompOps = 0;         // weighted ops
        uint64_t totalLoadBytes = 0;       // bytes read by acc
        uint64_t totalStoreBytes = 0;      // bytes written by acc
        uint64_t totalComputeCommits = 0;
        uint64_t totalLoadCount = 0;
        uint64_t totalStoreCount = 0;

        double   clockGHz = 0.0;
        uint64_t opsPerCyclePeak = 0;
        uint64_t bytesPerCyclePeak = 0;

    public:
        HWStatistics();
        HWStatistics(const HWStatisticsParams &params);
        bool use_cycle_tracking() { return cycle_tracking; }

        void print();
        void updateBuffer();
        void clearStats();

        // Aggregation from LLVMInterface (called multiple times per cycle)
        void accumulateCycleStart(const HW_Cycle_Stats& s);
        void accumulateCycleEvents(const HW_Cycle_Stats& s);

        // Called once per modeled cycle (after all ActiveFunctions processed)
        void finalizeCycle(int curr_cycle);

        // Compute a progress/stall partition over recorded cycles
        void computeCyclePartition(uint64_t &progressCycles,
                                   uint64_t &stallCycles,
                                   uint64_t &totalCycles) const;

        // Record a completed op's latency in modeled cycles
        inline void
        noteLoadLatency(uint64_t lat) {
            ldLatSum += lat;
            if (lat > ldLatMax)
                ldLatMax = lat;
            ldDone++;
        }

        inline void
        noteStoreLatency(uint64_t lat) {
             stLatSum += lat;
             if (lat > stLatMax)
                 stLatMax = lat;
             stDone++;
        }

        inline void
        noteComputeLatency(uint64_t lat) {
           compLatSum += lat;
           if (lat > compLatMax)
               compLatMax = lat;
           compDone++;
        }

        inline void countCompute(uint32_t op_weight) {
            totalCompOps += op_weight;
            totalComputeCommits++;
        }
        inline void countLoad(uint64_t bytes /*, bool internal*/) {
           // If later you can distinguish internal SPM hits, gate this here.
           totalLoadBytes += bytes;
           totalLoadCount++;
        }
        inline void countStore(uint64_t bytes) {
            totalStoreBytes += bytes;
            totalStoreCount++;
        }
        inline void setClockGHz(double ghz) { clockGHz = ghz; }

        uint64_t getTotalCompOps()   const { return totalCompOps; }
        uint64_t getTotalLoadBytes() const { return totalLoadBytes; }
        uint64_t getTotalStoreBytes()const { return totalStoreBytes; }
        uint64_t getTotalComputeCommits() const { return totalComputeCommits; }
        uint64_t getTotalLoadCount() const { return totalLoadCount; }
        uint64_t getTotalStoreCount()const { return totalStoreCount; }
        double   getClockGHz()       const { return clockGHz; }

        uint64_t getOpsPerCyclePeak()   const { return opsPerCyclePeak; }
        uint64_t getBytesPerCyclePeak() const { return bytesPerCyclePeak; }
	// Inject a single quiescent (idle) cycle, typically at run end.
	void pushIdleBubble(int next_cycle);

};

#endif //__HWMODEL_HW_STATISTICS_HH__
