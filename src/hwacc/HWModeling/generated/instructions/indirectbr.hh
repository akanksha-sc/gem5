#ifndef __HWMODEL_INDIRECTBR_HH__
#define __HWMODEL_INDIRECTBR_HH__

// GENERATED FILE - DO NOT MODIFY

#include "base.hh"
#include "params/Indirectbr.hh"
#include "sim/sim_object.hh"

using namespace gem5;

class Indirectbr: public SimObject, public InstConfigBase
{
        private:
        protected:
        public:
                Indirectbr();
                Indirectbr(const IndirectbrParams &params);
};
#endif // __HWMODEL_INDIRECTBR_HH__
