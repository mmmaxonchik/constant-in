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

#ifndef SYSFILTER_DL_RESOLVER_H
#define SYSFILTER_DL_RESOLVER_H

#include <map>
#include <set>
#include <string>
#include <vector>
#include <utility>

#include "chunk/module.h"
#include "elf/elfdynamic.h"
#include "pass/chunkpass.h"
#include "conductor/conductor.h"

#include "log.h"

struct ResolverContext {
public:
    std::set<std::string> backendLibsLoaded;

    std::map<std::string, std::set<Function *>> backendFuncsLoaded;
    std::set<std::string> libSearchFailures;
    std::vector<Function *> extraEntryPoints;
    std::map<Function *, std::set<Function *>> discoverEntryPoints;
    std::map<Module *, std::set<std::string>> backendFuncsUsed;

    Conductor *conductor;
    Program *program;

private:
    ElfDynamic edyn;

public:
    ResolverContext(Conductor *conductor) :
	conductor(conductor), program(conductor->getProgram()),
	edyn(ElfDynamic(program->getLibraryList())) {
    }

    Library *tryLoadLibrary(Library *parent, std::string &soname) {
	if (backendLibsLoaded.count(soname) == 0) {
	    LOG(1, "NSS:  Attempting to parse new library " << soname);

	    auto fullPath = edyn.findSharedObject(soname);
	    if (fullPath.empty()) {
		LOG(1, "NSS:  Unable to locate library "
		    << soname << ", skipping.");
		libSearchFailures.insert(soname);
		return nullptr;
	    }

	    edyn.addDependency(parent, soname);
	    conductor->parseLibraries();
	    backendLibsLoaded.insert(soname);
	}

	auto library = program->getLibraryList()->find(soname);

	return library;
    }
};
#endif
