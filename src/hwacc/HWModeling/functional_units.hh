#ifndef __HWMODEL_FUNCTIONAL_UNITS_HH__
#define __HWMODEL_FUNCTIONAL_UNITS_HH__

#include "params/FunctionalUnits.hh"
#include "sim/sim_object.hh"

// GENERATED HEADERS - DO NOT MODIFY
#include <cstdlib>
#include <iostream>
#include <vector>

#include "functional_units/base.hh"
#include "functional_units/bit_register.hh"
#include "functional_units/bit_shifter.hh"
#include "functional_units/bitwise_operations.hh"
#include "functional_units/double_adder.hh"
#include "functional_units/double_divider.hh"
#include "functional_units/double_multiplier.hh"
#include "functional_units/float_adder.hh"
#include "functional_units/float_divider.hh"
#include "functional_units/float_multiplier.hh"
#include "functional_units/integer_adder.hh"
#include "functional_units/integer_multiplier.hh"

using namespace gem5;

class FunctionalUnitBase;

class FunctionalUnits : public SimObject
{
        private:
        protected:

        public:
                // GENERATED CLASS MEMBERS - DO NOT MODIFY
                FloatDivider* _float_divider;
                FloatAdder* _float_adder;
                DoubleMultiplier* _double_multiplier;
                DoubleAdder* _double_adder;
                BitShifter* _bit_shifter;
                IntegerMultiplier* _integer_multiplier;
                BitRegister* _bit_register;
                DoubleDivider* _double_divider;
                FloatMultiplier* _float_multiplier;
                IntegerAdder* _integer_adder;
                BitwiseOperations* _bitwise_operations;
                FunctionalUnits();
                // DEFAULT CONSTRUCTOR - DO NOT MODIFY
                FunctionalUnits(const FunctionalUnitsParams &params);
                // END DEFAULT CONSTRUCTOR
                std::vector<FunctionalUnitBase*> functional_unit_list;
};
#endif //__HWMODEL_FUNCTIONAL_UNITS_HH__
