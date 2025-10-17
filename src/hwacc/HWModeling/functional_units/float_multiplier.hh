#ifndef __HWMODEL_FLOAT_MULTIPLIER_HH__
#define __HWMODEL_FLOAT_MULTIPLIER_HH__

// GENERATED FILE - DO NOT MODIFY

#include "base.hh"
#include "params/FloatMultiplier.hh"
#include "sim/sim_object.hh"

using namespace gem5;

class FloatMultiplier: public SimObject, public FunctionalUnitBase
{
        private:
        protected:
        public:
                FloatMultiplier();
                FloatMultiplier(const FloatMultiplierParams &params);
};
#endif // __HWMODEL_FLOAT_MULTIPLIER_HH__
