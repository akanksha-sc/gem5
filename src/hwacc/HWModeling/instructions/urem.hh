#ifndef __HWMODEL_UREM_HH__
#define __HWMODEL_UREM_HH__

// GENERATED FILE - DO NOT MODIFY

#include "base.hh"
#include "params/Urem.hh"
#include "sim/sim_object.hh"

using namespace gem5;

class Urem: public SimObject, public InstConfigBase
{
        private:
        protected:
        public:
                Urem();
                Urem(const UremParams &params);
};
#endif // __HWMODEL_UREM_HH__
