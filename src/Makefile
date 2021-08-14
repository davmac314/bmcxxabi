SRCS ::= typeinfo.cc personality.cc cxa_routines.cc
OBJS ::= $(SRCS:.cc=.o)

OUTDIR = .

# See top-level makefile.

CXX=g++

all: $(OBJS)
	ar -r $(OUTDIR)/libcxxabi.a $(OBJS)

$(OBJS): %.o: %.cc
	$(CXX) $(CCPPFLAGS) $(CCFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)
