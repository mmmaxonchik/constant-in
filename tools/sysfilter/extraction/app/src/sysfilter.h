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

#ifndef EGALITO_APP_SYSFILTER_H
#define EGALITO_APP_SYSFILTER_H

#include <map>
#include <set>
#include <string>
#include <vector>

// external libraries
#include "cxxopts.h"
#include "json.hpp"

#include "conductor/setup.h"

#include "arg_tracker.h"
#include "callgraph.h"
#include "data_symbols.h"
#include "dl_autoloader.h"
#include "dl_resolver.h"

class Module;
class Function;

class Sysfilter {
public:
    enum FCGType {
        DirectFCG = 0,
        UniversalFCG = 1,
        NaiveFCG = 2,
        VacuumFCG = 3,
        ATExtraFCG = 4,
    };
    const char *fcgStrings[5] = { "direct", "universal", "naive", "vacuum", "atextra" };
    const char *getFcgString(FCGType t) { return fcgStrings[t]; }

    enum CallgraphType {
        CallgraphInit = 0,
        CallgraphIfuncs = 1,
        CallgraphMain = 2,
        CallgraphFini = 3,
        CallgraphAll = 4,
        CallgraphNss = 5,
    };

    const char *cgStrings[6]
        = { "init", "ifunc", "main", "fini", "all", "nss" };
    const char *callgraphTypeString(CallgraphType t) { return cgStrings[t]; }
    const CallgraphType allCallgraphTypes[6] = {
        CallgraphInit,
        CallgraphIfuncs,
        CallgraphMain,
        CallgraphFini,
        CallgraphAll,
        CallgraphNss,
    };

private:
    ConductorSetup setup;

    std::string helpstr;

    enum DlType {
        DlOpen,
        DlSym,
        DlNone,
    };

    struct TrackedFuncInfo {
        std::string name;
        DlType dltype;
        TrackableArgType argType;
        int reg;
    };

    std::map<Function *, std::vector<TrackedFuncInfo>> trackedFuncInfo;
    std::map<std::string, std::vector<std::string>> nssDatabaseMap;

    struct DlAutoloadInfo {
        std::set<std::string> funcsToLoad;
        std::set<std::string> symsToLoad;
    };

    // Context information for each analysis pass (FCG, CallgraphType)
    // Contexts are passed between each analysis step, but NOT preserved
    // after the pass
    struct AnalysisContext {
        std::set<Function *> reachableFunctions;
        Callgraph reachableCallgraph;
	std::set<Function *> discoverFuncsFound;

        std::map<int, SyscallInfo> callerInfo;
        SyscallSiteInfo syscallSiteInfo;
        std::set<int> syscalls;
        std::set<std::tuple<Function *, RegTrackingStatus>> syscallExtractionFailures;

        std::vector<ArgTrackInfo> argTrackInfo;
    };

    // Result for each analysis pass (preserved after pass is complete)
    struct AnalysisResult {
        std::set<int> syscalls;
        std::set<Function *> reachableFunctions;

        nlohmann::json json;
    };

    double timeParse; // Time for ELF parsing
    double timeGetFuncs; // Time for gathering starting functions, NSS, etc.

    std::map<FCGType, std::map<CallgraphType, AnalysisResult>> outputs;
    std::map<std::string, std::set<Function *>> resolvedDlSymbols;
    std::set<std::string> dlLibraryLoadFailures;

    std::map<Module *, std::set<std::string>> nssBackendFuncsUsed;
    std::map<std::string, std::set<Function *>> nssBackendFuncsLoaded;
    std::set<std::string> nssLibSearchFailures;

    std::map<std::string, std::set<std::string>> nssLegacyFrontendFuncsUsed;
    std::set<Function *> nssLegacyBackendFuncsLoaded;

    std::map<CallgraphType, std::set<Function *>> startingFuncMap;
    std::set<Function *> &ctxFuncs(CallgraphType t) {
        if (!config.needMultiPass()) { return startingFuncMap[CallgraphAll]; }
        else {
            assert(startingFuncMap.count(t));
            return startingFuncMap[t];
        }
    }

    std::map<CallgraphType, std::set<Function *>> dlExtraTargets;
    std::set<Function *> &dlTargets(CallgraphType t) {
        if (!config.needMultiPass()) { return dlExtraTargets[CallgraphAll]; }
        else {
            assert(dlExtraTargets.count(t));
            return dlExtraTargets[t];
        }
    }

    DataSymbolList dataSymbols;
    bool dataSymbolsGenerated = false;

    DataSymbolList &getDataSymbolList() {
        if (!dataSymbolsGenerated) {
            auto program = setup.getConductor()->getProgram();
            dataSymbols.generate(program);

            dataSymbolsGenerated = true;
        }
        return dataSymbols;
    }

    const std::string entry_base_name = "___sysfilter_lib_base";
    const std::string executable_name = "(executable)";

    std::map<std::string, int> entrySymbolRunStatus;
    std::map<std::string, std::string> entrySymbolRunExceptions;

    struct Config {
        // general config
        std::string inputFile;
        int verbosity;
        std::string outputFile;
        std::ostream *outputStream = nullptr;
        enum {
            FlatOutput,
            SyscallsJsonOutput,
            FullJsonOutput
        } outputType
            = SyscallsJsonOutput;

        bool noCallers = false;

        // analysis config
        bool fcgOnly = false;
        // bool argMode = false;
        // bool doMultiPass = false;

        bool dumpCallgraph = false;
        bool findIndirectSyscalls = false;

        bool dlAutoload = true;
        int dlRecursionLimit = 3;

	bool addAllIfuncs = true;

	bool resolveNss = true;
	bool nssReportOnly = false;
	bool nssReportLegacy = false;
	std::string nssConfigFile = "/etc/nsswitch.conf";

	bool useStartFunc = true;
	bool startFuncOnly = false;

	bool resolvePthreadFuncs = true;
	bool computePthreadFuncs = true;

        std::set<FCGType> fcgPasses;
        FCGType defaultFcg;
        CallgraphType defaultCallgraph = CallgraphAll;

        std::set<FCGType> argTrackPasses;
        std::set<FCGType> multiPassPasses;

        std::string entry_point;
	std::string entry_data;
	std::string entry_symbol;
	std::string entry_module;

	bool multiFileMode = false;
	std::string entrySymbolFile;
	std::vector<std::tuple<std::string, std::string>> entrySymbolNames;
	std::string outputDir;

        bool extraAsDep = false;
        std::vector<std::string> dlFiles;
        std::vector<std::vector<std::string>> dlSymbolNames;
        std::vector<std::vector<Function *>> dlSymbols;

	bool extraAsDiscover = false;
        std::vector<Function *> extraEntryPoints;
	std::map<Function *, std::set<Function *>> discoverEntryPoints;

        // syscall extraction config
        enum { ExtractAll, ExtractReachable } extractSet = ExtractReachable;

        bool needMultiPass() const { return (multiPassPasses.size() > 0); }

        bool needArgTrack() const { return (argTrackPasses.size() > 0); }

    } config;

public:
    ~Sysfilter();

    int parse(int argc, char **argv);
    int run();
    int extractSyscallInfo(AnalysisContext &ctx);
    int doAnalysis();
    int writeOutput();

    ConductorSetup *getSetup() { return &setup; }

    const std::set<int> &getSyscalls(FCGType fcgType, CallgraphType cgType) {
        assert(outputs.count(fcgType));
        assert(outputs[fcgType].count(cgType));

        return outputs[fcgType][cgType].syscalls;
    }

    const std::set<Function *> &getReachableFunctions(
        FCGType fcgType, CallgraphType cgType) {
        assert(outputs.count(fcgType));
        assert(outputs[fcgType].count(cgType));

        return outputs[fcgType][cgType].reachableFunctions;
    }

    const std::set<int> &getSyscalls() {
        return getSyscalls(config.defaultFcg, config.defaultCallgraph);
    }

    const std::set<Function *> &getReachableFunctions() {
        return getReachableFunctions(
            config.defaultFcg, config.defaultCallgraph);
    }

private:
    void buildOptions(cxxopts::Options &opts);
    void printUsage();
    void error();

    void parseDl(const std::string &path);
    void parseEntrySymbolFile(const std::string &path);

    void extractConfig(cxxopts::Options &opts, int argc, char **argv);
    void parseFcgArgs(const std::string &s, std::set<FCGType> &ret);

    void buildTrackedFuncInfo();

    bool parse(const char *filename);
    bool parseExtras();
    bool parseExtrasDefault();
    bool parseExtrasAutoload();
    void parseFixedSyscalls();

    std::set<Function *> externalFunctions();
    void resolveNss();
    void resolveLegacyNss();

    void resolveStartingFunctions();
    bool findFuncsInDataSymbol(
        std::string symName, std::set<Function *> &funcs);
    void extractfcg(FCGType fcgType, CallgraphType ct, AnalysisContext &ctx);
    void extractArg(AnalysisContext &ctx, DlAutoloadInfo &aInfo);
    void doArgTrack(AnalysisContext &ctx, DlAutoloadInfo &aInfo, Function *f);

    void fixInitCallgraph(Callgraph &callgraph);

    void extractdl();

    void writeCallerSummary(
        std::map<int, SyscallInfo> &callerInfo, nlohmann::json &obj);
    void writeContextOutput(FCGType ft, CallgraphType ct, AnalysisContext &ctx,
        DlAutoloader &dla, nlohmann::json &obj);
    void buildCallObject(Function *function, nlohmann::json &obj);

    void writeCallgraph(AnalysisContext &ctx, std::set<Function *> &entryPoints,
        nlohmann::json &obj);
    void writeTrackedFuncInfo(AnalysisContext &ctx, nlohmann::json &obj);
    void writeNssDatabaseInfo(nlohmann::json &obj);
    void writeNssPassInfo(nlohmann::json &obj);

    Module *getMainModule(Program *program) {
        auto mainModule = program->getMain();
        if (!mainModule) {
            mainModule = program->getFirst();
            if (!mainModule) {
                throw std::runtime_error("No main module found!");
            }
        }
        return mainModule;
    }

    Module *getModuleByPath(Program *program, std::string pathName) {
	for (auto module : CIter::children(program)) {
	    if (module->getName() == pathName) {
		return module;
	    }

	    auto lib = module->getLibrary();
	    assert(lib);
	    if (lib->getResolvedPath() == pathName) {
		return module;
	    }
	}

	throw std::runtime_error("No module found matching target path " + pathName);
    }
};

#endif
