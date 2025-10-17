#ifndef __HWMODEL_PTRTOINT_HH__
#define __HWMODEL_PTRTOINT_HH__

// GENERATED FILE - DO NOT MODIFY

#include "base.hh"
#include "params/Ptrtoint.hh"
#include "sim/sim_object.hh"

using namespace gem5;

class Ptrtoint: public SimObject, public InstConfigBase
{
        private:
        protected:
        public:
                Ptrtoint();
                Ptrtoint(const PtrtointParams &params);
};
#endif // __HWMODEL_PTRTOINT_HH__
