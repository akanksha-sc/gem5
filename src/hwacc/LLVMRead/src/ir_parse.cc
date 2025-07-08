/*
 * Copyright (c) 2025 Akanksha Chaudhari, Matt Sinclair
 * All rights reserved.
 *
 * This file contains modifications and/or code derived from:
 * gem5-SALAM: https://github.com/TeCSAR-UNCC/gem5-SALAM
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "instruction.hh"
#include "ir_parse.hh"

namespace SALAM {
    int ir_parser(std::string file) {
        llvm::StringRef filename = file;
        llvm::LLVMContext context;
        llvm::SMDiagnostic error;

        // Load LLVM IR file
        llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> fileOrErr =
                llvm::MemoryBuffer::getFileOrSTDIN(filename);
        if (std::error_code ec = fileOrErr.getError()) {
            std::cerr << " Error opening input file: " + ec.message()
                    << std::endl;
            return 2;
        }

        // Load LLVM Module
        llvm::ErrorOr<std::unique_ptr<llvm::Module>> moduleOrErr =
                llvm::parseIRFile(filename, error, context);
        if (std::error_code ec = moduleOrErr.getError()) {
            std::cerr << "Error reading Module: " + ec.message() << std::endl;
            return 3;
        }

        std::unique_ptr<llvm::Module> m(llvm::parseIRFile(filename, error,
                context));
        if (!m) return 4;

        std::cout << "Successfully Loaded Module:" << std::endl;
        std::cout << " Name: " << m->getName().str() << std::endl;
        std::cout << " Target Triple: " << m->getTargetTriple() << std::endl;

        std::vector<std::shared_ptr<SALAM::Instruction>> inst_List;

        for (auto func_iter = m->getFunctionList().begin();
                       func_iter != m->getFunctionList().end(); func_iter++) {
            llvm::Function &f = *func_iter;
            std::cout << " Function: " << f.getName().str() << std::endl;
            for (auto bb_iter = f.getBasicBlockList().begin(); bb_iter !=
                            f.getBasicBlockList().end(); bb_iter++) {
                llvm::BasicBlock &bb = *bb_iter;
                std::cout << "  BasicBlock: " << bb.getName().str()
                        << std::endl;
                for (auto inst_iter = bb.begin(); inst_iter != bb.end();
                                inst_iter++) {
                    llvm::Instruction &llvm_inst = *inst_iter;
                    SALAM::register_instruction(llvm_inst.clone(), inst_List);
                }
            }
        }

        // Test Function Only
        for (auto inst_list_it = inst_List.begin() ; inst_list_it !=
                        inst_List.end(); inst_list_it++) {
            (*inst_list_it)->test();
        }

        return 0;
    }

    void register_instruction(llvm::Instruction * inst,
                std::vector<std::shared_ptr<SALAM::Instruction>> &inst_List) {
        std::shared_ptr<SALAM::Instruction>
                newInst(new SALAM::Instruction(inst));
        inst_List.push_back(std::move(newInst));
    }
}
