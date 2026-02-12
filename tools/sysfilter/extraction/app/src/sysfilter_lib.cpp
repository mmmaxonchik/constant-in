#include "sysfilter_lib.h"
#include "syscall_recorder.h"
#include "sysfilter.h"

#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <malloc.h>

// RAII: redirects stdout/stderr to /dev/null for the lifetime of the object,
// then restores them.  Prevents tool-internal log messages from leaking to the
// caller's stdout/stderr.
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

extern "C" {

uint16_t *sysfilter_analyze(const char *binary_path,
                             const char *ld_paths,
                             const char *sysroot,
                             int        *count_out,
                             char      **error_out)
{
    *count_out = 0;
    *error_out = nullptr;

    try {
        SilentIO _silence;

        // Build argc/argv for the Sysfilter::parse() API.
        // We disable NSS resolution since library paths in container images
        // may not match the host, and use --library-path / --sysroot when supplied.
        std::vector<std::string> args_storage;
        args_storage.push_back("sysfilter_extract");
        args_storage.push_back("--disable-nss=true");
        if (sysroot && sysroot[0] != '\0') {
            args_storage.push_back(std::string("--sysroot=") + sysroot);
        }
        if (ld_paths && ld_paths[0] != '\0') {
            args_storage.push_back(std::string("--library-path=") + ld_paths);
        }
        args_storage.push_back(binary_path);

        std::vector<char *> argv;
        for (auto &s : args_storage)
            argv.push_back(const_cast<char *>(s.c_str()));
        int argc = static_cast<int>(argv.size());

        Sysfilter sf;
        int ret = sf.parse(argc, argv.data());
        if (ret != 0) {
            const std::string msg = "sysfilter parse() returned " +
                                    std::to_string(ret);
            *error_out = strdup(msg.c_str());
            return nullptr;
        }

        ret = sf.run();
        if (ret != 0) {
            const std::string msg = "sysfilter run() returned " +
                                    std::to_string(ret);
            *error_out = strdup(msg.c_str());
            return nullptr;
        }

        const std::set<int> &syscalls = sf.getSyscalls();
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
        *error_out = strdup(msg);
        return nullptr;
    } catch (...) {
        *error_out = strdup("unknown exception in sysfilter_analyze");
        return nullptr;
    }
}

void sysfilter_free_result(uint16_t *ptr)
{
    free(ptr);
    malloc_trim(0);
}

} // extern "C"
