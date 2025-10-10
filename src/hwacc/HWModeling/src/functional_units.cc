#include "functional_units.hh"

// GENERATED CONSTRUCTOR - DO NOT MODIFY
FunctionalUnits::FunctionalUnits(const FunctionalUnitsParams &params) :
    SimObject(params),
	_float_divider(params.float_divider),
	_float_adder(params.float_adder),
	_double_multiplier(params.double_multiplier),
	_double_adder(params.double_adder),
	_bit_shifter(params.bit_shifter),
	_integer_multiplier(params.integer_multiplier),
	_bit_register(params.bit_register),
	_double_divider(params.double_divider),
	_float_multiplier(params.float_multiplier),
	_integer_adder(params.integer_adder),
	_bitwise_operations(params.bitwise_operations) {
		functional_unit_list.push_back(_float_divider);
		functional_unit_list.push_back(_float_adder);
		functional_unit_list.push_back(_double_multiplier);
		functional_unit_list.push_back(_double_adder);
		functional_unit_list.push_back(_bit_shifter);
		functional_unit_list.push_back(_integer_multiplier);
		functional_unit_list.push_back(_bit_register);
		functional_unit_list.push_back(_double_divider);
		functional_unit_list.push_back(_float_multiplier);
		functional_unit_list.push_back(_integer_adder);
		functional_unit_list.push_back(_bitwise_operations);
}
// END OF GENERATED CONSTRUCTOR

