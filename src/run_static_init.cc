#include <cstdint>

namespace {
struct opaque;
}

extern opaque *__init_array_start;
extern opaque *__init_array_end;

extern "C"
void bmcxxabi_run_init()
{
    // The __init_array_start/__init_array_end are set up by the linker, as per
    // the link script.
    uintptr_t *init_arr = (uintptr_t *) &__init_array_start;
    uintptr_t *end_init_arr = (uintptr_t *) &__init_array_end;

    while (init_arr < end_init_arr) {
        uintptr_t init_func_addr = *(uintptr_t *)init_arr;

        void (*init_func)() = (void (*)()) init_func_addr;
        init_func();

        init_arr++;
    }
}
