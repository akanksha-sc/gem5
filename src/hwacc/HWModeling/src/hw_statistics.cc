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
#include <sstream>
#include <iomanip>

HWStatistics::HWStatistics(const HWStatisticsParams &params) :
    SimObject(params) {

        statBufferSize = 10000;
        statBufferPreDefine = 2;
        dbg = params.dbg;
        cycle_tracking = params.cycle_tracking;

        // If not set in config, they default to 0 and will be ignored
        opsPerCyclePeak   = params.ops_per_cycle_peak;
        bytesPerCyclePeak = params.bytes_per_cycle_peak;

        for (int i = 0 ; i < statBufferPreDefine; i++ ) {
            std::vector<HW_Cycle_Stats> hw_cycle_buffer;
            hw_cycle_buffer.reserve(statBufferSize);
            hw_buffer_list.emplace_back();
            hw_buffer_list.back().reserve(statBufferSize);
        }
        clearStats();
    }

void HWStatistics::accumulateCycleStart(const HW_Cycle_Stats& s)
{
    // snapshot occupancies at cycle start; sum across ActiveFunctions
    current_cycle_stats.resInFlight  += s.resInFlight;
    current_cycle_stats.loadInFlight += s.loadInFlight;
    current_cycle_stats.storeInFlight+= s.storeInFlight;
    current_cycle_stats.compInFlight += s.compInFlight;
}

void HWStatistics::accumulateCycleEvents(const HW_Cycle_Stats& s)
{
    // per-cycle event counters summed across ActiveFunctions
    current_cycle_stats.loadInternal += s.loadInternal;
    current_cycle_stats.loadActive   += s.loadActive;
    current_cycle_stats.loadRawStall += s.loadRawStall;

    current_cycle_stats.storeActive  += s.storeActive;

    current_cycle_stats.compLaunched += s.compLaunched;
    current_cycle_stats.compActive   += s.compActive;
    current_cycle_stats.compStructStall  += s.compStructStall;
    current_cycle_stats.compCommited += s.compCommited;

    current_cycle_stats.compCommitThisCycle += s.compCommitThisCycle;
    current_cycle_stats.memCommitThisCycle  += s.memCommitThisCycle;
    current_cycle_stats.ctrlCommitThisCycle  += s.ctrlCommitThisCycle;

    if (s.compCommitThisCycle > 0 || s.memCommitThisCycle > 0
        || s.ctrlCommitThisCycle > 0)
        current_cycle_stats.anyCommit = 1;

    if (s.memRetry)        current_cycle_stats.memRetry = 1;
    if (s.memNoPort)       current_cycle_stats.memNoPort = 1;
    if (s.memInFlightFlag) current_cycle_stats.memInFlightFlag = 1;
}

void HWStatistics::finalizeCycle(int curr_cycle)
{
    // Stall categorization, if no commit this cycle
    const bool anyCommit = current_cycle_stats.anyCommit > 0;
    if (!anyCommit) {
        const int memInfl = current_cycle_stats.loadInFlight
                          + current_cycle_stats.storeInFlight;
        const int compInfl = current_cycle_stats.compInFlight;
        const int resInfl  = current_cycle_stats.resInFlight;
        // Structural-FU stall re-attribution policy:
        // If at least one ready instruction could not allocate a FU in this cycle,
        // attribute the stall to "compute" (even if there is memory activity)
        const bool structFuBlocked = (current_cycle_stats.compStructStall > 0);

        if (structFuBlocked) {
            // Re-attribute to compute stall
            current_cycle_stats.stallCompWait = 1;
        } else if (memInfl > 0 && compInfl == 0) {
            current_cycle_stats.stallMemWait = 1;
        } else if (compInfl > 0 && memInfl == 0) {
            current_cycle_stats.stallCompWait = 1;
        } else if (compInfl > 0 && memInfl > 0) {
            current_cycle_stats.stallBothWait = 1;
        } else if (resInfl > 0) {
            // Reservation has work; blocked by deps/scheduler (control)
            current_cycle_stats.stallDepSched = 1;
        } else {
            current_cycle_stats.stallIdle = 1;
        }
    }

    // stamp the cycle number
    // push one aggregated record for the cycle
    current_cycle_stats.cycle = curr_cycle;

    // push into the active buffer window
    auto& cur = hw_buffer_list.at(current_buffer_index);
    cur.push_back(current_cycle_stats);

    clearStats();   // prepare for next cycle aggregation
    updateBuffer();
}

void HWStatistics::computeCyclePartition(uint64_t &progressCycles,
                                         uint64_t &stallCycles,
                                         uint64_t &totalCycles) const
{
    progressCycles = stallCycles = totalCycles = 0;
    for (const auto& buf : hw_buffer_list) {
        for (const auto& c : buf) {
            if (c.cycle == 0)
                continue;
            totalCycles++;
            if (c.anyCommit > 0)
                progressCycles++;
            else
                stallCycles++;
        }
    }
}


void
HWStatistics::clearStats() {
    if (dbg) {
        DPRINTF(SALAM_Debug, "Clearing Cycle Statistics\n");
    }
    current_cycle_stats.reset();

}

void HWStatistics::updateBuffer() {
    if (dbg) {
        DPRINTF(SALAM_Debug, "Checking Buffer[%i][%zu]\n",
            current_buffer_index,
            hw_buffer_list.at(current_buffer_index).size());
    }

    // If current window reached capacity
    if (hw_buffer_list.at(current_buffer_index).size() >=
        (size_t)statBufferSize) {
        current_buffer_index++;
        if (current_buffer_index >= (int)hw_buffer_list.size()) {
            if (dbg) {
                DPRINTF(SALAM_Debug, "Creating New Buffer Window\n");
            }
            std::vector<HW_Cycle_Stats> buf;
            buf.reserve(statBufferSize);
            hw_buffer_list.push_back(std::move(buf));
        }
        else {
            if (dbg) {
                DPRINTF(SALAM_Debug, "Next Buffer Window\n");
            }
        }
    }
}

void HWStatistics::pushIdleBubble(int next_cycle)
{
    HW_Cycle_Stats z;
    z.reset();
    z.cycle = next_cycle;

    // This is a quiescent cycle: no in-flight work and no commits.
    // Mark it as an "idle" stall explicitly.
    z.stallIdle = 1;

    auto& cur = hw_buffer_list.at(current_buffer_index);
    cur.push_back(z);
    updateBuffer();
}


void HWStatistics::print()
{
    uint64_t ncycles = 0;
    uint64_t sumRes=0, sumLd=0, sumSt=0, sumComp=0;
    uint64_t sumLdInt=0, sumLdAct=0, sumLdRawStall=0, sumStAct=0;
    uint64_t sumCompLaunch=0, sumCompActive=0, sumCompCommit=0,
             sumCompStructStall=0;
    uint64_t anyMemInFlightCycles=0, anyCompInFlightCycles=0,
             fuStructStallCycles=0;
    uint64_t memRetryCycles=0, memNoPortCycles=0;
    // Stall-category and partition counters
    uint64_t stallMemWait=0, stallCompWait=0, stallBothWait=0,
             stallDepSched=0, stallIdle=0;
    uint64_t progressCycles=0, stallCycles=0, totalCycles=0;

    for (const auto& buf : hw_buffer_list) {
        for (const auto& c : buf) {
            if (c.cycle == 0)
                continue;
            ncycles++;

            sumRes  += c.resInFlight;
            sumLd   += c.loadInFlight;
            sumSt   += c.storeInFlight;
            sumComp += c.compInFlight;

            sumLdInt      += c.loadInternal;
            sumLdAct      += c.loadActive;
            sumLdRawStall += c.loadRawStall;
            sumStAct      += c.storeActive;

            sumCompLaunch += c.compLaunched;
            sumCompActive += c.compActive;
            sumCompCommit += c.compCommited;
            sumCompStructStall+= c.compStructStall;

            if ((c.loadInFlight + c.storeInFlight) > 0)
                anyMemInFlightCycles++;
            if (c.compInFlight > 0)
                anyCompInFlightCycles++;
            if (c.compStructStall > 0)
                fuStructStallCycles++;
            if (c.memRetry)        memRetryCycles++;
            if (c.memNoPort)       memNoPortCycles++;

            // partition counts
            totalCycles++;
            if (c.anyCommit) progressCycles++; else stallCycles++;

            // disjoint stall categories
            stallMemWait  += (c.stallMemWait  ? 1 : 0);
            stallCompWait += (c.stallCompWait ? 1 : 0);
            stallBothWait += (c.stallBothWait ? 1 : 0);
            stallDepSched += (c.stallDepSched ? 1 : 0);
            stallIdle     += (c.stallIdle     ? 1 : 0);
        }
    }

    auto avg = [&](uint64_t s) {
            return ncycles ? double(s)/double(ncycles) : 0.0;
    };
    auto pct = [&](uint64_t s) {
            return ncycles ? 100.0 * double(s)/double(ncycles) : 0.0;
    };

    // Stall breakdown (percent of stall cycles)
    //auto pct_stall = [&](uint64_t s) {
    //    return stallCycles ? 100.0 * double(s) / double(stallCycles) : 0.0;
    //};

    std::cout << "   ======= Accelerator Cycle Analysis =======" << std::endl;
    std::cout << "   Cycles Recorded:                " << ncycles << std::endl;

    // Queue occupancies (avg per modeled cycle)
    std::cout << "   Queues (avg per cycle):" << std::endl;
    std::cout << "        Reservation In-Flight:      " << std::fixed <<
        std::setprecision(2) << avg(sumRes)  << std::endl;
    std::cout << "        Loads In-Flight:            " << std::fixed <<
        std::setprecision(2) << avg(sumLd)   << std::endl;
    std::cout << "        Stores In-Flight:           " << std::fixed <<
        std::setprecision(4) << avg(sumSt)   << std::endl;
    std::cout << "        Compute In-Flight:          " << std::fixed <<
        std::setprecision(4) << avg(sumComp) << std::endl;

    // Memory activity
    std::cout << "   Memory Activity:" << std::endl;
    std::cout << "        Loads Launched (events):    " << sumLdAct
        << std::endl;
    std::cout << "        Internal Loads (events):    " << sumLdInt
        << std::endl;
    std::cout << "        Load RAW Stalls (events):   " << sumLdRawStall
        << std::endl;
    std::cout << "        Cycles w/ Mem In-Flight:    " << std::fixed <<
        std::setprecision(3) << pct(anyMemInFlightCycles) << "%" << std::endl;

    // Memory-side stall views
    std::cout << "        Cycles w/ Mem Backpressure:" << std::fixed
              << std::setprecision(3) << pct(memRetryCycles) << "%\n";
    std::cout << "        Cycles w/ No-Eligible-Port:" << std::fixed
              << std::setprecision(3) << pct(memNoPortCycles) << "%\n";

    // Compute activity
    std::cout << "   Compute Activity:" << std::endl;
    std::cout << "        Compute Launched (events):  " <<
        sumCompLaunch << std::endl;
    std::cout << "        Compute Committed (events): " <<
        sumCompCommit << std::endl;
    std::cout << "        Compute Active (events):    " <<
        sumCompActive << std::endl;
    std::cout << "        Cycles w/ Compute In-Flight:" << std::fixed <<
        std::setprecision(3) << pct(anyCompInFlightCycles) << "%" << std::endl;
    std::cout << "        Cycles w/ FU Struct Stall:   " << std::fixed <<
        std::setprecision(3) << pct(fuStructStallCycles) << "%" << std::endl;


    // Latency (modeled cycles)
    std::cout << "   Latency (cycles):" << std::endl;
    if (ldDone) {
        std::cout << "        Loads:   avg="
                  << std::fixed << std::setprecision(2)
                  << (double)ldLatSum / (double)ldDone
                  << "  max=" << ldLatMax << "  n=" << ldDone << std::endl;
    }
    if (stDone) {
        std::cout << "        Stores:  avg="
                  << std::fixed << std::setprecision(2)
                  << (double)stLatSum / (double)stDone
                  << "  max=" << stLatMax << "  n=" << stDone << std::endl;
    }
    if (compDone) {
        std::cout << "        Compute: avg="
                  << std::fixed << std::setprecision(2)
                  << (double)compLatSum / (double)compDone
                  << " max=" << compLatMax << " n=" << compDone << std::endl;
    }

    // Stall breakdown (disjoint)
    std::cout << "   Stall Breakdown (disjoint):\n";
    auto fmt3 = [](double v){
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3) << v;
        return oss.str();
    };
    auto of_total  = [&](uint64_t s){ return fmt3( (totalCycles ? 100.0 * double(s)/double(totalCycles) : 0.0) ) + "% of total"; };
    auto of_stalls = [&](uint64_t s){ return fmt3( (stallCycles ? 100.0 * double(s)/double(stallCycles)   : 0.0) ) + "% of stalls"; };

    std::cout << "        Mem-wait:   "
          << stallMemWait  << " cycles  (" << of_stalls(stallMemWait)  << ", "
          << of_total(stallMemWait)  << ")\n";
    std::cout << "        Comp-wait:  "
          << stallCompWait << " cycles  (" << of_stalls(stallCompWait) << ", "
          << of_total(stallCompWait) << ")\n";
    std::cout << "        Both-wait:  "
          << stallBothWait << " cycles  (" << of_stalls(stallBothWait) << ", "
          << of_total(stallBothWait) << ")\n";
    std::cout << "        Dep/Sched:  "
          << stallDepSched << " cycles  (" << of_stalls(stallDepSched) << ", "
          << of_total(stallDepSched) << ")\n";
    std::cout << "        Idle:       "
          << stallIdle     << " cycles  (" << of_stalls(stallIdle)     << ", "
          << of_total(stallIdle)     << ")\n";

    // Structural-FU stall visibility (not disjoint, informational)
    std::cout << "   Cycles with FU structural pressure: "
          << fuStructStallCycles << "  (" << std::fixed << std::setprecision(3)
          << pct(fuStructStallCycles) << "% of total)\n";

    // ===== Sanity checks =====
    const uint64_t stallCatSum = stallMemWait + stallCompWait
                           + stallBothWait + stallDepSched + stallIdle;

    std::cout << "   ===== Sanity Checks =====\n";
    std::cout << "        Total cycles:    " << totalCycles << "\n";
    std::cout << "        Progress cycles: " << progressCycles << "\n";
    std::cout << "        Stall cycles:    " << stallCycles << "\n";
    std::cout << "        Progress+Stall = " << (progressCycles + stallCycles)
          << ( (progressCycles + stallCycles) == totalCycles ? "  [OK]" : "  [MISMATCH]" )
          << "\n";
    std::cout << "        Sum(disjoint stall cats) = " << stallCatSum
          << ( stallCatSum == stallCycles ? "  [OK]" : "  [MISMATCH]" )
          << "\n";
}
