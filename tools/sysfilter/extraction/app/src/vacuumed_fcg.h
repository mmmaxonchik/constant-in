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

#ifndef SYSFILTER_VACUUMED_FCG_H
#define SYSFILTER_VACUUMED_FCG_H

#include <set>
#include <vector>

#include "callgraph.h"

#include "conductor/conductor.h"
#include "pass/chunkpass.h"

#include "data_symbols.h"

class VacuumedCallgraph {
private:
    Program *program;
    Callgraph result;

    DataSymbolList &dataSymbols;

    std::set<std::string> ignoreUnresolvedPLT;
    std::set<std::pair<std::string, std::string>> ignoreUnresolvedPLTInContext;

    std::vector<Function *> functionRoots;
    std::vector<DataSymbol *> dataRoots;
    std::set<Function *> visitedFunctions;
    std::set<DataSymbol *> visitedDataSymbols;

    std::set<Function *> indirectTargets;
    std::set<Function *> indirectSources;

    std::set<Function *> excludeFunctions;
    std::map<Function *, std::set<Function *>> extraFuncs;
    std::set<Function *> extraFuncsFound;

public:
    VacuumedCallgraph(Program *program, DataSymbolList &dataSymbols);

    void addRoot(Function *function) { functionRoots.push_back(function); }
    void addImplicitRoot(Function *function) {
        functionRoots.push_back(function);
        indirectTargets.insert(function);
    }

    Callgraph generate();

    void exclude(Function *function) { excludeFunctions.insert(function); }
    void addExtraFuncs(Function *src, std::set<Function *> &targets) {
	extraFuncs[src].insert(targets.begin(), targets.end());
    }

    const std::set<Function *> &getExtraFuncsFound() const {
	return extraFuncsFound;
    }

private:
    void generateDataSymbols();
    void walk(Function *function);
    void walk(DataSymbol *ds);
    void consider(Function *function);
    void consider(DataSymbol *ds);
    bool shouldIgnoreUnresolvedPLT(
        const std::string &fname, const std::string &pltName);
};

#endif
