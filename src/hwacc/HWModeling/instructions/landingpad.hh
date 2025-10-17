#ifndef __HWMODEL_LANDINGPAD_HH__
#define __HWMODEL_LANDINGPAD_HH__

// GENERATED FILE - DO NOT MODIFY

#include "base.hh"
#include "params/Landingpad.hh"
#include "sim/sim_object.hh"

using namespace gem5;

class Landingpad: public SimObject, public InstConfigBase
{
        private:
        protected:
        public:
                Landingpad();
                Landingpad(const LandingpadParams &params);
};
#endif // __HWMODEL_LANDINGPAD_HH__
