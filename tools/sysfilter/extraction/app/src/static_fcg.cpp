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

#include "instr/linked-riscv.h"
#include "instr/linked-x86_64.h"

#include "static_fcg.h"

#include <iostream>
void StaticFCGPass::visit(Function *function) {
    currentFunction = function;
    considerExtra(function);
    recurse(function);
}

void StaticFCGPass::considerExtra(Function *function) {
    if (seen.count(function) != 0) {
	return;
    }
    seen.insert(function);

    if (extraFuncs.count(function) != 0) {
	LOG(1, "SFCG:  Found extra function source " << function->getName());
	extraFuncsFound.insert(function);

	auto &targets = extraFuncs[function];

	for (auto targetFunc : targets) {
	    LOG(1, "SFCG:  Adding extra function:  " << targetFunc->getName());
	    callgraph[function].insert(targetFunc);
	}
    }
}

void StaticFCGPass::visit(Instruction *instruction) {
    auto cfi
        = dynamic_cast<ControlFlowInstruction *>(instruction->getSemantic());

    if (!cfi) return;
    auto link = cfi->getLink();
    auto target = link->getTarget();
    auto func_target = dynamic_cast<Function *>(target);
    if (func_target) {
        callgraph[currentFunction].insert(func_target);
	considerExtra(func_target);
    }
    else if (auto plt = dynamic_cast<PLTTrampoline *>(target)) {
        if (auto ext_target = dynamic_cast<Function *>(plt->getTarget())) {
            callgraph[currentFunction].insert(ext_target);
	    considerExtra(ext_target);
        }
        else {
            std::cerr << "warning: plt isn't resolved to a function"
                      << std::endl;
            std::cerr << currentFunction->getName() << ": " << plt->getName()
                      << std::endl;
        }
    }
}
