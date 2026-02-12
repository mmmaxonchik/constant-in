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
#include <memory>

#include "analysis/dataflow.h"
#include "analysis/slicingtree.h"
#include "analysis/usedef.h"
#include "analysis/usedefutil.h"
#include "analysis/walker.h"
#include "chunk/module.h"
#include "elf/elfmap.h"
#include "elf/elfspace.h"

#include "arg_tracker.h"
#include "constant_retriever.h"

#include "log.h"

// const std::string ArgTracker::ResolveFailed = "<failed>";

template <typename T>
void ArgTracker<T>::visit(Function *function) {
    visit(function, this->target, this->targetReg);
    LOG(1,
        "Completed argtrack on " << function->getName() << " (" << values.size()
                                 << " results)");
    if (!foundSomeCall) {
        LOG(1, "No calls found!  Adding failure with RT_NO_CALL");
        ArgTrackValue value(argType, nullptr, RT_NO_CALL);
        values[nullptr].push_back(value);
    }
}

template <typename T>
void ArgTracker<T>::visit(Function *function, Function *targetFunc, int reg) {
    if (funcsSeen.count(function) != 0) { return; }

    funcsSeen.insert(function);

    auto graph = ControlFlowGraph(function);
    auto config = UDConfiguration(&graph);
    auto working = UDRegMemWorkingSet(function, &graph);
    auto usedef = UseDef(&config, &working);
    auto program = dynamic_cast<Program *>(
        function->getParent()->getParent()->getParent());
    assert(program);

    SccOrder order(&graph);
    order.genFull(0);
    usedef.analyze(order.get());
    // bool searchedCallers = false;

    LOG(1,
        "ArgTracker(" << this->target->getName() << ") for function "
                      << function->getName() << " in module "
                      << function->getParent()->getParent()->getName()
                      << " with target " << targetFunc->getName());

    for (auto block : CIter::children(function)) {
        for (auto instr : CIter::children(block)) {
            auto assembly = instr->getSemantic()->getAssembly();
            auto state = working.getState(instr);

	    auto sr = std::make_unique<ConcreteRegisterRetriever<T>>(program, dataSymbols);

            // auto valuesAtStart = values.size();

            callStack.push_back(instr);

            // Search this function for a call to targetFunc
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

                if (func_target != targetFunc) { continue; }

                LOG(1, "starting search from " << instr->getAddress());
                foundSomeCall = true;
                sr->retrieve(state, reg);
            }
            else {
                // No call instruction found
                continue;
            }

            auto reportInstr = callStack.front(); // TODO:  Report whole stack

            for (auto v : sr->getValues()) {
                ArgTrackValue value(argType, reportInstr, sr->getStatus(), v);
                values[instr].push_back(value);
            }

            if (sr->getValues().size() == 0) {
                LOG(1,
                    "No value found, adding failing result with status "
                        << sr->getStatus());
                ArgTrackValue value(argType, reportInstr, sr->getStatus());
                values[instr].push_back(value);
            }
            callStack.pop_back();
        }
    }
}

template <typename T>
ArgTrackInfo ArgTracker<T>::getResults() {
    ArgTrackInfo info;

    info.in_function = callingFunc;
    info.argType = argType;
    info.targetReg = targetReg;
    info.func_is_implicit_target = callgraph.isImplicitTarget(callingFunc);
    info.func_has_direct_edge = callgraph.hasDirectEdge(callingFunc);

    info.target = target;
    info.values = values;

    return info;
}

template <typename T>
void ArgTracker<T>::considerCallers(Function *callee, int reg) {
    for (auto caller : callgraph.getKeys()) {
        auto targets = callgraph[caller];

        if (targets.count(callee) == 0) { continue; }
        else {
            LOG(1,
                "Found " << caller->getName() << " as caller of "
                         << callee->getName());
            visit(caller, callee, reg);
        }
    }
}

void ArgTrackInfo::writeArgTrackInfo(nlohmann::json &arr) {
    if (!in_function) {
	auto obj = nlohmann::json::object();
	obj["ver"] = ARG_TRACK_JSON_VERSION;
	obj["target_function"] = target->getName();
	obj["register"] = targetReg;
	obj["in_module"] = "";
	obj["in_function"] = "";
	obj["status"] = RT_TARGET_IS_AT;
	obj["arg_type"] = static_cast<int>(argType);
	obj["value"] = "";

	arr.push_back(obj);
    } else if (argType == TYPE_USEONLY) {
	auto mod = in_function->getParent()->getParent();

	auto obj = nlohmann::json::object();
	obj["ver"] = ARG_TRACK_JSON_VERSION;
	obj["in_module"] = mod->getName();
	obj["in_function"] = in_function->getName();
	obj["target_function"] = target->getName();
	obj["register"] = targetReg;
	obj["status"] = 0;
	obj["arg_type"] = static_cast<int>(argType);
	obj["value"] = "";

	arr.push_back(obj);
    } else {
	auto mod = in_function->getParent()->getParent();

        for (auto &kv : values) {
            Instruction *instr = kv.first;
            std::vector<ArgTrackValue> vals = kv.second;

	    for (auto atv : vals) {
		auto obj = nlohmann::json::object();
		obj["ver"] = ARG_TRACK_JSON_VERSION;
		obj["in_module"] = mod->getName();
		obj["in_function"] = in_function->getName();
		obj["target_function"] = target->getName();
		obj["register"] = targetReg;
		obj["func_implicit"] = func_is_implicit_target;
		obj["func_direct"] = func_has_direct_edge;

                if (!instr) { obj["at_offset"] = NULL; }
                else {
                    obj["at_offset"]
                        = instr->getAddress() - in_function->getAddress();
                }

                obj["status"] = static_cast<int>(atv.status);
                obj["arg_type"] = static_cast<int>(atv.argType);

		if (atv.argType == TYPE_STRING) {
		    obj["value"] = atv.strValue;
		} else if (atv.argType == TYPE_INTEGER) {
		    obj["value"] = atv.intValue;

		} else if (atv.argType == TYPE_ADDRESS) {
		    obj["value"] = atv.intValue;
		    if (atv.funcFromValue) {
			auto thisModule = atv.funcFromValue->getParent()->getParent();
			obj["module_from_value"] = thisModule->getName();
			obj["function_from_value"] = atv.funcFromValue->getName();
		    }
		} else {
		    throw std::logic_error("Unknown argtrack value type!");
		}

		// if (atv.isAddr) {
		//     obj["function_from_value"] = atv.funcValue;
		// }

		arr.push_back(obj);
	    }
	}
    }
}

ArgTrackInfo ArgTrackInfo::buildTrackedImplicitFailure(
    Function *trackedFunc, TrackableArgType argType) {
    ArgTrackInfo info;
    info.in_function = nullptr;
    info.target = trackedFunc;
    info.argType = argType;

    info.func_is_implicit_target = false;
    info.func_has_direct_edge = false;

    return info;
}

template class ArgTracker<std::string>;
template class ArgTracker<long int>;
template class ArgTracker<AddressInfo>;
