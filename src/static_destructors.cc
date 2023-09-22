#include <cstdint>
#include <cstdlib>
#include <cstring>

// Fake DSO handle; needs to be defined as it will be referenced by compiler-generated code.
// Its address will be passed to __cxa_atexit (3rd parameter).
int __dso_handle = 0;


#ifndef BMCXX_NO_SSD

namespace {

// function, and associated parameter value, to call "at exit"
struct atexit_func {
    void (*f)(void *);
    void *p;
};

atexit_func *atexit_funcs = nullptr;
unsigned atexit_funcs_size = 0;
unsigned num_atexit_funcs = 0;

}

#endif

extern "C"
int __cxa_atexit (void (*f)(void *), void *p, void *d)
{
    // THREAD-SAFETY : this needs a mutex for thread safety

#ifndef BMCXX_NO_SSD

    if (atexit_funcs == nullptr) {
        atexit_funcs = (atexit_func *) malloc(sizeof(atexit_func) * 16);
        if (atexit_funcs == nullptr) {
            return 1;
        }
        atexit_funcs_size = 16;
    }

    // Increase size if necessary
    if (num_atexit_funcs == atexit_funcs_size) {
        constexpr unsigned max = unsigned(-1) / 2 / sizeof(atexit_func);
        if (atexit_funcs_size >= max) {
            return 1;
        }
        unsigned new_funcs_size = atexit_funcs_size * 2;
        atexit_func *new_atexit_funcs = (atexit_func *) malloc(sizeof(atexit_func) * new_funcs_size);
        if (new_atexit_funcs == nullptr) {
            return 1;
        }

        memcpy(new_atexit_funcs, atexit_funcs, num_atexit_funcs * sizeof(atexit_func));
        free(atexit_funcs);
        atexit_funcs = new_atexit_funcs;
    }

    atexit_funcs[num_atexit_funcs] = {f,p};
    ++num_atexit_funcs;

#endif

    return 0;
}

// run static-storage destructors that were registered dynamically (via __cxa_atexit)
extern "C"
void __cxa_finalize(void *d)
{
#ifndef BMCXX_NO_SSD

    if (d != &__dso_handle) return; // shouldn't happen

    for (unsigned i = num_atexit_funcs; i > 0; ) {
        i--;;
        atexit_funcs[i].f(atexit_funcs[i].p);
    }

#endif
}
