# `sysfilter`

This is the repository for the `sysfilter` project, which statically analyzes
binaries in order to automatically produce system call (syscall) filters
tailored to the subset of the syscall API used by programs.

The project is split into two components:
 - An extraction tool that identifies the set of
   syscalls used by a given program.
 - An enforcement tool that applies a policy of
   permissible syscalls to a given program, restricting its syscall API usage to   the set of allowed syscalls only.

Documentation and source code for each component is available at the respective
directories in the repository root. See the `README` in each directory for
details on how to build and run each tool.

Further information about the design and implementation of `sysfilter` can be
found in our [paper](doc/sysfilter_raid2020.pdf), presented at [RAID 2020](https://www.usenix.org/conference/raid2020/presentation/demarinis).

```
@inproceedings{sysfilter_raid2020,
	title		= {{sysfilter: Automated System Call Filtering for
				Commodity Software}},
	author		= {DeMarinis, Nicholas and Williams-King, Kent
				and Jin, Di and Fonseca, Rodrigo
				and Kemerlis, Vasileios P.},
	booktitle	= {International Symposium on Research in Attacks,
				Intrusions and Defenses (RAID)},
	pages		= {459--474},
	year		= {2020}
}
```

## Cloning

This repository uses several git submodules for dependencies of the extraction
tool. To clone this repository, you __must__ use SSH and
have SSH access configured for [Gitlab](https://gitlab.com) __and__ [Github](https://github.com).

Before cloning, please test your public keys by connecting to both services:
```
$ ssh git@gitlab.com
$ ssh git@github.com
```
If you receive a failure message for either service, check your
account settings and SSH keys to ensure you are connecting with a
public key attached to your account.

Once you are sure your public keys are configured, you can clone the
repository recursively with:

```
git clone --recursive git@gitlab.com:egalito/sysfilter
```

## License

This software uses the [BSD License](./COPYING).

---------


# Sysfilter extraction tool

The extraction directory contains the source code of `sysfilter`'s syscall extraction
tool. Given a target binary, it uses static analysis techniques to
over-approximate the program's function call graph (FCG) and from there
identify the set of syscalls invoked by the application. Output is provided
as a JSON file containing the set of syscalls invoked by the program. The
extraction tool is built using the [Egalito](https://gitlab.com/Egalito/egalito) framework.

For more details on the tool's operation, including the different
callgraph extraction methods, please consult our [paper](../doc/sysfilter_raid2020.pdf).


## Requirements

Compiling the extraction tool requires the following components:
 - A modern GNU toolchain (`gcc`, `g++`, `make`, _etc._) 
 - The GNU debugger (`gdb`)
 - Debug symbols for the GNU C library (`glibc`)
 - Development headers for GNU `readline` library

Our tool is built and tested primarily on Debian [`sid`](https://www.debian.org/releases/sid/).
We have also tested the tool on [Debian Buster](https://wiki.debian.org/DebianBuster) with good results.

On Debian `sid` (and Buster), you can install these dependencies as
follows:
```
sudo apt install build-essential gdb libc6-dbg libreadline-dev
```

On other distributions, the required package names may vary.


## Compilation

To compile the tool on `x86-64`, run the following from the
`extraction` directory of the repository:
```
$ make
```

Parallel builds are supported (_e.g.,_ `$ make -j$(nproc)`), and in fact highly
recommended due to the codebase size.


By default, the tool is built as a dynamically-linked binary; to build it as a
statically-linked binary, run `make` as follows:
```
$ STATIC=1 make
```

## Usage

The output executable is present in
`app/build_x86_64/sysfilter_extract`. Either run this binary directly
or copy it to your `$PATH`. (No additional files are needed.)

An example workflow would look like the following. For this example,
and all following, the working directory should be the `extraction`
directory of the repository.

```
$ tee test.c <<EOF
#include <stdio.h>
int main(void) {
    printf("Hello world!\n");
    return 0;
}
EOF

$ gcc -g test.c -o test
$ app/sysfilter_extract -o test.json test

$ cat test.json
[0,1,3,4,5,6,7,8,9,10,11,12,13,14,15,16,20,21,24,25,28,32,39,41,42,44,45,47,49,51,54,60,62,72,79,89,96,99,137,138,186,201,202,217,228,229,231,234,257,262,302]
```

Full usage information can be found by running `sysfilter_extract --help`.
Useful options are:

* `--output-file/-o <filename>`: File to output the list of detected possible
  system calls to. By default, `sysfilter_extract` prints to `stdout(3)`.
* `--dl-file <filename>`: Specify a file to use as a dynamically-loaded code
  specification. (See below for an example.)
* `--verbose/-v`: Be more verbose. Can be repeated multiple times for more
  information as needed.

## Finding symbols

The extraction tool leverages (debug) symbol information to extract a _tight_
over-approximation of the program's FCG. Where symbol information is not
available, `sysfilter` will try to use other available information to identify
function boundaries.

For the most precise analysis, we recommend building applications with
symbol information (_e.g.,_ build your code with `-g`). To extract
syscalls for binaries provided in your distribution, you will likely
need to install separate debug packages that provide symbol
information for the package you want to analyze,
and their dependencies.

On Debian, this involves adding a debug repository alongside the
standard repository configuration. More information can be found at
https://wiki.debian.org/HowToGetABacktrace

For Debian `sid`, this would involve adding the following to your
`/etc/apt/sources.list`:

```
deb http://deb.debian.org/debian-debug/ sid-debug main contrib non-free
```

Once you add this repository, you should see symbol packages for most
repository packages.

Mapping system binaries to packages and their corresponding symbol
packages is not a completely straightforward process. In general, on
Debian, a package `pkg` usually has a corresponding package named
`pkg-dbgsym` or `pkg-dbg` containing
its debug symbols. There are some exceptions to this rule.

We have provided a script `pkgs-with-missing-symbols.sh` (in the
`utils` directory) to help identify the packages associated with a
given binary. From there, you can search `apt` to find the
corresponding debug packages.

For example, for `fdisk`, we could run (from the `extraction` directory):
```
$ sudo apt-date update # Update the repository list, if you have not one so already
$ utils/pkgs-with-missing-symbols.sh /usr/sbin/fdisk
Packages requiring installation of debug packages:
 fdisk libfdisk1 libsmartcols1

 Depending on your distribution, debug packages usually take the form
 pkgname-dbgsym or pkgname-dbg. Exceptions may exist. Consult your
 package manager (eg. apt-cache search <pkgname>) or distribution
 documentation to find the package names.
```

This script finds all shared libraries used by `fdisk` and looks up
their corresponding packages. From here, we can search `apt` to find
the corresponding debug packages:

```
$ apt-cache search fdisk
fdisk - collection of partitioning utilities
libfdisk1 - fdisk partitioning library
fdisk-dbgsym - debug symbols for fdisk
libfdisk1-dbgsym - debug symbols for libfdisk1
. . .

$ apt-cache search libsmartcols1
libsmartcols1 - smart column output alignment library
libsmartcols1-dbgsym - debug symbols for libsmartcols1
```

Since we know the debug packages for each library, so we can
install each of them:
```
$ sudo apt install fdisk-dbgsym libfdisk1-dbgsym libsmartcols1-dbgsym
```

In some cases, the name of the debug package may not be immediately
obvious using this process, or may not be available in the
repository. For these cases, we recommend checking your
the packaging website of your distribution, such as [https://packages.debian.org](https://packages.debian.org)
for more information.

Note that Debian `sid` has the largest number of debug symbol
packages, covering most of the repository&mdash;other Debian versions or
distributions may not provide as complete coverage.

## Dynamically-loaded objects
```
    [{
        "path": "~/heap-page-stats.so",
        "symbols": ["malloc", "free", "calloc", "realloc"]
    }]
```

This file, when passed to the extraction tool via `--dl-file`, will cause the
tool to treat the file `~/heap-page-stats.so` as a dynamically-loaded
dependency, where the exported symbols `malloc`, `free`, `calloc`, and
`realloc` are imported by the program. Constructors and destructors
(as well as any additional initialization or finalization code) of the
given file will also be considered as imported, just as if it was specified in
the analyzed files' `.dynamic` section.

## Parse overrides

Rarely, the [Egalito](https://gitlab.com/Egalito/egalito) framework may
encounter issues disassembling certain hand-crafted assembly functions.
In these cases, the tool may crash and require a manual override to determine
function boundaries.

We have included a few parse overrides for libraries in Debian `sid` in
the `overrides` directory. You can invoke the tool to use the
override files by setting `EGALITO_PARSE_OVERRIDES` to a directory
with files containing one override per library. For example, to use
the provided parse overrides (from this directory):

```
EGALITO_PARSE_OVERRIDES=overrides app/sysfilter_extract -o test.json test
```

You can find examples of parse override files in the `overrides`
directory.

----------------


# Sysfilter enforcement tool

The enforcement directory contains the source code of `sysfilter`'s enforcement tool.
Given a target binary, the tool applies a syscall policy generated by the
extraction tool to restrict the respective program to the set of syscalls
specified by the policy. Filtering is performed using a [`seccomp-BPF`](https://www.kernel.org/doc/html/latest/userspace-api/seccomp_filter.html)
filter installed before the program starts executing.


## Requirements

Using the extraction tool requires the following applications:
 - A modern GNU toolchain (`gcc`, `ld`, _etc._)
 - `patchelf`
 - Python 3

We developed and tested the tool using Debian `sid` and have carried out
additional testing using [Debian Buster](https://wiki.debian.org/DebianBuster) with good results.

On Debian `sid` (and Buster), you can install these dependencies as
follows:
```
sudo apt install build-essential patchelf python3
```

The enforcement tool does not currently support _cross-architecture_
binaries.


## Usage

The enforcement tool can be invoked as follows:

```
sysfilter_enforce <binary> <policy> [policy ...]
```

where `binary` is the target application, and `policy` specifies at least
one syscall policy in JSON format (_i.e.,_ the output of the extraction tool).
An example workflow is shown below.

Other options include:
 - `--mode bin|linear`: Create filter using binary- or linear-search method
   for checking syscalls. The default is to use the binary search method.
 - `--spec-allow`: Disable speculative store bypass (SSB) mitigations
   (Spectre v4) by setting `SECCOMP_FILTER_FLAG_SPEC_ALLOW`. See
   [seccomp(2)](https://man7.org/linux/man-pages/man2/seccomp.2.html) for additional details.

__Note__: The instructions below assume that the working directory is the
current working directory, and that the extraction tool has already been
compiled successfully. For more details about the extraction tool,
please consult its [`README`](../extraction/README.md).

```
$ tee app.c <<EOF
#include <stdio.h>
int main(void) {
    printf("Hello world!\n");
    return 0;
}
EOF

$ gcc -g app.c -o app
$ ../extraction/app/sysfilter_extract -o app.json app

$ cat app.json
[0,1,3,4,5,6,7,8,9,10,11,12,13,14,15,16,20,21,24,25,28,32,39,41,42,44,45,47,49,51,54,60,62,72,79,89,96,99,137,138,186,201,202,217,228,229,231,234,257,262,302]

$ sysfilter_enforce app app.json
BINARY: app
POLICIES: app.json
FILTER: binary
BACKUP: ./.sysfilter/app.orig
$ ./app
Hello world!
```

The script will create a directory `.sysfilter` inside the current
directory containing a shared library with code that installs the
filter. A modified version of the target binary will be created
that installs the filter during process bootstrap. A backup copy of the
input binary will be made to the `.sysfilter` directory.

If the enforcement tool applies the filter successfully, the enforcement
library will be added to the binary:

```
$ ldd app
linux-vdso.so.1 (0x00007fff46919000)
/home/user/app/./.sysfilter/libsysfilter-app.so (0x00007f9788f1f000)
libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f9788d56000)
/lib64/ld-linux-x86-64.so.2	(0x00007f9788f2c000)
```
## Enforcing programs that invoke other programs

By design, the child of a process with an installed `seccomp` filter
inherits the filter from its parent. If the child process has its own
filter, that filter can only be __more__ restrictive than the syscall
specified by the parent process.

Thus, if you add a filter to a program that uses `execve(2)` or similar,
any programs invoked by it are restricted to the syscalls in the policy
applied to `bash`. Attempts to make other syscalls will fail. If this is
not the desired behavior, any other syscalls desired must be added to the
original policy applied to the parent process. See our [paper](../doc/sysfilter_raid2020.pdf) for more details.

## Additional policies

The extraction tool's policy will reflect the syscalls that are used
by code in the target application. However, additional syscalls may
be invoked by dynamic linker/loader and from code in the `vDSO`. In addition,
the syscall `restart_syscall(2)` (&#35;219) is invoked by the kernel to
restart a system call after interruption by a signal, but is never
invoked by userland code.

The policy `restart_syscall.json` in the `policies` directory contains
a policy to include `restart_syscall`. The syscalls used by the dynamic
linker/loader and the `vDSO` are system-dependent and can be determined
using the extraction tool, as follows.

__Note__: These instructions assume the working directory is the
current working directory, and that the extraction tool has already been
compiled successfully. For details regarding the extraction tool, please
consult its [`README`](../extraction/README.md).

```
# Extract and find syscall policy for vDSO
$ ../extraction/utils/extract_vdso.so vdso.so
$ ../extraction/app/sysfilter_extract --universal-fcg -o vdso.json vdso.so

# Extract and find policy for the loader
# (Note: your path to ld.so may vary based on your distribution)
$ ../extraction/app/sysfilter_extract -o ld.json /usr/lib/x86_64-linux-gnu/ld-2.31.so
```

Once policies for each of these components have been extracted,
multiple policies can be applied to a program as additional arguments
when invoking the tool:

```
$ ./sysfilter_enforce app app.json policies/restart_syscall.json ld.json vdso.json
```

## Frequently asked questions

### When I clone this directory, I get `Permission denied (pubkey)`.

The extraction tool uses several git submodules which are specified
via SSH, which requires you have active [Gitlab](https://gitlab.com)
__and__ [Github](https://github.com) accounts with valid public keys.
See the main [`README`](../README.md) for more details.

### The option '-v' produces a lot of output. How can I filter it?

Logging is controlled by the [Egalito](https://gitlab.com/Egalito/egalito)
framework. You can find more information about `Egalito`'s logging in its
[tutorial](https://gitlab.com/Egalito/egalito-docs/-/blob/master/tutorial.rst).