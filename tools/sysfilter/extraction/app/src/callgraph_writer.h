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

#ifndef SYSFILTER_CALLGRAPH_WRITER_H
#define SYSFILTER_CALLGRAPH_WRITER_H

#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include <stdint.h>

#include "json.hpp"

#include "chunk/function.h"
#include "chunk/module.h"

#include "callgraph.h"
#include "syscall_recorder.h"

class CallgraphWriter {

private:
    const std::set<Function *> &allFuncs;
    const Callgraph &cg;
    const std::set<Function *> &entryPoints;

    std::map<std::string, Function *> funcsSeen;
    std::map<std::string, int> funcTags;
    std::map<Function *, std::string> funcKeys;
    int funcIndex = 0;
    std::map<std::string, int> dupWarnings;

    bool hasSyscallSiteInfo = false;
    SyscallSiteInfo syscallSiteInfo;

    bool hasIndirectSourceInfo = false;
    std::map<Function *, std::set<int>> indirectSyscalls;

    std::string addName(Function *func);
    std::string getName(Function *func);

    const static std::string hash(Function *func) {
        auto module = func->getParent()->getParent();

        auto libName = module->getName();
        auto funcName = func->getName();
        auto offset = func->getAddress();

        std::ostringstream ss;
        ss << libName.substr(7) << "@" << funcName << "+0x" << std::hex
           << offset;

        return ss.str();
    }

    int next() { return funcIndex++; }

    std::string nameFromIndex(int index) {
        std::ostringstream ss;
        ss << "f_" << index;
        return ss.str();
    }

public:
    CallgraphWriter(std::set<Function *> &allFuncs, Callgraph &cg,
        std::set<Function *> &entryPoints)
        : allFuncs(allFuncs), cg(cg), entryPoints(entryPoints) { }

    const static std::string getFuncName(Function *func) { return hash(func); }
    void loadSyscallSiteInfo(SyscallSiteInfo &siteInfo) {
        syscallSiteInfo = siteInfo;
        hasSyscallSiteInfo = true;
    }

    void loadIndirectSourceSyscallinfo(
        Function *func, std::set<int> &syscalls) {
        hasIndirectSourceInfo = true;
        indirectSyscalls[func].insert(syscalls.begin(), syscalls.end());
    }

    void writeSyscallSiteInfo(Function *func, nlohmann::json &obj);
    nlohmann::json dump(void);
    void dump(nlohmann::json &obj);
};
#endif
