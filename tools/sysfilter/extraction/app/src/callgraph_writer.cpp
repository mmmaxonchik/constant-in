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

#include "callgraph_writer.h"

#include "log.h"

std::string CallgraphWriter::addName(Function *func) {
    auto key = hash(func);

    if (funcKeys.count(func) != 0) {
        if (dupWarnings.count(key) == 0) { dupWarnings[key] = 1; }
        else {
            dupWarnings[key]++;
        }

        LOG(1, "WARNING:  Duplicate key found for " << key);
    }

    funcsSeen[key] = func;
    funcTags[key] = next();
    funcKeys[func] = key;

    return key;
}

std::string CallgraphWriter::getName(Function *func) {
    auto key = hash(func);

    if (funcKeys.count(func) == 0) { return addName(func); }

    return funcKeys[func];
}

void CallgraphWriter::dump(nlohmann::json &obj) {
    // First, add all of the keys to look for duplicates
    for (auto f : allFuncs) {
        addName(f);
    }

    // Add direct edges
    nlohmann::json directMap = nlohmann::json::object();

    for (auto &kv : cg.wrap) {
        auto func = kv.first;
        auto edges = kv.second;

        nlohmann::json edgeNames = nlohmann::json::array();

        for (auto f : edges) {
            auto fName = getName(f);
            edgeNames.push_back(fName);
        }

        auto srcName = getName(func);
        directMap[srcName] = edgeNames;
    }

    obj["direct_edges"] = directMap;

    // Add indirect sources
    nlohmann::json indirectSources = nlohmann::json::array();
    for (auto f : cg.implicitSources) {
        auto key = getName(f);
        indirectSources.push_back(key);
    }

    obj["indirect_sources"] = indirectSources;

    // Add indirect targets
    nlohmann::json indirectTargets = nlohmann::json::array();
    for (auto f : cg.implicitTargets) {
        auto key = getName(f);
        indirectTargets.push_back(key);
    }
    obj["indirect_targets"] = indirectTargets;

    // Write metadata
    nlohmann::json funcs = nlohmann::json::object();
    for (auto &kv : funcsSeen) {
        auto tag = kv.first;
        auto func = kv.second;

        nlohmann::json fobj = nlohmann::json::object();

        auto module = func->getParent()->getParent();

        fobj["lib"] = module->getName().substr(7);
        fobj["name"] = func->getName();

        fobj["offset"] = func->getAddress();

        fobj["implicit_source"] = cg.isImplicitSource(func);
        fobj["implicit_target"] = cg.isImplicitTarget(func);

        fobj["entry_point"] = (entryPoints.count(func) != 0);

        if (hasSyscallSiteInfo) {
            nlohmann::json sysObj = nlohmann::json::object();
            writeSyscallSiteInfo(func, sysObj);
            fobj["syscalls"] = sysObj;
        }

        if (hasIndirectSourceInfo && (cg.isImplicitTarget(func))) {
            nlohmann::json indSyscalls = nlohmann::json::array();

            for (auto nr : indirectSyscalls[func]) {
                indSyscalls.push_back(nr);
            }
            fobj["syscalls_from_here_direct"] = indSyscalls;
        }

        funcs[tag] = fobj;
    }

    obj["funcs"] = funcs;

    nlohmann::json dups = nlohmann::json::object();
    for (auto &kv : dupWarnings) {
        auto key = kv.first;
        auto count = kv.second;

        dups[key] = count;
    }
    obj["duplicate_warnings"] = dups;
}

nlohmann::json CallgraphWriter::dump(void) {
    nlohmann::json obj = nlohmann::json::object();

    dump(obj);

    return obj;
}

void CallgraphWriter::writeSyscallSiteInfo(
    Function *func, nlohmann::json &obj) {
    if (syscallSiteInfo.hasFunction(func)) {
        auto rawCalls = syscallSiteInfo.getRawCalls(func);

        nlohmann::json rawObj = nlohmann::json::array();
        for (auto nr : rawCalls) {
            rawObj.push_back(nr);
        }
        obj["raw"] = rawObj;

        auto funcCalls = syscallSiteInfo.getFuncCalls(func);

        nlohmann::json funcObj = nlohmann::json::array();
        for (auto nr : funcCalls) {
            funcObj.push_back(nr);
        }
        obj["func"] = funcObj;
    }
}
