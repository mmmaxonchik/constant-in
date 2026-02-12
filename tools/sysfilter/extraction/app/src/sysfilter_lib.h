#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Analyze a binary and return the set of syscall numbers it may invoke.
 *
 * binary_path  - path to the ELF binary to analyze
 * ld_paths     - colon-separated extra library search directories, may be NULL
 * sysroot      - filesystem root for resolving shared libraries (sets
 *               EGALITO_SYSROOT); use when analyzing binaries extracted from
 *               a container image, e.g. "/tmp/nginx".  May be NULL.
 * count_out    - set to the number of syscalls returned
 * error_out    - on error, set to a malloc'd error string (caller must free);
 *               on success, set to NULL
 *
 * Returns a malloc'd array of syscall numbers (caller must free with
 * sysfilter_free_result), or NULL on error.
 */
uint16_t *sysfilter_analyze(const char *binary_path,
                             const char *ld_paths,
                             const char *sysroot,
                             int        *count_out,
                             char      **error_out);

void sysfilter_free_result(uint16_t *ptr);

#ifdef __cplusplus
}
#endif
