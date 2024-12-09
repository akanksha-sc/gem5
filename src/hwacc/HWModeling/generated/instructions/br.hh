#ifndef __HWMODEL_BR_HH__
#define __HWMODEL_BR_HH__

// GENERATED FILE - DO NOT MODIFY

#include "base.hh"
#include "params/Br.hh"
#include "sim/sim_object.hh"

using namespace gem5;

class Br: public SimObject, public InstConfigBase
{
        private:
        protected:
        public:
                Br();
                Br(const BrParams &params);
};
#endif // __HWMODEL_BR_HH__
