#ifndef __HWMODEL_INTEGER_MULTIPLIER_HH__
#define __HWMODEL_INTEGER_MULTIPLIER_HH__

// GENERATED FILE - DO NOT MODIFY

#include "base.hh"
#include "params/IntegerMultiplier.hh"
#include "sim/sim_object.hh"

using namespace gem5;

class IntegerMultiplier: public SimObject, public FunctionalUnitBase
{
        private:
        protected:
        public:
                IntegerMultiplier();
                IntegerMultiplier(const IntegerMultiplierParams &params);
};
#endif // __HWMODEL_INTEGER_MULTIPLIER_HH__
