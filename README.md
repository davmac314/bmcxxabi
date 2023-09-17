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

Specifically, BMCXXABI contains implementations of:
 * the `__cxa_*` routines to deal with exception handling (as documented by the ABI)
 * the `__gxx_personality_v0` C++ "personality" routine of GCC (mostly undocumented)
 * the `std::type_info` class and its ABI-private derived types (as documented by the ABI)
 
It does not currently include:
 * support for function static variable initialisation/destruction
 * support for dynamic_cast
 * implementations of `std::uncaught_exception()`, `std::current_exception()`,
   `std::rethrow_exception(...)`, `std::exception_ptr`.

This software is absolutely free and you may use it as you wish, without restriction.

## Key features

 * easy to build and to integrate with a bare-metal project
 * requires only a few key standard headers/functions
 * no restrictions on use or reproduction

## Use

 * typically you would incorporate the build of BMCXXABI into another project. There is little
   point building it stand-alone, since the necessary build options may differ between projects.
 * you might include BMCXXABI as a sub-repo or simply copy it into the parent project
 * use "make OUTDIR=..." to build and produce the resulting library, libcxxabi.a, in the specified
   directory
 * you can set the compiler and build options on the "make" command line, too; check the top-level
   Makefile for information. 

## Requirements

In reality, this library is only one piece of the puzzle. You also need:
 * A "libunwind" implementation. Checkout out bmunwind.
 * Some C++ headers and functions. BMCXXABI source needs headers such as `cstring` and `cstdlib`.
   You can use libbmcxx.
 * Importantly, an implementation of `malloc` and `free`. The Itanium C++ exception model requires
   dynamic allocation when an exception is thrown.
 * An implementation of `new` and `delete` operators, which should be trivial when you have
   malloc/free anyway, and which are provided by libbmcxx.
 * An implementation of `std::terminate` (also provided by libbmcxx).
 
This project, "bmunwind", and "libbmcxx" form a trio which together provide support for C++ in
"bare metal" environments. In theory, you can swap out any of these components with suitable
replacements.

## Background documentation

For details about the Itanium C++ ABI and the functions provided by BMCXXABI, see:
 * https://itanium-cxx-abi.github.io/cxx-abi/abi-eh.html
 * https://refspecs.linuxfoundation.org/cxxabi-1.86.html
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
doing here). 

## Current status

 * Only tested on / designed for x86-64 (in principle, various parts should be somewhat portable)
 * throwing/catching some obscure types might cause link errors (incomplete RTTI)
 * Does not support threads, assumes single-threaded application
 * Does not include support for "foreign" (i.e. non-C++) exceptions
 * Uses various GCC built-ins, should work fine with Clang
