#include "syspart_lib.h"
#include "syspart.h"

#include "conductor/interface.h"
#include "conductor/setup.h"
#include "pass/resolveplt.h"
#include "pass/collapseplt.h"

#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <set>
#include <fcntl.h>
#include <unistd.h>
#include <malloc.h>

struct SilentIO {
    int sv_out, sv_err;
    SilentIO() {
        sv_out = dup(STDOUT_FILENO);
        sv_err = dup(STDERR_FILENO);
        int devnull = open("/dev/null", O_WRONLY);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        close(devnull);
    }
    ~SilentIO() {
        fflush(stdout);
        fflush(stderr);
        dup2(sv_out, STDOUT_FILENO);
        dup2(sv_err, STDERR_FILENO);
        close(sv_out);
        close(sv_err);
    }
};

// Force glibc to use only the main (brk-based) arena so that malloc_trim(0)
// can return all freed pages to the OS between analyses. Without this, glibc
// creates per-thread mmap arenas (128MB each) that malloc_trim cannot release.
__attribute__((constructor))
static void syspart_init_allocator() {
    mallopt(M_ARENA_MAX, 1);
}

extern "C" {

uint16_t *syspart_analyze(const char *binary_path,
                           const char *ld_paths,
                           const char *sysroot,
                           int         ic_analysis,
                           int        *count_out,
                           char      **error_out)
{
    *count_out = 0;
    *error_out = nullptr;

    try {
        Syspart sp;
        Syspart::SysNode *startNode = nullptr;

        // --- Phase 1: parse binary, build call graph, find direct syscalls ---
        // EgalitoInterface is scoped here so it is destroyed (and all egalito IR
        // freed) before the propagation phase runs.  This halves peak RSS for
        // large binaries (e.g. postgres + libpq + libssl + libcrypto).
        {
            SilentIO _silence;

            // Set library search paths before egalito initializes its ELF resolver.
            if (sysroot && sysroot[0] != '\0')
                setenv("EGALITO_SYSROOT", sysroot, 1);
            if (ld_paths && ld_paths[0] != '\0')
                setenv("EGALITO_LIBRARY_PATH", ld_paths, 1);

            EgalitoInterface egalito(/*verboseLogging=*/false,
                                      /*useLoggingEnvVar=*/true);
            egalito.initializeParsing();
            egalito.parse(binary_path, /*includeLibs=*/true);

            Program *prog = egalito.getProgram();
            if (!prog) {
                *error_out = strdup("egalito returned null Program");
                return nullptr;
            }

            ResolvePLTPass resolvePLT(egalito.getConductor());
            prog->accept(&resolvePLT);

            CollapsePLTPass collapsePLT(egalito.getConductor());
            prog->accept(&collapsePLT);

            sp.setConductorSetup(egalito.getSetup());
            sp.setProgram(prog);

            // Locate the entry-point function.  Try the ELF e_entry address first,
            // then fall back to well-known names.
            Function *startFunc = nullptr;
            address_t entryAddr = egalito.getSetup()->getEntryPoint();
            if (entryAddr)
                startFunc = sp.findFunctionByAddress(entryAddr);
            if (!startFunc)
                startFunc = sp.findFunctionByName("_start");
            if (!startFunc)
                startFunc = sp.findFunctionByName("main");
            if (!startFunc) {
                *error_out = strdup("could not locate entry-point function");
                return nullptr;
            }

            sp.setStartFunc(startFunc);
            startNode = sp.prepareForPropagation(ic_analysis != 0);

        } // EgalitoInterface destroyed here — Conductor/Program/Module/ElfSpace cascade freed

        // Return freed pages to the OS before the propagation phase allocates
        // its own working set (bitsets + post-order vector).
        malloc_trim(0);

        // --- Phase 2: propagate syscalls through the call graph ---
        // IPCallGraphNode graph and SysNode mapping are Syspart members — still alive.
        // No egalito IR is accessed from this point onward.
        std::set<int> syscalls = sp.finalizeSyscalls(startNode);
        if (syscalls.empty()) {
            *count_out = 0;
            return nullptr;
        }

        uint16_t *out = static_cast<uint16_t *>(
            malloc(syscalls.size() * sizeof(uint16_t)));
        if (!out) {
            *error_out = strdup("malloc failed");
            return nullptr;
        }
        int i = 0;
        for (int sc : syscalls)
            out[i++] = static_cast<uint16_t>(sc);
        *count_out = static_cast<int>(syscalls.size());
        return out;

    } catch (const std::exception &e) {
        *error_out = strdup(e.what());
        return nullptr;
    } catch (const char *msg) {
        *error_out = strdup(msg ? msg : "unknown char* exception");
        return nullptr;
    } catch (...) {
        *error_out = strdup("unknown exception in syspart_analyze");
        return nullptr;
    }
}

void syspart_free_result(uint16_t *ptr)
{
    free(ptr);
    malloc_trim(0);
}

} // extern "C"
