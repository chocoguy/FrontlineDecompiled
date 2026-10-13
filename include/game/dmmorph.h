// dmmorph: morph target blending of DM mesh display lists (faces).
// Hand-maintained.
#ifndef _GAME_DMMORPH_H_
#define _GAME_DMMORPH_H_

#include "types.h"
#include "game_types.h"

struct DMRenderObject_T;

#define DMMORPH_MAX_TARGETS 64
#define DMMORPH_MAX_ACTIVE 4

// An object's morph target weights. size 0x108
struct DMMorphObject_T {
    /* 0x000 */ f32 m_weights[DMMORPH_MAX_TARGETS];
    /* 0x100 */ u32 m_numTargets;
    /* 0x104 */ u32 m_maxActive;  // (the blend uses at most 4 anyway)
};

// A memory buffer, freed with its owner when it allocated it. size 0x10
class CMemBuffer {
public:
    CMemBuffer() : m_data(NULL), m_size(0), m_used(0), m_owned(false) {}
    ~CMemBuffer();  // 0x800F6F9C
    void Allocate(const char* name, int size);
    /* 0x0 */ u8* m_data;
    /* 0x4 */ int m_size;
    /* 0x8 */ int m_used;
    /* 0xC */ bool m_owned;
};

// Blended display lists, cached per object (the least recently used entry
// is reused); each entry has two buffers. size 0x10
class CBlendBufferCache {
public:
    // size 0x28
    class BufferCacheData {
    public:
        BufferCacheData();  // 0x800F70D4
        ~BufferCacheData();  // 0x800F7070
        /* 0x0 */ DMRenderObject_T* m_owner;
        /* 0x4 */ int m_age;
        /* 0x8 */ CMemBuffer m_buffers[2];
    };

    CBlendBufferCache(int count, int bufferSize);
    ~CBlendBufferCache();  // 0x800F7014
    /* 0x0 */ int m_count;
    /* 0x4 */ int m_bufferSize;
    /* 0x8 */ int m_current;  // buffer in use this frame
    /* 0xC */ BufferCacheData* m_entries;
};

// Free functions
void DMMorphBlendDisplayLists(DMRenderObject_T*);  // 0x800F712C
void DMMorphSetMaxActiveTargets(DMMorphObject_T*, unsigned long);  // 0x800F7618
void DMMorphInitObject(DMMorphObject_T*, unsigned long);  // 0x800F7620

extern CBlendBufferCache _blendCache;  // 0x8031429C

#endif
