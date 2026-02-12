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

#include "dl_autoloader.h"
#include "callgraph_writer.h"

#include "conductor/conductor.h"
#include "elf/elfdynamic.h"

#include "log.h"

std::set<Function *> &DlAutoloader::load(
    std::set<std::string> &libsToLoad, std::set<std::string> &symsToLoad) {
    DlPass &pass = newPass(libsToLoad, symsToLoad);

    auto liblist = setup.getConductor()->getProgram()->getLibraryList();

    // Find out which libs we need to load
    for (auto libName : libsToLoad) {
        if (strModulesLoaded.count(libName)) {
            // We already know we loaded this library
            continue;
        }
        else if (libraryLoadFailures.count(libName)) {
            // We already know this library failed to load
            continue;
        }

        for (auto lib : CIter::children(liblist)) {
            if (lib->getName() == libName) { // Already loaded
                loadedLibs.insert(lib);
                strModulesLoaded.insert(lib->getName());
            }
            else { // Need to load this lib
                pass.newLibs.insert(libName);
            }
        }
    }

    // Load those libs
    ElfDynamic edyn(setup.getConductor()->getProgram()->getLibraryList());

    for (auto libName : pass.newLibs) {
        std::vector<std::string> libToLoad;

        auto libPath = edyn.findSharedObject(libName);
        LOG(1, "Resolved:  " << libName << " -> " << libPath);

        libToLoad.push_back(libPath);

        // There are two ways a library load could fail:
        // 1. Egalito may return a nullptr in the module list
        // 2. Egalito may throw an exception if the ELF file
        // cannot be read
        try {
            auto modules = setup.addExtraLibraries(libToLoad);
            for (unsigned int i = 0; i < modules.size(); i++) {
                auto module = modules[i];
                if (!module) {
                    LOG(1,
                        "Failed to autoload library "
                            << libName
                            << "egalito failed to parse and returned NULL");
                    libraryLoadFailures.insert(libName);
                }
            }
        }
        catch (const char *message) {
            LOG(1,
                "Failed to autoload library "
                    << libName << ", egalito returned exception:  "
                    << ":  " << message);
            libraryLoadFailures.insert(libName);
        }
        catch (...) {
            LOG(1, "Failed to autoload library " << libName);
            libraryLoadFailures.insert(libName);
        }
    }

    // Search all libs for symbols matching the listed names
    for (auto symName : symsToLoad) {
        bool found = false;

        for (auto lib : CIter::children(liblist)) {
            auto funcList = lib->getModule()->getFunctionList();
            auto func = CIter::named(funcList)->find(symName);

            if (!func) { continue; }

            // If we found this function in a previous pass, skip searching for
            // it
            if (allSyms.count(func)) { found = true; }
            else {
                pass.newSyms.insert(func);
                LOG(10,
                    "Adding autoloaded symbol:  "
                        << CallgraphWriter::getFuncName(func));
                found = true;
            }
        }

        if (!found) { symbolLoadFailures.insert(symName); }
    }

    allSymNames.insert(symsToLoad.begin(), symsToLoad.end());
    allSyms.insert(pass.newSyms.begin(), pass.newSyms.end());

    return pass.newSyms;
}

void DlAutoloader::writeJsonSummary(nlohmann::json &obj) {
    writeSet(libraryLoadFailures, "lib_failures", obj);
    writeSet(symbolLoadFailures, "sym_failures", obj);

    nlohmann::json passInfo = nlohmann::json::array();
    int idx = 0;
    for (auto pass : passes) {
        if ((pass.newSyms.empty()) && (pass.newLibs.empty())) { continue; }

        nlohmann::json passObj = nlohmann::json::object();

        passObj["step"] = idx;
        writeSet(pass.newLibs, "libs", passObj);

        nlohmann::json symInfo = nlohmann::json::array();
        for (auto func : pass.newSyms) {
            auto fs = CallgraphWriter::getFuncName(func);
            symInfo.push_back(fs);
        }
        passObj["syms"] = symInfo;

        idx++;
        passInfo.push_back(passObj);
    }
    obj["dl_passes"] = passInfo;
}

void DlAutoloader::writeSet(
    std::set<std::string> &sset, std::string key, nlohmann::json &obj) {
    nlohmann::json arr = nlohmann::json::array();
    for (auto s : sset) {
        arr.push_back(s);
    }

    obj[key] = arr;
}
