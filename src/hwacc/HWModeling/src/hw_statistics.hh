#ifndef __HWMODEL_HW_STATISTICS_HH__
#define __HWMODEL_HW_STATISTICS_HH__

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
    int loadAcitve;
    int loadRawStall;

    int storeInFlight;
    int storeActive;

    int compInFlight;
    int compLaunched;
    int compActive;
    int compFUStall;
    int compCommited;



    void reset() {
        cycle = 0;
        resInFlight = 0;
        loadInFlight = 0;
        storeInFlight = 0;
        compInFlight = 0;
    }
};

class HWStatistics : public SimObject
{
    private:
        HW_Params hw_params;
        HW_Cycle_Stats current_cycle_stats;
        std::vector<HW_Cycle_Stats>::iterator cycle_buffer;
        std::vector<std::vector<HW_Cycle_Stats>> hw_buffer_list;
        std::vector<std::vector<HW_Cycle_Stats>>::iterator hw_buffer;

        // Make Into SimObjects to pass from config.yml
        bool cycle_tracking = false;
        bool dbg;
        int statBufferSize;
        int statBufferPreDefine;


        // Class Only
        int current_buffer_index = 0;


    public:
        HWStatistics();
        HWStatistics(const HWStatisticsParams &params);
        bool use_cycle_tracking() { return cycle_tracking; }

        void print();
        void simpleStats();
        void unitCorrections();
        void updateHWStatsCycleStart();
        void updateHWStatsCycleEnd(int curr_cycle);
        void updateBuffer();
        void clearStats();
};

#endif //__HWMODEL_HW_STATISTICS_HH__
