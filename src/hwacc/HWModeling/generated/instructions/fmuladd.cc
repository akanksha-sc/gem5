#include "fmuladd.hh"

//AUTO-GENERATED FILE

Fmuladd::Fmuladd(const FmuladdParams &params) :
	SimObject(params),
	InstConfigBase( params.functional_unit,
						params.functional_unit_limit,
						params.opcode_num,
						params.runtime_cycles) { }
