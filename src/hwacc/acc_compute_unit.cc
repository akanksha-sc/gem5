#include "hwacc/acc_compute_unit.hh"

AccComputeUnit::AccComputeUnit(const AccComputeUnitParams &p) :
    SimObject(p),
    comm(p.comm_int),
    hw(p.hw_int),
    tickEvent(this) {}

// AccComputeUnit*
// AccComputeUnitParams::create() {
//     return new AccComputeUnit(this);
// }
