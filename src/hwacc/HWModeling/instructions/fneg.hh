#ifndef __HWMODEL_FNEG_HH__
#define __HWMODEL_FNEG_HH__

// GENERATED FILE - DO NOT MODIFY

#include "base.hh"
#include "params/Fneg.hh"
#include "sim/sim_object.hh"

using namespace gem5;

class Fneg: public SimObject, public InstConfigBase
{
        private:
        protected:
        public:
                Fneg();
                Fneg(const FnegParams &params);
};
#endif // __HWMODEL_FNEG_HH__
