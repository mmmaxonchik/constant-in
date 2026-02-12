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

#include <chrono>
#include <cstdlib> // for std::exit
#include <cstring> // for std::strcmp, strdup
#include <stdexcept>
#include <errno.h>
#include <functional>
#include <iostream>
#include <sstream>
#include <sys/resource.h>
#include <wordexp.h>

#include "callgraph.h"
//#include "strarg_recorder.h"
#include "arg_tracker.h"
#include "callgraph_writer.h"
#include "data_symbols.h"
#include "indirect_fcg.h"
#include "nss.h"
#include "nss_legacy.h"
#include "reachable.h"
#include "static_fcg.h"
#include "syscall_recorder.h"
#include "sysfilter.h"
#include "vacuumed_fcg.h"

#include "chunk/dump.h"
#include "conductor/conductor.h"
#include "elf/elfdynamic.h"
#include "generate/bingen.h"
#include "log/registry.h"
#include "log/temp.h"
#include "pass/chunkpass.h"
#include "pass/collapseplt.h"
#include "pass/resolveplt.h"
#include "operation/find2.h"
#include "transform/sandbox.h"

// must be kept as last include
#include "log.h"

#define timeval_to_float(x)                                                    \
    (((float)(x)->tv_sec) + (((float)(x)->tv_usec) / 1000000))

Sysfilter::~Sysfilter() {
    // if output file is open, close it
    if (config.outputFile != "" && config.outputFile != "-"
        && config.outputStream != nullptr) {
        delete config.outputStream;
    }
}

int Sysfilter::parse(int argc, char **argv) {
    cxxopts::Options opts("sysfilter_extract",
        "Tool to statically extract system call API usage from binaries.");

    try {
        // Extract config from arguments
        buildOptions(opts);
        extractConfig(opts, argc, argv);

        auto timeParseStart = std::chrono::high_resolution_clock::now();
        // now do the actual parsing
        if (!parse(config.inputFile.c_str())) {
            std::cout << "Failed to parse input file \"" << config.inputFile
                      << "\"" << std::endl;
            throw std::runtime_error("Failed to parse input.");
        }

        if (config.dlFiles.size()) {
            if (!parseExtras())
                throw std::runtime_error(
                    "Failed to parse at least one extra library.");
        }

        auto timeParseEnd = std::chrono::high_resolution_clock::now();
        timeParse = std::chrono::duration<double>(timeParseEnd - timeParseStart)
                        .count();
    }
    catch (std::exception &e) {
        std::cout << e.what() << std::endl;
        return 1;
    }

    return 0;
}

int Sysfilter::run() {
    try {
        if (!config.multiFileMode) {
            // open output file
            if (config.outputFile == "" || config.outputFile == "-") {
                config.outputStream = &std::cout;
            }
            else {
                config.outputStream
                    = new std::ofstream(config.outputFile.c_str());
                if (!(*config.outputStream)) {
                    throw std::runtime_error("Couldn't open output file");
                }
            }
            doAnalysis();
            writeOutput();
        }
        else {
            auto makeOutputFile = [&](std::string symbolName) {
                std::stringstream outputFile;
                outputFile << ((config.outputDir.size() != 0) ? config.outputDir
                                                              : ".");
                outputFile << "/out_";
                outputFile << symbolName;
                outputFile << ".json";

                return outputFile.str();
            };

	    // Run for each symbol in the entry symbol file
	    // Note:  base is the first "symbol"
	    for (auto &kv : config.entrySymbolNames) {
		std::string entryModule;
		std::string entrySymbol;
		std::tie(entryModule, entrySymbol) = kv;

		LOG(1, "MULTI:  Running with entry symbol:  " << entrySymbol);

                config.outputStream
                    = new std::ofstream(makeOutputFile(entrySymbol));
                if (!(*config.outputStream)) {
                    throw std::runtime_error("Couldn't open output file");
                }

		if (entrySymbol == entry_base_name) {
		    config.entry_symbol = "";
		    config.useStartFunc = false;
		    config.startFuncOnly = false;
		} else {
		    config.entry_module = entryModule;
		    config.entry_symbol = entrySymbol;
		    config.useStartFunc = true;
		    config.startFuncOnly = true;
		}

                try {
                    doAnalysis();
                    writeOutput();
                    entrySymbolRunStatus[entrySymbol] = 0;
                }
                catch (std::exception &e) {
                    entrySymbolRunStatus[entrySymbol] = 1;
                    entrySymbolRunExceptions[entrySymbol] = e.what();
                    std::cout
                        << "Encountered exception for symbol:  " << entrySymbol
                        << ":  " << e.what() << std::endl;
                }

                // Clear non-pass context
                startingFuncMap.clear();
                dlExtraTargets.clear();

                delete config.outputStream;
                config.outputStream = nullptr;
            }
        }
    }
    catch (std::exception &e) {
        std::cout << e.what() << std::endl;
        return 1;
    }

    return 0;
}

#if 0
void Sysfilter::writeSymbolStatusFile() {
    std::stringstream statusFile;
    outputFile << ((config.outputDir.size() != 0) ? config.outputDir : ".");
    outputFile << "/status.json";

    std::ofstream outStream(statusFile.str());
    if (!outStream) {
	throw std::runtime_error("Unable to open output status file!");
    }
}
#endif

int Sysfilter::doAnalysis() {
    auto time_funcs_start = std::chrono::high_resolution_clock::now();

    buildTrackedFuncInfo();

    // if (config.doMultiPass) {
    if (config.needMultiPass()) {
        startingFuncMap[CallgraphType::CallgraphInit].count(nullptr);
        startingFuncMap[CallgraphType::CallgraphMain].count(nullptr);
        startingFuncMap[CallgraphType::CallgraphFini].count(nullptr);
        startingFuncMap[CallgraphType::CallgraphIfuncs].count(nullptr);
        startingFuncMap[CallgraphType::CallgraphAll].count(nullptr);
        startingFuncMap[CallgraphType::CallgraphNss].count(nullptr);

        dlExtraTargets[CallgraphType::CallgraphInit].count(nullptr);
        dlExtraTargets[CallgraphType::CallgraphMain].count(nullptr);
        dlExtraTargets[CallgraphType::CallgraphFini].count(nullptr);
        dlExtraTargets[CallgraphType::CallgraphIfuncs].count(nullptr);
        dlExtraTargets[CallgraphType::CallgraphAll].count(nullptr);
        dlExtraTargets[CallgraphType::CallgraphNss].count(nullptr);
    }
    else {
        startingFuncMap[CallgraphType::CallgraphAll].count(nullptr);
        dlExtraTargets[CallgraphType::CallgraphAll].count(nullptr);
    }

    if (config.resolveNss) {
	LOG(1, "Resolving NSS...");
	resolveNss();

	if (config.nssReportLegacy) {
	    LOG(1, "Resolving legacy NSS usage...");
	    resolveLegacyNss();
	}
    } else {
	// Do post-analysis cleanup that resolveNss would have otherwise handled
	auto program = setup.getConductor()->getProgram();

        // XXX: may not be needed?
        setup.ensureBaseAddresses();

        ResolvePLTPass resolvePLT(setup.getConductor());
        program->accept(&resolvePLT);

        CollapsePLTPass collapsePLT(setup.getConductor());
        program->accept(&collapsePLT);
    }

    LOG(1, "Collecting starting functions...");
    resolveStartingFunctions();

    auto time_funcs_end = std::chrono::high_resolution_clock::now();
    timeGetFuncs
        = std::chrono::duration<double>(time_funcs_end - time_funcs_start)
              .count();

    for (FCGType ft : config.fcgPasses) {
        std::set<CallgraphType> cgTypes;

        if (config.multiPassPasses.count(ft)) {
            for (auto _ct : allCallgraphTypes) {
                cgTypes.insert(_ct);
            }
        }
        else {
            cgTypes.insert(config.defaultCallgraph);
        }

        for (CallgraphType ct : cgTypes) {
            AnalysisContext ctx;
            nlohmann::json ctxJson = nlohmann::json::object();

            int passCount = 0;
            DlAutoloader dla(setup);

            bool done = false;

            double timeFcg = 0.0;

#define LOG_PASS(lvl, x)                                                       \
    LOG(lvl,                                                                   \
        "PASS <" << std::string(getFcgString(ft)) << ", "                      \
                 << callgraphTypeString(ct) << ", " << passCount               \
                 << ">:  " << x)

            while (!done && (passCount < config.dlRecursionLimit)) {
                DlAutoloadInfo aInfo;

                LOG_PASS(1, "Extracting FCG");
                auto fcg_start = std::chrono::high_resolution_clock::now();
                extractfcg(ft, ct, ctx);
                auto fcg_end = std::chrono::high_resolution_clock::now();

                timeFcg += std::chrono::duration<double>(fcg_end - fcg_start)
                               .count();

                if (config.argTrackPasses.count(ft)) {
                    auto argtrack_start
                        = std::chrono::high_resolution_clock::now();
                    extractArg(ctx, aInfo);
                    auto argtrack_end
                        = std::chrono::high_resolution_clock::now();
                    ctxJson["time_argtrack"] = std::chrono::duration<double>(
                        argtrack_end - argtrack_start)
                                                   .count();
                }

                if (config.dlAutoload) {
                    auto newSyms
                        = dla.load(aInfo.funcsToLoad, aInfo.symsToLoad);
                    if (newSyms.empty()) { done = true; }
                    else {
                        LOG_PASS(1,
                            "Repeating analysis with " << newSyms.size()
                                                       << " new symbols");
                        dlExtraTargets[ct].insert(
                            newSyms.begin(), newSyms.end());
                    }
                }
                else {
                    done = true;
                }

                passCount++;

                if (config.dlAutoload && !done
                    && (passCount >= config.dlRecursionLimit)) {
                    LOG_PASS(1,
                        "Recursion limit reached for dynamic symbol resolution "
                        "(max "
                            << config.dlRecursionLimit << ") aborting");
                    done = true;
                }
            }

            if (config.fcgOnly) {
                LOG(-1,
                    "Running in FCG-only mode, skipping syscall extraction");
            }
            else {
                LOG_PASS(1, "Extracting syscalls");

                auto ext_start = std::chrono::high_resolution_clock::now();
                extractSyscallInfo(ctx);
                auto ext_end = std::chrono::high_resolution_clock::now();
                ctxJson["time_extract"]
                    = std::chrono::duration<double>(ext_end - ext_start)
                          .count();

                // Convert the results to JSON immediately
                LOG_PASS(1, "Writing output");

                writeContextOutput(ft, ct, ctx, dla, ctxJson);

                ctxJson["time_fcg"] = timeFcg;
            }

            if (config.dumpCallgraph) {
                nlohmann::json cgJson = nlohmann::json::object();

                writeCallgraph(ctx, startingFuncMap[ct], cgJson);
                ctxJson["callgraph"] = cgJson;
            }

            outputs[ft][ct] = { .syscalls = ctx.syscalls,
                .reachableFunctions = ctx.reachableFunctions,
                .json = ctxJson };
#undef LOG_PASS
        }
    }

    return 0;
}

int Sysfilter::extractSyscallInfo(AnalysisContext &ctx) {
    // Record syscalls.
    SyscallRecorder syscallRecorder;

    if (config.extractSet == Config::ExtractAll) {
        syscallRecorder.run(setup.getConductor()->getProgram());
        auto &syscallFuncs = syscallRecorder.getSyscallFuncs();

        for (auto it : syscallFuncs) {
            if (ctx.reachableFunctions.find(it.first)
                != ctx.reachableFunctions.end()) {
                ctx.syscalls.insert(it.second.begin(), it.second.end());
            }
        }
    }
    else /* if (config.extractSet == Config::ExtractReachable) */ {
        syscallRecorder.functionSet(ctx.reachableFunctions);
        syscallRecorder.run(setup.getConductor()->getProgram());

        ctx.syscalls = syscallRecorder.getSyscalls();
        ctx.callerInfo = syscallRecorder.getCallerInfo();
        ctx.syscallExtractionFailures = syscallRecorder.getFailures();
        ctx.syscallSiteInfo = syscallRecorder.getSiteInfo();
    }

    return 0;
}

int Sysfilter::writeOutput() {
    if (config.outputType == Config::FlatOutput) {
        bool first = true;
        (*config.outputStream) << std::dec;
        for (auto n : getSyscalls()) {
            if (first) first = false;
            (*config.outputStream) << std::dec << n << " ";
        }
        (*config.outputStream) << std::endl;
    }
    else if (config.outputType == Config::SyscallsJsonOutput) {
        auto syscallList = nlohmann::json::array();

        auto syscalls = getSyscalls();
        LOG(1, "Total syscalls found:  " << std::dec << syscalls.size());
        for (auto n : syscalls) {
            syscallList.push_back(n);
        }

        (*config.outputStream) << syscallList << std::endl;
    }
    else {
        nlohmann::json oj;

        auto fcgModes = nlohmann::json::array();

        for (auto &okv : outputs) {
            FCGType fcgType = okv.first;
            std::map<CallgraphType, AnalysisResult> fcgResultsMap = okv.second;

            auto fcgOutput = nlohmann::json::object();

            nlohmann::json fcgResultList = nlohmann::json::array();
            nlohmann::json fcgResults = nlohmann::json::object();

            for (auto &ctkv : fcgResultsMap) {
                CallgraphType type = ctkv.first;
                nlohmann::json &out = ctkv.second.json;

                std::string key = std::string(callgraphTypeString(type));
                fcgResults[key] = out;
                fcgResultList.push_back(key);
            }

            fcgOutput["analysis"] = fcgResults;
            fcgOutput["analysis_passes"] = fcgResultList;

            std::string modeStr(getFcgString(fcgType));
            fcgModes.push_back(modeStr);
            oj[modeStr] = fcgOutput;
        }

        oj["analysis_modes"] = fcgModes;

        // Performance stats
        nlohmann::json perf = nlohmann::json::object();
        struct rusage rusage;
        int ret;
        if ((ret = getrusage(RUSAGE_SELF, &rusage)) != 0) {
            (*config.outputStream)
                << "Error getting rusage:  " << strerror(errno) << std::endl;
        }
        perf["rss_max"] = rusage.ru_maxrss;
        perf["time_user"] = timeval_to_float(&rusage.ru_utime);
        perf["time_sys"] = timeval_to_float(&rusage.ru_stime);
        perf["time_parse"] = timeParse;
        perf["time_funcs"] = timeGetFuncs;

        oj["perf"] = perf;

        // Write the names of all libraries in the analysis scope
        nlohmann::json analysisScope = nlohmann::json::object();
        auto liblist = setup.getConductor()->getProgram()->getLibraryList();
        for (auto lib : CIter::children(liblist)) {
            nlohmann::json libInfo = nlohmann::json::object();

            libInfo["path"] = lib->getResolvedPath();

            auto module = lib->getModule();
            auto symbolList = module->getElfSpace()->getSymbolList();
            bool hasSymbols = !!symbolList;

            libInfo["has_symbols"] = hasSymbols;
            analysisScope[lib->getName()] = libInfo;
        }

        oj["analysis_scope"] = analysisScope;

        // Include array of all syscalls at the end for backward compatibility
        nlohmann::json jsc = nlohmann::json::array();

        auto syscallList = getSyscalls();
        LOG(1, "Total syscalls found:  " << std::dec << syscallList.size());
        for (auto n : syscallList) {
            jsc.push_back(n);
        }
        oj["syscalls"] = jsc;

        oj["version"] = std::string(SYSFILTER_VERSION_STRING);

        nlohmann::json dlInfo = nlohmann::json::object();

        nlohmann::json dlLibFailures = nlohmann::json::array();
        for (auto lib : dlLibraryLoadFailures) {
            dlLibFailures.push_back(lib);
        }
        dlInfo["dl_library_failures"] = dlLibFailures;

        nlohmann::json dlSymMap = nlohmann::json::object();
        for (auto &kv : resolvedDlSymbols) {
            auto moduleName = kv.first;
            auto funcs = kv.second;

            nlohmann::json syms = nlohmann::json::array();
            for (auto f : funcs) {
                syms.push_back(f->getName());
            }
            dlSymMap[moduleName] = syms;
        }
        dlInfo["dl_symbols_loaded"] = dlSymMap;

        oj["dl_info"] = dlInfo;

	if (config.resolveNss) {
	    auto nssInfo = nlohmann::json::object();
	    writeNssDatabaseInfo(nssInfo);
	    oj["nss_db_info"] = nssInfo;
	}

        (*config.outputStream) << oj << std::endl;
    }

    return 0;
}

void Sysfilter::writeCallgraph(AnalysisContext &ctx,
    std::set<Function *> &entryPoints, nlohmann::json &obj) {
    CallgraphWriter cgw(
        ctx.reachableFunctions, ctx.reachableCallgraph, entryPoints);
    cgw.loadSyscallSiteInfo(ctx.syscallSiteInfo);

    if (config.findIndirectSyscalls) {
        std::map<Function *, AnalysisContext> indirectSourceInfo;

        for (auto indFunc : ctx.reachableCallgraph.getImplicitTargets()) {

            // Build the direct FCG with this function as an entry point
            StaticFCGPass icg;
            setup.getConductor()->acceptInAllModules(&icg, false);

            AnalysisContext &indCtx = indirectSourceInfo[indFunc];

            indCtx.reachableCallgraph = icg.getCallgraph();
            reachableSet(
                indFunc, indCtx.reachableCallgraph, indCtx.reachableFunctions);
            extractSyscallInfo(indCtx);

            cgw.loadIndirectSourceSyscallinfo(indFunc, indCtx.syscalls);
        }
    }

    cgw.dump(obj);
}

void Sysfilter::writeContextOutput(FCGType ft, CallgraphType ct,
    AnalysisContext &ctx, DlAutoloader &dla, nlohmann::json &obj) {
    nlohmann::json jsc = nlohmann::json::array();
    for (auto n : ctx.syscalls) {
        jsc.push_back(n);
    }
    obj["syscalls"] = jsc;

    if (!config.noCallers) {
        nlohmann::json callers = nlohmann::json::object();
        writeCallerSummary(ctx.callerInfo, callers);
        obj["callers"] = callers;
    }

    if (config.dlAutoload) {
        nlohmann::json autoloadInfo = nlohmann::json::object();
        dla.writeJsonSummary(autoloadInfo);
        obj["dl_autoload_info"] = autoloadInfo;
    }

    nlohmann::json failures = nlohmann::json::array();
    for (auto kv : ctx.syscallExtractionFailures) {
	Function *function;
	RegTrackingStatus status;
	std::tie(function, status) = kv;

        nlohmann::json failObj = nlohmann::json::object();
        buildCallObject(function, failObj);
	failObj["status"] = status;

        failures.push_back(failObj);
    }
    obj["failures"] = failures;

    if (config.argTrackPasses.count(ft)) {
        auto funcvals = nlohmann::json::array();
        for (auto ati : ctx.argTrackInfo) {
            ati.writeArgTrackInfo(funcvals);
        }
        obj["arg_track_values"] = funcvals;
    }

    auto trackedFuncInfo = nlohmann::json::array();
    writeTrackedFuncInfo(ctx, trackedFuncInfo);
    obj["tracked_func_info"] = trackedFuncInfo;

    auto cgInfo = nlohmann::json::object();
    auto &cg = ctx.reachableCallgraph;
    cgInfo["num_implicit_sources"] = cg.getImplicitSources().size();
    cgInfo["num_implicit_targets"] = cg.getImplicitTargets().size();
    cgInfo["num_direct_edges"] = cg.getDirectEdges().size();
    cgInfo["num_functions"] = ctx.reachableFunctions.size();
    obj["callgraph_info"] = cgInfo;

    if (config.resolveNss) {
	auto nssInfo = nlohmann::json::object();
	writeNssPassInfo(nssInfo);
	obj["nss_info"] = nssInfo;
    }

    if (config.extraAsDiscover) {
	auto extraDiscoverInfo = nlohmann::json::array();
	for (auto func : ctx.discoverFuncsFound) {
	    auto mod = func->getParent()->getParent();

	    auto funcObj = nlohmann::json::object();
	    funcObj["module"] = mod->getName();
	    funcObj["function"] = func->getName();

	    extraDiscoverInfo.push_back(funcObj);
	}
	obj["extra_funcs_discovered"] = extraDiscoverInfo;
    }
}

void Sysfilter::writeNssPassInfo(nlohmann::json &obj)
{
    auto backendFuncsUsed = nlohmann::json::object();
    for (auto &kv : nssBackendFuncsUsed) {
        auto module = kv.first;
        auto backendNames = kv.second;

        auto names = nlohmann::json::array();
        for (auto n : backendNames) {
            names.push_back(n);
        }
        backendFuncsUsed[module->getName()] = names;
    }
    obj["frontend_funcs_used"] = backendFuncsUsed;

    auto backendFuncsLoaded = nlohmann::json::array();
    for (auto &kv : nssBackendFuncsLoaded) {
	auto backendName = kv.first;
	auto funcs = kv.second;

	for (auto func : funcs) {
	    auto mod = func->getParent()->getParent();

	    auto funcObj = nlohmann::json::object();
	    funcObj["module"] = mod->getName();
	    funcObj["function"] = func->getName();

	    backendFuncsLoaded.push_back(funcObj);
	}
    }
    obj["backend_funcs_loaded"] = backendFuncsLoaded;

    if (config.nssReportLegacy) {
	auto frontendFuncsUsed = nlohmann::json::object();
	for (auto &kv : nssLegacyFrontendFuncsUsed) {
	    auto moduleName = kv.first;
	    auto backendNames = kv.second;

	    auto names = nlohmann::json::array();
	    for (auto n : backendNames) {
		names.push_back(n);
	    }
	    frontendFuncsUsed[moduleName] = names;
	}
	obj["legacy_frontend_funcs_used"] = frontendFuncsUsed;

	auto backendFuncsLoaded = nlohmann::json::array();
	for (Function *func : nssLegacyBackendFuncsLoaded) {
	    auto mod = func->getParent()->getParent();

	    auto funcObj = nlohmann::json::object();
	    funcObj["module"] = mod->getName();
	    funcObj["function"] = func->getName();

	    backendFuncsLoaded.push_back(funcObj);
	}
	obj["legacy_backend_funcs_loaded"] = backendFuncsLoaded;

    }
}

void Sysfilter::writeNssDatabaseInfo(nlohmann::json &obj)
{
    obj["config_file"] = config.nssConfigFile;

    auto dbInfo = nlohmann::json::object();

    for (auto &kv : nssDatabaseMap) {
	auto databaseName = kv.first;
	auto libList = kv.second;

	auto dbList = nlohmann::json::array();
	for (auto lib : libList) {
	    dbList.push_back(lib);
	}

	dbInfo[databaseName] = dbList;
    }

    obj["database_info"] = dbInfo;
}

void Sysfilter::writeTrackedFuncInfo(
    AnalysisContext &ctx, nlohmann::json &obj) {
    for (auto &kv : trackedFuncInfo) {
	Function *func = kv.first;
	//std::set<TrackedFuncInfo> &infos = kv.second;

        auto mod = func->getParent()->getParent();

        auto funcObj = nlohmann::json::object();
        funcObj["module"] = mod->getName();
        funcObj["function"] = func->getName();

        funcObj["has_direct_edge"] = ctx.reachableCallgraph.hasDirectEdge(func);
        funcObj["is_implicit_source"]
            = ctx.reachableCallgraph.isImplicitSource(func);
        funcObj["is_implicit_target"]
            = ctx.reachableCallgraph.isImplicitTarget(func);

        obj.push_back(funcObj);
    }
}

void Sysfilter::writeCallerSummary(
    std::map<int, SyscallInfo> &callerInfo, nlohmann::json &obj) {
    for (auto &kv : callerInfo) {
        int nr = kv.first;
        SyscallInfo &info = kv.second;

        nlohmann::json sysObj = nlohmann::json::object();

        if (info.rawCallers.size() > 0) {
            nlohmann::json rawCalls = nlohmann::json::array();
            for (auto f : info.rawCallers) {
                auto o = nlohmann::json::object();
                buildCallObject(f, o);
                rawCalls.push_back(o);
            }
            sysObj["raw"] = rawCalls;
        }

        if (info.funcCallers.size() > 0) {
            nlohmann::json funcCalls = nlohmann::json::array();
            for (auto f : info.funcCallers) {
                auto o = nlohmann::json::object();
                buildCallObject(f, o);
                funcCalls.push_back(o);
            }
            sysObj["func"] = funcCalls;
        }

        obj[std::to_string(nr)] = sysObj;
    }
}

void Sysfilter::buildCallObject(Function *function, nlohmann::json &obj) {
    auto module = static_cast<Module *>(function->getParent()->getParent());
    auto lib = module->getLibrary();

    obj["lib"] = lib->getName();
    obj["func"] = function->getName();
}

void Sysfilter::buildOptions(cxxopts::Options &opts) {
    // clang-format off
    opts.add_options("Internal")
        ("input-file", "Input file.",
            cxxopts::value<std::vector<std::string>>());
    opts.add_options("General")
        ("h,help", "Display this information.")
        ("v,verbose", "Verbose mode. Repeat as needed for additional verbosity.")
        ("arg-mode", "Perform argument tracking for supported functions. (EXPERIMENTAL)")
	("arg-mode-passes", "Comma separated list of passes to perform argument tracking"
	 " (eg. '--arg-mode-passes=vacuum,naive,direct' or '--arg-mode-passes=all').",
	 cxxopts::value<std::string>()->default_value(""))
        ("dump-fcg", "Export the callgraph. (EXPERIMENTAL)")
	("find-at-syscalls", "Find syscalls reachable (via direct calls) from each "
	 "address-taken function. (WARNING: High memory usage)")
	("dl-autoload", "Automatically load libraries and symbols loaded with dlopen/dlsym \
that can be resolved statically (implies '--arg-mode').")
	("dl-autoload-limit", "Recursion limit for dlopen/dlsym autoloading.",
	 cxxopts::value<int>()->default_value(std::to_string(config.dlRecursionLimit)))
	("extra-as-dep", "Parse extra libraries added with '--dl-file' as dependencies of main binary. (EXPERIMENTAL)")
	("nss-discover", "Add NSS funcs to FCG as discovered")
	("nss-report-only", "Don't add discovered NSS functions to the callgraph or load extra libraries, just report on observable NSS API usage")
	("nss-report-legacy", "Report on functions that would have been loaded in legacy NSS implementation.  Does not affect callgraph.")
        ("dl-file", "ARG=JSON file describing dlopen() and dlsym() usage.",
            cxxopts::value<std::vector<std::string>>())
        ("no-callers", "Disable the reporting of functions making each syscall.")
        ("entry-point", "Use single-entry-point analysis from ARG in main binary.",
            cxxopts::value<std::string>()->default_value(""))
	("no-entry-point", "Analyze background entry points only.")
	("entry-point-only", "Analyze only the starting function.")
	("entry-data", "Use function pointers in data symbol ARG as entry points.",
	 cxxopts::value<std::string>()->default_value(""))
	("entry-symbol", "Consider this symbol (code or data) as an entry point.",
	 cxxopts::value<std::string>()->default_value(""))
	("entry-symbol-file", "JSON file with list of entry symbols to run, output stored in separate files.",
	 cxxopts::value<std::string>()->default_value(""))
	("no-add-all-ifuncs", "Don't explicitly add all ifuncs at startup.")
	("disable-nss", "Disable the automatic loading of libs in /etc/nsswitch.conf.")
	("nss-config", "NSS database config file",
	 cxxopts::value<std::string>()->default_value("/etc/nsswitch.conf"))
        ("multi-pass", "Use separate analysis passes for init/main/fini/ifuncs.")
	("multi-passes", "Comma separated list of passes to do separate analyses"
	 " for init/main/fini/ifuncs (eg. '--multi-passes=vacuum,naive,direct' or '--multi-passes=all').",
	 cxxopts::value<std::string>()->default_value(""))
        ("fcg-only", "Do not do syscall extraction; just extract the FCG.")
        ("vacuum-fcg", "Build the vacuumed callgraph. (default: )")
        ("naive-fcg", "Use the alternative method (naive) to extract the callgraph.")
        ("direct-fcg", "Generate a callgraph that does not consider address-taken functions.")
        ("atextra-fcg", "Generate a callgraph that contains only AT functions, except main.  Used for testing.")
        ("multi-fcg", "Equivalent to '--vacuum-fcg --naive-fcg --direct-fcg'. Ignores other FCG options.")
        ("universal-fcg", "Generate an \"all-calls-all\" callgraph instead of being intelligent.");
    opts.add_options("Syscall analysis")
        ("extract-from-all",
            "Try extracting syscalls from all functions, not just the reachable ones.");
    opts.add_options("Library resolution")
        ("sysroot", "Sysroot prefix for resolving shared libraries (sets EGALITO_SYSROOT). "
            "Use this to analyze binaries from a mounted container image, e.g. '--sysroot=/mnt/bundle'.",
            cxxopts::value<std::string>()->default_value(""))
        ("library-path", "Colon-separated list of additional directories to search for shared "
            "libraries (prepended to EGALITO_LIBRARY_PATH), e.g. '--library-path=/mnt/bundle/lib:/mnt/bundle/usr/lib'.",
            cxxopts::value<std::string>()->default_value(""));
    opts.add_options("Output")
        ("syscalls-flat", "Use a flat textual representation for output.")
        ("syscalls-json", "Output only syscall list as JSON.")
        ("full-json", "Use verbose JSON output.")
        ("o,output", "Output results into ARG.",
	 cxxopts::value<std::string>()->default_value(""))
	("j,json-stats","Same as --output.",
	 cxxopts::value<std::string>()->default_value(""));
    // clang-format on

    opts.parse_positional("input-file");
    opts.positional_help("input-file");

    // Ignore unrecognized options.  This allows us to share
    // CLI opts with libfilter for now
    opts.allow_unrecognised_options();

    helpstr = opts.help({ "General", "Library resolution", "Syscall analysis", "Output" });
}

void Sysfilter::printUsage() {
    std::cout << "Sysfilter extraction tool, version "
              << SYSFILTER_VERSION_STRING << std::endl;
    std::cout << "Copyright (C) 2017-2021, Brown University, Secure Systems Lab"
              << std::endl;
    std::cout << std::endl;
    std::cout << helpstr << std::endl;
}

void Sysfilter::error() {
    std::exit(1);
}

void Sysfilter::parseDl(const std::string &path) {
    auto sep = path.rfind('/');
    std::string dirpath;
    if (sep == std::string::npos) { dirpath = "./"; }
    else {
        dirpath = path.substr(0, sep);
    }

    std::ifstream dl_file(path.c_str());
    if (!dl_file) {
        std::cout << "Couldn't open dl file \"" << path << "\"" << std::endl;
        error();
    }

    nlohmann::json dlj;
    dl_file >> dlj;

    if (!dlj.is_array()) {
        std::cout << "dl file format mismatch: expected array at root."
                  << std::endl;
        error();
    }

    for (auto lib : dlj) {
        if (!lib.is_object()) {
            std::cout << "dl file format mismatch: expected object in "
                         "root array."
                      << std::endl;
            error();
        }

        std::string path = lib["path"];
        wordexp_t we;
        if (wordexp(path.c_str(), &we, WRDE_NOCMD) != 0) {
            LOG(0, "Failed to expand path \"" << path << "\"");
            continue;
        }

        path = "";
        for (size_t i = 0; i < we.we_wordc; i++) {
            path += we.we_wordv[i];
        }
        wordfree(&we);

        if (path[0] != '/' && path[0] != '~') { path = dirpath + '/' + path; }
        LOG(9, "Marking extra shared object " << path << " for parsing");

        config.dlFiles.push_back(path);
        config.dlSymbolNames.push_back(lib["symbols"]);
    }
}

void Sysfilter::parseEntrySymbolFile(const std::string &path) {
    std::ifstream inputFile(path.c_str());
    if (!inputFile) {
        throw std::logic_error("Could not open entry symbol file");
    }

    nlohmann::json jf;
    inputFile >> jf;

    if (!jf.is_array()) {
        throw std::logic_error(
            "Error parsing entry point symbol file: expected array at root");
    }

    config.entrySymbolNames.push_back(std::make_tuple("", entry_base_name));
    for (auto entry : jf) {
	if (entry.is_string()) {
	    config.entrySymbolNames.push_back(std::make_tuple("",
							      entry.get<std::string>()));
	} else if (entry.is_object()) {
		config.entrySymbolNames.push_back(std::make_tuple(entry["module"].get<std::string>(),
								  entry["function"].get<std::string>()));
	}
    }
}

void Sysfilter::parseFcgArgs(const std::string &s, std::set<FCGType> &ret) {
    const static std::string delim = ",";
    const static std::map<std::string, FCGType> fcgStringMap = {
	{ "direct", FCGType::DirectFCG },
	{ "universal", FCGType::UniversalFCG },
	{ "naive", FCGType::NaiveFCG },
	{ "vacuum", FCGType::VacuumFCG },
	{ "atextra", FCGType::ATExtraFCG },
    };

    if (s.size() == 0) { return; }

    auto handleToken = [&](const std::string &tok) {
        if (tok == "all") {
            ret.insert(FCGType::VacuumFCG);
            ret.insert(FCGType::NaiveFCG);
            ret.insert(FCGType::DirectFCG);
        }
        else if (fcgStringMap.count(tok) == 0) {
            throw std::logic_error("Invalid FCG type!");
        }
        else {
            FCGType ft = fcgStringMap.at(tok);
            ret.insert(ft);
        }
    };

    size_t start = 0;
    size_t end = s.find(delim);

    while (end != std::string::npos) {
        std::string token = s.substr(start, end - start);
        handleToken(token);

        start = end + delim.length();
        end = s.find(delim, start);
    }

    handleToken(s.substr(start, end));
}

void Sysfilter::extractConfig(cxxopts::Options &opts, int argc, char **argv) {
    auto copt = opts.parse(argc, argv);

    // Must be set before egalito initializes ConductorFilesystem/ElfDynamic
    auto sysroot = copt["sysroot"].as<std::string>();
    if (!sysroot.empty()) {
        setenv("EGALITO_SYSROOT", sysroot.c_str(), 1);
    }
    auto libraryPath = copt["library-path"].as<std::string>();
    if (!libraryPath.empty()) {
        setenv("EGALITO_LIBRARY_PATH", libraryPath.c_str(), 1);
    }

    if (copt["help"].as<bool>()) {
        printUsage();

        throw std::runtime_error("Help requested, so not doing any work.");
    }

    if (copt.count("input-file") == 0) {
        printUsage();
        throw std::runtime_error("No input file given.");
    }
    else if (copt.count("input-file") > 1) {
        printUsage();
        throw std::runtime_error("Multiple input files given!");
    }

    config.inputFile
        = copt["input-file"].as<std::vector<std::string>>().front();

    // output specification
    if (copt.count("json-stats") != 0) {
        config.outputFile = copt["json-stats"].as<std::string>();
    }

    if (copt.count("output") != 0) {
        config.outputFile = copt["output"].as<std::string>();
    }

    if (copt["syscalls-flat"].as<bool>()) {
        config.outputType = Config::FlatOutput;
    }
    else if (copt["syscalls-json"].as<bool>()) {
        config.outputType = Config::SyscallsJsonOutput;
    }
    else if (copt["full-json"].as<bool>()) {
        config.outputType = Config::FullJsonOutput;
    }

    // sysfilter log verbosity
    config.verbosity = 0;
    if (copt["verbose"].as<bool>()) {
        config.verbosity = 8 + copt.count("verbose");
    }
    GroupRegistry::getInstance()->applySetting("app", config.verbosity);
    if (config.verbosity == 0) GroupRegistry::getInstance()->muteAllSettings();

    // egalito log verbosity
    if (!SettingsParser().parseEnvVar("EGALITO_DEBUG")) {
        throw std::runtime_error("Failed to parse EGALITO_DEBUG.");
    }

    if (copt["dl-file"].count() >= 1) {
        for (auto dlf : copt["dl-file"].as<std::vector<std::string>>()) {
            parseDl(dlf);
        }
    }

    config.noCallers = copt["no-callers"].as<bool>();
    config.fcgOnly = copt["fcg-only"].as<bool>();
    config.extraAsDep =  copt["extra-as-dep"].as<bool>();

    config.addAllIfuncs = !copt["no-add-all-ifuncs"].as<bool>();

    config.resolveNss = !copt["disable-nss"].as<bool>();
    config.nssConfigFile = copt["nss-config"].as<std::string>();
    config.extraAsDiscover = copt["nss-discover"].as<bool>();
    config.nssReportOnly = copt["nss-report-only"].as<bool>();
    config.nssReportLegacy = copt["nss-report-legacy"].as<bool>();

    config.entry_point = copt["entry-point"].as<std::string>();
    config.useStartFunc = !copt["no-entry-point"].as<bool>();
    config.startFuncOnly = copt["entry-point-only"].as<bool>();
    config.entry_data = copt["entry-data"].as<std::string>();
    config.entry_symbol = copt["entry-symbol"].as<std::string>();
    config.entrySymbolFile = copt["entry-symbol-file"].as<std::string>();


    if (config.entrySymbolFile.size() != 0) {
	if ((config.entry_point.size() != 0) ||
	    (config.entry_data.size() != 0) ||
	    (config.entry_symbol.size() != 0)) {
	    throw std::runtime_error("Cannot specify --entry-symbol-file with other --entry-{point,data,symbol} options");
	}

        config.multiFileMode = true;
        config.outputDir = config.outputFile;
        config.outputFile = "";
        parseEntrySymbolFile(config.entrySymbolFile);
    }

    const static std::vector<FCGType> defaultFcgPasses
        = { FCGType::VacuumFCG, FCGType::NaiveFCG, FCGType::DirectFCG };

    // FCG construction
    if (copt["multi-fcg"].as<bool>()) {
        config.fcgPasses.insert(
            defaultFcgPasses.begin(), defaultFcgPasses.end());
    }
    else {
        if (copt["universal-fcg"].as<bool>())
            config.fcgPasses.insert(FCGType::UniversalFCG);
        if (copt["naive-fcg"].as<bool>())
            config.fcgPasses.insert(FCGType::NaiveFCG);
        if (copt["direct-fcg"].as<bool>())
            config.fcgPasses.insert(FCGType::DirectFCG);
        if (copt["vacuum-fcg"].as<bool>())
            config.fcgPasses.insert(FCGType::VacuumFCG);
	if (copt["atextra-fcg"].as<bool>())
	    config.fcgPasses.insert(FCGType::ATExtraFCG);
    }

    // If no FCG construction specified, vacuumed is default
    if (config.fcgPasses.size() == 0) {
        config.fcgPasses.insert(FCGType::VacuumFCG);
    }

    // Set default FCG and callgraph
    config.defaultCallgraph = CallgraphAll;
    config.defaultFcg = (config.fcgPasses.count(FCGType::VacuumFCG))
        ? FCGType::VacuumFCG
        : *config.fcgPasses.begin();

    // Setup multi-pass/argtrack for passes where enabled
    parseFcgArgs(
        copt["multi-passes"].as<std::string>(), config.multiPassPasses);
    parseFcgArgs(
        copt["arg-mode-passes"].as<std::string>(), config.argTrackPasses);

    // If we used --multi-pass or --arg-mode, turn on these features
    // for whatever callgraphs are already enabled
    if (copt["multi-pass"].as<bool>()) {
        config.multiPassPasses.insert(
            config.fcgPasses.begin(), config.fcgPasses.end());
    }

    if (copt["arg-mode"].as<bool>()) {
        config.argTrackPasses.insert(
            config.fcgPasses.begin(), config.fcgPasses.end());
    }

    // Alternately, if any FCG passes are being used by multi-pass
    // or argtrack modes, but are not already enabled, enable them
    config.fcgPasses.insert(
        config.multiPassPasses.begin(), config.multiPassPasses.end());
    config.fcgPasses.insert(
        config.argTrackPasses.begin(), config.argTrackPasses.end());

    config.dlAutoload = copt["dl-autoload"].as<bool>();
    config.dlRecursionLimit = copt["dl-autoload-limit"].as<int>();

    config.dumpCallgraph = copt["dump-fcg"].as<bool>();
    config.findIndirectSyscalls = copt["find-at-syscalls"].as<bool>();

    if (config.dlAutoload && config.extraAsDep) {
        throw std::runtime_error(
            "Combining --extra-as-dep and --dl-autoload not supported!");
    }

    // Any of these verbose options implies --full-json
    if ((config.needMultiPass()) || config.dumpCallgraph
        || config.findIndirectSyscalls || (config.needArgTrack())
        || (config.fcgPasses.size() > 1)) {
        config.outputType = Config::FullJsonOutput;
    }

    // syscall-specific config
    if (!config.needArgTrack()) {
        if (copt["extract-from-all"].as<bool>())
            config.extractSet = Config::ExtractAll;
    }
    // dlsym specific config
    else {
    }
}

bool Sysfilter::parse(const char *filename) {
    LOG(1, "processing file [" << filename << "]");

    try {
        // parse file recursively, but don't include the egalito library.
        setup.parseElfFiles(filename, true, false);

        // TODO: also allow parsing egalito archives
    }
    catch (const char *message) {
        LOG(1, "Caught exception: " << message);
        throw std::runtime_error(std::string("parseElfFiles: ") + message);
    }
    return true;
}

bool Sysfilter::parseExtras() {
    if (config.dlAutoload) { return parseExtrasAutoload(); }
    else {
        return parseExtrasDefault();
    }
}

bool Sysfilter::parseExtrasDefault() {
    auto program = setup.getConductor()->getProgram();
    auto mainModule = getMainModule(program);
    ElfDynamic edyn(program->getLibraryList());

    if (config.extraAsDep) {
        for (unsigned i = 0; i < config.dlFiles.size(); i++) {
            auto moduleName = config.dlFiles[i];
            edyn.addDependency(mainModule->getLibrary(), moduleName);
            setup.getConductor()->parseLibraries();

            auto slash = moduleName.rfind("/");
            auto searchName = (moduleName.rfind(".", 0) == 0)
                ? moduleName
                : (slash != std::string::npos) ? moduleName.substr(slash + 1)
                                               : moduleName;

            auto thisLib = program->getLibraryList()->find(searchName);
            if (!thisLib) {
                LOG(1, "Couldn't parse " << moduleName << " as an addon file.");
                return false;
            }

            auto module = thisLib->getModule();
            if (!module) {
                LOG(1, "Couldn't parse " << moduleName << " as an addon file.");
                return false;
            }

            config.dlSymbols.push_back(std::vector<Function *>());
            for (auto &name : config.dlSymbolNames[i]) {
                auto func = CIter::named(module->getFunctionList())->find(name);
                if (!func) {
                    LOG(1,
                        "Couldn't find symbol " << name << " in "
                                                << config.dlFiles[i]);
                    return true;
                }
                config.dlSymbols.back().push_back(func);
            }

            // XXX: may not be needed?
            setup.ensureBaseAddresses();

            ResolvePLTPass resolvePLT(setup.getConductor());
            program->accept(&resolvePLT);

            CollapsePLTPass collapsePLT(setup.getConductor());
            program->accept(&collapsePLT);
        }
    }
    else {
        auto modules = setup.addExtraLibraries(config.dlFiles);
        for (unsigned i = 0; i < modules.size(); i++) {
            if (!modules[i]) {
                LOG(1,
                    "Couldn't parse " << config.dlFiles[i]
                                      << " as an addon file.");
                return false;
            }

            config.dlSymbols.push_back(std::vector<Function *>());
            for (auto &name : config.dlSymbolNames[i]) {
                auto func
                    = CIter::named(modules[i]->getFunctionList())->find(name);
                if (!func) {
                    LOG(1,
                        "Couldn't find symbol " << name << " in "
                                                << config.dlFiles[i]);
                    return false;
                }
                config.dlSymbols.back().push_back(func);
            }
        }
    }

    return true;
}

bool Sysfilter::parseExtrasAutoload() {

    for (auto moduleName : config.dlFiles) {
        try {
            std::vector<std::string> libToLoad = { moduleName };
            auto modules = setup.addExtraLibraries(libToLoad);
            for (unsigned int i = 0; i < modules.size(); i++) {
                auto module = modules[i];
                if (!module) {
                    LOG(1,
                        "Failed to load DL library "
                            << moduleName
                            << "egalito failed to parse and returned NULL");
                    dlLibraryLoadFailures.insert(moduleName);
                }
                else {
                    config.dlSymbols.push_back(std::vector<Function *>());

                    auto &symMap = resolvedDlSymbols[moduleName];

                    for (auto &name : config.dlSymbolNames[i]) {
                        auto func = CIter::named(module->getFunctionList())
                                        ->find(name);
                        if (!func) {
                            LOG(1,
                                "Couldn't find symbol " << name << " in "
                                                        << config.dlFiles[i]);
                        }
                        else {
                            config.dlSymbols.back().push_back(func);
                            symMap.insert(func);
                        }
                    }
                }
            }
        }
        catch (const char *message) {
            LOG(1,
                "Failed to autoload library "
                    << moduleName << ", egalito returned exception:  "
                    << ":  " << message);
            dlLibraryLoadFailures.insert(moduleName);
        }
        catch (...) {
            LOG(1, "Failed to autoload library " << moduleName);
            dlLibraryLoadFailures.insert(moduleName);
        }
    }

    return true;
}

std::set<Function *> Sysfilter::externalFunctions() {
    class ExternalFunctionVisitor : public ChunkPass {
    public:
        std::set<Function *> externals;
        virtual void visit(ExternalSymbol *symbol) {
            Function *f = dynamic_cast<Function *>(symbol->getResolved());
            if (f) {
                LOG(10, "Found external symbol: " << f->getName());
                externals.insert(f);
            }
        }
        virtual void visit(Function *function) {
            if (function->getName() == "_dl_start") {
                externals.insert(function);
            }
        }
        virtual void visit(Module *module) {
            recurse(module->getFunctionList());
            recurse(module->getExternalSymbolList());
        }
    };
    ExternalFunctionVisitor efv;
    setup.getConductor()->getProgram()->getMain()->accept(&efv);
    return efv.externals;
}



void Sysfilter::resolveNss() {
    NSSFuncsPass nss(config.nssConfigFile);
    nssDatabaseMap = nss.getDatabaseMapping();

    ResolverContext rctx(setup.getConductor());
    nss.resolve(rctx);

    // Fetch resolver context
    if (!config.extraAsDiscover) {
	for (auto f : rctx.extraEntryPoints) {
	    config.extraEntryPoints.push_back(f);
	}
    }

    nssLibSearchFailures.insert(rctx.libSearchFailures.begin(),
				rctx.libSearchFailures.end());

    for (auto &kv : rctx.discoverEntryPoints) {
	auto func = kv.first;
	auto &children = kv.second;

	auto &x = config.discoverEntryPoints[func];
	x.insert(children.begin(), children.end());
    }

    for (auto &kv : rctx.backendFuncsLoaded) {
	auto &x = nssBackendFuncsLoaded[kv.first];
	x.insert(kv.second.begin(), kv.second.end());
    }

    for (auto &kv : rctx.backendFuncsUsed) {
	auto &x = nssBackendFuncsUsed[kv.first];
	x.insert(kv.second.begin(), kv.second.end());
    }

    // get libc library
    auto program = setup.getConductor()->getProgram();

    // XXX: may not be needed?
    setup.ensureBaseAddresses();

    ResolvePLTPass resolvePLT(setup.getConductor());
    program->accept(&resolvePLT);

    CollapsePLTPass collapsePLT(setup.getConductor());
    program->accept(&collapsePLT);

    LOG(1,
        "Extra entry points from NSS dependencies: "
            << config.extraEntryPoints.size());
}

void Sysfilter::resolveLegacyNss() {
    LegacyNSSFuncsPass nss(config.nssConfigFile);
    setup.getConductor()->acceptInAllModules(&nss, false);

    // get libc library
    auto program = setup.getConductor()->getProgram();
    if (program->getLibc() == nullptr) {
        LOG(1, "LNSS:  No libc found, skipping NSS detection --- parsing loader?");
        return;
    }
    //auto libc = program->getLibc()->getLibrary();
    ElfDynamic edyn(program->getLibraryList());

    auto libneeded = nss.needed();
    for (auto needed : libneeded) {

	// Always add the frontend functions to the reporting info
        for (auto fname : needed.second) {
	    nssLegacyFrontendFuncsUsed["UNKNOWN"].insert(fname);
	}

	if (config.nssReportOnly) {
	    LOG(1, "LNSS:  Using report only mode, skipping backend function search");
	    continue;
	}

        std::string soname = "libnss_" + needed.first + ".so.2";
        auto library = program->getLibraryList()->find(soname);

	// NOTE:  Don't load any libraries:  the default NSS handling should have found this for us
	if (!library) {
	    if (nssLibSearchFailures.count(soname)) {
		// We didn't find the library, but the primary NSS handling didn't either,
		// so it's okay
		continue;
	    } else {
		// If the primary handling didn't log a failure to find this lib,
		// we are the first to ask for it, which is an error
		LOG(1, "LNSS:  Unable to find lib:  " << soname);
		assert(0 && "ERROR:  Legacy NSS found library that the standard version did not!");
	    }
	}
	// edyn.addDependency(libc, soname);
	// setup.getConductor()->parseLibraries();

        //auto library = program->getLibraryList()->find(soname);
        // the NSS config can validly specify nonexistent libraries.

        auto module = library->getModule();
        for (auto fname : needed.second) {
	    auto searchName = "_nss_" + needed.first + "_" + fname;
            auto func = CIter::named(module->getFunctionList())->find(searchName);
            if (!func) {
                LOG(1,
                    "LNSS:  Looked for function with name "
		    << searchName
		    << ", result: " << func);

                // try _r version of function instead
		searchName = "_nss_" + needed.first + "_" + fname + "_r";
                func = CIter::named(module->getFunctionList())
                           ->find(searchName);
            }
            LOG(1,
                "LNSS:  Looked for function with name " << searchName
		<< ", result: " << func);

            // hopefully we found one of those two, but it may not be present
            // in this NSS library.
            if (!func) continue;
            assert(func);
            //config.extraEntryPoints.push_back(func);
	    nssLegacyBackendFuncsLoaded.insert(func);
        }
    }

    // XXX: may not be needed?
    setup.ensureBaseAddresses();

    ResolvePLTPass resolvePLT(setup.getConductor());
    program->accept(&resolvePLT);

    CollapsePLTPass collapsePLT(setup.getConductor());
    program->accept(&collapsePLT);

    LOG(1,
        "Extra entry points from NSS dependencies: "
            << config.extraEntryPoints.size());
}


void Sysfilter::resolveStartingFunctions() {
    auto program = setup.getConductor()->getProgram();

    auto resolveEntryPoint = [&](std::string name, std::set<Function *> &target,
                                 bool doSearch, Module* searchModule = nullptr) {
        bool found = false;
	auto mainModule = (searchModule) ? searchModule : getMainModule(program);
        auto start = CIter::named(mainModule->getFunctionList())->find(name);
        auto verPos = name.find("@");
        if (!start && verPos != std::string::npos) {
            auto baseName = name.substr(0, verPos);
            start = CIter::named(mainModule->getFunctionList())->find(baseName);
            if (start) {
                LOG(1,
                    "Found non-versioned starting function "
                        << start->getName() << " to match " << name);
            }
        }

        if (!start) {
            if (doSearch) {
                LOG(9,
                    "[" << name
                        << "] not found, trying ELF entry point instead.");

                auto elfmap = mainModule->getElfSpace()->getElfMap();
                auto entryAddr = elfmap->getEntryPoint();

                for (auto f : CIter::children(mainModule->getFunctionList())) {
                    if (f->getPosition()->get() == entryAddr) {
                        target.insert(f);
                        found = true;
                        break;
                    }
                }

                if(!found) {
                    throw std::runtime_error(
                        "could not find entry point function for " + name
                        + " (stripped PIE or unsupported binary format)");
                }
            }
            else {
                LOG(9, "[" << name << "] not found, but search not requested");
            }
        }
        else {
            found = true;
            target.insert(start);
        }

        return found;
    };

    if (config.needMultiPass()) {
        // _start should always be part of the init set
        resolveEntryPoint("_start", ctxFuncs(CallgraphInit), true);

        // In addition, we want to add an entry point for main
        // since later we will remove the link from _start->main
        // This entry point can be configurable
        auto start_name = config.entry_point;
        if (start_name.length() == 0) { start_name = "main"; }
        resolveEntryPoint(start_name, ctxFuncs(CallgraphMain), false);
    }
    else {
        if (config.useStartFunc) {
            auto start_name = config.entry_point;
            bool universalOnly = config.fcgPasses.count(UniversalFCG)
                && (config.fcgPasses.size() == 1);

            if (!universalOnly) {
		if (config.entry_symbol.length() != 0)  {
		    bool found = false;

		    auto searchModule = (config.entry_module.length() != 0) ?
			getModuleByPath(program, config.entry_module) : nullptr;

		    // First, try to search for the function
		    LOG(1, "Attempting to search for entry function " << config.entry_symbol);
                    found = resolveEntryPoint(config.entry_symbol, ctxFuncs(CallgraphAll), false,
					      searchModule);

                    if (!found) {
                        std::set<Function *> funcsFromDataSymbol;

                        LOG(1,
                            "Attempting to search for entry symbol "
                                << config.entry_symbol);
                        found = findFuncsInDataSymbol(
                            config.entry_symbol, funcsFromDataSymbol);

                        ctxFuncs(CallgraphMain)
                            .insert(funcsFromDataSymbol.begin(),
                                funcsFromDataSymbol.end());
                    }

                    if (!found) {
                        throw std::runtime_error(
                            "No function or symbol found with name specified "
                            "with --entry-symbol");
                    }
                }
                else if (config.entry_point.length() != 0) {
                    auto foundEntryPoint = resolveEntryPoint(
                        start_name, ctxFuncs(CallgraphAll), false);
                    if (!foundEntryPoint) {
                        throw std::runtime_error("Entry point specified with "
                                                 "--entry-point not found!");
                    }
                }
                else if (config.entry_data.length() != 0) {
                    std::set<Function *> funcsFromDataSymbol;
                    findFuncsInDataSymbol(
                        config.entry_data, funcsFromDataSymbol);

                    if (funcsFromDataSymbol.size() == 0) {
                        throw std::runtime_error(
                            "Data symbol specified with --entry-data, but no "
                            "functions added!");
                    }
                    ctxFuncs(CallgraphMain)
                        .insert(funcsFromDataSymbol.begin(),
                            funcsFromDataSymbol.end());
                }
                else {
                    start_name = "_start";
                    resolveEntryPoint(start_name, ctxFuncs(CallgraphAll), true);
                }

                // If we're doing the direct FCG without multi-pass, add main in
                // addition to _start since we know there is a link between them
                if (config.fcgPasses.count(DirectFCG)) {
                    resolveEntryPoint("main", ctxFuncs(CallgraphAll), false);
                }
            }
        }
        else {
            LOG(1,
                "Main entry point disabled, not adding to starting functions");
        }
    }

    // Extra entry points (eg, NSS entry points are added here)
    if (!config.extraAsDiscover) {
	LOG(1, "DISC:  Not adding NSS entry points since discovery is enabled");
	ctxFuncs(CallgraphNss)
	    .insert(config.extraEntryPoints.begin(), config.extraEntryPoints.end());
    }

    if (config.startFuncOnly) {
        LOG(1, "Skipping addition of entry points for loader and init/fini");
    }
    else {
        /* XXX: temporarily needed for libfilter, should be cleaned up */
        auto dl_needed = std::vector<std::string> {
            /* needed by the dynamic loader */
            "_dl_catch_exception",
            "malloc",
            "_dl_signal_exception",
            "calloc",
            "realloc",
            "_dl_signal_error",
            "_dl_catch_error",
            /* elf entry points */
            "__libc_main",
            "__libc_start_main",
            "libc_main",
            "__dls2b",
            "__dls3",
        };

        std::vector<std::string> ifuncs_needed;
        LibraryList *liblist = setup.getConductor()->getLibraryList();

        for (auto library : CIter::children(liblist)) {
            auto symbols = library->getModule()->getElfSpace()->getSymbolList();
            if (!symbols) { continue; }
            LOG(11, "Looking for IFUNCS in " << library->getName());
            for (auto it = symbols->begin(); it != symbols->end(); ++it) {
                auto symbol = *it;
                if (symbol->getType() == Symbol::TYPE_IFUNC) {
                    ifuncs_needed.push_back(symbol->getName());
                    LOG(11, "IFUNC Symbol: " << symbol->getName());
                }
            }
        }

        auto resolveFuncs
            = [&](std::vector<std::string> names, std::set<Function *> &funcs) {
                  for (auto fname : names) {
                      bool found = false;
                      for (auto library : CIter::children(liblist)) {
                          auto f = CIter::named(
                              library->getModule()->getFunctionList())
                                       ->find(fname);
                          if (f) {
                              found = true;
                              funcs.insert(f);
                          }
                      }
                      if (!found) {
                          LOG(1,
                              "Couldn't find function " << fname
                                                        << " in any libraries");
                      }
                  }
              };
        resolveFuncs(dl_needed, ctxFuncs(CallgraphInit));

	if (config.addAllIfuncs) {
	    resolveFuncs(ifuncs_needed, ctxFuncs(CallgraphIfuncs));
	}

        /* XXX: end needed by libfilter */

        // Since the preinit/init/fini_array sections may be separated by
        // symbols, we have to manually add their contents rather than having
        // the callgraph generation code automatically find them.

        auto process_array = [&](DataSection *array,
                                 std::set<Function *> &dest) {
            if (array) {
                for (auto var : CIter::children(array)) {
                    auto func
                        = dynamic_cast<Function *>(var->getDest()->getTarget());
                    LOG(10,
                        "Processing entry point "
                            << func << "/" << var->getName() << " at address "
                            << var->getDest()->getTargetAddress());
                    //
                    LOG(10,
                        "    from link of type "
                            << typeid(*var->getDest()).name());
                    LOG(10,
                        "    which points to chunk of type "
                            << typeid(*var->getDest()->getTarget()).name());

                    if (!func) {
                        LOG(1,
                            "Compromising accuracy by using containing "
                            "function for instruction!");
                        auto instr = dynamic_cast<Instruction *>(
                            var->getDest()->getTarget());
                        if (instr)
                            func = static_cast<Function *>(
                                instr->getParent()->getParent());
                    }
                    // var->getDest()
                    // assume that the array element is pointing to a function.
                    assert(func);
                    dest.insert(func);
                }
            }
        };

        for (auto module : CIter::children(program)) {
            process_array(
                module->getDataRegionList()->findDataSection(".preinit_array"),
                ctxFuncs(CallgraphInit));
            process_array(
                module->getDataRegionList()->findDataSection(".init_array"),
                ctxFuncs(CallgraphInit));
            process_array(
                module->getDataRegionList()->findDataSection(".fini_array"),
                ctxFuncs(CallgraphFini));

            process_array(
                module->getDataRegionList()->findDataSection(".ctors"),
                ctxFuncs(CallgraphInit));
            process_array(
                module->getDataRegionList()->findDataSection(".dtors"),
                ctxFuncs(CallgraphFini));

            auto init = CIter::named(module->getFunctionList())->find("_init");
            if (init) ctxFuncs(CallgraphInit).insert(init);
            auto fini = CIter::named(module->getFunctionList())->find("_fini");
            if (fini) ctxFuncs(CallgraphFini).insert(fini);

            auto elfsec
                = module->getElfSpace()->getElfMap()->findSection(".dynamic");
            if (elfsec) {
#ifdef ARCH_X86_64
                typedef Elf64_Dyn ElfXX_Dyn;
#elif ARCH_X86_32
                typedef Elf32_Dyn ElfXX_Dyn;
#else
#error "Invalid architecture"
#endif
                ElfXX_Dyn *dynsec = (ElfXX_Dyn *)elfsec->getReadAddress();

                for (size_t i = 0; i < elfsec->getSize() / sizeof(dynsec[0]);
                     i++) {
                    if (dynsec[i].d_tag != DT_INIT
                        && dynsec[i].d_tag != DT_FINI)
                        continue;

                    for (auto f : CIter::children(module->getFunctionList())) {
                        if (f->getPosition()->get() == dynsec[i].d_un.d_val) {
                            ctxFuncs(CallgraphFini).insert(f);
                            break;
                        }
                    }
                }
            }
        }

        // add dynamically-loaded symbols
        for (auto mlist : config.dlSymbols) {
            for (auto func : mlist) {
                ctxFuncs(CallgraphMain).insert(func);
            }
        }
    }

    //  Finally, combine all starting functions for the "all" pass
    for (auto &kv : startingFuncMap) {
        CallgraphType t = kv.first;
        std::set<Function *> &funcs = kv.second;

        if (t != CallgraphAll) {
            ctxFuncs(CallgraphAll).insert(funcs.begin(), funcs.end());
        }
    }
}

bool Sysfilter::findFuncsInDataSymbol(
    std::string symName, std::set<Function *> &funcs) {
    bool found = false;
    auto program = setup.getConductor()->getProgram();

    DataSymbolList &dataSymbols = getDataSymbolList();
    // dataSymbols.generate(program);

    std::set<DataSymbol *> matches; // = dataSymbols.getByName(symName);

    std::vector<DataSymbol *> toConsider;
    std::set<DataSymbol *> symbolsConsidered;
    std::set<Function *> out;

    auto resolveSymbolName = [&](std::string targetName,
                                 std::set<DataSymbol *> &matches) {
        auto mainModule = getMainModule(program);
        auto elfspace = mainModule->getElfSpace();

        std::set<int> sectionIgnores;
        bool isLibc
            = (mainModule->getLibrary()->getRole() == Library::ROLE_LIBC);
        if (isLibc) {
            auto ignore = [&sectionIgnores, elfspace](const char *name) {
                auto section = elfspace->getElfMap()->findSection(name);
                if (section)
                    sectionIgnores.insert((int)section->getNdx());
                else
                    LOG(1, "Couldn't find section " << name << " in libc");
            };

            ignore("__libc_subfreeres");
            ignore("__libc_atexit");
            ignore("__libc_thread_subfreeres");
        }

        if (!elfspace->getSymbolList()) {
            throw std::runtime_error(
                "No symbols found for main module, cannot search for data "
                "symbols with --entry-data!");
        }

        auto verPos = targetName.find("@");
        bool targetIsVersioned = (verPos != std::string::npos);
        auto targetBaseName
            = (targetIsVersioned) ? targetName.substr(0, verPos) : "";

        for (auto symbol : *elfspace->getSymbolList()) {
            if (symbol->getType() != Symbol::TYPE_OBJECT) { continue; }
            /* Handle libc wonkiness */
            if (sectionIgnores.count(symbol->getSectionIndex())) { continue; }

            if (!symbol->getName()) { continue; }

            auto symbolName = std::string(symbol->getName());
            bool match = false;

            if (symbolName == targetName) { match = true; }
            else if (targetIsVersioned && (symbolName == targetBaseName)) {
                LOG(1,
                    "DSS:  Found non-versioned function "
                        << symbolName << " to match " << targetBaseName);
                match = true;
            }

            if (match) {
                auto addr = symbol->getAddress() + mainModule->getBaseAddress();
                auto ds = dataSymbols.forAddress(addr);
                assert(ds);

                matches.insert(ds);
            }
        }
    };

    // Try to find the symbol
    resolveSymbolName(symName, matches);

    if (matches.size() == 0) {
        LOG(1, "DSS:  No matches found!");
        found = false;
    }
    else {
        found = true;
        for (auto ds : matches) {
            LOG(1,
                "DSS:  Considering initial symbol:  "
                    //<< ds->name
                    << "@0x" << std::hex << ds->start);
            toConsider.push_back(ds);
        }
    }

    while (toConsider.size() != 0) {
        DataSymbol *sym = toConsider.back();
        toConsider.pop_back();
        symbolsConsidered.insert(sym);

        LOG(1,
            "DSS:  Considering symbol:  "
                //<< sym->name
                << "@0x" << std::hex << sym->start);

        for (auto f : sym->codeReferences) {
            LOG(1, "DSS:  Adding function:  " << f->getName());
            out.insert(f);
        }

        for (auto ds : sym->dataReferences) {
            LOG(1,
                "DSS:  Considering new symbol:  "
                    //<< ds->name
                    << "@0x" << std::hex << ds->start);

            if (symbolsConsidered.count(ds) == 0) { toConsider.push_back(ds); }
        }
    }

    funcs.insert(out.begin(), out.end());
    return found;
}

void Sysfilter::extractfcg(
    FCGType fcgType, CallgraphType ct, AnalysisContext &ctx) {
    auto program = setup.getConductor()->getProgram();

    Callgraph callgraph;
    std::set<Function *> &startingFunctions = startingFuncMap[ct];

    // mark all starting functions as reachable
    ctx.reachableFunctions.insert(
        startingFunctions.begin(), startingFunctions.end());

    if ((fcgType == FCGType::VacuumFCG) || (fcgType == FCGType::ATExtraFCG)) {
        VacuumedCallgraph vc(program, getDataSymbolList());

        for (auto f : startingFunctions)
            vc.addRoot(f);

        for (auto f : dlTargets(ct)) {
            vc.addImplicitRoot(f);
        }

        for (auto mlist : config.dlSymbols) {
            for (auto f : mlist) {
                vc.addImplicitRoot(f);
            }
        }

	if (config.extraAsDiscover) {
	    for (auto &kv : config.discoverEntryPoints) {
		auto src = kv.first;
		auto targets = kv.second;
		LOG(1, "DISC:  Adding extra function " << src->getName()
		    << " with " << targets.size() << " targets");
		vc.addExtraFuncs(src, targets);
	    }
	}

        if (ct == CallgraphInit) {
            auto mainModule = getMainModule(program);
            Function *fMain
                = CIter::named(mainModule->getFunctionList())->find("main");
            if (fMain) {
                vc.exclude(fMain);
                LOG(1, "Excluding main function from init callgraph");
            }
            else {
                LOG(1, "No main found, cannot exclude from init callgraph");
            }
        }
        callgraph = vc.generate();

	LOG(1, "callgraph generated. grabbing the reachable functions.");

        if (fcgType == FCGType::ATExtraFCG) {
            LOG(1, "OCG:  Clearing existing reachable function list");
            ctx.reachableFunctions.clear();

            LOG(1, "OCG:  Generating OCG from VCG.");
            VacuumedCallgraph atOnlyVc(program, getDataSymbolList());

            auto mainModule = getMainModule(program);
            Function *fMain = CIter::named(mainModule->getFunctionList())->find("main");
            if (fMain) {
                LOG(1, "OCG:  Excluding main from callgraph");
                atOnlyVc.exclude(fMain);
            } else {
                LOG(1, "OCG:  Could not find main function, so it will not be excluded from OCG.");
            }

            for (auto f : callgraph.getImplicitTargets()) {
                if (fMain && (f == fMain)) {
                    LOG(1, "OCG:  Not adding main to OCG.");
                    continue;
                }
                atOnlyVc.addImplicitRoot(f);
            }
            callgraph = atOnlyVc.generate();
        }

        for (auto node : callgraph.getKeys()) {
            if ((ct == CallgraphInit) && (node->getName() == "main")) {
                LOG(1, "WARNING:  main() found in init callgraph!");
            }

            ctx.reachableFunctions.insert(node);
            auto n = callgraph[node];
            ctx.reachableFunctions.insert(n.begin(), n.end());
        }
        ctx.reachableCallgraph = callgraph;

	auto &extrasFound = vc.getExtraFuncsFound();
	ctx.discoverFuncsFound.insert(extrasFound.begin(), extrasFound.end());

        LOG(1, "grabbed!");
    }
    else if (fcgType == FCGType::NaiveFCG) {
        // extract call graph
        StaticFCGPass staticFCG;

	if (config.extraAsDiscover) {
	    for (auto &kv : config.discoverEntryPoints) {
		auto src = kv.first;
		auto targets = kv.second;
		LOG(1, "DISC:  Adding extra function " << src->getName()
		    << " with " << targets.size() << " targets");
		staticFCG.addExtraFuncs(src, targets);
	    }
	}

        setup.getConductor()->acceptInAllModules(&staticFCG, false);

        CoarseIndirectFCG ifcg(staticFCG.getCallgraph());

        ifcg.run(*setup.getConductor());
        callgraph = Callgraph(ifcg.getCallgraph());

	auto &extrasFound = staticFCG.getExtraFuncsFound();
	ctx.discoverFuncsFound.insert(extrasFound.begin(), extrasFound.end());

        ctx.reachableFunctions = startingFunctions;

        if (ct == CallgraphInit) { fixInitCallgraph(callgraph); }

        for (auto st : startingFunctions) {
            reachableSet(st, callgraph, ctx.reachableFunctions);
        }

        for (auto mlist : config.dlSymbols) {
            for (auto func : mlist) {
                reachableSet(func, callgraph, ctx.reachableFunctions);
            }
        }

        ctx.reachableCallgraph = callgraph;
    }
    else if (fcgType == FCGType::UniversalFCG) {
        std::set<Function *> functions = startingFunctions;
        for (auto module : CIter::children(program)) {
            for (auto function : CIter::children(module->getFunctionList())) {
                functions.insert(function);
            }
        }

	// Nothing to do for extra funcs here:
	// any added modules are already part of the program

        ctx.reachableFunctions = functions;
        ctx.reachableCallgraph = callgraph;
    }
    else if (fcgType == FCGType::DirectFCG) {
        // extract static call graph
        StaticFCGPass staticFCG;

	if (config.extraAsDiscover) {
	    for (auto &kv : config.discoverEntryPoints) {
		auto src = kv.first;
		auto targets = kv.second;
		LOG(1, "DISC:  Adding extra function " << src->getName()
		    << " with " << targets.size() << " targets");
		staticFCG.addExtraFuncs(src, targets);
	    }
	}

        setup.getConductor()->acceptInAllModules(&staticFCG, false);
        callgraph = staticFCG.getCallgraph();

	auto &extrasFound = staticFCG.getExtraFuncsFound();
	ctx.discoverFuncsFound.insert(extrasFound.begin(), extrasFound.end());

        if (ct == CallgraphInit) {
            fixInitCallgraph(callgraph);
        }

        for (auto st : startingFunctions) {
            reachableSet(st, callgraph, ctx.reachableFunctions);
        }

        for (auto mlist : config.dlSymbols) {
            for (auto func : mlist) {
                reachableSet(func, callgraph, ctx.reachableFunctions);
            }
        }
        ctx.reachableCallgraph = callgraph;
    }
    else {
        throw std::logic_error("Unexpected FCG type!");
    }
}

void Sysfilter::fixInitCallgraph(Callgraph &callgraph) {
    auto program = setup.getConductor()->getProgram();

    Function *fStart = CIter::named(getMainModule(program)->getFunctionList())
                           ->find("_start");

    Function *fMain
        = CIter::named(getMainModule(program)->getFunctionList())->find("main");

    if (!fStart) { LOG(9, "_start not found, not fixing init callgraph"); }
    if (!fMain) { LOG(9, "main not found, not fixing init callgraph"); }

    if (fStart && fMain) {
        LOG(1, "Removing edge from _start->main from init set");
        callgraph.removeEdge(fStart, fMain);
    }
}


void Sysfilter::buildTrackedFuncInfo() {
#define _reg(x) (X86Register::convertToPhysical(x))

    const std::map<std::string,
        std::vector<std::tuple<const char *, DlType, TrackableArgType, int>>>
        syms = {
            { "libdl.so.2",
                {
#ifdef ARCH_X86_64
                    { "dlsym", DlSym, TYPE_STRING, _reg(X86_REG_RSI) },
                    { "dlvsym", DlSym, TYPE_STRING, _reg(X86_REG_RSI) },
                    { "dlopen", DlSym, TYPE_STRING, _reg(X86_REG_RDI) },
                    // HACK: egalito and versioned symbols ...
                    { "dlopen@@GLIBC_2.2.5", DlOpen, TYPE_STRING,
                        _reg(X86_REG_RDI) },
                    { "dlmopen", DlOpen, TYPE_STRING, _reg(X86_REG_RSI) },
#else
#error "Arch unsupported"
#endif
                } },
            { "libc.so.6",
                {
#ifdef ARCH_X86_64
                    { "__libc_dlopen", DlOpen, TYPE_STRING, _reg(X86_REG_RDI) },
                    { "__libc_dlsym", DlSym, TYPE_STRING, _reg(X86_REG_RSI) },
                    { "execl", DlNone, TYPE_STRING, _reg(X86_REG_RDI) },
                    { "execlp", DlNone, TYPE_STRING, _reg(X86_REG_RDI) },
                    { "execle", DlNone, TYPE_STRING, _reg(X86_REG_RDI) },
                    { "execv", DlNone, TYPE_STRING, _reg(X86_REG_RDI) },
                    { "execvp", DlNone, TYPE_STRING, _reg(X86_REG_RDI) },
                    { "execvpe", DlNone, TYPE_STRING, _reg(X86_REG_RDI) },
                    { "execve", DlNone, TYPE_STRING, _reg(X86_REG_RDI) },
                    { "system", DlNone, TYPE_STRING, _reg(X86_REG_RDI) },

		      // Don't actually track this here, we just want it in the tracked function info
                      { "syscall",       DlNone, TYPE_NONE, _reg(X86_REG_RDI) },
		      { "fork",          DlNone, TYPE_USEONLY, 0 },
		      { "fork@@GLIBC_2.2.5",  DlNone, TYPE_USEONLY, 0 },
		      { "clone",         DlNone, TYPE_ADDRESS, _reg(X86_REG_RDI) },
		      { "unshare",         DlNone, TYPE_INTEGER, _reg(X86_REG_RDI) },
#else
#error "Arch unsupported"
#endif
		  }
	      },
	    { "libpthread.so.0",
	      {
		  { "pthread_create@@GLIBC_2.2.5", DlNone, TYPE_ADDRESS, _reg(X86_REG_RDX) },
	      },
	    },
	    { "(executable)",
	      {
		  { "__sysfilter_argtrack_test", DlNone, TYPE_INTEGER, _reg(X86_REG_RDI) },
	      },
	    },
    };

#undef _reg

    auto liblist = setup.getConductor()->getProgram()->getLibraryList();
    for (auto lib : CIter::children(liblist)) {
        auto it = syms.find(lib->getName());
        if (it == syms.end()) continue;
        auto flist = lib->getModule()->getFunctionList();

        for (auto s : it->second) {
            std::string name;
            DlType dltype;
            TrackableArgType argType;
            int reg;
            std::tie(name, dltype, argType, reg) = s;

            auto f = CIter::named(flist)->find(name);
            if (!f) continue;
            // funcs.insert(f);
            // funcArg[f] = reg;

	    std::vector<TrackedFuncInfo> &funcInfo = trackedFuncInfo[f];

	    TrackedFuncInfo info = {
		.name = name,
		.dltype = dltype,
		.argType = argType,
		.reg = reg,
	    };
	    funcInfo.push_back(info);
        }
    }
}

void Sysfilter::extractArg(AnalysisContext &ctx, DlAutoloadInfo &aInfo) {
    // collect list of dl*() functions that we're interested in
    std::set<Function *> funcs;
    // which argument we're interested in tracing for the function we're
    // interested in
    std::map<Function *, int> funcArg;

    auto &cg = ctx.reachableCallgraph;
    auto &edges = cg.getDirectEdges();

#if 0
    for(auto f : cg.getKeys()) {
        auto out = cg[f];
    }
#endif
    for (auto &kv : edges) {
        Function *f = kv.first;

        doArgTrack(ctx, aInfo, f);
    }

    for (auto f : cg.getImplicitTargets()) {
        if (cg.hasDirectEdge(f)) { continue; }

        doArgTrack(ctx, aInfo, f);
    }

    for(auto kv : trackedFuncInfo) {
	auto trackedFunc = kv.first;
	auto &funcInfos = kv.second;

	for (auto &funcInfo : funcInfos) {
	    if (cg.isImplicitTarget(trackedFunc)) {
		LOG(1, "WARNING:  Tracked function " << trackedFunc->getName()
		    << " is address-taken.  Argument tracking will be incomplete.");
		ctx.argTrackInfo.emplace_back(
		    ArgTrackInfo::buildTrackedImplicitFailure(trackedFunc, funcInfo.argType));
	    }
	}
    }
}

void Sysfilter::doArgTrack(
    AnalysisContext &ctx, DlAutoloadInfo &aInfo, Function *f) {
    auto &cg = ctx.reachableCallgraph;

    for(auto kv : trackedFuncInfo) {
	auto trackedFunc = kv.first;
	auto &funcInfos = kv.second;

	// Don't try to do tracking if the function is calling itself
	// (This can also appear in the callgraph due to aliasing)
	if (f == trackedFunc) {
	    continue;
	}

        auto &direct = cg.getDirectEdges();
        auto &out = direct[f];

        // if f contains any calls to trackedFunc, start tracking
        if (out.count(trackedFunc) == 0) { continue; }

	DataSymbolList *dsList = &getDataSymbolList();

	for (auto &thisFuncInfo : funcInfos) {
	    if (thisFuncInfo.argType == TYPE_NONE) {
		// Hack so we can add extra functions to trackedFuncInfo
		continue;
	    } else if (thisFuncInfo.argType == TYPE_STRING) {
		ArgTracker<std::string> rec(cg, dsList, f, trackedFunc,
					    thisFuncInfo.argType, thisFuncInfo.reg);
		rec.visit(f);
		ctx.argTrackInfo.emplace_back(rec.getResults());
	    } else if (thisFuncInfo.argType == TYPE_INTEGER) {
		ArgTracker<long int> rec(cg, dsList, f, trackedFunc,
					 thisFuncInfo.argType, thisFuncInfo.reg);
		rec.visit(f);
		ctx.argTrackInfo.emplace_back(rec.getResults());
	    } else if (thisFuncInfo.argType == TYPE_ADDRESS) {
		ArgTracker<AddressInfo> rec(cg, dsList, f, trackedFunc,
					    thisFuncInfo.argType, thisFuncInfo.reg);
		rec.visit(f);
		ctx.argTrackInfo.emplace_back(rec.getResults());
	    } else if (thisFuncInfo.argType == TYPE_USEONLY) {
		ArgTracker<long int> rec(cg, dsList, f, trackedFunc,
					 thisFuncInfo.argType, 0);
		//rec.visit(f);
		ctx.argTrackInfo.emplace_back(rec.getResults());
	    } else {
		throw std::logic_error("Invalid type!");
	    }
	}
    }
}

// the sysfilter code may be used as a library from elsewhere
// make sure we're actually compiling the real sysfilter before defining main()
#ifdef SYSFILTER
int main(int argc, char *argv[]) {
    Sysfilter sf;
    int ret = sf.parse(argc, argv);
    if (ret) return ret;

    ret = sf.run();
    return ret;
}
#endif
