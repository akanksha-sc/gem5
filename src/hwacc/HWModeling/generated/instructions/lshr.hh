#ifndef __HWMODEL_LSHR_HH__
#define __HWMODEL_LSHR_HH__

// GENERATED FILE - DO NOT MODIFY

#include "base.hh"
#include "params/Lshr.hh"
#include "sim/sim_object.hh"

using namespace gem5;

class Lshr: public SimObject, public InstConfigBase
{
        private:
        protected:
        public:
                Lshr();
                Lshr(const LshrParams &params);
};
#endif // __HWMODEL_LSHR_HH__
