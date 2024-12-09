#ifndef __HWMODEL_DOUBLE_MULTIPLIER_HH__
#define __HWMODEL_DOUBLE_MULTIPLIER_HH__

// GENERATED FILE - DO NOT MODIFY

#include "base.hh"
#include "params/DoubleMultiplier.hh"
#include "sim/sim_object.hh"

using namespace gem5;

class DoubleMultiplier: public SimObject, public FunctionalUnitBase
{
        private:
        protected:
        public:
                DoubleMultiplier();
                DoubleMultiplier(const DoubleMultiplierParams &params);
};
#endif // __HWMODEL_DOUBLE_MULTIPLIER_HH__
