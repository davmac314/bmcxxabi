#include "../include/typeinfo"

namespace __cxxabiv1 {

// This source file is compiled with -frtti. That allows use of typeid, so we can get the
// type_info object for nullptr_t, which turns out to be handy.
const std::type_info *get_npti()
{
    return &typeid(decltype(nullptr));
}

}
