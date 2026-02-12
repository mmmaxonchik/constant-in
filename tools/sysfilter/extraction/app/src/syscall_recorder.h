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

#ifndef SYSFILTER_SYSCALL_RECORDER_H
#define SYSFILTER_SYSCALL_RECORDER_H

#include <set>
#include <tuple>

#include "analysis/dataflow.h"
#include "analysis/slicingtree.h"
#include "analysis/usedef.h"
#include "analysis/usedefutil.h"
#include "analysis/walker.h"
#include "chunk/dump.h"
#include "conductor/conductor.h"
#include "log/registry.h"
#include "log/temp.h"
#include "pass/chunkpass.h"

#include "constant_retriever.h"


class SyscallInfo {
public:
    int nr;
    std::set<Function *> rawCallers;
    std::set<Function *> funcCallers;

    SyscallInfo(int nr) : nr(nr) { }
};

class SyscallSiteFuncInfo {
public:
    std::set<long int> rawCalls;
    std::set<long int> funcCalls;
};

class SyscallSiteInfo {
public:
    std::map<Function *, SyscallSiteFuncInfo> info;

    bool hasFunction(Function *func) { return (info.count(func) != 0); }

    std::set<long int> &getRawCalls(Function *func) {
        return info[func].rawCalls;
    }

    std::set<long int> &getFuncCalls(Function *func) {
        return info[func].funcCalls;
    }
};

class SyscallRecorder {
private:
    std::map<Function *, std::set<int>> syscallFuncs;
    std::set<int> syscalls;
    std::set<Function *> functions;
    std::map<int, SyscallInfo> callerInfo;
    std::set<std::tuple<Function *, RegTrackingStatus>> failures;
    SyscallSiteInfo siteInfo;

    bool usePresetFuncs = false;
    bool built = false;
    bool dietlibc_mode = false;

public:
    void run(Program *program);

    bool wasBuilt() const { return built; }

    void functionSet(std::set<Function *> funset) {
        functions = funset;
        usePresetFuncs = true;
    }

    SyscallInfo &getInfo(int nr);

    const std::map<Function *, std::set<int>> &getSyscallFuncs() const {
        return syscallFuncs;
    }
    // only useful when a functionSet has been provided.
    const std::set<int> &getSyscalls() const { return syscalls; }
    const std::set<std::tuple<Function *, RegTrackingStatus>> &getFailures() const { return failures; }

    const std::map<int, SyscallInfo> &getCallerInfo() const {
        return callerInfo;
    }

    const SyscallSiteInfo &getSiteInfo() const { return siteInfo; }

private:
    void buildFunctionSet(Program *program);
    void process(Function *function);
    bool isSyscallFunction(Function *function);
    bool isSyscallcpFunction(Function *function);
    bool isSetxidFunction(Function *function);
    bool isMuslIgnoreFunction(Function *function);
    bool isDirectSyscallFunction(Function *function);
};

#endif
