#include "game/memory.h"

#include <string.h>
#include "ea/real.h"
#include "game/MallocInit.h"

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
    MEM_free(ptr);
}

void operator delete(void* ptr) {
    MEM_free(ptr);
}

void* operator new[](unsigned long size, char* name) {
    void* ptr;

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

    if (!g_bMallocInitialized) {
        mallocinit();
    }
    ptr = MEM_alloc("Default Debug Heap", size, 0x400);
    memset(ptr, 0, size);
    memset(ptr, 0, size);
    return ptr;
}
