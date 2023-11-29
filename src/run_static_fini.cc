#include <cstdint>

struct opaque;

// These should be defined via linker script
extern opaque *__fini_array_start;
extern opaque *__fini_array_end;

// Defined in static_destructors.cc
extern "C" void __cxa_finalize(void *d);
extern int __dso_handle;

// Defined below
extern "C" void bmcxxabi_run_fini();


// run all static-storage destructors, regardless of how they were registered
extern "C"
void bmcxxabi_run_destructors()
{
    __cxa_finalize(&__dso_handle);
    bmcxxabi_run_fini();
}

// run static-storage destructors that were registered statically
extern "C"
void bmcxxabi_run_fini()
{
    // The __fini_array_start/__fini_array_end are set up by the linker, as per
    // the link script.
    uintptr_t *fini_arr = (uintptr_t *) &__fini_array_end;
    uintptr_t *begin_fini_arr = (uintptr_t *) &__fini_array_start;

    while (fini_arr > begin_fini_arr) {
        fini_arr--;

        uintptr_t fini_func_addr = *(uintptr_t *)fini_arr;

        void (*fini_func)() = (void (*)()) fini_func_addr;
        fini_func();
    }
}
