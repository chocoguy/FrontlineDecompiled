// EA memory manager (MEM_*): memory classes split into blocks with 0x10-byte headers,
// free blocks kept on an address-ordered list per class.
#ifndef _EA_SYS_MEM_H_
#define _EA_SYS_MEM_H_

#include "types.h"
#include "ea/real.h"

// Block magic numbers.
#define MB_MAGIC_NEW 0x424D       // 'BM' freshly made header
#define MB_MAGIC_FREE 0x4246      // 'BF' on the free list
#define MB_MAGIC_USED 0x424F      // 'BO' taken off the free list
#define MB_MAGIC_SENTINEL 0x4253  // 'BS' class bounds and the free list head

// Block and allocation flags (the low 6 bits are the memory class).
#define MB_CLASSMASK 0x3F
#define MB_TOP 0x100         // allocate from the top of the class
#define MB_LARGEST 0x200     // take the largest free block
#define MB_CLASSALIGN 0x400  // align to the class's data alignment
#define MB_FREE 0x4000       // block is on the free list
#define MB_FIXED 0x8000      // class bound block (LOW/HIGH)

// A block header (0x10 bytes); the block's data follows it.
#define MEMBLOCK_HEADER 0x10

struct MEMBLOCK {
    /* 0x00 */ u16 magic;
    /* 0x02 */ u16 flags;  // class | MB_*
    /* 0x04 */ s32 size;   // data size; for a free block the whole span up to `next`
    /* 0x08 */ MEMBLOCK* next;
    /* 0x0C */ MEMBLOCK* prev;
    // Free blocks only (in the data area):
    /* 0x10 */ MEMBLOCK* nextFree;
    /* 0x14 */ MEMBLOCK* prevFree;
};

// A memory class, kept in the data of its LOW block.
struct MEMCLASS {
    /* 0x00 */ char name[8];
    /* 0x08 */ MEMBLOCK* low;   // bound blocks at both ends
    /* 0x0C */ MEMBLOCK* high;
    /* 0x10 */ MEMBLOCK freeList;  // head of the free list (size 0x7FFFFFFF stops searches)
    /* 0x28 */ s32 blockAlign;
    /* 0x2C */ s32 dataAlign;  // used with MB_CLASSALIGN
    /* 0x30 */ s32 tailSize;   // extra bytes reserved after each block's data
    /* 0x34 */ s32 flags;
    /* 0x38 */ u8 hasMutex;
    /* 0x3C */ u8 mutex[0x1C];
    /* 0x58 */ void* osBlock;  // OS heap block holding the class (MEM_init)
};

// Header of a block from its data pointer, and back. (sizeof(MEMBLOCK) includes the free
// list links, so never use pointer arithmetic on MEMBLOCK*.)
inline MEMBLOCK* MEM_header(void* data) {
    return (MEMBLOCK*)((u8*)data - MEMBLOCK_HEADER);
}
inline void* MEM_data(MEMBLOCK* block) {
    return (u8*)block + MEMBLOCK_HEADER;
}

// Called when an allocation fails; returning 1 retries it.
typedef int (*MEM_AllocFailFn)(const char* name, int size, int flags);

extern MEMCLASS* memclass[64];  // memvars.cpp
extern int mb_default;          // mbdefault.cpp: class used for flags 0
extern MEM_AllocFailFn MEM_allocfailcallback;

int MEM_initblock(MEMBLOCK* block, const char* name, int size, int tail, int flags,
                  MEMBLOCK* prev, MEMBLOCK* next);
MEMBLOCK* FREE_find(MEMCLASS* mc, int size, int fromTop);
MEMBLOCK* FREE_findlargest(MEMCLASS* mc, int size, int fromTop);
int FREE_gettotalfree(MEMCLASS* mc, int fromTop);
void FREE_add(MEMCLASS* mc, MEMBLOCK* block);
void FREE_remove(MEMBLOCK* block);
int MEM_tailsize(const char* name, int flags);

u32 iGC_GetMaxOsMemory();
void iGC_InitMem();

extern "C" {
int MEMCLASS_create(int id, const char* name, void* base, int size, int blockAlign, int dataAlign,
                    int tailSize, bool sentinel, bool named, bool mutex);
int MEMCLASS_remove(int id);
int MEM_init(void);
void* MEM_resize(void* ptr, int size);
int MEM_size(void* ptr);
void MEM_move(void* dest, const void* src, int size);
void* __sys_alloc(u32 size);
void __sys_free(void* ptr);
}

#endif
