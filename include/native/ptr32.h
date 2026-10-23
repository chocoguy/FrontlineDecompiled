// Native (64-bit) build only: ptr32<T>, a pointer stored in 32 bits.
//
// Structs laid over loaded file data declare their pointer fields with
// PTR32(T) (see types.h): a plain T* on GameCube, a ptr32<T> natively, so the
// struct keeps its file layout. The stored value is a GameCube-style address
// (see native/address.h), which is also what the field holds before the loader
// relocates it: a small file offset. `(u32)field` reads that raw value on both
// builds, so relocation code reads the same either way.
#ifndef _NATIVE_PTR32_H_
#define _NATIVE_PTR32_H_

#include <stddef.h>
#include <stdint.h>

#include "native/address.h"

extern "C++" {

template <class T>
class ptr32 {
public:
    // Trivial, so file structs stay trivially copyable and can sit in unions.
    ptr32() = default;
    ptr32(T* p) : m_addr(NativePtrToAddr((const void*)p)) {}
    ptr32& operator=(T* p) {
        m_addr = NativePtrToAddr((const void*)p);
        return *this;
    }

    T* get() const { return (T*)NativeAddrToPtr(m_addr); }
    operator T*() const { return get(); }
    T* operator->() const { return get(); }
    template <class U = T>
    U& operator*() const {
        return *(U*)get();
    }

    // (Foo*)field, as a C-style cast to an unrelated pointer type.
    template <class U>
    explicit operator U*() const {
        return (U*)NativeAddrToPtr(m_addr);
    }

    // The raw 32-bit value, as `(u32)field` gave on GameCube.
    explicit operator uint32_t() const { return m_addr; }
    explicit operator int32_t() const { return (int32_t)m_addr; }
    uint32_t addr() const { return m_addr; }
    void setAddr(uint32_t addr) { m_addr = addr; }

    ptr32& operator+=(ptrdiff_t n) { return *this = get() + n; }
    ptr32& operator-=(ptrdiff_t n) { return *this = get() - n; }
    ptr32& operator++() { return *this = get() + 1; }
    ptr32& operator--() { return *this = get() - 1; }
    T* operator++(int) {
        T* old = get();
        *this = old + 1;
        return old;
    }
    T* operator--(int) {
        T* old = get();
        *this = old - 1;
        return old;
    }

private:
    uint32_t m_addr;
};

// PTR_TO_ADDR(field) on a PTR32 field: the stored 32 bits, without a round
// trip through a native pointer. Loaders compare and add fields that still hold
// file offsets, which aren't valid addresses yet.
template <class T>
inline uint32_t NativePtrToAddr(const ptr32<T>& p) {
    return p.addr();
}

// Relocates a file field in place: adds `base` (an address) to the offset it
// holds. Only 4-byte fields qualify, so a field left as a native pointer is a
// compile error instead of a corrupted struct.
template <class T>
inline void NativeRelocate(ptr32<T>& p, uint32_t base) {
    p.setAddr(p.addr() + base);
}
inline void NativeRelocate(uint32_t& v, uint32_t base) { v += base; }
inline void NativeRelocate(int32_t& v, uint32_t base) { v += base; }

}  // extern "C++"

#endif
