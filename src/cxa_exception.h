#ifndef _CXA_EXCEPTION_H_INCLUDED
#define _CXA_EXCEPTION_H_INCLUDED 1

#include <unwind.h>

#include "../include/typeinfo"

struct __cxa_exception { 

    // This field isn't documented in the C++ ABI, but LLVM's libunwind includes it with a
    // comment that it's for C++0x exception_ptr support.
    //
    // THREAD-SAFETY : if exception_ptr is to be supported in a thread-safe way, this needs to be
    //                 an atomic counter.
    size_t referenceCount;
    
    // From this point, structure is specified by the ABI. However, the compiler does not AFAIK
    // generate code that in any way relies on this structure.
    // -----------------------------------------

    std::type_info *exceptionType;
    void (*exceptionDestructor)(void *);

    typedef void (*terminate_handler) ();

    // Any replacement "unexpected" handler must be of this type.
    typedef void (*unexpected_handler) ();

    /* std:: */ unexpected_handler unexpectedHandler;
    /* std:: */ terminate_handler terminateHandler;

    __cxa_exception *nextException;

    int handlerCount;
    int	handlerSwitchValue;

    // following fields can be used by the personality return, eg to cache
    // values between search phase and unwind phase:
    const char *actionRecord;
    const char *languageSpecificData;
    void *catchTemp;
    void *adjustedPtr;

    // Must be last:
    _Unwind_Exception unwindHeader;
};

extern "C" void *__cxa_begin_catch(void *exception_object) noexcept;

#endif
