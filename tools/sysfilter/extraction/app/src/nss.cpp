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

#include <set>
#include <fstream>
#include <sstream>
#include <string>
#include <cassert>

#include "conductor/filesystem.h"
#include "nss.h"

#include "log.h"

NSSFuncsPass::NSSFuncsPass(std::string &configFile) : configFile(configFile) {
    for (auto &kv : databaseNameMap) {
	auto dbName = kv.second;
	allDatabases.insert(dbName);
    }

    loadConf();
}

void NSSFuncsPass::visit(Module *module) {
    if (modulesSeen.count(module)) {
	return;
    }

    LOG(10, "NSSFuncPass on " << module->getName());
    modulesSeen.insert(module);

    if (!module->getExternalSymbolList()) return;

    for (auto extsymb : CIter::children(module->getExternalSymbolList())) {
	auto targetName = extsymb->getName();

	if (frontendNameMap.count(targetName)) {
	    LOG(1, "NSS:  Frontend function "
		<< targetName << " is used by " << module->getName());

	    frontendFuncUsage[module].insert(extsymb->getName());
	    auto backendSymsNeeded = frontendNameMap[targetName];

	    for (auto symName : backendSymsNeeded) {
		auto dbName = databaseNameMap[symName];

		assert(databaseLibs.count(dbName) != 0);
		auto libsNeeded = databaseLibs[dbName];

		for (auto libName : libsNeeded) {
		    //auto backendSymName = "_nss_" + libName + "_" + symName;

		    if (symsNeededByLibrary.count(targetName) == 0) {
			LOG(1, "NSS:  Frontend symbol " << symName
			    << " requires " << dbName << "@" << symName);
		    }

		    symsNeededByLibrary[targetName].insert(std::make_pair(libName, symName));
		}
	    }
	}
    }
}


size_t NSSFuncsPass::process(Conductor *conductor) {
    auto modulesCount = modulesSeen.size();

    conductor->acceptInAllModules(this);

    // If we added any modules, we need to run again
    return modulesSeen.size() - modulesCount;
}

void NSSFuncsPass::loadConf() {
    std::ifstream conf(ConductorFilesystem::getInstance()
		       ->transform(configFile).c_str());
    std::string line;
    while (std::getline(conf, line)) {
        if (line.length() == 0 || line[0] == '#') continue;

        std::istringstream ss(line);
        std::string db;
        ss >> db;
        if (db.back() != ':') {
            LOG(1, "Unknown nsswitch.conf file format!");
            continue;
        }

        db.pop_back();
        auto &liblist = databaseLibs[db];

	// If we have a default mapping for this database, clear it in favor
	// of what we extract from the config file
	if (liblist.size() != 0) {
	    liblist.clear();
	}

        std::string lib;
        while (ss >> lib) {
            if (lib[0] == '[') continue;
            liblist.push_back(lib);
        }
    }


    // Special case:  if "initgroups" does not have a mapping,
    // it uses the mapping for the "group" database
    if (databaseLibs.count("initgroups") == 0) {
	auto &initGroupsLibs = databaseLibs["initgroups"];
	auto &groupsLibs = databaseLibs["group"];
	initGroupsLibs.insert(initGroupsLibs.end(),
			      groupsLibs.begin(), groupsLibs.end());
    }

    for (auto dbName : allDatabases) {
	if (databaseLibs.count(dbName) == 0) {
	    LOG(1, "NSS:  Warning:  no default database config for " << dbName);
	}
    }
}

void NSSFuncsPass::resolve(ResolverContext &rctx) {
    auto program = rctx.program;
    auto conductor = rctx.conductor;

    if (program->getLibc() == nullptr) {
        LOG(1, "NSS:  No libc found, skipping NSS detection --- parsing loader?");
        return;
    }
    auto libc = program->getLibc()->getLibrary();
    auto libcModule = libc->getModule();
    assert(libc);
    assert(libcModule);

    auto findLibcFunc = [&](std::string &funcName) {

        // First, try searching any known alias names
	auto aliasNames = getAliasNames(funcName);
	for (auto n : aliasNames) {
	    auto f = CIter::named(libcModule->getFunctionList())->find(n);
	    if (f) {
		return f;
	    }
	}

	// Otherwise, fall back to searching the symbol name and guessing versioned permutations
	LOG(1, "NSS:  No alias names found for frontend function "
	    << funcName << " attempting to search.");

	auto f = CIter::named(libcModule->getFunctionList())->find(funcName);

	if (!f) {
	    f = CIter::named(libcModule->getFunctionList())->find(funcName + "@@GLIBC_PRIVATE");
	}
	if (!f) {
	    f = CIter::named(libcModule->getFunctionList())->find(funcName + "@@GLIBC_2.2.5");
	}

	return f;
    };

    std::set<std::tuple<std::string, std::string, std::string>> namesSearched;

    while(process(conductor)) {
	LOG(1, "NSS:  Starting new round");
	const std::map<std::string, std::set<std::pair<std::string, std::string>>> &symsNeeded =
	    getSymsNeeded();

	for (auto &kv : symsNeeded) {
	    auto backendFuncName = kv.first;
	    auto syms = kv.second;

	    // Create an entry in nssBackendFuncsLoaded if it does not exist
	    // This ensures that we keep track of when we attempted to find functions
	    // for this name
	    auto &funcsLoaded = rctx.backendFuncsLoaded[backendFuncName];

	    auto libcBackendFunc = findLibcFunc(backendFuncName);
	    if(!libcBackendFunc) {
		LOG(1, "Unable to find libc backend func");
		assert(0 && "Unable to find libc backend func");
	    }

	    // if (config.nssReportOnly) {
	    // 	// Add the frontend function to the discover set with no targets so its usage is recorded
	    // 	if (config.extraAsDiscover) {
	    // 	    std::set<Function *> emptySet;
	    // 	    config.discoverEntryPoints[libcBackendFunc].insert(emptySet.begin(), emptySet.end());
	    // 	}

	    // 	LOG(1, "NSS:  Using report only mode, skipping search for backend functions for "
	    // 	    << backendFuncName);
	    // 	continue;
	    // }

	    for (auto &sp : syms) {
		auto libName = sp.first;
		auto backendSymName = sp.second;

		auto fname = "_nss_" + libName + "_" + backendSymName;

		auto key = std::make_tuple(backendFuncName, libName, backendSymName);
		if (namesSearched.count(key)) {
		    continue;
		}
		namesSearched.insert(key);

		std::string soname = "libnss_" + libName + ".so.2";

		auto library = rctx.tryLoadLibrary(libc, soname);

		// the NSS config can validly specify nonexistent libraries.
		if (!library) {
		    rctx.libSearchFailures.insert(soname);
		    continue;
		}

		// Now actually try to find the symbol
		auto module = library->getModule();
		auto func = CIter::named(module->getFunctionList())->find(fname);
		LOG(1, "NSS:  Searching for function "
		    << fname << ", result: " << func);

		if (!func) {
		    LOG(1,
			"NSS:  Looked for function "
			<< fname
			<< ", result: " << func);
		}

		// hopefully we found one of those two, but it may not be present
		// in this NSS library.
		if (!func) {
		    continue;
		}
		assert(func);
		rctx.extraEntryPoints.push_back(func);
		funcsLoaded.insert(func);
		rctx.discoverEntryPoints[libcBackendFunc].insert(func);
	    }
	}
    }

    // Check each backend symbol name to make sure >1 NSS DSO function is present for it
    //if (!config.nssReportOnly) {
    for (auto &kv : rctx.backendFuncsLoaded) {
	auto backendFuncName = kv.first;
	auto funcs = kv.second;

	if (funcs.size() == 0) {
	    LOG(1, "NSS:  Error:  No NSS libraries loaded with functions satisfying "
		<< backendFuncName);
	    assert(0 && "NSS:  Unable to find backend functions to satisfy requirements");
	}
    }

    for (auto &kv : getFrontendFuncUsage()) {
	auto m = kv.first;
	auto symNames = kv.second;
	rctx.backendFuncsUsed[m].insert(symNames.begin(), symNames.end());
    }

    // Now try to load any undef symbols
    for (auto soname : rctx.backendLibsLoaded) {
	auto library = program->getLibraryList()->find(soname);

	// the NSS config can validly specify nonexistent libraries.
	if (!library) {
	    continue;
	}

	auto module = library->getModule();

	// Now try to find each undef symbol
	for (auto fname : getUndefNames()) {
	    auto func = CIter::named(module->getFunctionList())->find(fname);
	    if (!func) {
		LOG(1,
		    "NSS:  Looked for undef function "
		    << fname
		    << " in " << module->getName()
		    << ", result: " << func);
	    }

	    if (!func) {
		continue;
	    }

	    rctx.extraEntryPoints.push_back(func);
	    rctx.backendFuncsLoaded["undef"].insert(func);
	}
    }
}
