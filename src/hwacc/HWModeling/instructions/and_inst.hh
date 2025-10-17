#ifndef __HWMODEL_AND_INST_HH__
#define __HWMODEL_AND_INST_HH__

// GENERATED FILE - DO NOT MODIFY

#include "base.hh"
#include "params/AndInst.hh"
#include "sim/sim_object.hh"

using namespace gem5;

class AndInst: public SimObject, public InstConfigBase
{
        private:
        protected:
        public:
                AndInst();
                AndInst(const AndInstParams &params);
};
#endif // __HWMODEL_AND_INST_HH__
