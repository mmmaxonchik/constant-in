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

#ifndef SYSFILTER_NSS_H
#define SYSFILTER_NSS_H

#include <map>
#include <set>
#include <string>
#include <vector>
#include <utility>

#include "chunk/module.h"
#include "pass/chunkpass.h"
#include "conductor/conductor.h"
#include "dl_resolver.h"

class NSSFuncsPass : public ChunkPass {
private:
    std::string configFile;

    std::set<Module *> modulesSeen;

    std::set<std::string> allDatabases;

    std::map<std::string, std::vector<std::string>> databaseLibs = {
#include "nss-gen/nss-default-databases.def"
    };

    std::map<Module *, std::set<std::string>> frontendFuncUsage;

    // Map<backend glibc func, set<pair<lib name, symbol name>>>
    std::map<std::string, std::set<std::pair<std::string, std::string>>> symsNeededByLibrary;


#define FRONTEND_SYM(frontend_name, backend_syms) {frontend_name, backend_syms}

    std::map<std::string, std::set<std::string>> frontendNameMap = {
#include "nss-gen/nss-frontend.def"
    };

    std::map<std::string, std::string> databaseNameMap = {
#include "nss-gen/nss-backend.def"
    };

    std::set<std::string> undefNames = {
#include "nss-gen/nss-undef.def"
    };

    std::map<std::string, std::set<std::string>> frontendAliasMap = {
#include "nss-gen/nss-frontend-aliases.def"
};

    std::set<std::string> emptySet;

public:
    NSSFuncsPass(std::string &configFile);

    virtual void visit(Module *module);
    size_t process(Conductor *conductor);

    //const std::map<std::string, std::set<std::string>>
    const std::map<std::string, std::set<std::pair<std::string, std::string>>> &getSymsNeeded() const {
        return symsNeededByLibrary;
    }

    const std::map<Module *, std::set<std::string>> &getFrontendFuncUsage() const {
	return frontendFuncUsage;
    }

    const std::set<std::string> getUndefNames() const {
	return undefNames;
    }

    const std::map<std::string, std::vector<std::string>> getDatabaseMapping() const {
	return databaseLibs;
    }

    const std::set<std::string> &getAliasNames(std::string &funcName) {
	if (frontendAliasMap.count(funcName) == 0) {
	    return emptySet;
	} else {
	    return frontendAliasMap[funcName];
	}
    }

    void resolve(ResolverContext &rctx);

private:
    void loadConf();

};

#endif
