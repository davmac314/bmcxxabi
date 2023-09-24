# BMCXXABI: Bare Metal C++ ABI

by Davin McCall - <davmac@davmac.org>

**BMCXXABI**: _C++ ABI routines for x86-64 baremetal applications using GCC-compatible compiler._

This is an implementation of the support routines specified in the Itanium C++ ABI, designed for
use in "bare metal" applications - such as kernel code, or UEFI applications - when using GCC or
a compatible compiler with "dwarf exception handling" (eg GCC on Linux) for the x86-64 (AMD64)
architecture. Using BMCXXABI you can write code which throws and catches exceptions in such an
application, and use various other C++ language features that require runtime support.

Note that the "Itanium" C++ ABI is also used (at least as baseline) for other processor
architectures.

## Key features

 * Easy to build and to integrate with a bare-metal project
 * Requires only a few key standard headers/functions to be provided
 * No significant restrictions on use, distribution or modification - no attribution required, and
   you can redistribute the code under the license of your choice.

## Features in detail

Specifically, BMCXXABI contains implementations of:
 * The `__cxa_*` routines to deal with exception handling (as documented by the ABI)
 * The `__gxx_personality_v0` C++ exception-handling "personality" routine of GCC (mostly
   undocumented)
 * The `std::type_info` class and its ABI-private derived types (as documented by the ABI)
 * Various other (non-exception-related) `__cxa_*` support routines that the compiler may
   generate calls to (eg. guards for static initialisation, registration of destructors for
   static-storage objects).
 * Helper routines to run linker-defined static-storage constructors and destructors
   (initialisers and finalisers).
 
It does not currently (and there are no current plans to) include:
 * Support for `dynamic_cast`
 * Implementations of `std::uncaught_exception()`, `std::current_exception()`,
   `std::rethrow_exception(...)`, `std::exception_ptr`.
 * The demangling API (`__cxa_demangle`).

Implementations of the above, of suitable quality, would be welcome as contributions. 

This software is absolutely free and you may use it as you wish, without restriction.

Feature requests are not accepted.


## Use

 * Typically you would incorporate the build of BMCXXABI into another project. There is little
   point building it stand-alone, since the necessary build options may differ between projects.
 * You might include BMCXXABI as a sub-repo or simply copy it into the parent project
 * Use "make OUTDIR=..." to build and produce the resulting library, libcxxabi.a, in the specified
   directory.
 * You can set the compiler and build options on the "make" command line, too; check the top-level
   Makefile for information. Typically you should set the include path so that the build can find
   the correct versions of the required headers.
 * The "include" directory should be added to your project's include path (or the contents copied
   to it). It contains the `<typeinfo>` header.
 * To run static-storage initialisers/constructors, `bmcxxabi_run_init()` should be called at
   startup (usually, as early as possible). To run destructors at exit, call
   `bmcxxabi_run_destructors()`.

The `bmcxxabi_run_init`/`bmcxxabi_run_destructors` functions can be declared as follows:

    extern "C" void bmcxxabi_run_init();
    extern "C" void bmcxxabi_run_destructors();

Additionally, for static storage construction/destruction support, the linker must generate
appropriate symbols; for the GNU linker this can be done via the following link script fragment:

    .init_array : {
        PROVIDE (__init_array_start = .);
        KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP (*(.init_array .ctors))
        PROVIDE (__init_array_end = .);
    }
    
    .fini_array : {
        PROVIDE (__fini_array_start = .);
        KEEP (*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
        KEEP (*(.fini_array .dtors))
        PROVIDE (__fini_array_end = .);
    }

(The output section names are generally not important; it is the `_start` and `_end` symbols which
the runtime will use to locate the arrays). 

To build without support for running static storage destructors (eg for a kernel that will never
terminate), build with the `BMCXX_NO_SSD` macro defined (eg via `-DBMCXX_NO_SSD=1`), and do not
call the `bmcxxabi_run_destructors` function.

For exceptions support, you should use `--eh-frame-hdr` on the `ld` command line when linking, and
additionally need something like the following in your linker script:

    .eh_frame_hdr : {
        PROVIDE (__eh_frame_hdr_start = .);
        KEEP(*(.eh_frame_hdr .eh_frame_hdr.*))
        PROVIDE (__eh_frame_hdr_end = .);
    }

    .eh_frame : {
        PROVIDE (__eh_frame_start = .);
        KEEP(*(.eh_frame .eh_frame.*))
        PROVIDE (__eh_frame_end = .);
        LONG (0);
    }
    
    .gcc_except_table   : { *(.gcc_except_table .gcc_except_table.*) }

In this case some of the output section names are important: binutils' `ld` will seemingly not
honour `--eh-frame-hdr` if it cannot see an output section named `.eh_frame`, and the section it
generates will always be put in `.eh_frame_hdr`.

(Strictly speaking, the use of `--eh-frame-hdr` and the inclusion of the `.eh_frame_hdr` section
that it generates are not required, if the libunwind implementation can operate without that
section; the `.eh_frame_hdr` section is an optimisation, providing a sorted table of entries in
the `.eh_frame` sections, so that it can be binary-searched).


## Requirements

In reality, this library is only one piece of the puzzle. You also need:
 * A "libunwind" implementation, including "libunwind.h" header. Check out [bmunwind](https://github.com/davmac314/bmunwind).
 * Some C++ headers and functions. BMCXXABI source needs (at least basic versions of) the
   following headers:
   - `cstring` (including `memcpy`)
   - `cstdlib` (including `malloc`/`free`)
   - `cstdint`
   - `cstddef`
   - `exception` (for `std::terminate()`)
   All of these are provided by [libbmcxx](https://github.com/davmac314/libbmcxx), or you can
   provide your own (or another) implementation. Note that `cstring`, `cstdlib`, `cstdint` and
   `cstddef` are wrappers around similarly-named C headers.
 * Importantly, an implementation of `malloc` and `free`. The Itanium C++ exception model requires
   dynamic allocation when an exception is thrown.
 * An implementation of `std::terminate` (also provided by libbmcxx).
 
This project, "bmunwind", and "libbmcxx" form a trio which together provide support for C++ in
"bare metal" environments. In theory, you can swap out any of these components with suitable
replacements.


## Background documentation

The x86-64 processor-specific supplement to the Sys V ABI details a stack unwinding mechanism
and associated support routines (`_Unwind_XXX`) that can be used to throw and handle exceptions
(amongst other uses). In essence, the ABI dictates the format of "unwind tables" specify how to
"unwind" through stack frames for each function in the program; these tables also contain, for
each function, a pointer to a "personality routine" which can handle language-specific details of
exception catching or cleanup for stack frames corresponding to the function.

At the simplest level, unwinding a stack frame is just a matter of loading saved register values
from the stack (the registers and locations are encoded in the unwind tables). For C++, exceptions
can also be caught, which involves transferring execution to a "landing pad", and if not caught,
destructors may need to be run (handled via another landing pad) before unwinding continues. 

The `_Unwind` API and unwind table format are specified in the psABI document:

 * https://gitlab.com/x86-psABIs/x86-64-ABI/

However, the specific details of the personality routine are largely undocumented. GCC (and
compatible compilers such as Clang) generates unwind tables referring to a personality routine
called `__gxx_personality_v0`. The personality routine uses the "language specific" data, also
stored in the unwind tables, to determine whether an exception is caught, and if not, whether
any cleanup is required, for each frame that is unwound. In either case it arranges for execution
to continue at the appropriate landing pad, with certain specific registers set to values which
the landing pad uses to determine which exception handler (or cleanup) to jump to.

The compiler does not generate calls to the personality routine, but does generate the unwind
tables which reference it.

In addition, the C++ ABI defines various helper functions (`__cxa_XXX`) for dealing with
exceptions (among other things). In fact, of the various `_Unwind_` functions, the compiler will
generally only generate calls to `_Unwind_Resume` directly; it otherwise uses the `__cxa_`
functions. It is these `__cxa_` functions, and the `__gxx_personality_v0` routine, that BMCXXABI
provides.

For details about the Itanium C++ ABI and the functions provided by BMCXXABI, see:
 * https://itanium-cxx-abi.github.io/cxx-abi/abi.html
 * https://itanium-cxx-abi.github.io/cxx-abi/abi-eh.html
 * https://github.com/itanium-cxx-abi/cxx-abi/ (more up-to-date source of above two)

For details of the `__gxx_personality_v0` implementation, see:

 * https://www.airs.com/blog/archives/460
 * https://www.airs.com/blog/archives/462
 * https://www.airs.com/blog/archives/464

Also see comments in "personality.cc" (`__gxx_personality_v0`).


## Current status

 * Only tested on / designed for x86-64 (in principle, various parts should be somewhat portable to
   other processor architectures)
 * Support for exceptions is essentially complete
   - some edge cases are not supported: setting the termination handler or unexpected exception
     handler is not possible
 * Does not support threads, assumes single-threaded application. Known issues for thread support
   are highlighted in the code via a `// THREAD_SAFETY` comment.
 * Does not yet include dynamic array creation helpers; using eg. "new int[10]" may cause link-
   time errors (`__cxa_vec_new` etc). In practice it seems rare for the compiler to generate calls
   to these anyway.
 * Does not include implementations of `__cxa_get_globals` / `__cxa_get_globals_fast`. Calls to
   these are not (AFAIK) generated by the compiler.
 * Does not include support for any handling of "foreign" (i.e. non-C++) exceptions
 * Uses various GCC built-ins, should work fine with Clang
