#include "hw_statistics.hh"

HWStatistics::HWStatistics(const HWStatisticsParams &params) :
    SimObject(params) {

        statBufferSize = 10000;
        statBufferPreDefine = 2;
        dbg = false;

        for (int i=0 ; i<statBufferPreDefine; i++ ) {
            std::vector<HW_Cycle_Stats> hw_cycle_buffer;
            hw_cycle_buffer.reserve(statBufferSize);
            hw_buffer_list.push_back(hw_cycle_buffer);
        }
        hw_buffer = hw_buffer_list.begin();
        cycle_buffer = hw_buffer->begin();
        clearStats();
    }


void
HWStatistics::updateHWStatsCycleStart() {
    if (dbg) {
        DPRINTF(SALAM_Debug, "Updating Cycle Statistics Buffer\n");
    }
    (*hw_buffer).insert(cycle_buffer, current_cycle_stats);
    clearStats();
    updateBuffer();

}

void
HWStatistics::clearStats() {
    if (dbg) {
        DPRINTF(SALAM_Debug, "Clearing Cycle Statistics\n");
    }
    current_cycle_stats.reset();

}

void
HWStatistics::updateHWStatsCycleEnd(int curr_cycle) {
    if (dbg) {
        DPRINTF(SALAM_Debug, "Updating Cycle Statistics\n");
    }
    current_cycle_stats.cycle = curr_cycle;

}

void
HWStatistics::updateBuffer() {
    if (dbg) {
        DPRINTF(SALAM_Debug, "Checking Buffer[%i][%i]\n", current_buffer_index,
                        hw_buffer_list.at(current_buffer_index).size());
    }
    if (hw_buffer_list.at(current_buffer_index).size() == statBufferSize) {
        current_buffer_index++;
        if (current_buffer_index == statBufferPreDefine) {
            if (dbg) {
                DPRINTF(SALAM_Debug, "Creating New Buffer Window\n");
            }
            std::vector<HW_Cycle_Stats> hw_cycle_buffer;
            hw_cycle_buffer.reserve(statBufferSize);
            hw_buffer_list.push_back(hw_cycle_buffer);
            hw_buffer = hw_buffer_list.end();
            cycle_buffer = hw_buffer->begin();
        }
        else {
            if (dbg) {
                DPRINTF(SALAM_Debug, "Next Buffer Window\n");
            }
            hw_buffer++;
            cycle_buffer = hw_buffer->begin();
        }
    }
    else {
        cycle_buffer = hw_buffer->end();
    }
}


void
HWStatistics::print() {
    if (dbg) {
        DPRINTF(SALAM_Debug," Buffers: %i\n", (current_buffer_index + 1));
    }
    for (auto buffers : hw_buffer_list) {
        for (auto cycles : buffers) {
            // This loops through the full runtime,
            // starting at cycle 1 to completion
            std::cout << " Cycle: " << cycles.cycle;
        }
    }
}


void
HWStatistics::simpleStats() {
}

void
HWStatistics::unitCorrections() {
}
