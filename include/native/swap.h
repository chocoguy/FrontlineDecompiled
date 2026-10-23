// Native (little-endian) build only: in-place byte swaps for loaders of
// GameCube-only (big-endian) file formats that the CPU reads directly. The
// GameCube build reads those as-is, so it never includes this. Data the GPU
// reads (vertex arrays, display lists, texels) stays big-endian; see
// FrontlineRewritten's docs/architecture/0003-byte-order.md.
#ifndef _NATIVE_SWAP_H_
#define _NATIVE_SWAP_H_

#include <stdint.h>
#include <string.h>

static inline uint32_t NativeLoad32(const void* p) {
    uint32_t v;
    memcpy(&v, p, 4);
    return v;
}

static inline void NativeSwap32(void* p) {
    uint32_t v = __builtin_bswap32(NativeLoad32(p));
    memcpy(p, &v, 4);
}

static inline void NativeSwap16(void* p) {
    uint16_t v;
    memcpy(&v, p, 2);
    v = __builtin_bswap16(v);
    memcpy(p, &v, 2);
}

// `count` consecutive 4-byte (or 2-byte) fields.
static inline void NativeSwap32s(void* p, uint32_t count) {
    uint8_t* b = (uint8_t*)p;
    for (uint32_t i = 0; i < count; i++) {
        NativeSwap32(b + i * 4);
    }
}

static inline void NativeSwap16s(void* p, uint32_t count) {
    uint8_t* b = (uint8_t*)p;
    for (uint32_t i = 0; i < count; i++) {
        NativeSwap16(b + i * 2);
    }
}

#ifdef __cplusplus
#include <stdlib.h>
// For swap passes over structures that can share records: true the first time
// a record is seen, so it's swapped exactly once. (A small open-addressing
// set; the game declares its own operator new, so no std containers here.)
struct NativeSwapOnce {
    const void** slots;
    uint32_t capacity, size;
    NativeSwapOnce() : slots(0), capacity(0), size(0) {}
    ~NativeSwapOnce() { free(slots); }
    bool first(const void* p) {
        if ((size + 1) * 2 > capacity) {
            grow();
        }
        uint32_t i = hash(p) & (capacity - 1);
        while (slots[i] != 0) {
            if (slots[i] == p) {
                return false;
            }
            i = (i + 1) & (capacity - 1);
        }
        slots[i] = p;
        size++;
        return true;
    }
    // `count` fields from p, each swapped the first time it's seen. Shared
    // arrays can overlap only partly (one list a prefix of another), so these
    // track fields, not arrays.
    void swap32s(void* p, uint32_t count) {
        uint8_t* b = (uint8_t*)p;
        for (uint32_t i = 0; i < count; i++) {
            if (first(b + i * 4)) {
                NativeSwap32(b + i * 4);
            }
        }
    }
    void swap16s(void* p, uint32_t count) {
        uint8_t* b = (uint8_t*)p;
        for (uint32_t i = 0; i < count; i++) {
            if (first(b + i * 2)) {
                NativeSwap16(b + i * 2);
            }
        }
    }

private:
    static uint32_t hash(const void* p) {
        uint64_t v = (uint64_t)(uintptr_t)p * 0x9E3779B97F4A7C15ull;
        return (uint32_t)(v >> 32);
    }
    void grow() {
        uint32_t oldCap = capacity;
        const void** old = slots;
        capacity = capacity ? capacity * 2 : 64;
        slots = (const void**)calloc(capacity, sizeof(void*));
        size = 0;
        for (uint32_t i = 0; i < oldCap; i++) {
            if (old[i] != 0) {
                first(old[i]);
            }
        }
        free(old);
    }
};
#endif

#endif
