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
#include "chunk/library.h"
#include "chunk/module.h"
#include "elf/elfmap.h"
#include "elf/elfspace.h"
#include "log/temp.h"

#include "constant_retriever.h"

#include "log.h"

void ConstantRetriever::retrieve(UDState *state, int curreg) {
    regsSearched.push_back(curreg);
    // auto module = static_cast<Module *>(state->getInstruction()
    // 					->getParent()
    // 					->getParent()
    // 					->getParent()
    // 					->getParent());
    if (seen.find(state) == seen.end()) {
        auto refstates = state->getRegRef(curreg);
        if (refstates.size() == 0) {
            LOG(1, "Failed to find any register definitions for RAX!");
            all_constants = false;
            addStatusFlag(RT_NO_REF_STATE);
        }
        for (auto s : refstates) {
            TreeNode *node = nullptr;
            int count = 0;
            for (auto pair : s->getRegDefList()) {
                if (pair.second) {
                    count++;
                    node = pair.second;
                }
            }
            if (count > 1) {
                TreePrinter tp;
                LOG(1, "More than one definition in a single state!");
                for (auto pair : s->getRegDefList()) {
                    if (pair.second) { pair.second->print(tp); }
                }
            }

#if 0
	    if (auto linstr = dynamic_cast<LinkedInstruction *>(
                    s->getInstruction()->getSemantic())) {
		auto link = linstr->getLink();
		if (auto dLink = dynamic_cast<DataOffsetLink *>(link)) {
		    if (auto pltLink = dynamic_cast<PLTTrampoline *>(dLink)) {
			auto extTarget = dynamic_cast<Function *>(pltLink->getTarget());
			assert(pltLink);
			assert(extTarget);
			address_t va = extTarget->getAddress();
			cs.insert(va);
		    } else {
			LOG(1, "PTH:  DataOffsetLink not a PLT link!");
		    }
		    address_t va = dLink->getTargetAddress();
		    cs.insert(va);
		} else if (link->isWithinModule()) {
		    address_t modAddress = module->getBaseAddress();
		    address_t offset = link->getTargetAddress();
		    address_t va = modAddress + offset;
		    cs.insert(va);
		} else {
		    address_t va = linstr->getLink()->getTargetAddress();
		    cs.insert(va);
		}
            }
#else
	    if (0) {
		// Empty
	    }
#endif
	    else if (auto cnode = dynamic_cast<TreeNodeConstant *>(node)) {
                cs.insert(cnode->getValue());
            }
            else if (auto rnode
                = dynamic_cast<TreeNodePhysicalRegister *>(node)) {
                auto reg = rnode->getRegister();
                retrieve(s, reg);
            }
            else if (auto rnode = dynamic_cast<TreeNodeAddition *>(node)) {
                bool hasRIP = false;
                bool hasOffset = false;
                address_t offset = 0;
                address_t rip = 0;

		auto resolveSide = [&](TreeNode *hs) {
		 if (auto nn = dynamic_cast<TreeNodeConstant *>(hs)) {
		     hasOffset = true;
		     offset = nn->getValue();
		 } else if (auto nn = dynamic_cast<TreeNodeRegisterRIP *>(hs)) {
		     hasRIP = true;
		     rip = nn->getValue();
		 }
	        };
		resolveSide(rnode->getLeft());
		resolveSide(rnode->getRight());
		if (hasRIP && hasOffset) {
		    LOG(1, "Resolved RIP-relative offset!");
		    address_t value = rip + offset;
		    cs.insert(value);
		} else {
		    LOG(1, "Conditions for resolving RIP-relative offset not met");
		    s->dumpState();
		    TreePrinter tp;
		    node->print(tp);
		    all_constants = false;
		    addStatusFlag(RT_NO_ARITH);
		}
	    }
            else {
                LOG(1,
                    "At instruction 0x"
                        << std::hex << state->getInstruction()->getAddress());
                LOG(1,
                    "In module "
                        << static_cast<Module *>(state->getInstruction()
                                                     ->getParent()
                                                     ->getParent()
                                                     ->getParent()
                                                     ->getParent())
                               ->getName());

                if (!node) {
                    LOG(1,
                        "No value definition available while tracking value "
                        "of register "
                            << curreg);
                }
                else {
                    LOG(1,
                        "Don't know how to parse node for value of register "
                            << curreg);
                    // s->dumpState();
                    // TreePrinter tp;
                    // node->print(tp);
                }
                all_constants = false;
                addStatusFlag(RT_UNKNOWN_OP);
            }
        }
        seen.insert(state);
    }
}

const char *ATStringRetriever::readStringAtAddress(address_t address) {
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

void ATStringRetriever::retrieve(UDState *state, int curreg) {
    regsSearched.push_back(curreg);

    if (seen.find(state) == seen.end()) {
        auto refstates = state->getRegRef(curreg);
        if (refstates.size() == 0) {
            // If we are here, we don't know the state of this register
            // (eg, it's an argument register)
            all_constants = false;
            addStatusFlag(RT_NO_REF_STATE);
        }
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
                    addStatusFlag(RT_BAD_STRING);
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
                    addStatusFlag(RT_BAD_STRING);
                }
                else
                    values.insert(str);
            }
            else if (auto rnode
                = dynamic_cast<TreeNodePhysicalRegister *>(node)) {
                auto reg = rnode->getRegister();
                retrieve(s, reg);
            }
#if 0
	    else if (auto rnode
		= dynamic_cast<TreeNodeAddition *>(node)) {
		bool hasRIP = false;
		bool hasOffset = false;
		address_t offset = 0;
		address_t rip = 0;

		auto resolveSide = [&](TreeNode *hs) {
		 if (auto nn = dynamic_cast<TreeNodeConstant *>(hs)) {
		     hasOffset = true;
		     offset = nn->getValue();
		 } else if (auto nn = dynamic_cast<TreeNodeRegisterRIP *>(hs)) {
		     hasRIP = true;
		     rip = nn->getValue();
		 }
	        };
		resolveSide(rnode->getLeft());
		resolveSide(rnode->getRight());
		if (hasRIP && hasOffset) {
		    LOG(1, "Resolved RIP-relative offset!");
		    address_t va = rip + offset;

		    auto str = readStringAtAddress(va);
		    if(!str) {
			LOG(1, "Failed to read string constant!");
			values.insert("<failed>");
			addStatusFlag(RT_BAD_STRING);
		    }
		    else {
			values.insert(str);
		    }
		} else {
		    LOG(1, "Conditions for resolving RIP-relative offset not met");
		    TreePrinter tp;
		    node->print(tp);
		}
	    }
#endif
            else {
                // If we are here, the dataflow tracking led us to
                // something we can't parse (eg, memory op)
                all_constants = false;
                values.insert("<failed>");
                addStatusFlag(RT_UNKNOWN_OP);
            }
        }
        seen.insert(state);
    }
}

void AddressRetriever::retrieve(UDState *state, int curreg) {
    regsSearched.push_back(curreg);
    auto module = static_cast<Module *>(state->getInstruction()
					->getParent()
					->getParent()
					->getParent()
					->getParent());
    if (seen.find(state) == seen.end()) {
        auto refstates = state->getRegRef(curreg);
        if (refstates.size() == 0) {
            LOG(1, "Failed to find any register definitions for RAX!");
            all_constants = false;
        }
        for (auto s : refstates) {
            TreeNode *node = nullptr;
            int count = 0;
            for (auto pair : s->getRegDefList()) {
                if (pair.second) {
                    count++;
                    node = pair.second;
                }
            }
            if (count > 1) {
                TreePrinter tp;
                LOG(1, "More than one definition in a single state!");
                for (auto pair : s->getRegDefList()) {
                    if (pair.second) {
                        pair.second->print(tp);
                    }
                }
            }
	    if (auto linstr = dynamic_cast<LinkedInstruction *>(
                    s->getInstruction()->getSemantic())) {
		auto link = linstr->getLink();
		auto target = link->getTarget();

		if (auto funcTarget = dynamic_cast<Function *>(target)) {
		    address_t va = funcTarget->getAddress();
		    cs.emplace(va, funcTarget);
		}  else if (auto plt = dynamic_cast<PLTTrampoline *>(target)) {
		    if (auto pltTarget
			= dynamic_cast<Function *>(plt->getTarget())) {
			address_t va = pltTarget->getAddress();
			cs.emplace(va, funcTarget);
		    } else {
			LOG(1, "ArgTracker could not resolve PLT link for instruction");
			addStatusFlag(RT_ADDR_RESOLUTION_FAILED);
		    }
		} else if (auto dLink = dynamic_cast<DataOffsetLink *>(link)) {
		    auto dataRegionList = module->getDataRegionList();

		    auto dataVar = dataRegionList->findVariable(dLink->getTargetAddress());
		    if (dataVar) {
			auto dataTarget = dataVar->getDest()->getTarget();
			if (auto funcTarget = dynamic_cast<Function *>(dataTarget)) {
			    address_t va = funcTarget->getAddress();
			    cs.emplace(va, funcTarget);
			} else {
			    LOG(1, "ArgTracker found unhandled DataOffsetLink type");
			    addStatusFlag(RT_ADDR_RESOLUTION_FAILED);
			}
		    } else { // This address is not the start of a variable
			LOG(1, "ArgTracker found address that might be inside data symbol");

			std::set<Function *> matches;
			auto found = findFuncsInDataSymbol(dLink->getTargetAddress(),
							   matches);
			for (auto funcTarget : matches) {
			    address_t va = funcTarget->getAddress();
			    cs.emplace(va, funcTarget);
			}
			if ((!found) || (matches.size() == 0)) {
			    LOG(1, "ArgTracker found no matches for data symbol");
			    addStatusFlag(RT_ADDR_SYMBOL_RESOLUTION_FAILED);
			}
		    }
		} else {
		    LOG(1,
			"ArgTracker could not handle link at instruction 0x"
			<< std::hex << state->getInstruction()->getAddress());
		    LOG(1, "In module " << module->getName());
		    if (!node) {
			LOG(1,
			    "No value definition available while tracking value "
			    "of register "
			    << curreg);
		    }
		    else {
			LOG(1,
			    "Don't know how to parse node for value of register "
			    << curreg);
			s->dumpState();
			TreePrinter tp;
			node->print(tp);
		    }
		    all_constants = false;
		    addStatusFlag(RT_ADDR_RESOLUTION_FAILED);
		}
            }
	    else if (auto cnode = dynamic_cast<TreeNodeConstant *>(node)) {
		LOG(1, "Argtracker dataflow tracking for address resulted in constant, should not be here");
                cs.emplace(cnode->getValue(), nullptr);
		addStatusFlag(RT_ASSERT);
            }
            else if (auto rnode
                = dynamic_cast<TreeNodePhysicalRegister *>(node)) {
                auto reg = rnode->getRegister();
                retrieve(s, reg);
            } else {
                LOG(1,
                    "At instruction 0x"
		    << std::hex << state->getInstruction()->getAddress());
                LOG(1, "In module " << module->getName());
                if (!node) {
                    LOG(1,
                        "No value definition available while tracking value "
                        "of register "
			<< curreg);
                }
                else {
                    LOG(1,
                        "Don't know how to parse node for value of register "
			<< curreg);
#if 1
                    s->dumpState();
		    TreePrinter tp;
		    node->print(tp);
#endif
                }
                all_constants = false;
		addStatusFlag(RT_UNKNOWN_OP);
            }
	}
	seen.insert(state);
    }
}

bool AddressRetriever::findFuncsInDataSymbol(address_t searchAddr,
					     std::set<Function *> &funcs) {
    bool found = false;

    std::set<DataSymbol *> matches;// = dataSymbols.getByName(symName);

    std::vector<DataSymbol *> toConsider;
    std::set<DataSymbol *> symbolsConsidered;
    std::set<Function *> out;

#if 0
    auto getMainModule = [&](void) {
	auto mainModule = program->getMain();
	if (!mainModule) {
	    mainModule = program->getFirst();
	    if (!mainModule) {
		throw std::runtime_error(
		    "No main module found!");
	    }
	}
	return mainModule;
    }
#endif

    // Try to find the symbol
    auto ds = dsList->forAddress(searchAddr);
    assert(ds);
    matches.insert(ds);

    if (matches.size() == 0) {
	LOG(1, "CDSS:  No matches found!");
	found = false;
    } else {
	found = true;
	for (auto ds : matches) {
	    LOG(1, "CDSS:  Considering initial symbol:  "
		//<< ds->name
		<< "@0x" << std::hex << ds->start);
	    toConsider.push_back(ds);
	}
    }

    while (toConsider.size() != 0) {
	DataSymbol *sym = toConsider.back();
	toConsider.pop_back();
	symbolsConsidered.insert(sym);

	LOG(1, "CDSS:  Considering symbol:  "
	    //<< sym->name
	    << "@0x" << std::hex << sym->start);

	for (auto f : sym->codeReferences) {
	    LOG(1, "CDSS:  Adding function:  " << f->getName());
	    out.insert(f);
	}

	for (auto ds : sym->dataReferences) {
	    LOG(1, "CDSS:  Considering new symbol:  "
		//<< ds->name
		<< "@0x" << std::hex << ds->start);

	    if (symbolsConsidered.count(ds) == 0) {
		toConsider.push_back(ds);
	    }
	}
    }

    funcs.insert(out.begin(), out.end());
    return found;
}
