/*-
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

#ifndef SYSFILTER_CONSTANT_RETRIEVER_H
#define SYSFILTER_CONSTANT_RETRIEVER_H

#include <set>
#include <vector>

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
#include "data_symbols.h"

#include "log.h"

enum RegTrackingStatus {
    RT_OK = 0,
    RT_NO_REF_STATE = (1 << 0),
    RT_UNKNOWN_OP = (1 << 1),
    RT_BAD_STRING = (1 << 2),
    RT_NOT_ALL_CONSTANTS = (1 << 3),
    RT_NO_CALL = (1 << 4),
    RT_ASSERT = (1 << 5),

    // Used if function being tracked is AT
    // This value is set ONCE per binary by ArgTracker, so
    // ConstantRetriever should not assign it
    RT_TARGET_IS_AT = (1 << 6),
    RT_ADDR_RESOLUTION_FAILED = (1 << 7),
    RT_ADDR_SYMBOL_RESOLUTION_FAILED = (1 << 8),
    RT_NO_ARITH = (1 << 9),
};

struct AddressInfo {
public:
    address_t address;
    Function *function;

    AddressInfo(address_t address, Function *function) :
	address(address), function(function) {}

    bool operator<(const AddressInfo &other) const {
	return address < other.address;
    }
};

template <typename T>
class RegisterRetriever {
protected:
    std::vector<int> regsSearched;
    RegTrackingStatus status = RT_OK;

    virtual void addStatusFlag(RegTrackingStatus flag) {
        status = static_cast<RegTrackingStatus>(status | flag);
    }

public:
    RegisterRetriever() { }
    virtual ~RegisterRetriever() { }

    virtual void retrieve(UDState *state, int curreg) = 0;
    virtual bool allConstant() = 0;
    virtual RegTrackingStatus getStatus() {
        if (!allConstant()) { addStatusFlag(RT_NOT_ALL_CONSTANTS); }
        return status;
    }

    virtual const std::set<T> &getValues() const = 0;

    virtual const std::vector<int> &getRegsSearched() const {
        return regsSearched;
    }
};

template <typename T>
class ConcreteRegisterRetriever : public RegisterRetriever<T> {
private:
    std::set<T> emptySet;

public:
    virtual void retrieve(UDState *state, int curreg) {
        throw std::logic_error("Invalid base!");
    }
    virtual const std::set<T> &getValues() { return emptySet; }
};

template <>
class ConcreteRegisterRetriever<long int> : public RegisterRetriever<long int> {
private:
    bool all_constants = true;
    std::set<UDState *> seen;

public:
    ConcreteRegisterRetriever<long int>() {}
    ConcreteRegisterRetriever<long int>(Program *program) {}
    ConcreteRegisterRetriever<long int>(Program *program, DataSymbolList *dsList) {}

    std::set<long int> cs;
    void notConstant() { all_constants = false; };
    bool allConstant() { return all_constants; };
    void retrieve(UDState *state, int curreg);

    const std::set<long int> &getValues() const { return cs; }
};

typedef ConcreteRegisterRetriever<long int> ConstantRetriever;

template <>
class ConcreteRegisterRetriever<std::string>
    : public RegisterRetriever<std::string> {
private:
    bool all_constants = true;
    Program *program;
    std::set<UDState *> seen;
    std::set<std::string> values;

    const char *readStringAtAddress(address_t address);

public:
    ConcreteRegisterRetriever<std::string>(Program *program, DataSymbolList *dsList)
	: program(program) {}

    void notConstant() { all_constants = false; };
    bool allConstant() { return all_constants; };
    void retrieve(UDState *state, int curreg);

    const std::set<std::string> &getValues() const { return values; }
};

typedef ConcreteRegisterRetriever<std::string> ATStringRetriever;


template <>
class ConcreteRegisterRetriever <AddressInfo> : public RegisterRetriever<AddressInfo> {
private:
    bool all_constants = true;
    std::set<UDState *> seen;
    Program *program;
    DataSymbolList *dsList;

public:
    ConcreteRegisterRetriever<AddressInfo>() {}
    ConcreteRegisterRetriever<AddressInfo>(Program *program) {}
    ConcreteRegisterRetriever<AddressInfo>(Program *program, DataSymbolList *dsList)
	: program(program), dsList(dsList) {}

    std::set<AddressInfo> cs;
    void notConstant() { all_constants = false; };
    bool allConstant() { return all_constants; };
    void retrieve(UDState *state, int curreg);

    const std::set<AddressInfo> &getValues() const { return cs; }
private:
    bool findFuncsInDataSymbol(address_t searchAddr, std::set<Function *> &funcs);
};

typedef ConcreteRegisterRetriever<AddressInfo> AddressRetriever;


#endif
