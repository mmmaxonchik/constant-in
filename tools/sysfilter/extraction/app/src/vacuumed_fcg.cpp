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

#include <cassert>

#include "chunk/dump.h"
#include "chunk/program.h"
#include "elf/elfspace.h"
#include "instr/linked-x86_64.h"

#include "vacuumed_fcg.h"

#include "log.h"

VacuumedCallgraph::VacuumedCallgraph(
    Program *program, DataSymbolList &dataSymbols)
    : program(program), dataSymbols(dataSymbols) {
    // _dl_find_dso_for_object is a harmless helper function that
    // doesn't invoke any syscalls unless an assertion fails,
    // but that case will be tracked elsewhere in libc.
    ignoreUnresolvedPLT.insert("_dl_find_dso_for_object@plt");
    ignoreUnresolvedPLT.insert("_dl_find_dso_for_object");

    // __tls_get_addr is another harmless helper function that's in the loader.
    ignoreUnresolvedPLT.insert("__tls_get_addr");

    // _dl_argv is a data symbol. it's fine.
    ignoreUnresolvedPLT.insert("_dl_argv");

    // this is present in the GOT for libc even when libpthread isn't present.
    ignoreUnresolvedPLT.insert("__libpthread_freeres");
    // invokes malloc, but otherwise harmless function from loader.
    ignoreUnresolvedPLT.insert("__tunable_get_val");
    // weak symbol, may not exist, which is fine.
    ignoreUnresolvedPLT.insert("_dl_starting_up");
    ignoreUnresolvedPLT.insert("_dl_starting_up");
    // more loader functionality...
    ignoreUnresolvedPLT.insert("_dl_exception_create");

    // this one's from libdl. weak symbol, only invoked when libdl is used.
    ignoreUnresolvedPLT.insert("__libdl_freeres");

    // these are pthread functions only invoked by libstdc++ when pthread is
    // actually loaded; so if they're unresolved it's OK.
    ignoreUnresolvedPLTInContext.insert(std::make_pair(
        std::string("uw_init_context_1"), std::string("pthread_once@plt")));
    ignoreUnresolvedPLTInContext.insert(
        std::make_pair(std::string("_ZNSt6locale13_S_initializeEv"),
            std::string("pthread_once@plt")));
    ignoreUnresolvedPLTInContext.insert(
        std::make_pair(std::string("_ZNSt6locale5facet15_S_get_c_localeEv"),
            std::string("pthread_once@plt")));
    ignoreUnresolvedPLTInContext.insert(std::make_pair(
        std::string("_ZSt9call_onceIMSt6threadFvvEJSt17reference_wrapperIS0_"
                    "EEEvRSt9once_flagOT_DpOT0_"),
        std::string("pthread_once@plt")));
    ignoreUnresolvedPLTInContext.insert(std::make_pair(
        std::string("_ZNSt6thread4joinEv"), std::string("pthread_join@plt")));
}

Callgraph VacuumedCallgraph::generate() {
    // Prevent any excluded functions from being visited
    visitedFunctions.insert(excludeFunctions.begin(), excludeFunctions.end());

    // generateDataSymbols();

    while (functionRoots.size() || dataRoots.size()) {
        while (functionRoots.size()) {
            LOG(11,
                "walking new function root "
                    << functionRoots.back()->getName());
            auto next = functionRoots.back();
            functionRoots.pop_back();
            walk(next);
        }
        while (dataRoots.size()) {
            LOG(11, "walking new data root... " << dataRoots.back()->start);
            auto next = dataRoots.back();
            dataRoots.pop_back();
            walk(next);
        }
    }

    // If any excluded functions were added to the the source/target
    // list, remove them.  We know we did not recurse into these functions,
    // since they were marked visited before starting.
    for (auto f : excludeFunctions) {
        indirectSources.erase(f);
        indirectTargets.erase(f);
    }

    // ensure nodes exist in the callgraph map for all sources and targets
    for (auto source : indirectSources) {
        result[source].count(nullptr);
    }
    for (auto target : indirectTargets) {
        result[target].count(nullptr);
    }

    result.setImplicit(std::move(indirectSources), std::move(indirectTargets));

    return result;
}

void VacuumedCallgraph::generateDataSymbols() {
    // dataSymbols.generate(program);
}

void VacuumedCallgraph::walk(Function *function) {
    class FunctionWalker : public ChunkPass {
    private:
        VacuumedCallgraph *vc;
        Function *currentFunction;

    public:
        FunctionWalker(VacuumedCallgraph *vc) : vc(vc) { }

        virtual void visit(Function *function) {
            currentFunction = function;
            recurse(function);
        }

        virtual void visit(Instruction *instruction) {
            IF_LOG(11) {
                ChunkDumper cd;
                cd.visit(instruction);
            }
            if (auto cfi = dynamic_cast<ControlFlowInstruction *>(
                    instruction->getSemantic())) {
                visit(cfi);
            }
            else if (auto li = dynamic_cast<LinkedInstruction *>(
                         instruction->getSemantic())) {
                visit(li);
            }
            else if (/* auto ici = */ dynamic_cast<IndirectCallInstruction *>(
                instruction->getSemantic())) {
                vc->indirectSources.insert(currentFunction);
            }
            else if (auto iji = dynamic_cast<IndirectJumpInstruction *>(
                         instruction->getSemantic())) {
                if (!iji->isForJumpTable()) {
                    vc->indirectSources.insert(currentFunction);
                }
            }
            else if (auto dlcfi
                = dynamic_cast<DataLinkedControlFlowInstruction *>(
                    instruction->getSemantic())) {
                vc->indirectSources.insert(currentFunction);
                visit(dlcfi);
            }
        }

        void doLinked(Link *link) {
            auto target = link->getTarget();

            if (auto funcTarget = dynamic_cast<Function *>(target)) {
                vc->indirectTargets.insert(funcTarget);
                vc->consider(funcTarget);
            }
            else if (auto plt = dynamic_cast<PLTTrampoline *>(target)) {
                if (auto pltTarget
                    = dynamic_cast<Function *>(plt->getTarget())) {
                    vc->indirectTargets.insert(pltTarget);
                    vc->consider(pltTarget);
                }
                else {
                    std::string fname = currentFunction->getName();
                    std::string pltname = plt->getName();

                    if (vc->ignoreUnresolvedPLT.count(pltname) == 0
                        && vc->ignoreUnresolvedPLTInContext.count(
                               std::make_pair(fname, pltname))
                            == 0) {

                        LOG(1, "warning: plt isn't resolved to a function");
                        LOG(1, fname << ": " << pltname);
                    }
                }
            }
            else if (DataSymbol *ds
                = vc->dataSymbols.forAddressMaybe(link->getTargetAddress())) {
                LOG(12, "referencing a DS (" << ds->start << ")");
                vc->consider(ds);
            }
            else if (DataSection *ds = dynamic_cast<DataSection *>(target)) {
                if (link->getTargetAddress()
                    == ds->getAddress() + ds->getSize()) {
                    LOG(11, "Found reference to end of DataSection");
                }
                else if (ds->getType() == DataSection::TYPE_BSS) {
                    LOG(11, "Found reference to BSS section");
                }
                else {
                    LOG(1,
                        "Found reference to unknown point in DataSection: "
                        "target is "
                            << link->getTargetAddress()
                            << " and DataSection range is "
                            << target->getRange());
                }
            }
            else if (target == nullptr) {
                // this probably means that _end is being accessed and we're
                // parsing ld-linux.so. check to see if it's the very last
                // known data address; if so, we'll let it slide.

                if (link->getTargetAddress()
                    != vc->dataSymbols.getHighestAddress()) {
                    auto module = dynamic_cast<Module *>(
                        currentFunction->getParent()->getParent());
                    LOG(1,
                        "Unknown link in "
                            << module->getName()
                            << ", target appears to be at offset "
                            << link->getTargetAddress()
                                - module->getBaseAddress());
                    LOG(1,
                        "Highest address: "
                            << vc->dataSymbols.getHighestAddress());
                }
                else {
                    LOG(11, "Found reference to end of data section.");
                }
            }
            else {
                LOG(1, "unhandled target " << target);
                LOG(1, "    of type " << typeid(*target).name());
                LOG(1, "    from link of type " << typeid(*link).name());
                LOG(1, "    targetting address " << target->getAddress());
                LOG(1, "    from address " << target->getAddress());
            }
        }

        void visit(LinkedInstruction *li) { doLinked(li->getLink()); }

        void visit(DataLinkedControlFlowInstruction *dlcfi) {
            doLinked(dlcfi->getLink());
        }

        void visit(ControlFlowInstruction *cfi) {
            auto link = cfi->getLink();
            auto target = link->getTarget();
            auto func_target = dynamic_cast<Function *>(target);
            if (func_target) {
                vc->result[currentFunction].insert(func_target);
                vc->consider(func_target);
            }
            else if (auto plt = dynamic_cast<PLTTrampoline *>(target)) {
                if (auto ext_target
                    = dynamic_cast<Function *>(plt->getTarget())) {
                    vc->result[currentFunction].insert(ext_target);
                    vc->consider(ext_target);
                }
                else if (vc->shouldIgnoreUnresolvedPLT(
                             currentFunction->getName(), plt->getName())) {

                    LOG(1,
                        "warning: plt isn't resolved to a function: "
                            << currentFunction->getName() << ": "
                            << plt->getName());
                }
            }
            else if (auto instr = dynamic_cast<Instruction *>(target)) {
                auto gp
                    = static_cast<Function *>(instr->getParent()->getParent());
                if (gp) {
                    vc->result[currentFunction].insert(gp);
                    vc->consider(gp);
                }
            }
            else if (!target) {
                LOG(1,
                    "control-flow instruction with NULL target at address "
                        << cfi->getSource()->getAddress() << ", parent at "
                        << cfi->getSource()->getParent()->getAddress());
                LOG(1,
                    "in " << dynamic_cast<Module *>(
                        currentFunction->getParent()->getParent())
                                 ->getName());
                LOG(1,
                    "CONTINUING, but probably shouldn't. callgraph will be "
                    "incomplete.");
            }
            else {
                LOG(1, "unhandled CFI target type:" << typeid(*target).name());
            }
        }
    };

    FunctionWalker fw(this);
    fw.visit(function);
}

void VacuumedCallgraph::walk(DataSymbol *ds) {
    // deal with self-referential DataSymbols
    LOG(12, "seen data symbol " << ds->start);

    for (auto ref : ds->dataReferences) {
        LOG(12, "    contains reference to ds " << ref->start);
        consider(ref);
    }
    for (auto ref : ds->codeReferences) {
        LOG(12, "    contains reference to code " << ref->getName());
        consider(ref);
        indirectTargets.insert(ref);
    }
}

void VacuumedCallgraph::consider(Function *function) {
    if (visitedFunctions.count(function)) return;

    LOG(11, "Considering new function " << function->getName());
    functionRoots.push_back(function);
    visitedFunctions.insert(function);

    if (extraFuncs.count(function) != 0) {
	extraFuncsFound.insert(function);
	LOG(1, "Found extra function source " << function->getName());

	auto targets = extraFuncs[function];
	for (auto f : targets) {
	    LOG(1, "Considering extra function " << f->getName());
	    consider(f);
	}
    }
}

void VacuumedCallgraph::consider(DataSymbol *ds) {
    if (visitedDataSymbols.count(ds)) return;
    dataRoots.push_back(ds);
    visitedDataSymbols.insert(ds);
}

bool VacuumedCallgraph::shouldIgnoreUnresolvedPLT(
    const std::string &fname, const std::string &pltName) {

    if (ignoreUnresolvedPLT.count(pltName) == 0
        && ignoreUnresolvedPLTInContext.count(std::make_pair(fname, pltName))
            == 0)
        return false;

    return true;
}
