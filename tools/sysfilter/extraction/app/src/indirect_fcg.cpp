/*
 * Copyright (C) 2017-2021, Brown University, Secure Systems Lab.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Brown University nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
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

#include <iostream>
#include <map>

#include "instr/linked-riscv.h"
#include "instr/linked-x86_64.h"

#include "chunk/function.h"
#include "elf/elfspace.h"
#include "elf/reloc.h"
#include "elf/symbol.h"
#include "indirect_fcg.h"
#include "operation/find2.h"

std::set<Function *> IndirectFCG::getAddrTakenFunctions(Conductor &conductor) {
    class LinkedFunctionPass : public ChunkPass {
    private:
        Function *currentFunction;

    public:
        std::set<Function *> addrTakenFunctions;

        virtual void visit(DataSection *section) { recurse(section); }

        virtual void visit(DataVariable *variable) {
            if (variable->getDest() == nullptr) return;

            auto link = variable->getDest();

            if (auto func = dynamic_cast<Function *>(link->getTarget())) {

                addrTakenFunctions.insert(func);
            }
#if 0
            else if (auto block
                = dynamic_cast<Block *>(link->getTarget())) {

                addrTakenFunctions.insert(static_cast<Function *>(
                    block->getParent()));
            }
            else if (auto instr
                = dynamic_cast<Instruction *>(link->getTarget())) {

                addrTakenFunctions.insert(static_cast<Function *>(
                    instr->getParent()->getParent()));
            }
#endif
        }

        virtual void visit(Instruction *instruction) {
            // should be a linked instruction
            auto li
                = dynamic_cast<LinkedInstruction *>(instruction->getSemantic());

            if (!li) return;
            auto link = li->getLink();
            auto target = link->getTarget();
            auto func_target = dynamic_cast<Function *>(target);
            if (func_target) { addrTakenFunctions.insert(func_target); }
            else if (auto plt = dynamic_cast<PLTTrampoline *>(target)) {
                if (auto ext_target
                    = dynamic_cast<Function *>(plt->getTarget())) {
                    addrTakenFunctions.insert(ext_target);
                }
                else {
                    std::cerr << "warning: plt isn't resolved to a function"
                              << std::endl;
                    std::cerr << currentFunction->getName() << ": "
                              << plt->getName() << std::endl;
                }
            }
            else if (auto instr = dynamic_cast<Instruction *>(target)) {
                auto gp
                    = static_cast<Function *>(instr->getParent()->getParent());
                if (gp) addrTakenFunctions.insert(gp);
            }
        }
    };

    LinkedFunctionPass lip;
    conductor.acceptInAllModules(&lip, false);
    return lip.addrTakenFunctions;
}

std::set<Instruction *> IndirectFCG::getIndirectFlowInstructions(
    Conductor &conductor) {
    class IndirectFlowPass : public ChunkPass {
    public:
        std::set<Instruction *> indirectFlows;

        virtual void visit(Instruction *instruction) {
            auto sem = instruction->getSemantic();
            if (!sem) return;
            auto as = sem->getAssembly();
            if (!as) return;

            if (as->getMnemonic() != "callq" && as->getMnemonic() != "jmpq") {
                return;
            }

            indirectFlows.insert(instruction);
        }
    };

    IndirectFlowPass ifp;
    conductor.acceptInAllModules(&ifp, false);

    return ifp.indirectFlows;
}

void CoarseIndirectFCG::run(Conductor &conductor) {
    auto functions = getAddrTakenFunctions(conductor);
    auto instructions = getIndirectFlowInstructions(conductor);

    outputCallgraph = inputCallgraph;

    /*for (auto func : functions) {
        std::cerr << "at func: " << func->getName() << std::endl;
    }*/

    std::set<Function *> indirectInvokers;

    for (auto instr : instructions) {
        auto function
            = dynamic_cast<Function *>(instr->getParent()->getParent());
        if (!function) {
            std::cerr
                << "something odd happened, instruction not part of function!"
                << std::endl;
            continue;
        }

        indirectInvokers.insert(function);
    }

    outputCallgraph.setImplicit(
        std::move(indirectInvokers), std::move(functions));
}
