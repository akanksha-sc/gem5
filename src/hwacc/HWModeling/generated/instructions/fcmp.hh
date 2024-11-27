#ifndef __HWMODEL_FCMP_HH__
#define __HWMODEL_FCMP_HH__

// GENERATED FILE - DO NOT MODIFY

#include "base.hh"
#include "params/Fcmp.hh"
#include "sim/sim_object.hh"

using namespace gem5;

class Fcmp: public SimObject, public InstConfigBase
{
        private:
        protected:
        public:
                Fcmp();
                Fcmp(const FcmpParams &params);
};
#endif // __HWMODEL_FCMP_HH__
