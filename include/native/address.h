// Native (64-bit) build only: pointers as 32-bit GameCube-style addresses.
//
// The game keeps pointers in 32-bit places: pointer fields in loaded files,
// script and sound state slots, DMA addresses. Natively such a value is an
// address in a GameCube-like map:
//
//   0                              null
//   [0x00010000, arena size)       the memory arena that holds every game heap
//   [0x80000000, +image size)      the executable image (code, statics, literals)
//
// So `(u32)ptr` round-trips for heap and static data, alignment tests still
// work (both bases are page aligned), and the platform layer can turn a DMA
// address back into a pointer.
//
// The arena sits at the bottom of a reserved 4 GB window, and every other
// 32-bit value maps into the rest of it (inaccessible). The game also passes
// plain numbers where a pointer goes (a float's bits as script event data, an
// ARAM address as a void*); those convert to a pointer and back unchanged, and
// trap only if dereferenced. A real pointer outside the window and the image
// (a stack address, a host allocation) traps when converted. FrontlineRewritten's
// platform layer sets the bases; see its docs/architecture/0001-pointers-and-memory.md.
#ifndef _NATIVE_ADDRESS_H_
#define _NATIVE_ADDRESS_H_

#include <stdint.h>

#define NATIVE_IMAGE_ADDR 0x80000000u
#define NATIVE_WINDOW_SIZE 0x100000000ull

#ifdef __cplusplus
extern "C" {
#endif

extern uintptr_t g_nativeArenaBase;  // the window's base (0 until the arena is set up)
extern uint32_t g_nativeArenaSize;
extern uintptr_t g_nativeImageBase;
extern uint32_t g_nativeImageSize;

// Slow paths: find the image range on first use (static initializers run
// before the platform layer starts), and trap on pointers with no address.
uint32_t NativePtrToAddrSlow(const void* p);
void* NativeAddrToPtrSlow(uint32_t addr);

#ifdef __cplusplus
}
#endif

static inline uint32_t NativePtrToAddr(const volatile void* p) {
    uintptr_t v = (uintptr_t)p;
    // The window (or, before the arena exists, null: macOS maps nothing below
    // 4 GB). Null is in the slow path otherwise.
    if (v - g_nativeArenaBase < NATIVE_WINDOW_SIZE) {
        return (uint32_t)(v - g_nativeArenaBase);
    }
    if (v - g_nativeImageBase < g_nativeImageSize) {
        return NATIVE_IMAGE_ADDR + (uint32_t)(v - g_nativeImageBase);
    }
    return NativePtrToAddrSlow((const void*)v);
}

static inline void* NativeAddrToPtr(uint32_t addr) {
    if (addr == 0) {
        return 0;
    }
    if (addr - NATIVE_IMAGE_ADDR < g_nativeImageSize) {
        return (void*)(g_nativeImageBase + (addr - NATIVE_IMAGE_ADDR));
    }
    if (g_nativeArenaBase != 0 && g_nativeImageSize != 0) {
        return (void*)(g_nativeArenaBase + addr);
    }
    return NativeAddrToPtrSlow(addr);
}

#ifdef __cplusplus
// Function pointers don't convert to void* implicitly in C++.
extern "C++" {
template <class R, class... A>
inline uint32_t NativePtrToAddr(R (*f)(A...)) {
    return NativePtrToAddr((const void*)f);
}
}
#endif

#endif
