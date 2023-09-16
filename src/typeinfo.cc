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


// __class_type_info : implements type_info for classes with no base classes, and provides a base
// class for type_info structures representing classes *with* base classes.

class __class_type_info : public std::type_info
{
public:
    virtual ~__class_type_info();
    virtual bool __do_catch(const std::type_info *__thrown_type, void **__thrown_obj,
            unsigned __outer) const noexcept override;
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

    return __base_type->__do_upcast(__base_type, obj_ptr);
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
    if (*thrown_type == *this) {
        return true;
    }

    // A qualified pointer can catch a non-qualified pointer (to suitable type) but every outer
    // pointer must be const qualified (true iff outer&1 == 1).

    // first we need to check if the thrown type *is* a pointer type:
    const __pointer_type_info *thrown_ptr_type = thrown_type->__as_pointer_type();
    if (thrown_ptr_type == nullptr) {
        return false;
    }

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

} // namespace __cxxabiv1
