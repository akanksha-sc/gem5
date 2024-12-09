#ifndef __HWMODEL_PHI_HH__
#define __HWMODEL_PHI_HH__

// GENERATED FILE - DO NOT MODIFY

#include "base.hh"
#include "params/Phi.hh"
#include "sim/sim_object.hh"

using namespace gem5;

class Phi: public SimObject, public InstConfigBase
{
        private:
        protected:
        public:
                Phi();
                Phi(const PhiParams &params);
};
#endif // __HWMODEL_PHI_HH__
