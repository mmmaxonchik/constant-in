/*
 * Copyright (c) 2017-2020, Brown University Secure Systems Lab.
 * All rights reserved.
 *
 * This software was developed by Di Jin <di_jin@brown.edu> and Kent
 * Williams-King <kent_williams-king@brown.edu> at Brown University,
 * Providence, RI, USA.
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

#ifndef SYSFILTER_DL_AUTOLOADER_H
#define SYSFILTER_DL_AUTOLOADER_H

#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include <stdint.h>

#include "json.hpp"

#include "chunk/function.h"
#include "chunk/module.h"
#include "conductor/setup.h"

#include "callgraph.h"

class DlPass {
public:
    std::set<std::string> argLibs;
    std::set<std::string> argSyms;

    std::set<std::string> newLibs;
    std::set<Function *> newSyms;

    DlPass(std::set<std::string> &libs, std::set<std::string> &syms)
        : argLibs(libs), argSyms(syms) { }
};

class DlAutoloader {
private:
    ConductorSetup &setup;

    std::set<std::string> allSymNames;
    std::set<Function *> allSyms;

    std::set<Library *> loadedLibs;
    std::set<std::string> strModulesLoaded;

    std::vector<DlPass> passes;

    std::set<std::string> libraryLoadFailures;
    std::set<std::string> symbolLoadFailures;

public:
    DlAutoloader(ConductorSetup &setup) : setup(setup) { }

    std::set<Function *> &load(
        std::set<std::string> &libsToLoad, std::set<std::string> &symsToLoad);

    void writeJsonSummary(nlohmann::json &obj);

private:
    DlPass &newPass(
        std::set<std::string> &libsToLoad, std::set<std::string> &symsToLoad) {
        passes.emplace_back(libsToLoad, symsToLoad);
        return passes.back();
    }

    DlPass &getCurrent() { return passes.back(); }

    void writeSet(
        std::set<std::string> &sset, std::string key, nlohmann::json &obj);
};

#endif
