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

#include <iostream>

#include "chunk/library.h"
#include "log/temp.h"

#include "constant_retriever.h"
#include "syscall_recorder.h"

#include "log.h"

void SyscallRecorder::run(Program *program) {
    if (usePresetFuncs || (functions.size() != 0)) {
        LOG(10, "Using pre-provided function set.");
    }
    else {
        LOG(10, "Building function set automatically.");
        buildFunctionSet(program);
    }

    // check for dietlibc presence
    const char *dietlibc_presence_symbol
        = "__you_tried_to_link_a_dietlibc_object_against_glibc";
    if (program->getMain()
        && CIter::named(program->getMain()->getFunctionList())
               ->find(dietlibc_presence_symbol)) {

        dietlibc_mode = true;
    }

    for (auto f : functions) {
        process(f);
    }
}

void SyscallRecorder::buildFunctionSet(Program *program) {
    built = true;
    class FunctionLister : public ChunkPass {
    public:
        std::set<Function *> functions;

        virtual void visit(Function *function) { functions.insert(function); }
    };

    FunctionLister fl;
    program->accept(&fl);
    functions = fl.functions;
}

void SyscallRecorder::process(Function *function) {
    LOG(10, "Visiting function " << function->getName());
    // skip the syscall() function inside libc, as it will definitely not have
    // a constant syscall set, and in fact we actually track calls to it as
    // equivalent to a syscall() instruction.
    if (isSyscallFunction(function)) return;
    // handle musl's __syscall_cp() the same way
    if (isSyscallcpFunction(function)) return;
    // handle musl's __setxid() the same way
    if (isSetxidFunction(function)) return;
    // __syscall_cp() and __setxid() wrap other functions that we no longer
    // need to consider
    if (isMuslIgnoreFunction(function)) return;
    // if this is an empty symbol (most likely stubbed out by parse overrides),
    // don't do anything
    if (function->getChildren()->genericGetSize() == 0) return;

    ControlFlowGraph graph(function);
    UDConfiguration config(&graph);
    UDRegMemWorkingSet working(function, &graph);
    UseDef usedef(&config, &working);

    SccOrder order(&graph);
    order.genFull(0);
    usedef.analyze(order.get());

    for (auto block : CIter::children(function)) {
        for (auto instr : CIter::children(block)) {
            auto assembly = instr->getSemantic()->getAssembly();

            ConstantRetriever cr;
            ConstantRetriever callCr; // Syscalls invoked using syscall()

            auto state = working.getState(instr);
#ifdef ARCH_X86_64
            if (assembly && assembly->getId() == X86_INS_SYSCALL) {
                cr.retrieve(state, X86Register::convertToPhysical(X86_REG_RAX));
            }
#elif defined(ARCH_RISCV)
            if (assembly && assembly->getId() == rv_op_ecall) {
                cr.retrieve(state, rv_ireg_a7);
            }
#endif
            else if (auto cfi = dynamic_cast<ControlFlowInstruction *>(
                         instr->getSemantic())) {
                Function *func_target;
                auto target = cfi->getLink()->getTarget();
                if (auto plt = dynamic_cast<PLTTrampoline *>(target)) {
                    func_target = dynamic_cast<Function *>(plt->getTarget());
                    // some PLT entry hasn't been resolved at this point.
                    // however, since we (presumably) are operating only
                    // on the subset of the callgraph that we care about,
                    // the user will already have been notified about this,
                    // or perhaps it's already on a whitelist somewhere.
                    continue;
                }
                else {
                    func_target = dynamic_cast<Function *>(target);
                }

                if (func_target && isSyscallFunction(func_target)) {
                    callCr.retrieve(
#ifdef ARCH_X86_64
                        state,
                        // dietlibc uses rax directly for syscall functions
                        dietlibc_mode
                            ? X86Register::convertToPhysical(X86_REG_RAX)
                            : X86Register::convertToPhysical(X86_REG_RDI));
#elif defined(ARCH_RISCV)
                        state, rv_ireg_a0);
#endif
                }
                else if (func_target && isSyscallcpFunction(func_target)) {
                    // for musl; syscall number is passed from %RDI to %RAX
                    callCr.retrieve(
                        state, X86Register::convertToPhysical(X86_REG_RDI));
                }
                else if (func_target && isSetxidFunction(func_target)) {
                    // for musl
                    callCr.retrieve(
                        state, X86Register::convertToPhysical(X86_REG_RDI));
                }
            }
            else {
                continue;
            }

            // If we failed to get the value for a syscall instruction, check
            // for any known workarounds
            if (!cr.allConstant()) {
                auto moduleName = function->getParent()->getParent()->getName();
                bool isStdLib = (moduleName == "module-libpthread.so.0"
                    || moduleName == "module-libc.so.6"
                    || moduleName == "module-libc.musl-x86_64.so.1");
                if (0) { }
#ifdef ARCH_X86_64
                else if (function->getName() == "read" && isStdLib)
                    cr.cs.insert(0);
                else if (function->getName() == "_int_free")
                    cr.cs.insert(0);
                else if (function->getName() == "__spawni_child")
                    cr.cs.insert(1);
                else if (function->getName() == "__stdio_read") {
                    // musl
                    cr.cs.insert(0);
                    cr.cs.insert(19);
                }
#elif defined(ARCH_RISCV)
#endif
                /* setXid means it can be any of: (or the 32 variant on
                 *  appropriate platforms)
                 *  - setgid
                 *  - setgroups
                 *  - setregid
                 *  - setresgid
                 *  - setresuid
                 *  - setreuid
                 *  - setuid
                 */
                else if (function->getName() == "__nptl_setxid"
                    || function->getName() == "sighandler_setxid") {

                    LOG(10, "call to __nptl_setxid found!");

#ifdef ARCH_X86_64
                    cr.cs.insert(106); // setgid
                    cr.cs.insert(116); // setgroups
                    cr.cs.insert(114); // setregid
                    cr.cs.insert(119); // setresgid
                    cr.cs.insert(117); // setresuid
                    cr.cs.insert(113); // setreuid
                    cr.cs.insert(105); // setuid
#elif defined(ARCH_RISCV)
                    cr.cs.insert(144); // setgid
                    cr.cs.insert(159); // setgroups
                    cr.cs.insert(143); // setregid
                    cr.cs.insert(149); // setresgid
                    cr.cs.insert(147); // setresuid
                    cr.cs.insert(145); // setreuid
                    cr.cs.insert(146); // setuid
#else
#error "Need __nptl_setxid list for current arch!"
#endif
                }
                else {
                    failures.insert(std::make_tuple(function, cr.getStatus()));
                    LOG(1,
                        "Couldn't resolve all possible syscall values in "
                        "function "
                            << function->getName());
                }
            }

            // If we fail to find a syscall number for the syscall()
            // instruction, log the failure
            if (!callCr.allConstant()) {
                LOG(1,
                    "Couldn't resolve all possible syscall values to "
                    "syscall() function called from  "
                        << function->getName());

	    failures.insert(std::make_tuple(function, callCr.getStatus()));
	}
            // for (auto value : cr.cs) {
            //     std::cerr << function->getName() << ":" << std::dec
            //               << value << std::endl;
            // }

            syscallFuncs[function].insert(cr.cs.begin(), cr.cs.end());
            syscallFuncs[function].insert(callCr.cs.begin(), callCr.cs.end());

            syscalls.insert(cr.cs.begin(), cr.cs.end());
            syscalls.insert(callCr.cs.begin(), callCr.cs.end());

            for (int nr : cr.cs) {
                SyscallInfo &info = getInfo(nr);
                info.rawCallers.insert(function);
            }

            for (int nr : callCr.cs) {
                SyscallInfo &info = getInfo(nr);
                info.funcCallers.insert(function);
            }

            if ((!cr.cs.empty()) || (!callCr.cs.empty())) {
                auto &site = siteInfo.info[function];
                site.rawCalls.insert(cr.cs.begin(), cr.cs.end());
                site.funcCalls.insert(callCr.cs.begin(), callCr.cs.end());
            }
        }
    }
#if 0
    std::cerr << "syscallFuncs:" << std::endl;
    for (auto value : syscallFuncs[function]) {
        std::cerr << function->getName() << ":" << std::dec
                              << value << std::endl;
    }
#endif
}

SyscallInfo &SyscallRecorder::getInfo(int nr) {
    if (callerInfo.count(nr) == 0) { callerInfo.emplace(nr, nr); }

    return callerInfo.at(nr);
}

bool SyscallRecorder::isSyscallFunction(Function *function) {
    if (function->getName() == "syscall") {
        auto module = static_cast<Module *>(function->getParent()->getParent());
        if (module->getLibrary()->getRole() == Library::ROLE_LIBC) return true;
        /* statically-linked executables have syscall() in the main executable.
         */
        else if (module->getLibrary()->getRole() == Library::ROLE_MAIN)
            return true;
    }
    /* dietlibc */
    else if (dietlibc_mode && function->getName() == "__unified_syscall") {
        return true;
    }
    return false;
}

bool SyscallRecorder::isSyscallcpFunction(Function *function) {
    if (function->getName() == "__syscall_cp") {
        auto module = static_cast<Module *>(function->getParent()->getParent());
        if (module->getLibrary()->getRole() == Library::ROLE_LIBC) return true;
        // XXX: double-check that __syscall_cp() will be in the main executable
        // when statically linked
        else if (module->getLibrary()->getRole() == Library::ROLE_MAIN)
            return true;
    }
    return false;
}

bool SyscallRecorder::isSetxidFunction(Function *function) {
    if (function->getName() == "__setxid") {
        auto module = static_cast<Module *>(function->getParent()->getParent());
        if (module->getLibrary()->getRole() == Library::ROLE_LIBC) return true;
        // XXX: double-check that __setxid() will be in the main executable when
        // statically linked
        else if (module->getLibrary()->getRole() == Library::ROLE_MAIN)
            return true;
    }
    return false;
}

bool SyscallRecorder::isMuslIgnoreFunction(Function *function) {
    auto name = function->getName();
    if (name == "__syscall_cp_c" || name == "__syscall_cp_asm"
        || name == "do_setxid") {
        auto module = static_cast<Module *>(function->getParent()->getParent());
        if (module->getLibrary()->getRole() == Library::ROLE_LIBC) return true;
        // XXX: double-check that these functions will be in the main executable
        // when statically linked
        else if (module->getLibrary()->getRole() == Library::ROLE_MAIN)
            return true;
    }
    return false;
}
