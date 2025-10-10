#include "fneg.hh"

//AUTO-GENERATED FILE

Fneg::Fneg(const FnegParams &params) :
	SimObject(params),
	InstConfigBase( params.functional_unit,
                        params.functional_unit_limit,
                        params.opcode_num,
                        params.runtime_cycles) { }
