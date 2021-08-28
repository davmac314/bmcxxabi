# Variables, which can be set on command line or here:
#
# OUTDIR
#   Where the resulting library, libcxxabi.a, should be placed
#
# CXXPPFLAGS
#   Preprocessor options; should generally include "-nostdinc++" and include path(s) for (suitable)
#   C++ standard library (and unwind library, though GCC provides unwind.h so this should not really
#   be required).
#
# CXXFLAGS
#   C++ compilation options. Should include necessary architectural options. For x86-64, probably
#   should include "-march=x86-64 -mno-sse -mno-red-zone". Should disable any features which may
#   require additional runtime support.
#
# CXX
#   The c++ compiler, eg g++/clang++
#
# Eg values:
# CCPPFLAGS=-nostdinc++ -I/some/dir/libunwind/include -isystem /some/dir/libbmcxx/include
# CCFLAGS=-g -march=x86-64 -mno-sse -mno-red-zone -ffreestanding -fno-stack-protector

CXX=g++

export OUTDIR CXX CXXFLAGS CXXPPFLAGS

all:
	$(MAKE) -C src all

clean:
	$(MAKE) -C src clean
