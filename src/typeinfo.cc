// The Itanium C++ ABI specifies a bunch of subclasses of std::type_info for different cases
// (class type, class with single inheritance, class with multiple inheritance, etc). The
// data layout of the type_info class itself, as well as these subclasses, is also specified by
// the ABI.
//
// The compiler then generates type_info objects with vtable pointers referring to these ABI
// types.

#include "../include/typeinfo"

namespace std {

type_info::~type_info() { }

bool type_info::__do_catch(const type_info *thrown_type, void **thrown_obj, unsigned outer) const noexcept
{
    return *this == *thrown_type;
}

bool type_info::__do_upcast(const __cxxabiv1::__class_type_info *target_type, void **obj_ptr) const noexcept
{
    return false;
}

const __cxxabiv1::__pointer_type_info *type_info::__as_pointer_type() const noexcept
{
    return nullptr;
}

} // namespace std

namespace __cxxabiv1 {

// __fundamental_type_info

// "the run-time support library should contain type_info objects for the types X, X* and
// X const*, for every X in: void, std::nullptr_t, bool, wchar_t, char, unsigned char, signed
// char, short, unsigned short, int, unsigned int, long, unsigned long, long long, unsigned long
// long, float, double, long double, char8_t, char16_t, char32_t, and the IEEE 754r decimal and
// half-precision floating point types. Each of the type_info objects for X shall have type
// abi::__fundamental_type_info (or a type derived therefrom), whereas the objects corresponding
// to X* and X const* shall have type abi::__pointer_type_info (or a type derived therefrom)."

// Apparently GCC recognizes the __fundamental_type_info destructor definition and emits type_info
// for all the required types.

class __fundamental_type_info : public std::type_info {
public:
    virtual ~__fundamental_type_info();
};

__fundamental_type_info::~__fundamental_type_info() {}

class __array_type_info : public std::type_info {};
class __function_type_info : public std::type_info {};
class __enum_type_info : public std::type_info {};


// __class_type_info : implements type_info for classes with no base classes, and provides a base
// class for type_info structures representing classes *with* base classes.

class __class_type_info : public std::type_info
{
public:
    virtual ~__class_type_info();
    virtual bool __do_catch(const std::type_info *__thrown_type, void **__thrown_obj,
            unsigned __outer) const noexcept override;

    // current_subobj: the current subobject (within original thrown object) corresponding to a
    // base of this class type
    // *found_subobj: nullptr if not found; otherwise, a candidate subobject
    // inh_flags: 0x1 = continue search after candidate found (i.e. report ambiguity)
    //
    // return: false if ambiguity was discovered; true otherwise (*found_subobj is single found
    //         subobject, or null)
    virtual bool __do_vmi_upcast(const __cxxabiv1::__class_type_info *target_type,
            void *current_subobj, void **found_subobj, int inh_flags) const noexcept;
};

__class_type_info::~__class_type_info() {}

bool __class_type_info::__do_catch(const std::type_info *thrown_type, void **thrown_obj,
        unsigned outer) const noexcept
{
    if (*thrown_type == *this) {
        return true;
    }

    // bit 0: all outer pointers have been const
    // bit 1+: number of outer pointers
    // >=4 implies more than one level of pointer.
    // One level is ok because "catch(Base *b)" can catch a thrown "Derived *"
    if (outer >= 4) {
        return false;
    }

    return thrown_type->__do_upcast(this, thrown_obj);
}

bool __class_type_info::__do_vmi_upcast(const __cxxabiv1::__class_type_info *target_type,
            void *current_subobj, void **found_subobj, int inh_flags) const noexcept
{
    if (__do_upcast(target_type, &current_subobj)) {
        if (*found_subobj != nullptr) {
            return *found_subobj == current_subobj;
        }
        *found_subobj = current_subobj;
    }

    return true;
}

// __si_class_type_info : type_info for class with single inheritance

class __si_class_type_info : public __class_type_info
{
public:
    const __class_type_info *__base_type;
    virtual ~__si_class_type_info() override;
    virtual bool __do_upcast(const __cxxabiv1::__class_type_info *__target_type, void **__obj_ptr)
            const noexcept override;
};

__si_class_type_info::~__si_class_type_info() {}

bool __si_class_type_info::__do_upcast(const __cxxabiv1::__class_type_info *target_type,
        void **obj_ptr) const noexcept
{
    if (*__base_type == *target_type) {
        return true;
    }

    return __base_type->__do_upcast(target_type, obj_ptr);
}

// __pbase_type_info : type_info base class for pointer types (regular pointers, and
// pointers-to-members)

class __pbase_type_info : public std::type_info
{
public:
    unsigned int __flags;
    const std::type_info *__pointee;

    enum __masks {
        __const_mask = 0x1,
        __volatile_mask = 0x2,
        __restrict_mask = 0x4,
        __incomplete_mask = 0x8,
        __incomplete_class_mask = 0x10,
        __transaction_safe_mask = 0x20,
        __noexcept_mask = 0x40
    };
};

// A pointer to typeid(nullptr_t), lazily initialised
static const std::type_info *nullptr_ti = nullptr;

// An (external) function to retrieve type_info for nullptr_t
const std::type_info *get_npti();

class __pointer_type_info : public __pbase_type_info
{
public:
    ~__pointer_type_info();
    virtual bool __do_catch(const std::type_info *thrown_type, void **thrown_obj,
            unsigned outer) const noexcept override;

    virtual const __pointer_type_info *__as_pointer_type() const noexcept override;
};

__pointer_type_info::~__pointer_type_info() { }

bool __pointer_type_info::__do_catch(const std::type_info *thrown_type, void **thrown_obj,
        unsigned outer) const noexcept
{
    auto fixup_ptr = [&]() {
        if (outer < 2) {
            // This is the first level of indirection. The handler will expect the actual pointer
            // value that was thrown, not the object containing it, so fix that now:
            *thrown_obj = **(void ***)thrown_obj;
        }
    };

    if (*thrown_type == *this) {
        fixup_ptr();
        return true;
    }

    // Check for nullptr_t.
    // Although "*thrown_type == typeid(decltype(nullptr))" would be nice, we can't use typeid if
    // this file is compiled with -fno-rtti. Rather than force compilation of the whole file with
    // -frtti, use a function in an external compilation unit (which will be compiled with -frtti)
    // to retrieve the typeinfo.

    if (nullptr_ti == nullptr) {
        nullptr_ti = get_npti();
    }

    if (*thrown_type == *nullptr_ti) {
        return true;
    }

    // A qualified pointer can catch a non-qualified pointer (to suitable type) but every outer
    // pointer must be const qualified (true iff outer&1 == 1).

    // first we need to check if the thrown type *is* a pointer type:
    const __pointer_type_info *thrown_ptr_type = thrown_type->__as_pointer_type();
    if (thrown_ptr_type == nullptr) {
        return false;
    }

    fixup_ptr();

    if (thrown_ptr_type->__flags != __flags) {
        // If other type has any qualifiers we don't, no match:
        if ((thrown_ptr_type->__flags & ~__flags) != 0) {
            return false;
        }

        // If it hasn't been const so far, no match:
        if ((outer & 1) == 0) {
            return false;
        }
    }

    unsigned newOuter = outer + 2; // (bits 1+ keep count, so this is adding one)
    newOuter &= ~(~__flags & __const_mask);
    return __pointee->__do_catch(thrown_ptr_type->__pointee, thrown_obj, newOuter);
}

const __pointer_type_info *__pointer_type_info::__as_pointer_type() const noexcept
{
    return this;
}

class __pointer_to_member_type_info : public __pbase_type_info {
    public:
    const __class_type_info *__context;
};


// __vmi_class_type_info: classes using virtual and/or multiple inheritance

// base class info for __vmi_class_type_info

struct __base_class_type_info
{
    const __class_type_info *__base_type;

    // offset, and virtual/public flags. The offset is either the (positive) offset of the base
    // subobject (non-virtual base) or, for virtual bases, the (negative) offset in the virtual
    // table of the entry holding the offset (positive or negative) of the base subobject.
    long __offset_flags;

    // bit masks for values in __offset_flags:
    static const long __virtual_mask = 0x1;
    static const long __public_mask = 0x2;

    // shift needed to get base class offset from __offset_flags
    static const int __offset_shift = 8;
};


// __vmi_class_type_info: class using virtual or multiple inheritance

class __vmi_class_type_info : public __class_type_info {
public:
    unsigned int __flags;
    unsigned int __base_count;
    __base_class_type_info __base_info[];

    static const unsigned __non_diamond_repeat_mask = 0x1;
    static const unsigned __diamond_shaped_mask = 0x2;

    virtual bool __do_upcast(const __cxxabiv1::__class_type_info *__target_type, void **__obj_ptr)
            const noexcept override;
    virtual bool __do_vmi_upcast(const __cxxabiv1::__class_type_info *target_type,
            void *current_subobj, void **found_subobj, int inh_flags) const noexcept override;
};

static void *get_base_subobj(const __base_class_type_info *base_info, void *this_obj)
{
    long offset = base_info->__offset_flags >> __base_class_type_info::__offset_shift;
    if (!(base_info->__offset_flags & __base_class_type_info::__virtual_mask)) {
        // non-virtual base
        return (char *)this_obj + offset;
    }
    else {
        // virtual base; offset is a vtable offset to an entry with actual offset
        char *vtable_ptr = *(char **)this_obj;
        int subobj_offs = *(int *)(vtable_ptr + offset);
        return (char *)this_obj + subobj_offs;
    }
}

bool __vmi_class_type_info::__do_upcast(const __cxxabiv1::__class_type_info *target_type,
        void **obj_ptr) const noexcept
{
    void *found_subobj = nullptr;
    for (unsigned i = 0; i < __base_count; ++i) {
        if (!(__base_info[i].__offset_flags & __base_class_type_info::__public_mask))
            continue;
        void *base_subobj = get_base_subobj(&__base_info[i], *obj_ptr);
        if (*__base_info[i].__base_type == *target_type) {
            if (found_subobj == nullptr) {
                found_subobj = base_subobj;
                if (~__flags & __non_diamond_repeat_mask) {
                    break;
                }
            }
            else {
                if (found_subobj != base_subobj) {
                    // ambiguous
                    return false;
                }
            }
        }
        else {
            if (!__base_info[i].__base_type->__do_vmi_upcast(target_type, base_subobj,
                    &found_subobj, __flags)) {
                return false;
            }
            if (found_subobj != nullptr && (~__flags & __non_diamond_repeat_mask)) {
                break;
            }
        }
    }

    if (found_subobj) {
        *obj_ptr = found_subobj;
        return true;
    }

    return false;
}

bool __vmi_class_type_info::__do_vmi_upcast(const __cxxabiv1::__class_type_info *target_type,
            void *current_subobj, void **found_subobj, int inh_flags) const noexcept
{
    for (unsigned i = 0; i < __base_count; ++i) {
        if (!(__base_info[i].__offset_flags & __base_class_type_info::__public_mask))
            continue;
        void *base_subobj = get_base_subobj(&__base_info[i], current_subobj);
        if (*__base_info[i].__base_type == *target_type) {
            if (*found_subobj == nullptr) {
                *found_subobj = base_subobj;
                if (~inh_flags & __non_diamond_repeat_mask) {
                    return true;
                }
            }
            else {
                if (*found_subobj != base_subobj) {
                    // ambiguous
                    return false;
                }
            }
        }
        else {
            if (!__base_info[i].__base_type->__do_vmi_upcast(target_type, base_subobj, found_subobj, inh_flags)) {
                return false;
            }
            if (*found_subobj != nullptr && (~inh_flags & __non_diamond_repeat_mask)) {
                return true;
            }
        }
    }

    return true;
}

} // namespace __cxxabiv1
