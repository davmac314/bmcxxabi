SRCS=typeinfo.cc personality.cc cxa_routines.cc
OBJS=$(SRCS:.cc=.o)

CPPFLAGS=-nostdinc++

CCFLAGS=-g -march=x86-64 -mno-sse -mno-red-zone -ffreestanding -fno-stack-protector

all: $(OBJS)
	ar -r libcxxabi.a $(OBJS)

$(OBJS): %.o: %.cc
	g++ $(CPPFLAGS) $(CCFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)
