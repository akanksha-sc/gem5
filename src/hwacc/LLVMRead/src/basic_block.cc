#include "basic_block.hh"
#include "llvm/IR/CFG.h"
#include "sim/sim_object.hh"

using namespace SALAM;

SALAM::BasicBlock::BasicBlock(uint64_t id, gem5::SimObject * owner, bool dbg):
    SALAM::Value(id, owner, dbg) {
}

SALAM::BasicBlock::~BasicBlock()
{
}

SALAM::BasicBlock::BasicBlock_Debugger::BasicBlock_Debugger()
{
}

void
SALAM::BasicBlock::BasicBlock_Debugger::dumper(SALAM::BasicBlock *bb)
{
}

void
SALAM::BasicBlock::initialize(llvm::Value * irval, irvmap *vmap,
                SALAM::valueListTy * valueList) {
    if (dbg) {
        DPRINTFS(LLVMParse, owner,
                        "Initialize Values - BasicBlock::initialize\n");
    }
    Value::initialize(irval, vmap);
        //Parse irval for BasicBlock params
        llvm::BasicBlock * bb = llvm::dyn_cast<llvm::BasicBlock>(irval);
        assert(bb);

    for (auto it = llvm::pred_begin(bb); it != pred_end(bb); ++it) {
        llvm::BasicBlock * predecessor = *it;
        std::shared_ptr<SALAM::BasicBlock> pred =
            std::dynamic_pointer_cast<SALAM::BasicBlock>
            (vmap->find(predecessor)->second);
        predecessors.push_back(pred);
    }

    if (dbg) {
        DPRINTFS(LLVMParse, owner, "Initialize BasicBlocks\n");
    }

    for (auto inst_iter = bb->begin(); inst_iter != bb->end(); inst_iter++) {
        llvm::Instruction &inst = *inst_iter;
        std::shared_ptr<SALAM::Value> instval = vmap->find(&inst)->second;
        assert(instval);
        std::shared_ptr<SALAM::Instruction> instruct =
                std::dynamic_pointer_cast<SALAM::Instruction>(instval);
        assert(instruct);
        instructions.push_back(instruct);
        instruct->initialize(&inst, vmap, valueList);
        if (dbg) {
            DPRINTFS(LLVMParse, owner,
                            "Instruction (UID: %d) Initialization Complete\n",
                            instruct->getUID());
        }
    }
}
