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

#include <cassert>

#include "chunk/dump.h"
#include "chunk/program.h"
#include "elf/elfspace.h"
#include "instr/linked-x86_64.h"

#include "data_symbols.h"

class SymbolLister : public ChunkPass {
public:
    std::map<address_t, DataSymbol *> dataSymbols;

public:
    virtual void visit(Module *module) {
        // LOG(1, "Generating data symbols for module " <<
        // module->getName());
        auto elfspace = module->getElfSpace();
        assert(elfspace);

        bool isLibc = module->getLibrary()
            ? (module->getLibrary()->getRole() == Library::ROLE_LIBC)
            : false;
        std::set<int> section_ignores;
        if (isLibc) {
            auto ignore = [&section_ignores, elfspace](const char *name) {
                auto section = elfspace->getElfMap()->findSection(name);
                if (section)
                    section_ignores.insert((int)section->getNdx());
                else
                    LOG(1, "Couldn't find section " << name << " in libc");
            };
            ignore("__libc_subfreeres");
            ignore("__libc_atexit");
            ignore("__libc_thread_subfreeres");
        }

        if (!elfspace->getSymbolList()) {
            LOG(1,
                "No symbols available for "
                    << module->getName()
                    << ", will use very large overapproximation.");
            recurse(module);
            return;
        }
        else {
            LOG(10, "Adding symbols from " << module->getName());
            for (auto symbol : *elfspace->getSymbolList()) {
                if (symbol->getType() != Symbol::TYPE_OBJECT) continue;

                /* Handle libc wonkiness */
                if (section_ignores.count(symbol->getSectionIndex())) continue;

                if (symbol->getSize() == 0) {
                    LOG(10, "Skipping zero-sized object symbol @"
                                << (symbol->getAddress() + module->getBaseAddress()));
                    continue;
                }

                DataSymbol *ds = new DataSymbol();
                ds->start = symbol->getAddress() + module->getBaseAddress();
                ds->size = symbol->getSize();

#if 0
		if (symbol->getName()) {
		    ds->name = symbol->getName();
		}
#endif

                LOG(10,
                    "    Adding DataSymbol ["
                        << ((symbol->getName()) ? symbol->getName() : "") << ","
                        << ds->start << "," << (ds->start + ds->size) << "]");
                dataSymbols[ds->start] = ds;
            }
        }
    }

    virtual void visit(DataSection *dataSection) {
        DataSymbol *ds = new DataSymbol();
        ds->start = dataSection->getAddress();
        ds->size = dataSection->getSize();

        if (ds->size == 0) {
            LOG(10, "Skipping zero-sized DataSection symbol @" << ds->start);
            delete ds;
            return;
        }

        if (dataSymbols.count(ds->start) > 0) {
            LOG(1,
                "    WARNING: Multiple symbols at " << ds->start << ". Will use larger size\n");
            if (dataSymbols[ds->start]->size < ds->size) {
                dataSymbols[ds->start]->size = ds->size;
            }
        }
        else {
            LOG(10,
                "    Adding DataSymbol from DataSection ["
                    << ds->start << "," << (ds->start + ds->size) << "]");
            dataSymbols[ds->start] = ds;
        }
    }
};

class VariableFinder : public ChunkPass {

public:
    DataSymbolList &dataSymbols;
    VariableFinder(DataSymbolList &dataSymbols) : dataSymbols(dataSymbols) { }

    virtual void visit(DataSection *section);
    virtual void visit(Module *module) { recurse(module); }
};

DataSymbol *DataSymbolList::forAddress(address_t address) {
    auto it = dataSymbols.upper_bound(address);

    if (it == dataSymbols.begin()) {
        LOG(1, "forAddress: address below first symbol: " << address);
        return nullptr;
    }

    if (it == dataSymbols.end()) {
        auto last = std::prev(dataSymbols.end());
        if (last->second && last->second->isInside(address)) {
            return last->second;
        }
        LOG(1, "forAddress: address not covered (end): " << address);
        return nullptr;
    }

    --it;
    if (!it->second || !it->second->isInside(address)) {
        LOG(1, "forAddress: not inside. address=" << address
               << " candidate=[" << it->first << ","
               << (it->second ? (it->second->start + it->second->size) : 0)
               << ")");
        return nullptr;
    }

    return it->second;
}

DataSymbol *DataSymbolList::forAddressMaybe(address_t address) {
    auto it = dataSymbols.upper_bound(address);

    if (it == dataSymbols.begin()) {
        LOG(1, "forAddressMaybe: address below first symbol: " << address);
        return nullptr;
    }

    if (it == dataSymbols.end()) {
        auto last = std::prev(dataSymbols.end());
        if (last->second && last->second->isInside(address)) {
            return last->second;
        }
        LOG(1, "forAddressMaybe: address not covered (end): " << address);
        return nullptr;
    }

    --it;
    if (!it->second || !it->second->isInside(address)) {
        LOG(1, "forAddressMaybe: not inside. address=" << address
               << " candidate=[" << it->first << ","
               << (it->second ? (it->second->start + it->second->size) : 0)
               << ")");
        return nullptr;
    }

    return it->second;
}

address_t DataSymbolList::getHighestAddress() {
    auto last = --dataSymbols.end();

    return last->second->start;
}

void DataSymbolList::generate(Program *program) {

    // make DataSymbol for each relevant symbol
    // XXX: ideally this should not be using elf information directly...

    SymbolLister sl;
    program->accept(&sl);

    // build symbols for GOT entries to prevent GOT pollution of callgraph
    for (auto module : CIter::children(program)) {
        auto elfspace = module->getElfSpace();
        auto elfmap = elfspace->getElfMap();

        auto got = elfmap->findSection(".got");
        if (!got) {
            LOG(1, "Warning: no GOT found for " << module->getName());
            continue;
        }

        address_t start = got->getVirtualAddress() + module->getBaseAddress();
        /* assumes all pointers are the same size. */
        for (unsigned i = 0; i < got->getSize(); i += sizeof(void *)) {
            DataSymbol *ds = new DataSymbol();
            ds->start = start + i;
            ds->size = 8;
            ds->got = true;
            sl.dataSymbols[ds->start] = ds;
        }

        auto pltgot = elfmap->findSection(".plt.got");
        if (!pltgot) continue;

        start = pltgot->getVirtualAddress() + module->getBaseAddress();
        /* assumes all pointers are the same size. */
        for (unsigned i = 0; i < pltgot->getSize(); i += sizeof(void *)) {
            DataSymbol *ds = new DataSymbol();
            ds->start = start + i;
            ds->size = 8;
            ds->got = true;
            sl.dataSymbols[ds->start] = ds;
        }
    }
    auto dataSymbols = sl.dataSymbols;

    // XXX: this is not a reasonable assumption for extremely minimal programs
    assert(dataSymbols.size() > 1);

    // explitictly iterate over old list, because we're modifying the new one
    auto it = sl.dataSymbols.begin();
    auto pit = it;
    it++;
    while (it != sl.dataSymbols.end()) {
        ssize_t delta
            = it->second->start - (pit->second->start + pit->second->size);

        if (delta < 0) {
            LOG(1,
                "Overlapping symbols @" << std::hex << it->second->start
                                        << " and @" << pit->second->start
                                        << " by delta -0x" << -delta);
            LOG(1, "it: [" << it->second->start << ",+" << it->second->size);
            LOG(1, "pit: [" << pit->second->start << ",+" << pit->second->size);

            // merge it into pit
            pit->second->size = std::max(pit->second->start + pit->second->size,
                                    it->second->start + it->second->size)
                - pit->second->start;
            auto oit = it;
            it = pit;
            sl.dataSymbols.erase(oit);
        }
        else {
            assert(delta >= 0);

            if (delta > 0) {
                DataSymbol *gap = new DataSymbol();
                gap->start = it->second->start - delta;
                gap->size = delta;
                gap->gap = true;
                dataSymbols[gap->start] = gap;
            }
        }

        pit = it;
        ++it;
    }

    // extra DataSymbols to cover the entire address range...
    {
        DataSymbol *ds = new DataSymbol();
        ds->start = 0;
        ds->size = dataSymbols.begin()->second->start;
        ds->gap = true;

        dataSymbols[ds->start] = ds;
    }
    {
        DataSymbol *ds = new DataSymbol();
        ds->start = dataSymbols.rbegin()->second->start
            + dataSymbols.rbegin()->second->size;
        ds->size = (1ull << 48) - ds->start;
        ds->gap = true;

        dataSymbols[ds->start] = ds;
    }

    initialize(dataSymbols);

    VariableFinder vf(*this);
    program->accept(&vf);
}

void VariableFinder::visit(DataSection *section) {
    for (auto variable : CIter::children(section)) {
        auto begin = variable->getPosition()->get();
        auto last = begin + variable->getSize() - 1;

        auto container = dataSymbols.forAddress(begin);
        if (!container) {
            LOG(1, "VariableFinder: no container for var range [" << begin << "," << last << "]");
            continue;
        }

        // sanity check (без assert’ов, чтобы не валить весь анализ)
        if (!container->isInside(begin) || !container->isInside(last)) {
            LOG(1, "VariableFinder: container does not cover var range. "
                << "container=[" << container->start << ","
                << (container->start + container->size) << ") "
                << "var=[" << begin << "," << last << "]");
            continue;
        }
        IF_LOG(11) if (container->gap) {
            LOG(11, "in gap");
            LOG(11,
                "\tcontainer bounds: [" << container->start << ","
                                        << container->start + container->size
                                        << ")");
            LOG(11, "\tvariable bounds: [" << begin << "," << last << "]");
        }

        auto link = variable->getDest();
        if (!link) { continue; }
        assert(link);
        if (/*auto mlink =*/dynamic_cast<MarkerLink *>(link)) {
            LOG(10, "Ignoring marker link for now.");
        }
        else if (auto function = dynamic_cast<Function *>(link->getTarget())) {
            container->codeReferences.insert(function);
        }
#if 0
	else if (auto block
		 = dynamic_cast<Block *>(link->getTarget())) {
	    container->codeReferences.insert(
		static_cast<Function *>(block->getParent()));
	}
	else if (auto instr
		 = dynamic_cast<Instruction *>(link->getTarget())) {
	    container->codeReferences.insert(static_cast<Function *>(
						 instr->getParent()->getParent()));
	}
#endif
        else if (auto region = dynamic_cast<DataRegion *>(link->getTarget())) {

            // for now, only know how to handle TLS region.
            assert(region->getName() == "region-TLS");
            // XXX: going to ignore TLS for now
        }
        else {
            auto targetSymbol
                = dataSymbols.forAddress(link->getTargetAddress());
            container->dataReferences.insert(targetSymbol);
        }
    }
}
