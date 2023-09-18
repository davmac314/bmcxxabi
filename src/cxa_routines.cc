#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <new>

#include "cxa_exception.h"

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

namespace {

// THREAD-SAFETY : these variables should be thread-local variables
__cxa_exception *handled_exc_stack_top = nullptr;
unsigned num_uncaught_exceptions = 0;

}

extern "C"
void * __cxa_allocate_exception(size_t thrown_size) noexcept
{
    // We need space for __cxa_exception + the exception object
    size_t needed = thrown_size + sizeof(__cxa_exception);
    
    char *buf = (char *) malloc(needed);
    
    if (buf == nullptr) {
        std::terminate();
    }

    memset(buf, 0, sizeof(__cxa_exception));
    return buf + sizeof(__cxa_exception);
}

extern "C"
void __cxa_free_exception(void *exc) noexcept
{
    char *exc_p = (char *)exc - sizeof(__cxa_exception);
    free(exc_p);
}

// Cleanup exception, would not normally be called except by foreign exception handler(?)
static void cleanup_exception(_Unwind_Reason_Code, _Unwind_Exception *)
{

}

// Note we mustn't specify noexcept here: exceptions must propagate through
extern "C"
void __cxa_throw(void *thrown, std::type_info *tinfo, void (*destructor)(void *))
{
    uintptr_t cxa_addr = (uintptr_t)thrown - sizeof(__cxa_exception);
    __cxa_exception *cxa_ex = (__cxa_exception *) cxa_addr;
    
    new(cxa_ex) __cxa_exception;
    
    cxa_ex->referenceCount = 1; // <-- totally undocumented, sigh.
    
    cxa_ex->exceptionType = tinfo;
    cxa_ex->exceptionDestructor = destructor;

    // We're supposed to set these to the current handlers, but we don't support that.
    cxa_ex->unexpectedHandler = nullptr;
    cxa_ex->terminateHandler = nullptr;
    
    num_uncaught_exceptions++;
    
    cxa_ex->handlerCount = 0;
    
    char exception_class[8] = {'\0','+','+','C','X','X','M','B'}; // BMXXC++\0
    memcpy(&(cxa_ex->unwindHeader.exception_class), exception_class, sizeof(exception_class));
    
    cxa_ex->unwindHeader.exception_cleanup = cleanup_exception;

    _Unwind_RaiseException(&cxa_ex->unwindHeader);
    
    __cxa_begin_catch(thrown);
    std::terminate();
}

extern "C"
void *__cxa_begin_catch(void *exception_object) noexcept
{
    // TODO check it's a C++ exception: if a foreign exception, we mustn't/can't do most of the following

    uintptr_t cxa_addr = (uintptr_t)exception_object - sizeof(__cxa_exception);
    __cxa_exception *cxa_ex = (__cxa_exception *) cxa_addr;

    if (cxa_ex->handlerCount < 0) {
        // negative handler count indicates in-flight re-thrown exception
        cxa_ex->handlerCount = -cxa_ex->handlerCount;
    }
    else {
        // otherwise, handler count should be 0
        cxa_ex->nextException = handled_exc_stack_top;
        handled_exc_stack_top = cxa_ex;
    }

    cxa_ex->handlerCount++;
    num_uncaught_exceptions--;
    
    return cxa_ex->adjustedPtr;
}

extern "C"
void __cxa_end_catch() noexcept
{
    // Take the exception at the top of the caught exception stack
    __cxa_exception *st_top = handled_exc_stack_top;

    // There are three cases where end catch is called:
    // 1. a handler is completing normally
    // 2. a handler is exiting via a new thrown exception
    //    (or theoretically, a previous exception re-thrown via std::rethrow_exception)
    // 3. a handler is exiting because the exception it handles was re-thrown
    //
    // The ABI doesn't allow distinguish the cases, but we need to differentiate case (3)
    // because the exception must not be destroyed if it is in flight.

    if (st_top->handlerCount > 0) {
        // positive handler count, not re-thrown

        if (--(st_top->handlerCount) == 0) {
            handled_exc_stack_top = st_top->nextException;
            if (--(st_top->referenceCount) == 0) {
                // destroy.
                if (st_top->exceptionDestructor) {
                    uintptr_t native_exc_addr = (uintptr_t)(st_top) + sizeof(__cxa_exception);
                    st_top->exceptionDestructor((void *) native_exc_addr);
                }
            }
        }
    }
    else {
        // negative handler count, in-flight rethrown exception

        ++(st_top->handlerCount); // decrement (negative) count
        if (st_top->handlerCount == 0) {
            handled_exc_stack_top = st_top->nextException;
        }
    }
}

extern "C"
void __cxa_rethrow()
{
    if (handled_exc_stack_top == nullptr) {
        std::terminate();
    }

    // remove the exception from the stack of uncaught exceptions
    __cxa_exception *exc = handled_exc_stack_top;
    handled_exc_stack_top = exc->nextException;

    // Make the handlerCount negative to mark this exception as in-flight rethrown
    exc->handlerCount = -exc->handlerCount;

    num_uncaught_exceptions++;

    _Unwind_RaiseException(&handled_exc_stack_top->unwindHeader);

    void *cxx_exception = (void *)((uintptr_t)exc + sizeof(__cxa_exception));
    __cxa_begin_catch(cxx_exception);
    std::terminate();
}

extern "C"
int __cxa_guard_acquire (int64_t *guard_object)
{
    // THREAD-SAFETY : this should acquire a mutex
    return (*(char *)guard_object) == 0;
}

extern "C"
void __cxa_guard_release(int64_t *guard_object)
{
    // THREAD-SAFETY : this should release the mutex acquired via __cxa_guard_acquire
    *guard_object = 1;
}

extern "C"
void __cxa_guard_abort(int64_t *guard_object)
{
    // THREAD-SAFETY : this should release the mutex acquired via __cxa_guard_acquire
}
