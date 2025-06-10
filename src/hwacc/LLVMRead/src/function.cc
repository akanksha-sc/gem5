#include "function.hh"

using namespace SALAM;

SALAM::Function::Function(uint64_t id, gem5::SimObject * owner, bool dbg) :
    SALAM::Value(id, owner, dbg) {
}

void
SALAM::Function::initialize(llvm::Value * irval,
                            irvmap *vmap,
                            SALAM::valueListTy *valueList,
                            std::string topName) {
    if (dbg)
        DPRINTFS(LLVMParse, owner,
                "Initialize Values - Function::initialize\n");
    Value::initialize(irval, vmap);

    //Parse irval for function params
    llvm::Function * func = llvm::dyn_cast<llvm::Function>(irval);
    assert(func);

    if (func->getName() == topName)
        top = true;
    else
        top = false;

    if (dbg)
        DPRINTFS(LLVMParse, owner, "Initialize Function Arguments\n");
    for (auto arg_iter = func->arg_begin(); arg_iter != func->arg_end();
        arg_iter++) {
        llvm::Argument &arg = *arg_iter;
        std::shared_ptr<SALAM::Value> argval = vmap->find(&arg)->second;
        assert(argval);
        std::shared_ptr<SALAM::Argument> argum =
                std::dynamic_pointer_cast<SALAM::Argument>(argval);
        assert(argum);
        arguments.push_back(argum);
        argum->initialize(&arg, vmap);
    }

    // Fill bbList
    if (dbg)
        DPRINTFS(LLVMParse, owner, "Initialize BasicBlocks\n");
    for (auto bb_iter = func->begin(); bb_iter != func->end(); bb_iter++) {
        llvm::BasicBlock &bb = *bb_iter;
        std::shared_ptr<SALAM::Value> bbval = vmap->find(&bb)->second;
        assert(bbval);
        std::shared_ptr<SALAM::BasicBlock> bblock =
                std::dynamic_pointer_cast<SALAM::BasicBlock>(bbval);
        assert(bblock);
        bbList.push_back(bblock);
        bblock->initialize(&bb, vmap, valueList);
    }
}
