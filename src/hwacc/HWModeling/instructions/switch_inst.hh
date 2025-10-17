#ifndef __HWMODEL_SWITCH_INST_HH__
#define __HWMODEL_SWITCH_INST_HH__

// GENERATED FILE - DO NOT MODIFY

#include "base.hh"
#include "params/SwitchInst.hh"
#include "sim/sim_object.hh"

using namespace gem5;

class SwitchInst: public SimObject, public InstConfigBase
{
        private:
        protected:
        public:
                SwitchInst();
                SwitchInst(const SwitchInstParams &params);
};
#endif // __HWMODEL_SWITCH_INST_HH__
