SRCS ::= typeinfo.cc personality.cc cxa_routines.cc run_static_init.cc run_static_fini.cc static_destructors.cc
OBJS ::= $(SRCS:.cc=.o)

# sources which need RTTI enabled:
RTTI_SRCS ::= typeinfo_get_npti.cc
RTTI_OBJS ::= $(RTTI_SRCS:.cc=.o)

OUTDIR = .

# See top-level makefile.

CXX=g++

all: $(OBJS) $(RTTI_OBJS)
	ar -r $(OUTDIR)/libcxxabi.a $(OBJS) $(RTTI_OBJS)

$(OBJS): %.o: %.cc
	$(CXX) $(CXXPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(RTTI_OBJS): %.o: %.cc
	$(CXX) $(CXXPPFLAGS) $(CXXFLAGS) -frtti -c $< -o $@

clean:
	rm -f $(OBJS)
