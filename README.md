# BMCXXABI: Bare Metal C++ ABI

by D. Mcall - <davmac@davmac.org>

*Work-in-progress! Only for x86-64 at the moment! NOT complete!*

This is an implementation of the support routines specified in the Itanium C++ ABI, specifically
the `__cxa_*` routines to deal with exception handling, designed for use in "bare metal" situations -
such as kernel code, or UEFI applications.

This software is absolutely free and you may use it as you wish, without restriction.

## Key features

 * easy to compile and integrate with a bare-metal project
 * requires only a few key headers and functions to build/link

## Background documentation

For details about the Itanium C++ ABI and the functions provided by BMCXXABI, see:
 * https://itanium-cxx-abi.github.io/cxx-abi/abi-eh.html
 * https://refspecs.linuxfoundation.org/cxxabi-1.86.html 

Also included as an implementation of `__gxx_personality_v0`, the exception-handling "personality"
routine as implemented by GCC (and LLVM's clang++) when configured for so-called "DWARF exception handling"
(which isn't actually part of the DWARF specification at all). This is the standard exception handling
mechanism used on Linux, for instance.

See:
 * https://www.airs.com/blog/archives/460
 * https://www.airs.com/blog/archives/462
 * https://www.airs.com/blog/archives/464

(The first two are about unwinding more generally, the last link above is more specific to what we are
doing here). 

## Requirements

In reality, this library is only one piece of the puzzle. You also need:
 * A "libunwind" implementation. There are several to choose from.
 * Some C++ headers and functions. BMCXXABI source needs headers such as `cstring` and `cstdlib`.
 * Importantly, an implementation of `malloc` and `free`. The Itanium C++ exception model requires
   dynamic allocation when an exception is thrown.
 * An implementation of `new` and `delete` operators, which should be trivial when you have
   malloc/free anyway.

## Current status

 * Only tested on / designed for x86-64
 * Uses various GCC builtins, should work fine with Clang (but not tested)
 * Currently only supports catching exceptions by the exact correct type (i.e. `catch (Base &)` won't
   catch a thrown `Derived` exception).
