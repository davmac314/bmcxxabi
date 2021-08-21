#ifndef _CXA_EXCEPTION_H_INCLUDED
#define _CXA_EXCEPTION_H_INCLUDED 1

#include <exception>

#include <unwind.h>

struct __cxa_exception { 

    // So this field isn't documented in the C++ ABI, but LLVM's libunwind includes it with a
    // comment that it's for C++0x exception_ptr support. However it's only included if
    // __LP64__ is defined, which makes no sense at all.
    size_t referenceCount;
    
    std::type_info *exceptionType;
    void (*exceptionDestructor)(void *);
    std::unexpected_handler unexpectedHandler;
    std::terminate_handler terminateHandler;
    __cxa_exception *nextException;

    int handlerCount;
    int	handlerSwitchValue;

    // following fields can be used by the personality return, eg to cache
    // values between search phase and unwind phase:
    const char *actionRecord;
    const char *languageSpecificData;
    void *catchTemp;
    void *adjustedPtr;

    _Unwind_Exception unwindHeader;
};

#endif
