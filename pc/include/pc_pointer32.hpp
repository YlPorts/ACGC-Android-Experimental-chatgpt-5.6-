#ifndef PC_POINTER32_HPP
#define PC_POINTER32_HPP
#include "pc_address.h"
#include <type_traits>

/* For the reserved pointer slot in an on-disc RARC file entry. This keeps the
 * entry stride at 20 bytes. Only a process-local reference is written into that
 * slot after loading; native addresses are stored in pc_address's registry. */
template <class T> struct PcPointer32 {
    uint32_t reference;
    operator T*() const { return reinterpret_cast<T*>(pc_address_unpack(reference)); }
    template <class U> explicit operator U*() const {
        return reinterpret_cast<U*>(pc_address_unpack(reference));
    }
    explicit operator uintptr_t() const { return pc_address_unpack(reference); }
    PcPointer32& operator=(T* pointer) {
        reference = pc_address_pack(reinterpret_cast<uintptr_t>(pointer));
        return *this;
    }
};
static_assert(sizeof(PcPointer32<void>) == 4, "RARC runtime reference is a 32-bit field");
static_assert(std::is_trivially_copyable<PcPointer32<void>>::value, "RARC entry must be byte-copyable");
#endif
