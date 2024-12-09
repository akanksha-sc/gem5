#ifndef __HWMODEL_ADD_HH__
#define __HWMODEL_ADD_HH__

// GENERATED FILE - DO NOT MODIFY

#include "base.hh"
#include "params/Add.hh"
#include "sim/sim_object.hh"

using namespace gem5;

class Add: public SimObject, public InstConfigBase
{
        private:
        protected:
        public:
                Add();
                Add(const AddParams &params);
};
#endif // __HWMODEL_ADD_HH__
