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

#include <cstring>
#include <iostream>

#include "analysis/dataflow.h"
#include "analysis/slicingtree.h"
#include "analysis/usedef.h"
#include "analysis/usedefutil.h"
#include "analysis/walker.h"
#include "chunk/module.h"
#include "elf/elfmap.h"
#include "elf/elfspace.h"
#include "strarg_recorder.h"

#include "log.h"

class StringRetriever {
private:
    bool all_constants = true;
    Program *program;
    std::set<UDState *> seen;
    const char *readStringAtAddress(address_t address);
    std::set<std::string> values;

public:
    StringRetriever(Program *program) : program(program) {};
    void notConstant() { all_constants = false; };
    bool allConstant() { return all_constants; };
    void retrieve(UDState *state, int curreg);

    const std::set<std::string> &getValues() const { return values; }
};

const char *StringRetriever::readStringAtAddress(address_t address) {
    if (address == 0) return nullptr;

    Module *candidate = nullptr;
    for (auto module : CIter::children(program)) {
        if (module->getBaseAddress() > address) continue;
        if (module->getBaseAddress() <= address) {
            if (!candidate
                || module->getBaseAddress() > candidate->getBaseAddress()) {
                candidate = module;
            }
        }
    }

    if (!candidate) {
        LOG(-1, "unknown/unhandled constant address " << address);
        LOG(-1, "no module found");
        return nullptr;
    }

    address_t module_offset = address - candidate->getBaseAddress();

    auto elfmap = candidate->getElfSpace()->getElfMap();
    for (auto sec : elfmap->getSectionList()) {
        if (module_offset < sec->getVirtualAddress()) continue;
        if (module_offset >= sec->getVirtualAddress() + sec->getSize())
            continue;
        auto sec_offset = sec->convertVAToOffset(module_offset);
        // LOG(1, "found string in section [" << sec->getName() << "] at offset
        // " << sec_offset);

        const char *ret = reinterpret_cast<const char *>(sec->getReadAddress())
            + sec_offset;
        size_t remsize = sec->getSize() - sec_offset;
        size_t length = strnlen(ret, remsize);
        // check that it's null-terminated
        if (length == remsize) {
            LOG(10, "not null-terminated!");
            return nullptr;
        }
        // check that it's printable
        const char *p = ret;
        while (*p)
            if (!isprint(*(p++))) {
                LOG(10, "non-printable character!");
                return nullptr;
            }
        return ret;
    }

    return nullptr;
}

void StringRetriever::retrieve(UDState *state, int curreg) {
    if (seen.find(state) == seen.end()) {
        auto refstates = state->getRegRef(curreg);
        if (refstates.size() == 0) { all_constants = false; }
        for (auto s : refstates) {
            TreeNode *node = nullptr;
            node = s->getRegDef(curreg);
            if (auto linstr = dynamic_cast<LinkedInstruction *>(
                    s->getInstruction()->getSemantic())) {
                address_t va = linstr->getLink()->getTargetAddress();
                auto str = readStringAtAddress(va);
                if (!str) {
                    LOG(1, "Failed to read string constant!");
                    values.insert("<failed>");
                }
                else
                    values.insert(str);
            }
            else if (auto cnode = dynamic_cast<TreeNodeConstant *>(node)) {
                address_t va = cnode->getValue();
                // LOG(1, "found dl*() constant at " << va);
                auto str = readStringAtAddress(va);
                if (!str) {
                    LOG(1, "Failed to read string constant!");
                    values.insert("<failed>");
                }
                else
                    values.insert(str);
            }
            else if (auto rnode
                = dynamic_cast<TreeNodePhysicalRegister *>(node)) {
                auto reg = rnode->getRegister();
                retrieve(s, reg);
            }
            else {
                all_constants = false;
                values.insert("<failed>");
            }
        }
        seen.insert(state);
    }
}

const std::string StrArgRecorder::ResolveFailed = "<failed>";

void StrArgRecorder::visit(Function *function) {
    auto graph = ControlFlowGraph(function);
    auto config = UDConfiguration(&graph);
    auto working = UDRegMemWorkingSet(function, &graph);
    auto usedef = UseDef(&config, &working);

    SccOrder order(&graph);
    order.genFull(0);
    usedef.analyze(order.get());

    LOG(10,
        "StrArgRecorder for function "
            << function->getName() << " in module "
            << function->getParent()->getParent()->getName());

    for (auto block : CIter::children(function)) {
        for (auto instr : CIter::children(block)) {
            auto assembly = instr->getSemantic()->getAssembly();
            StringRetriever sr(dynamic_cast<Program *>(
                function->getParent()->getParent()->getParent()));
            auto state = working.getState(instr);
            if (auto cfi = dynamic_cast<ControlFlowInstruction *>(
                    instr->getSemantic())) {
                Function *func_target;
                auto target = cfi->getLink()->getTarget();
                if (auto plt = dynamic_cast<PLTTrampoline *>(target)) {
                    func_target = dynamic_cast<Function *>(plt->getTarget());
                }
                else {
                    func_target = dynamic_cast<Function *>(target);
                }
                if (func_target != this->target) continue;

                LOG(1, "starting search from " << instr->getAddress());

                sr.retrieve(state, targetReg);
            }
            else {
                continue;
            }

            for (auto s : sr.getValues()) {
                values[instr].insert(s);
            }
            if (sr.getValues().size() == 0) {
                values[instr].insert(ResolveFailed);
            }
        }
    }
}
