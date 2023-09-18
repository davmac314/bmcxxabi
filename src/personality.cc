#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>

#include <unwind.h>

#include "cxa_exception.h"
#include "../include/typeinfo"

// Definition of the "personality" routine, __gxx_personality_v0, which is referenced in g++-
// generated stack unwind information. When unwinding the stack (eg due to an exception being
// thrown) this routine is called (from libunwind) to perform language-specific handling. See
// comments on the function itself below for more details.
//
// This implementation uses information from Ian Lance Taylor's blog entries:
//
//   https://www.airs.com/blog/archives/date/2011/01
//
// Additionally, the (LLVM) libcxxabi source was consulted.

namespace {

// DWARF EH encodings. These specify how a value is encoded, and what it is relative to
enum {
  DW_EH_PE_absptr = 0,  // (not relative)
  
  // value encodings  
  DW_EH_PE_uleb128 = 1,  // variable-length unsigned
  DW_EH_PE_udata2 = 2,   // 2-byte unsigned
  DW_EH_PE_udata4 = 3,   // 4-byte unsigned
  DW_EH_PE_udata8 = 4,   // 8-byte unsigned
  
  DW_EH_PE_sleb128 = 9,  // variable-length signed
  DW_EH_PE_sdata2 = 10,  // 2-byte signed
  DW_EH_PE_sdata4 = 11,  // 4-byte signed
  DW_EH_PE_sdata8 = 12,  // 8-byte signed

  // What is it relative to?
  DW_EH_PE_pcrel = 0x10,
  DW_EH_PE_textrel = 0x20,
  DW_EH_PE_datarel = 0x30,
  DW_EH_PE_funcrel = 0x40,
  DW_EH_PE_aligned = 0x50,  // ??
  
  DW_EH_PE_indirect = 0x80, // value is indirect, i.e. specifies address holding value
  
  DW_EH_PE_omit = 0xFF  // no value
};

template <typename T>
T read_val(const uint8_t *& p) noexcept
{
    T val;
    memcpy(&val, p, sizeof(T));
    p += sizeof(T);
    return val;
}

// Read ULEB128-encoded value, bump pointer
uintptr_t read_ULEB128(const uint8_t *& p) noexcept
{
    // A series of bytes, each worth 7 bits of value. The last byte has bit 8 clear.
    
    uintptr_t val = 0;    
    unsigned shift = 0;
    uint8_t bval;
    
    do {
        bval = *p++;
        val |= ((uintptr_t)(bval & 0x7F)) << shift;
        shift += 7;
    } while (bval & 0x80);
    
    return val;
}

// Read SLEB128-encoded value, bump pointer
intptr_t read_SLEB128(const uint8_t *& p) noexcept
{
    // A series of bytes, each worth 7 bits of value. The last byte has bit 8 clear.
    
    uintptr_t val = 0;    
    unsigned shift = 0;
    uint8_t bval;
    
    do {
        bval = *p++;
        val |= ((uintptr_t)(bval & 0x7F)) << shift;
        shift += 7;
    } while (bval & 0x80);
    
    if (bval & 0x40) {
        // sign bit is set, need to extend it
        if (shift < (sizeof(uintptr_t) * 8 /* CHAR_BIT */)) {
            val |= ((uintptr_t)-1) << shift;
        }
    }
    
    return val;
}

// Read DWARF EH value with the specified encoding, bump pointer
uintptr_t read_dwarf_encoded_val(const uint8_t *& p, uint8_t encoding) noexcept
{
    const uint8_t *orig_p = p;

    if (encoding == DW_EH_PE_omit) {
        return 0;
    }
    
    uintptr_t val;
    
    switch (encoding & 0x0Fu) {
    case DW_EH_PE_uleb128:
        val = read_ULEB128(p);
        break;
    case DW_EH_PE_udata2:
        val = (uintptr_t) read_val<uint16_t>(p);
        break;
    case DW_EH_PE_udata4:
        val = (uintptr_t) read_val<uint32_t>(p);
        break;
    case DW_EH_PE_udata8:
        val = (uintptr_t) read_val<uint64_t>(p);
        break;
    case DW_EH_PE_sleb128:
        val = read_SLEB128(p);
        break;
    case DW_EH_PE_sdata2:
        val = read_val<int16_t>(p);
        break;
    case DW_EH_PE_sdata4:
        val = read_val<int32_t>(p);
        break;
    case DW_EH_PE_sdata8:
        val = read_val<int64_t>(p);
        break;
    default:
        abort(); // unsupported
    }
    
    switch (encoding & 0x70u) {
    case DW_EH_PE_absptr:
        // not relative
        break;
    case DW_EH_PE_pcrel:
        // "PC" relative
        if (val) {
            val += (uintptr_t) orig_p;
        }
        break;
    default:
        abort(); // unsupported
    }

    if (encoding & DW_EH_PE_indirect) {
        val = *(uintptr_t *)val;
    }

    return val;
}

// Read DWARF EH encoded value: encoding, followed by encoded value; bump pointer
uintptr_t read_dwarf_encoded_val(const uint8_t *& p) noexcept
{
    uint8_t encoding = *p++;
    return read_dwarf_encoded_val(p, encoding);    
}

// Get the fixed size for a particular encoding, if it exists, or 0
unsigned size_from_encoding(uint8_t encoding) noexcept
{
    unsigned val = 0;
    
    switch (encoding & 0x0Fu) {
    case DW_EH_PE_omit:
    case DW_EH_PE_uleb128:
    case DW_EH_PE_sleb128:
        break;
    case DW_EH_PE_udata2:
    case DW_EH_PE_sdata2:
        val = 2;
        break;
    case DW_EH_PE_udata4:
    case DW_EH_PE_sdata4:
        val = 4;
        break;
    case DW_EH_PE_udata8:
    case DW_EH_PE_sdata8:
        val = 8;
        break;
    default:
        abort(); // unsupported
    }
    
    return val;
}

} // anon namespace


// This is the "personality" routine for C++ exceptions (using the so-called "dwarf exception
// handling"). It will be called during stack unwinding for any frame where the unwind information
// for the frame has this routine specified as the personality (i.e. any frame in a C++ function).
//
// Unwinding is done in two phases, the "search" and "cleanup" phase. The search phase does not
// actually unwind the stack; this is inefficient since a lot of the work in the two phases may
// need to be duplicated, but has the benefit that if the exception isn't caught we can recover
// a full stack trace of where it was thrown from.
//
// In the search phase, we are only interested in finding a suitable "catch" handler or identifying
// uncaught exceptions (i.e. which enter a frame for which we have no unwind information). So we
// return:
//   - _URC_HANDLER_FOUND if we find a handler, this then begins the cleanup phase
//   - _URC_CONTINUE_UNWIND if we have no catch handler, this continues search in the calling frame
//   - _URC_FATAL_PHASE1_ERROR if we can't find any language-specific unwind info at all, this
//             shouldn't really happen however.
//
// In the cleanup phase, we want to run cleanup handlers or the catch handler.
// So we return:
//   - _URC_INSTALL_CONTEXT to run a catch/cleanup or
//   - _URC_CONTINUE_UNWIND if there is no catch/cleanup for this frame
// The _UA_HANDLER_FRAME bit will be set if this frame should contain the handler (i.e. if the
// search phase returned _URC_HANDLER_FOUND on this frame), in that case we will definitely return
// _URC_INSTALL_CONTEXT.
//
// Note that there can only be one handler per address [range] and it needs to handle both catches
// and cleanup. The "handler switch" register (__builtin_eh_return_data_regno(1)) is 0 for a cleanup
// and in this case the "exception ptr" register (..._regno(0)) is a pointer to the _Unwind_Exception.
// However for a catch (..._regno(1) is non-zero) then regno(0) is a pointer to the actual thrown
// object.
extern "C"
_Unwind_Reason_Code __gxx_personality_v0(int version, _Unwind_Action actions, uint64_t exception_class,
    _Unwind_Exception *unwind_exc, _Unwind_Context *context) noexcept {
    
    uint32_t cpp;
    char cppstr[4] = {0,'+','+','C'};
    char *cpp_cp = (char *)&cpp;
    for (unsigned i = 0; i < 4; i++) cpp_cp[i] = cppstr[i];
    
    bool native_exception = (exception_class & 0xFFFFFF00U) == cpp;
    // ILT's blog (Jan 2011) states that "C++\1" (rather than "C++\0") is used for "dependent"
    // exceptions, "which is used when rethrowing an exception". So we mask out the last byte.
    // The first four bytes are for vendor, CLNG for clang/llvm, GNUC for GCC; we don't really
    // care so just ignore them (technically we should probably check that they match what we
    // set ourselves in _cxa_throw).
    //
    // Note: it appears that by "rethrowing an exception" ILT means throwing via
    // std::rethrow_exception (i.e. throwing an exception captured in a std::exception_ptr) and
    // not a regular "throw;". Since we don't support std::rethrow_exception/exception_ptr (yet?)
    // then we don't need to worry about it. (The purpose is to apparently create a separate
    // __cxa_exception object that can be linked into a current-exception stack separately from
    // the original).

    if (actions & _UA_HANDLER_FRAME) {
        // If this is the frame where we found a handler,
        // retrieve cached items, install context

        uintptr_t cxa_exception_addr = (uintptr_t)unwind_exc - offsetof(__cxa_exception, unwindHeader);
        __cxa_exception *cxa_exception = (__cxa_exception *) cxa_exception_addr;

        // for a catch, as opposed to a cleanup operation, EHregister#0 is set to the address of
        // the thrown object (for a cleanup it points to the _Unwind_Exception).
        _Unwind_SetGR(context, (int)__builtin_eh_return_data_regno(0),
                (uintptr_t)unwind_exc + sizeof(*unwind_exc));
        _Unwind_SetGR(context, (int)__builtin_eh_return_data_regno(1), cxa_exception->handlerSwitchValue);
        _Unwind_SetIP(context, (uintptr_t) cxa_exception->catchTemp);
        return _URC_INSTALL_CONTEXT;        
    }
    else {
        // Need to scan language-specific data for the frame. For C++ this is essentially a list of
        // catch() blocks, and cleanup handlers.
        
        // Specifically the format of the LSDA is:
        //    (DE = dwarf encoded value, i.e. a u8 encoding indicator followed by encoded value;
        //     [xyz encoding] = value encoded according to some previous encoding indicator, xyz;
        //     [table] = a table, format described in table description)
        //
        //   LSDA:
        //       DE   landing pad start; if 0, use function address
        //       u8   types table encoding (may be DW_EH_PE_omit, i.e. not present)
        //
        //   if types table encoding indicates table is present:
        //     ULEB128   offset to classInfo (from current address in LSDA, i.e. end of this
        //               field). Note that it actually points at the end of the classInfo
        //               table! See classInfo table below.
        //
        //       u8   call site encoding;
        //  ULEB128   callsiteTableLength  (length in bytes)
        //  [table]   (callsite table)
        //  [table]   (action table)

        //   Call sites (callsite table):
        //      - have a start and length; are non-overlapping, ordered by start
        //     [call site encoding] start offset (from function start)
        //     [call site encoding] length
        //     [call site encoding] landing pad offset (from landing pad start); 0 = none?
        //                  ULEB128 actionEntry
        //                            0 = cleanup
        //                            1+ = action table offset + 1 (i.e. 1 = offset 0)

        //   Action table:
        //     Note each entry consists of (potentially) multiple actions, with an end marker.
        //     Actions include: handlers, cleanup, check vs throw(...) specification.
        //
        //     "Each entry in the action table is a pair of signed LEB128 values":
        //
        //    SLEB128 (int64_t) typeIndex
        //                      > 0:  catch; type specifies the type caught.
        //                            value is a *negated* index which must be multiplied
        //                            by the size of entries (according to encoding)
        //                            eg 1 = -1 index from classInfo table pointer.
        //
        //                      < 0: "exception spec".
        //                           This is a *negated* *byte offset* from the classInfo table
        //                           pointer (i.e. from the end of the classInfo table) to a list
        //                           of types which are allowed to propagate; this represents the
        //                           "throws(...)" clause from C++98.
        //
        //                           The list is a series of ULEB128 entries which each encode a
        //                           negated index into the classInfo table (eg 1 = -1 index from
        //                           classInfo table pointer); value of 0 terminates the list.
        //
        //    SLEB128 (int64_t) offset (from the location of this value) to next action. Note that
        //                      offset is not from location after the value - it is from the
        //                      actual location where the encoded value begins.
        //                      If 0, end of action list.
        //
        
        //   classInfo table:
        //     The classInfo pointer from the header points (just past) the *end* of this table.
        //     The encoding of each entry depends on call site encoding field from the header, but
        //        should be a fixed size; each entry is a pointer to a std::typeinfo object.
        //     The classInfo table is followed by the "throws(...)" specifications table (see
        //        description of typeIndex in the action table).

        const uint8_t *lsda = (const uint8_t *) _Unwind_GetLanguageSpecificData(context);
        const uintptr_t rIP = _Unwind_GetIP(context) - 1;
        const uintptr_t func_start = _Unwind_GetRegionStart(context);
    
        // Landing pad start; defaults to function start
        const uint8_t *lp_start = (const uint8_t *) read_dwarf_encoded_val(lsda);
        if (lp_start == nullptr) {
            lp_start = (uint8_t *) func_start;
        }
        
        // Types table pointer
        const uint8_t *types_tbl_ptr = nullptr;  // default to null
        uint8_t types_encoding = *lsda++;
        if (types_encoding != DW_EH_PE_omit) {
            //  "This is an unsigned LEB128 value, and is the byte offset from this field to the
            // start of the types table used for exception matching".
            // It is the offset from the *end* of this field:
            uintptr_t types_tbl_offs = read_ULEB128(lsda);
            types_tbl_ptr = lsda + types_tbl_offs;
        }
        
        uint8_t callsite_encoding = *lsda++;

        uintptr_t callsite_tbl_len = read_ULEB128(lsda);
        
        const uint8_t *callsite_tbl = lsda;
        
        const uint8_t *actions_tbl = callsite_tbl + callsite_tbl_len;
        
        // Now we walk through the callsites until we find our current IP
        // ILT blog says the callsite start is offset from the landing pad base not the
        // function start, in which case this rIP_offs calculation is wrong. However,
        // 1) libunwind from LLVM does the following;
        // 2) as does libsupc++ from GCC;
        // 3) basing callsites off landing pad base makes little sense;
        // 4) (Are landing pad base and func start ever different in practice anyway?).
        uintptr_t rIP_offs = rIP - func_start;

        while (lsda < actions_tbl) {
            uintptr_t cs_start = read_dwarf_encoded_val(lsda, callsite_encoding);
            uintptr_t cs_len = read_dwarf_encoded_val(lsda, callsite_encoding);
            uintptr_t cs_end = cs_start + cs_len;
            uintptr_t lp_offs = read_dwarf_encoded_val(lsda, callsite_encoding); // landing pad
            uintptr_t action_entry = read_ULEB128(lsda);
                      
            // check match
            if (rIP_offs >= cs_start) {
                if (rIP_offs < cs_end) {
                    // matches location, we still need to check actions
                    
                    if (lp_offs == 0) {
                        // Apparently, offset of 0 means no cleanup/catch
                        return _URC_CONTINUE_UNWIND;
                    }

                    if (action_entry == 0) {
                        // action_entry == 0 : cleanup only, no catches
                        if (actions & _UA_SEARCH_PHASE) {
                            return _URC_CONTINUE_UNWIND;
                        }
                        
                        // Forced unwind, or cleanup phase
                        // Set the registers in context so that the landing pad can resume unwind
                        // when done:
                        
                        _Unwind_SetGR(context, (int)__builtin_eh_return_data_regno(0),
                                (uintptr_t)unwind_exc);
                        _Unwind_SetGR(context, (int)__builtin_eh_return_data_regno(1),
                                (uintptr_t)0);
                        _Unwind_SetIP(context, (uintptr_t)(lp_start + lp_offs));
                        return _URC_INSTALL_CONTEXT;
                    }

                    const uint8_t *action_entry_ptr = actions_tbl + (action_entry - 1);

                    while (action_entry != 0) {
                        // "Each entry in the action table is a pair of signed LEB128 values"...
                        // read the first one now, act on it, and read the 2nd (offset to next
                        // entry) afterwards.
                        intptr_t type_info_index = read_SLEB128(action_entry_ptr);
                        
                        // cleanup?
                        if (type_info_index == 0) {
                            if (actions & _UA_SEARCH_PHASE) {
                                return _URC_CONTINUE_UNWIND;
                            }
                            _Unwind_SetGR(context, (int)__builtin_eh_return_data_regno(0),
                                    (uintptr_t)unwind_exc);
                            _Unwind_SetGR(context, (int)__builtin_eh_return_data_regno(1),
                                    (uintptr_t)0);
                            _Unwind_SetIP(context, (uintptr_t)(lp_start + lp_offs));
                            return _URC_INSTALL_CONTEXT;
                        }
                        
                        if (actions & _UA_FORCE_UNWIND) {
                            // Used for thread cancellation or unwind-based longjmp
                            continue;
                        }

                        unsigned type_info_sz = size_from_encoding(types_encoding);
                        if (type_info_sz == 0) abort();

                        uintptr_t cxa_exception_addr = (uintptr_t)unwind_exc - offsetof(__cxa_exception, unwindHeader);
                        __cxa_exception *cxa_exception = (__cxa_exception *) cxa_exception_addr;

                        // catch handler for single type?
                        if (type_info_index > 0) {

                            const uint8_t *catch_type_p = (types_tbl_ptr - type_info_index * type_info_sz);

                            std::type_info *catch_type = (std::type_info *)
                                    read_dwarf_encoded_val(catch_type_p, types_encoding);

                            void * cxx_exception_ptr = (void *)(cxa_exception + 1);

                            // A null catch_type is a catch-any aka "catch(...)". Otherwise we
                            // need to check the type.
                            if (catch_type == nullptr
                                    || catch_type->__do_catch(cxa_exception->exceptionType,
                                            &cxx_exception_ptr, 1)) {
                                // Cache the values that will be used in phase 2:
                                cxa_exception->adjustedPtr = cxx_exception_ptr;
                                cxa_exception->handlerSwitchValue = type_info_index;
                                cxa_exception->catchTemp = (void *)(lp_start + lp_offs);
                                return _URC_HANDLER_FOUND;
                            }
                        }
                        else /* (type_info_index < 0) */ {
                            // throw specification (C++98). This is matched if the exception thrown is *not*
                            // any from a list of types.
                            const uint8_t *throw_spec_start = types_tbl_ptr - type_info_index;
                            uintptr_t ts_index = read_ULEB128(throw_spec_start);
                            while (ts_index != 0) {
                                const uint8_t *catch_type_p = (types_tbl_ptr - ts_index * type_info_sz);

                                std::type_info *catch_type = (std::type_info *)
                                        read_dwarf_encoded_val(catch_type_p, types_encoding);

                                void * cxx_exception_ptr = (void *)(cxa_exception + 1);

                                if (catch_type->__do_catch(cxa_exception->exceptionType, &cxx_exception_ptr, 1)) {
                                    cxa_exception->adjustedPtr = (void *)(cxa_exception + 1); // un-adjusted!
                                    cxa_exception->handlerSwitchValue = type_info_index;
                                    cxa_exception->catchTemp = (void *)(lp_start + lp_offs);
                                    // The handler should just call __cxa_call_unexpected(), but
                                    // that's in the hands of the compiler...
                                    return _URC_HANDLER_FOUND;
                                }

                                ts_index = read_ULEB128(throw_spec_start);
                            }
                        }

                        // The next value we read is an offset from the current position in the
                        // action entry table, so we need to avoid modifying the current position
                        // before adding the offset; copy the value and use the copy in read_SLEB:
                        const uint8_t *action_entry_read_next = action_entry_ptr;
                        intptr_t action_entry_offs = read_SLEB128(action_entry_read_next);
                        if (action_entry_offs == 0) break;
                        action_entry_ptr += action_entry_offs;
                    }
                    
                    // Got to end of actions without a match, continue unwind
                    return _URC_CONTINUE_UNWIND;
                }
            }
            else {
                // we have: rIP_offs < cs_start
                // call sites ordered by start address, therefore, we won't find one from here
                
                // "If the personality function finds that there is no entry for the current
                // PC in the call-site table, then there is no exception information. This
                // should not happen in normal operation, and in C++ will lead to a call to
                // std::terminate"
                
                // We return an error here, that way _Unwind_RaiseException returns (instead of
                // unwinding) and std::terminate() can be called from _cxa_throw(...).

                return _URC_FATAL_PHASE1_ERROR;
                
                break;
            }
        }
    } // not handler frame

    return _URC_CONTINUE_UNWIND; 
}
