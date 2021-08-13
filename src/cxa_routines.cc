#include <cstdlib>
#include <cstring>
#include <new>

#include "cxa_exception.h"

void debug_write(const char16_t *msg); // XXX

// Funky C++ stuff.
//
// Note that "throw;" can be written anywhere, not just directly inside a catch block. It re-throws
// the most recently caught exception (that hasn't yet been "purged"). This means we need to maintain
// a stack of caught exceptions (so that we know which was most recently caught).
//
// (An exception is "purged" via when all exception handler blocks that caught the exception have
// completed.)
//
// Now, here's the truly funky thing: you can "throw;" and then let it propagate out as is pretty
// usual, or you can "throw;" and then re-catch while *still within the handler*. In the latter case
// there are now 2 handlers actively handling the exception (and we mustn't delete the exception until
// they are both done). So, each exception has an active handler count.
//
// In the first case - where you rethrow and it propagates out of the handler - the cleanup pad for
// the handler will call __cxa_end_catch, and that would normally decrement the handler count, but in
// this case we *don't* want to destroy the exception since it's been re-thrown and is still in
// flight. So we need to mark the exception has having been re-thrown, until it is caught again,
// and not destroy it in the meantime.


extern "C"
void * __cxa_allocate_exception(size_t thrown_size) noexcept
{
    debug_write(u"__cxa_allocate_exception()\r\n");
    
    // We need space for __cxa_exception + the exception object
    
    size_t needed = thrown_size + sizeof(__cxa_exception);
    
    // C++17 has aligned new, but let's not require that yet; we'll just use malloc.
    char *buf = (char *) malloc(needed);
    
    if (buf != nullptr) {
        // Need to do anything?
    }
    
    return buf + sizeof(__cxa_exception);
}

extern "C"
void __cxa_free_exception(void *exc) noexcept
{
    // TODO call destructor?
    char *exc_p = (char *)exc - sizeof(__cxa_exception);
    free(exc_p);
}

// Note we mustn't specify noexcept here: exceptions must propagate through
extern "C" void __cxa_throw(void *thrown, std::type_info *tinfo, void (*destructor)(void *))
{
    uintptr_t cxa_addr = (uintptr_t)thrown - sizeof(__cxa_exception);
    __cxa_exception *cxa_ex = (__cxa_exception *) cxa_addr;
    
    new(cxa_ex) __cxa_exception;
    
    cxa_ex->referenceCount = 1; // <-- totally undocumented, sigh.
    
    cxa_ex->exceptionType = tinfo;
    cxa_ex->exceptionDestructor = destructor;

    // TODO maybe. We're supposed to set these to the current handlers.
    cxa_ex->unexpectedHandler = nullptr;
    cxa_ex->terminateHandler = nullptr;
    
    // TODO
    // increment uncaught_exceptions
    
    cxa_ex->handlerCount = 0; // TODO undocumented by ABI, do it here?
    
    char exception_class[8] = {'\0','+','+','C','X','X','M','B'}; // BMXXC++\0
    memcpy(&(cxa_ex->unwindHeader.exception_class), exception_class, sizeof(exception_class));
    
    _Unwind_RaiseException(&cxa_ex->unwindHeader);
    
    abort();
}

extern "C"
void *__cxa_begin_catch(void *exception_object) noexcept
{
    // TODO check it's a C++ exception: if a foreign exception, we mustn't/can't do most of the following

    uintptr_t cxa_addr = (uintptr_t)exception_object - sizeof(__cxa_exception);
    __cxa_exception *cxa_ex = (__cxa_exception *) cxa_addr;

    // TODO if re-thrown exception, un-mark as re-thrown    
    cxa_ex->handlerCount++;
    
    // TODO: place on caught exception stack
    // TODO: decrement uncaught_exceptions
    
    return cxa_ex->adjustedPtr;
}

extern "C"
void __cxa_end_catch() noexcept
{
    // TODO
}
