// dmmorph: morph target blending. An object's (up to four) strongest
// morph targets are blended into a copy of its base part's display list,
// with the positions and normals written directly instead of indexed.
#include "game/dmmorph.h"
#include "game/gxstore.h"
#include "game/dmrender.h"
#include "game/dmesh.h"
#include "game/memory.h"
#include "ea/real.h"
#include "math.h"
#include "string.h"

CBlendBufferCache _blendCache(0x10, 0x4C00);

static u8 s_listPad = 0;

inline void CMemBuffer::Allocate(const char* name, int size) {
    if (m_owned) {
        MEM_free(m_data);
    }
    m_data = (u8*)DWI_alloc(name, size, 0x400);
    m_size = size;
    m_used = 0;
    m_owned = true;
}

CBlendBufferCache::CBlendBufferCache(int count, int bufferSize) {
    m_count = count;
    m_bufferSize = bufferSize;
    m_current = 0;
    m_entries = new BufferCacheData[count];
    for (int i = 0; i < count; i++) {
        m_entries[i].m_buffers[0].Allocate("Animation memory buffer", m_bufferSize);
        m_entries[i].m_buffers[1].Allocate("Animation memory buffer", m_bufferSize);
    }
}

CMemBuffer::~CMemBuffer() {
    if (m_owned) {
        MEM_free(m_data);
    }
}

CBlendBufferCache::~CBlendBufferCache() {
    delete[] m_entries;
}

CBlendBufferCache::BufferCacheData::~BufferCacheData() {}

CBlendBufferCache::BufferCacheData::BufferCacheData() {
    m_owner = NULL;
    m_age = 0;
}

// A selected target and its weight.
struct SMorphTarget {
    u32 m_target;
    f32 m_weight;
};

// A blended vertex (unaligned in the list). size 0xF
struct SBlendVertex {
    u8 m_0;
    s16 m_pos[3];
    s16 m_nrm[3];
    u16 m_6;
};

void DMMorphBlendDisplayLists(DMRenderObject_T* obj) {
    SBlendVertex v;
    SMorphTarget targets[DMMORPH_MAX_ACTIVE + 1];
    u32 numTargets = 0;
    DMMorphObject_T* morph = obj->m_morph;
    CDMesh* mesh = obj->m_model;

    // The strongest targets (weights from 0.05).
    if (morph != NULL) {
        for (u32 t = 0; t < morph->m_numTargets - 1; t++) {
            f32 w = morph->m_weights[t];
            if (w >= 0.05f) {
                if (numTargets < DMMORPH_MAX_ACTIVE) {
                    targets[numTargets].m_target = t;
                    numTargets++;
                    targets[numTargets - 1].m_weight = w;
                } else {
                    int weakest = numTargets - 1;
                    f32 weakestWeight = (f32)fabs(targets[numTargets - 1].m_weight);
                    for (u32 i = 0; i < numTargets; i++) {
                        f32 a = fabs(targets[i].m_weight);
                        if (a < weakestWeight) {
                            weakest = i;
                            weakestWeight = a;
                        }
                    }
                    if (fabs(w) > weakestWeight) {
                        targets[weakest].m_target = t;
                        targets[weakest].m_weight = w;
                    }
                }
            }
        }
    }
    targets[numTargets].m_target = 0;
    targets[numTargets].m_weight = 1.0f;
    if (numTargets + 1 == 0) {
        return;
    }

    // The object's cache entry (or the oldest/free one).
    CDMeshPart* parts = mesh->m_parts;
    CDMeshDisplayList* base = parts[0].m_lists;
    int pick = 0;
    int pickAge = 0;
    CMemBuffer* buffer = NULL;
    CBlendBufferCache::BufferCacheData* entry = _blendCache.m_entries;
    for (int i = 0; i < _blendCache.m_count; i++, entry++) {
        if (entry->m_owner == obj) {
            _blendCache.m_entries[i].m_age = 0;
            buffer = &_blendCache.m_entries[i].m_buffers[_blendCache.m_current];
            break;
        }
        if (entry->m_owner == NULL) {
            pick = i;
            pickAge = 0x7FFFFFFF;
        } else if (entry->m_age > pickAge) {
            pick = i;
            pickAge = entry->m_age;
        }
    }
    if (buffer == NULL) {
        _blendCache.m_entries[pick].m_owner = obj;
        _blendCache.m_entries[pick].m_age = 0;
        buffer = &_blendCache.m_entries[pick].m_buffers[_blendCache.m_current];
    }

    buffer->m_used = 0;
    obj->m_parts[0].m_list = buffer->m_data;
    int pairOffset = 0;
    u8* cmd = base->data;
    u8* end = base->data + (base->size << 5);
    while (*cmd != 0 && cmd != end) {
        // Primitive header: op byte and vertex count.
        u8* out = buffer->m_data + buffer->m_used;
        out[0] = cmd[0];
        *(u16*)(out + 1) = *(u16*)(cmd + 1);
        buffer->m_used += 3;
        u8* vtx = cmd + 3;
        u8* vtxEnd = vtx + GX_LOAD16(cmd + 1) * 7;
        int pair = pairOffset;
        for (; vtx != vtxEnd; vtx += 7) {
            s16* pos = mesh->m_positions[GX_LOAD16(vtx + 1)];
            s16* nrm = mesh->m_normals[GX_LOAD16(vtx + 3)];
            v.m_0 = vtx[0];
            v.m_pos[0] = GX_LOADS16(&pos[0]);
            v.m_pos[1] = GX_LOADS16(&pos[1]);
            v.m_pos[2] = GX_LOADS16(&pos[2]);
            v.m_nrm[0] = GX_LOADS16(&nrm[0]);
            v.m_nrm[1] = GX_LOADS16(&nrm[1]);
            v.m_nrm[2] = GX_LOADS16(&nrm[2]);
            v.m_6 = *(u16*)(vtx + 5);
            for (u32 i = 0; i < numTargets; i++) {
                f32 w = targets[i].m_weight;
                u16* indices = (u16*)(parts[targets[i].m_target].m_lists->data + pair);
                s16* tp = mesh->m_positions[GX_LOAD16(&indices[0])];
                s16* tn = mesh->m_normals[GX_LOAD16(&indices[1])];
                v.m_pos[0] += (int)((f32)GX_LOADS16(&tp[0]) * w);
                v.m_pos[1] += (int)((f32)GX_LOADS16(&tp[1]) * w);
                v.m_pos[2] += (int)((f32)GX_LOADS16(&tp[2]) * w);
                v.m_nrm[0] += (int)((f32)GX_LOADS16(&tn[0]) * w);
                v.m_nrm[1] += (int)((f32)GX_LOADS16(&tn[1]) * w);
                v.m_nrm[2] += (int)((f32)GX_LOADS16(&tn[2]) * w);
            }
            pair += 4;
            pairOffset += 4;
            u8* o = buffer->m_data + buffer->m_used;
            o[0] = v.m_0;
            GX_STORE16(o + 1, v.m_pos[0]);
            GX_STORE16(o + 3, v.m_pos[1]);
            GX_STORE16(o + 5, v.m_pos[2]);
            GX_STORE16(o + 7, v.m_nrm[0]);
            GX_STORE16(o + 9, v.m_nrm[1]);
            GX_STORE16(o + 0xB, v.m_nrm[2]);
            *(u16*)(o + 0xD) = v.m_6;
            buffer->m_used += 0xF;
        }
        cmd = vtxEnd;
    }
    // Pad to 32 bytes.
    while (buffer->m_used & 0x1F) {
        buffer->m_data[buffer->m_used] = s_listPad;
        buffer->m_used++;
    }
    obj->m_parts[0].m_listSize = buffer->m_used;
}

void DMMorphSetMaxActiveTargets(DMMorphObject_T* morph, unsigned long max) {
    morph->m_maxActive = max;
}

void DMMorphInitObject(DMMorphObject_T* morph, unsigned long numTargets) {
    morph->m_numTargets = numTargets;
    for (u32 i = 0; i < morph->m_numTargets; i++) {
        morph->m_weights[i] = 0.0f;
    }
}
