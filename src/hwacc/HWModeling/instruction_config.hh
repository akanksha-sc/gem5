#ifndef __HWMODEL_INSTRUCTION_CONFIG_HH__
#define __HWMODEL_INSTRUCTION_CONFIG_HH__

#include "params/InstConfig.hh"
#include "sim/sim_object.hh"

// GENERATED HEADERS - DO NOT MODIFY
#include <cstdlib>
#include <iostream>
#include <vector>

#include "instructions/add.hh"
#include "instructions/addrspacecast.hh"
#include "instructions/alloca.hh"
#include "instructions/and_inst.hh"
#include "instructions/ashr.hh"
#include "instructions/base.hh"
#include "instructions/bitcast.hh"
#include "instructions/br.hh"
#include "instructions/call.hh"
#include "instructions/fadd.hh"
#include "instructions/fcmp.hh"
#include "instructions/fdiv.hh"
#include "instructions/fence.hh"
#include "instructions/fmul.hh"
#include "instructions/fmuladd.hh"
#include "instructions/fneg.hh"
#include "instructions/fpext.hh"
#include "instructions/fptosi.hh"
#include "instructions/fptoui.hh"
#include "instructions/fptrunc.hh"
#include "instructions/frem.hh"
#include "instructions/fsub.hh"
#include "instructions/gep.hh"
#include "instructions/icmp.hh"
#include "instructions/indirectbr.hh"
#include "instructions/inttoptr.hh"
#include "instructions/invoke.hh"
#include "instructions/landingpad.hh"
#include "instructions/load.hh"
#include "instructions/lshr.hh"
#include "instructions/mul.hh"
#include "instructions/or_inst.hh"
#include "instructions/phi.hh"
#include "instructions/ptrtoint.hh"
#include "instructions/resume.hh"
#include "instructions/ret.hh"
#include "instructions/sdiv.hh"
#include "instructions/select.hh"
#include "instructions/sext.hh"
#include "instructions/shl.hh"
#include "instructions/srem.hh"
#include "instructions/store.hh"
#include "instructions/sub.hh"
#include "instructions/switch_inst.hh"
#include "instructions/trunc.hh"
#include "instructions/udiv.hh"
#include "instructions/uitofp.hh"
#include "instructions/unreachable.hh"
#include "instructions/urem.hh"
#include "instructions/vaarg.hh"
#include "instructions/xor_inst.hh"
#include "instructions/zext.hh"

using namespace gem5;

class InstConfigBase;

class InstConfig : public SimObject
{
        private:
        protected:

        public:
                // GENERATED CLASS MEMBERS - DO NOT MODIFY
                Add* _add;
                Addrspacecast* _addrspacecast;
                Alloca* _alloca;
                AndInst* _and_inst;
                Ashr* _ashr;
                Bitcast* _bitcast;
                Br* _br;
                Call* _call;
                Fadd* _fadd;
                Fcmp* _fcmp;
                Fdiv* _fdiv;
                Fence* _fence;
                Fmul* _fmul;
                Fmuladd* _fmuladd;
                Fneg* _fneg;
                Fpext* _fpext;
                Fptosi* _fptosi;
                Fptoui* _fptoui;
                Fptrunc* _fptrunc;
                Frem* _frem;
                Fsub* _fsub;
                Gep* _gep;
                Icmp* _icmp;
                Indirectbr* _indirectbr;
                Inttoptr* _inttoptr;
                Invoke* _invoke;
                Landingpad* _landingpad;
                Load* _load;
                Lshr* _lshr;
                Mul* _mul;
                OrInst* _or_inst;
                Phi* _phi;
                Ptrtoint* _ptrtoint;
                Resume* _resume;
                Ret* _ret;
                Sdiv* _sdiv;
                Select* _select;
                Sext* _sext;
                Shl* _shl;
                Srem* _srem;
                Store* _store;
                Sub* _sub;
                SwitchInst* _switch_inst;
                Trunc* _trunc;
                Udiv* _udiv;
                Uitofp* _uitofp;
                Unreachable* _unreachable;
                Urem* _urem;
                Vaarg* _vaarg;
                XorInst* _xor_inst;
                Zext* _zext;
                InstConfig();
                // DEFAULT CONSTRUCTOR - DO NOT MODIFY
                InstConfig(const InstConfigParams &params);
                // END DEFAULT CONSTRUCTOR
                std::vector<InstConfigBase*> inst_list;};
#endif //__INSTRUCTION_CONFIG_HH__
