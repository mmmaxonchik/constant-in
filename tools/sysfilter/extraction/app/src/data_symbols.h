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

#ifndef SYSFILTER_DATA_SYMBOLS_H
#define SYSFILTER_DATA_SYMBOLS_H

#include <vector>

#include "callgraph.h"

#include "conductor/conductor.h"
#include "pass/chunkpass.h"

#include "log.h"

class DataSymbol {
public:
    address_t start;
    size_t size;
    std::set<DataSymbol *> dataReferences;
    std::set<Function *> codeReferences;
    bool gap = false;
    bool got = false;

    bool isInside(address_t addr) const {
        return addr >= start && addr < (start + size);
    }

#if 0
    std::string name;
#endif
};

class DataSymbolList {
private:
    std::map<address_t, DataSymbol *> dataSymbols;

public:
    void initialize(std::map<address_t, DataSymbol *> &dataSymbols) {
        this->dataSymbols = dataSymbols;
    }

    void generate(Program *program);

    DataSymbol *forAddress(address_t address);
    DataSymbol *forAddressMaybe(address_t address);

    address_t getHighestAddress();
};

#endif
