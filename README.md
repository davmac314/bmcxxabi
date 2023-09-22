# BMCXXABI: Bare Metal C++ ABI

by Davin McCall - <davmac@davmac.org>

**Work-in-progress! Only for x86-64 at the moment! Not complete!**
_May be usable and useful in some scenarios; see "current status" below)._

This is an implementation of the support routines specified in the Itanium C++ ABI, designed for
use in "bare metal" applications - such as kernel code, or UEFI applications - when using GCC or
a compatible compiler with "dwarf exception handling" (eg GCC on Linux). Using BMCXXABI you can
write code which throws and catches exceptions in such an application.

Note that the "Itanium" C++ ABI is also used (at least as baseline) for other processor
architectures.

## Key features

 * Easy to build and to integrate with a bare-metal project
 * Requires only a few key standard headers/functions
 * No significant restrictions on use, distribution or modification - no attribution required, and
   you redistribute the code under another license.

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
appropriate symbols; for the GNU linker this can be done via the following link script commands:

    .init_array : {
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP (*(.init_array .ctors))
        PROVIDE_HIDDEN (__init_array_end = .);
    }
    
    .fini_array : {
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP (*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
        KEEP (*(.fini_array .dtors))
        PROVIDE_HIDDEN (__fini_array_end = .);
    }

To build without support for running static storage destructors (eg for a kernel that will never
terminate), build with the `BMCXX_NO_SSD` macro defined (eg via `-DBMCXX_NO_SSD=1`), and do not
call the `bmcxxabi_run_destructors` function.


## Requirements

In reality, this library is only one piece of the puzzle. You also need:
 * A "libunwind" implementation, including "libunwind.h" header. Check out bmunwind.
 * Some C++ headers and functions. BMCXXABI source needs (at least basic versions of) the
   following headers:
   - `cstring` (including `memcpy`)
   - `cstdlib` (including `malloc`/`free`)
   - `cstdint`
   - `cstddef`
   - `exception` (for `std::terminate()`)
   All of these are provided by libbmcxx, or you can provide your own (or any other)
   implementation. Note that `cstring`, `cstdlib`, `cstdint` and `cstddef` are wrappers around
   similarly-named C headers.
 * Importantly, an implementation of `malloc` and `free`. The Itanium C++ exception model requires
   dynamic allocation when an exception is thrown.
 * An implementation of `std::terminate` (also provided by libbmcxx).
 
This project, "bmunwind", and "libbmcxx" form a trio which together provide support for C++ in
"bare metal" environments. In theory, you can swap out any of these components with suitable
replacements.


## Background documentation

For details about the Itanium C++ ABI and the functions provided by BMCXXABI, see:
 * https://itanium-cxx-abi.github.io/cxx-abi/abi.html
 * https://itanium-cxx-abi.github.io/cxx-abi/abi-eh.html
 * https://github.com/itanium-cxx-abi/cxx-abi/ (more up-to-date source of above two)
 * https://gitlab.com/x86-psABIs/x86-64-ABI/

Also included is an implementation of `__gxx_personality_v0`, the exception-handling "personality"
routine as implemented by GCC (and LLVM's clang++) when configured for so-called "DWARF exception handling"
(which isn't actually part of the DWARF specification at all). This is the standard exception handling
mechanism used on Linux, for instance.

See:
 * https://www.airs.com/blog/archives/460
 * https://www.airs.com/blog/archives/462
 * https://www.airs.com/blog/archives/464

(The first two are about unwinding more generally, the last link above is more specific to what we are
doing here). Also see comments in "personality.cc" (`__gxx_personality_v0`).


## Current status

 * Only tested on / designed for x86-64 (in principle, various parts should be somewhat portable to
   other processor architectures)
 * Support for exceptions is essentially complete
   - some edge cases are not supported: setting the termination handler or unexpected exception
     handler is not possible
 * Does not support threads, assumes single-threaded application. Known issues for thread support
   are highlighted in the code via a `// THREAD_SAFETY` comment.
 * Does not yet include dynamic array creation helpers; using eg. "new int[10]" may cause link-
   time errors (`__cxa_vec_new` etc).
 * Does not yet include support for destruction of static-storage variables; global variables of
   class type with non-trival destructors may cause link-time errors (`__dso_handle`,
   `__cxa_atexit`).
 * Does not include support for any handling of "foreign" (i.e. non-C++) exceptions
 * Uses various GCC built-ins, should work fine with Clang
