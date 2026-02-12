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

#ifndef SYSFILTER_ARG_TRACKER_H
#define SYSFILTER_ARG_TRACKER_H

#include <deque>
#include <functional>
#include <memory>
#include <set>

#include "callgraph.h"
#include "data_symbols.h"
#include "chunk/dump.h"
#include "conductor/conductor.h"
#include "constant_retriever.h"
#include "json.hpp"
#include "log/registry.h"
#include "log/temp.h"
#include "pass/chunkpass.h"

enum TrackableArgType {
    TYPE_STRING = 0,
    TYPE_INTEGER = 1,
    TYPE_NONE = 2,
    TYPE_ADDRESS = 3,
    TYPE_USEONLY = 4,
};

#define ARG_TRACK_JSON_VERSION 4

struct ArgTrackValue {
    TrackableArgType argType;
    Instruction *instr;

    RegTrackingStatus status;
    long intValue;
    std::string strValue;

    //bool isAddr = false;
    Function *funcFromValue;
    std::string funcValue; // For address resolution

    //const static std::string ResolveFailed = "<failed>";
    ArgTrackValue(TrackableArgType argType, Instruction *instr,
		  RegTrackingStatus status) :
    	argType(argType), instr(instr), status(status), intValue(0),
	funcFromValue(nullptr) {}

    ArgTrackValue(TrackableArgType argType, Instruction *instr,
		  RegTrackingStatus status, std::string &s) :
	argType(argType), instr(instr), status(status), intValue(0), strValue(s),
	funcFromValue(nullptr) {}

    ArgTrackValue(TrackableArgType argType, Instruction *instr,
		  RegTrackingStatus status, long l) :
    	argType(argType), instr(instr), status(status), intValue(l),
	funcFromValue(nullptr) {}

    ArgTrackValue(TrackableArgType argType, Instruction *instr,
		  RegTrackingStatus status, AddressInfo ai) :
    	argType(argType), instr(instr), status(status) {
	intValue = ai.address;
	funcFromValue = ai.function;
	//isAddr = true;
    }

};

struct ArgTrackInfo {
    // Instruction *call;
    Function *in_function;
    Function *target;
    TrackableArgType argType;
    int targetReg;

    bool func_is_implicit_target;
    bool func_has_direct_edge;

    std::map<Instruction *, std::vector<ArgTrackValue>> values;

    void writeArgTrackInfo(nlohmann::json &arr);
    static ArgTrackInfo buildTrackedImplicitFailure(
        Function *trackedFunc, TrackableArgType argType);
};

template <typename T>
class ArgTracker : public ChunkPass {
private:
    Callgraph &callgraph;
    DataSymbolList *dataSymbols;
    Function *callingFunc;
    Function *target;
    TrackableArgType argType;
    int targetReg;
    std::map<Instruction *, std::vector<ArgTrackValue>> values;

    std::set<Function *> funcsSeen;

    std::deque<Instruction *> callStack;
    bool foundSomeCall = false;

    void considerCallers(Function *callee, int reg);

public:
    ArgTracker(Callgraph &callgraph,
	       DataSymbolList *dataSymbols,
	       Function *callingFunc,
	       Function *target,
	       TrackableArgType argType, int reg)
	: callgraph(callgraph), dataSymbols(dataSymbols), callingFunc(callingFunc),
	  target(target), argType(argType), targetReg(reg) {}

    virtual ~ArgTracker() {};
    virtual void visit(Function *function);
    virtual void visit(Function *function, Function *targetFunc, int reg);

    const std::map<Instruction *, std::vector<ArgTrackValue>> &
    getValues() const {
        return values;
    }
    ArgTrackInfo getResults();
};

#endif
