#include "game/memory.h"

#include <string.h>
#include "ea/real.h"
#include "game/MallocInit.h"

#ifdef FRONTLINE_NATIVE
#include <stdlib.h>
#include "native/hooks.h"
// Natively these replace operator new/delete only inside the executable (the
// native build unexports them, so Apple's frameworks keep the system
// allocator). Calls from threads other than the game's (the platform layer's
// audio and file threads) also use the system allocator, since EA's heap
// isn't thread-safe. See FrontlineRewritten docs/architecture/0001 and 0002.
#define GAME_NEW_FALLBACK(size)                    \
    if (!NativeOnGameThread()) {                   \
        return calloc(1, (size) != 0 ? (size) : 1); \
    }
#define GAME_DELETE_FALLBACK(ptr)                                  \
    if ((uptr)(ptr) - g_nativeArenaBase >= g_nativeArenaSize) {    \
        free(ptr);                                                 \
        return;                                                    \
    }                                                              \
    if (!NativeOnGameThread()) {                                   \
        NativeFreeLater(ptr);                                      \
        return;                                                    \
    }
#else
#define GAME_NEW_FALLBACK(size)
#define GAME_DELETE_FALLBACK(ptr)
#endif

void* DWI_allocalign(const char* name, int size, int align, int flags) {
    void* ptr;

    if (!g_bMallocInitialized) {
        mallocinit();
    }
    ptr = MEM_allocalign(name, size, align, 0, flags);
    memset(ptr, 0, size);
    return ptr;
}

void* DWI_alloc(const char* name, int size, int flags) {
    void* ptr;

    if (!g_bMallocInitialized) {
        mallocinit();
    }
    ptr = MEM_alloc(name, size, flags);
    memset(ptr, 0, size);
    return ptr;
}

void operator delete[](void* ptr) {
    GAME_DELETE_FALLBACK(ptr)
    MEM_free(ptr);
}

void operator delete(void* ptr) {
    GAME_DELETE_FALLBACK(ptr)
    MEM_free(ptr);
}

void* operator new[](unsigned long size, char* name) {
    void* ptr;
    GAME_NEW_FALLBACK(size)

    if (!g_bMallocInitialized) {
        mallocinit();
    }
    ptr = MEM_alloc(name, size, 0x400);
    memset(ptr, 0, size);
    memset(ptr, 0, size);
    return ptr;
}

void* operator new[](unsigned long size) {
    void* ptr;
    GAME_NEW_FALLBACK(size)

    if (!g_bMallocInitialized) {
        mallocinit();
    }
    ptr = MEM_alloc("Default Debug Heap", size, 0x400);
    memset(ptr, 0, size);
    memset(ptr, 0, size);
    return ptr;
}

void* operator new(unsigned long size) {
    void* ptr;
    GAME_NEW_FALLBACK(size)

    if (!g_bMallocInitialized) {
        mallocinit();
    }
    ptr = MEM_alloc("Default Debug Heap", size, 0x400);
    memset(ptr, 0, size);
    memset(ptr, 0, size);
    return ptr;
}
