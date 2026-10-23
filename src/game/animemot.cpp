// Animation files (.emt, exported by eMotion): relocation and byte-swapping
// after loading, the compact copy kept in the animation database (frames
// go to ARAM), and queries by state.
#include "game/animemot.h"

#include <stdio.h>
#include <string.h>

#include "dolphin/os.h"
#include "dolphin/vi.h"
#include "game/Moh2.h"
#include "game/anim_callback.h"
#include "game/animdb.h"
#include "game/animfile.h"
#include "game/animst.h"
#include "game/memory.h"

extern "C" {
void* MEM_resize(void* block, int size);
int MEM_size(void* block);
int MEM_totalunused(int pool);
u32 ARAM_alloc(void* pool, const char* name, u32 size, int direction);

typedef void (*ARQCallback)(u32 request);
struct ARQRequest {
    /* 0x00 */ ARQRequest* next;
    /* 0x04 */ u32 owner;
    /* 0x08 */ u32 type;
    /* 0x0C */ u32 priority;
    /* 0x10 */ u32 source;
    /* 0x14 */ u32 dest;
    /* 0x18 */ u32 length;
    /* 0x1C */ ARQCallback callback;
};
void ARQPostRequest(ARQRequest* request, u32 owner, u32 type, u32 priority, u32 source, u32 dest,
                    u32 length, ARQCallback callback);
void DCFlushRange(void* addr, u32 size);
}

#define ARQ_TYPE_MRAM_TO_ARAM 0
#define ARQ_TYPE_ARAM_TO_MRAM 1
#define ARQ_PRIORITY_HIGH 1

#define FRAME_CACHE_SIZE 0xC800
#define COPY_MEMORY_GROW 100000

// Frames streamed back from ARAM (a ring; wraps when full). ARAM DMA works
// on 32-byte blocks, so the cache must be 32-byte aligned (as the
// original's is) or every frame lands at the wrong offset.
u8 g_frame_cache[FRAME_CACHE_SIZE] __attribute__((aligned(32)));
s32 g_frame_cache_used;
volatile u8 g_bDMAToARAM;
volatile u8 g_bDMAToMRAM;
u32 end;  // tick of the last finished transfer
static ARQRequest g_toARAM_request __attribute__((aligned(32)));
static ARQRequest g_fromARAM_request;

// The database's copies of the files' meta-data are carved out of one
// block (grown on demand).
s32 g_amount_allocated;
s32 g_num_allocations;
u8* g_anim_copy_memory;
u32 g_anim_copy_memory_sz;
u32 g_anim_copy_memory_cur;

static inline void Swap16(void* p) {
#ifdef FRONTLINE_NATIVE
    return;  // little-endian data on a little-endian CPU: nothing to swap
#endif
    u8* b = (u8*)p;
    u8 t = b[0];
    b[0] = b[1];
    b[1] = t;
}

static inline void Swap32(void* p) {
#ifdef FRONTLINE_NATIVE
    return;  // little-endian data on a little-endian CPU: nothing to swap
#endif
    u8* b = (u8*)p;
    u8 b0 = b[0];
    u8 b3 = b[3];
    u8 b1 = b[1];
    u8 b2 = b[2];
    b[0] = b3;
    b[3] = b0;
    b[1] = b2;
    b[2] = b1;
}

inline void EndianSwap(AnimFileFormat_t& file) {
    Swap32(&file.magic);
    Swap32(&file.version);
    Swap16(&file.objectType);
    Swap16(&file.fileNum);
    Swap16(&file.numFrames);
    Swap16(&file.field_0xE);
    Swap32(&file.states);
    Swap32(&file.anims);
    Swap32(&file.frames);
}

static inline u8* CopyAlloc(u32 size) {
    g_num_allocations++;
    g_amount_allocated += size;
    if (size & 3) {
        size = (~size & 3) + size + 1;
    }
    if (g_anim_copy_memory_cur + size > g_anim_copy_memory_sz) {
        DebugMsg("Growing animation data array\n");
        if (g_anim_copy_memory == NULL) {
            g_anim_copy_memory_sz += COPY_MEMORY_GROW;
            g_anim_copy_memory = (u8*)DWI_alloc("Animation meta-data", COPY_MEMORY_GROW, 0x400);
        } else {
            g_anim_copy_memory_sz += COPY_MEMORY_GROW;
            g_anim_copy_memory_sz = MEM_size(MEM_resize(g_anim_copy_memory, g_anim_copy_memory_sz));
        }
    }
    u8* p = g_anim_copy_memory + g_anim_copy_memory_cur;
    g_anim_copy_memory_cur += size;
    return p;
}

// Copies a block whose size (after its 4-byte header) is in its second
// halfword (callback data, transitions).
static inline void* CopySizedBlock(void* src) {
    if (src == NULL) {
        return NULL;
    }
    u32 size = ((u16*)src)[1] + 4;
    u8* dst = CopyAlloc(size);
    memcpy(dst, src, size);
    return dst;
}

// Length (in halfwords) of an animation's program: up to its frame info,
// else up to the next animation (or the list, for the last one).
static inline int ProgramLength(AnimEmotAnimation_t* anim, AnimEmotAnimList_t* list, u32 index) {
    if (anim->frameInfo != NULL) {
        return (u32)((u8*)anim->frameInfo - (u8*)anim->program) >> 1;
    }
    int size;
    if (index < list->count - 1) {
        size = (u8*)list->anims[index + 1] - (u8*)list->anims[index];
    } else {
        size = (u8*)list - (u8*)list->anims[index];
    }
    return (u32)(size - 10) >> 1;
}

AnimFileFormat_t* CopyAnimFileFormatStructureToDynMem(AnimFileFormat_t* file) {
    int freeBefore = MEM_totalunused(0);
    DebugMsg("Memory Free before: %d\n", freeBefore);
    g_anim_copy_memory_sz = (int)(0.2 * MEM_size(file));
    g_anim_copy_memory = (u8*)DWI_alloc("Animation meta-data", g_anim_copy_memory_sz, 0x400);
    g_anim_copy_memory_cur = 0;
    AnimFileFormat_t* copy = (AnimFileFormat_t*)CopyAlloc(sizeof(AnimFileFormat_t));
    copy->magic = file->magic;
    copy->version = file->version;
    copy->objectType = file->objectType;
    copy->fileNum = file->fileNum;
    copy->numFrames = file->numFrames;
    copy->states = ADDR_TO_PTR(AnimEmotStateList_t, CopyStateList(file));
    copy->anims = ADDR_TO_PTR(AnimEmotAnimList_t, CopyAnimList(file));
    PTR32(u8)* frames = (PTR32(u8)*)CopyAlloc((file->numFrames + 1) * sizeof(PTR32(u8)));
    for (int i = 0; i < file->numFrames; i++) {
        frames[i] = file->frames[i];
    }
    DebugMsg("Frame Data start %x end %x size %d\n", frames[0], file->frames,
             (u8*)file->frames - frames[0]);
    copy->frames = frames;
    g_anim_copy_memory_sz = MEM_size(MEM_resize(g_anim_copy_memory, g_anim_copy_memory_cur));
    DebugMsg("Allocated %d Bytes for Anim File copy (%d allocations)\n", g_amount_allocated,
             g_num_allocations);
    int freeAfter = MEM_totalunused(0);
    DebugMsg("Memory Free after: %d (diff %d)\n", freeAfter, freeBefore - freeAfter);
    return copy;
}

int CopyAnimList(AnimFileFormat_t* file) {
    AnimEmotAnimList_t* dst = (AnimEmotAnimList_t*)CopyAlloc((file->anims->count - 1) * 4 + 8);
    dst->count = file->anims->count;
    for (u32 i = 0; i < dst->count; i++) {
        dst->anims[i] = ADDR_TO_PTR(AnimEmotAnimation_t, CopyEmotAnimation(file->anims->anims[i], file, i));
    }
    return (int)PTR_TO_ADDR(dst);
}

int CopyEmotAnimation(AnimEmotAnimation_t* anim, AnimFileFormat_t* file, int index) {
    int n = ProgramLength(anim, file->anims, index);
    AnimEmotAnimation_t* dst = (AnimEmotAnimation_t*)CopyAlloc((n - 1) * 2 + 0xC);
    dst->frameTime = anim->frameTime;
    dst->unk_0x1 = anim->unk_0x1;
    dst->flags = anim->flags;
    dst->unk_0x3 = anim->unk_0x3;
    dst->frameInfo = (AnimFrameInfo_t*)CopyCompressedData((u8*)anim->frameInfo);
    for (int i = 0; i < n; i++) {
        dst->program[i] = anim->program[i];
    }
    return (int)PTR_TO_ADDR(dst);
}

void* CopyCompressedData(unsigned char* data) {
    if (data == NULL) {
        return NULL;
    }
    AnimFrameInfo_t* src = (AnimFrameInfo_t*)data;
    if (src->magic != ANIMFRAMEINFO_MAGIC) {
        return NULL;
    }
    AnimFrameInfo_t* dst = (AnimFrameInfo_t*)CopyAlloc(sizeof(AnimFrameInfo_t));
    dst->magic = src->magic;

    // Base pose: a count, then 3 halfwords per joint.
    u16* pose = (u16*)src->basePose;
    u16* poseCopy = (u16*)CopyAlloc((pose[0] - 1) * 6 + 8);
    poseCopy[0] = pose[0];
    for (int i = 0; i < pose[0]; i++) {
        poseCopy[1 + i * 3] = pose[1 + i * 3];
        poseCopy[2 + i * 3] = pose[2 + i * 3];
        poseCopy[3 + i * 3] = pose[3 + i * 3];
    }
    dst->basePose = (u8*)poseCopy;

    AnimJointGroups_t* groups = src->groups;
    AnimJointGroups_t* groupsCopy = (AnimJointGroups_t*)CopyAlloc((groups->count - 1) * 2 + 4);
    groupsCopy->count = groups->count;
    for (int i = 0; i < groupsCopy->count; i++) {
        groupsCopy->groups[i].start = groups->groups[i].start;
        groupsCopy->groups[i].count = groups->groups[i].count;
    }
    dst->groups = groupsCopy;
    return dst;
}

int CopyStateList(AnimFileFormat_t* file) {
    AnimEmotStateList_t* dst =
        (AnimEmotStateList_t*)CopyAlloc((file->states->count - 1) * 0x14 + 0x18);
    dst->count = file->states->count;
    for (int i = 0; i < (int)file->states->count; i++) {
        CopyStateDef(&dst->states[i], &file->states->states[i]);
    }
    return (int)PTR_TO_ADDR(dst);
}

void CopyStateDef(AnimEmotStateDef_t* dst, AnimEmotStateDef_t* src) {
    dst->state = src->state;
    dst->field_0x2 = src->field_0x2;
    dst->callback = src->callback;

    AnimEmotStateAnims_t* anims = src->anims;
    AnimEmotStateAnims_t* animsCopy = (AnimEmotStateAnims_t*)CopyAlloc((anims->count - 1) * 8 + 0xC);
    animsCopy->count = anims->count;
    for (u32 i = 0; i < animsCopy->count; i++) {
        animsCopy->anims[i].animIndex = anims->anims[i].animIndex;
        animsCopy->anims[i].plugin = (AnimFilePluginData_t*)CopySizedBlock(anims->anims[i].plugin);
    }
    dst->anims = animsCopy;

    dst->info = (AnimFilePluginData_t*)CopySizedBlock(src->info);

    AnimTranList_t* trans = src->transitions;
    AnimTranList_t* transCopy = (AnimTranList_t*)CopyAlloc((trans->count - 1) * 0x10 + 0x14);
    transCopy->count = trans->count;
    for (u32 i = 0; i < (u32)transCopy->count; i++) {
        AnimTranEntry_t* e = &transCopy->entries[i];
        AnimTranEntry_t* s = &trans->entries[i];
        e->key = s->key;
        e->sub = s->sub;
        e->cond = s->cond;
        e->lo = s->lo;
        e->hi = s->hi;
        e->transition = (AnimTransition_t*)CopySizedBlock(s->transition);
    }
    dst->transitions = transCopy;
}

// Loads (a 32-byte aligned stretch of) one frame from ARAM into the frame
// cache and returns it.
void* AnimFileGetFrame(AnimFileFormat_t* file, u16 frame) {
    u32 start = (u32)PTR_TO_ADDR(file->frames[frame]);
    u32 aligned = start & ~0x1F;
    u32 offset = start - aligned;
    s32 size = ((u32)PTR_TO_ADDR(file->frames[frame + 1]) - start + offset + 0x1F) & ~0x1F;
    if (size > FRAME_CACHE_SIZE - g_frame_cache_used) {
        g_frame_cache_used = 0;
    }
    u8* dst = g_frame_cache + g_frame_cache_used;
    OSGetTick();
    DCFlushRange(dst, size);
    g_bDMAToMRAM = 1;
    ARQPostRequest(&g_fromARAM_request, 0, ARQ_TYPE_ARAM_TO_MRAM, ARQ_PRIORITY_HIGH, aligned,
                   (u32)PTR_TO_ADDR(dst), size, DMAFromARAMCallback);
    while (g_bDMAToMRAM != 0) {
        SPIN_WAIT();
    }
    g_frame_cache_used += size;
    return dst + offset;
}

// Moves the frame data [first, end) to ARAM; the copy's frame table then
// holds ARAM addresses.
void UploadFramesToARAM(AnimFileFormat_t* copy, unsigned char* first, unsigned char* last) {
    u8* aligned = (u8*)((uptr)first & ~0x1F);
    u32 dataSize = last - first;
    u32 offset = first - aligned;
    u32 size = (dataSize + offset + 0x1F) & ~0x1F;
    u32 aram = ARAM_alloc(g_ARAM_poolman, "Animation", size, 0);
    u32 startTick = OSGetTick();
    DCFlushRange(aligned, size);
    g_bDMAToARAM = 1;
    ARQPostRequest(&g_toARAM_request, 0, ARQ_TYPE_MRAM_TO_ARAM, ARQ_PRIORITY_HIGH, (u32)PTR_TO_ADDR(aligned),
                   aram, size, DMAToARAMCallback);
    while (g_bDMAToARAM != 0) {
        VIWaitForRetrace();
    }
    DebugMsg("Transfered %d bytes of animation frame data to ARAM in %f seconds\n", size,
             (float)(end - startTick) / (float)(OS_BUS_CLOCK >> 2));
    for (int i = 0; i < copy->numFrames; i++) {
        copy->frames[i] = ADDR_TO_PTR(u8, aram + (copy->frames[i] - first) + offset);
    }
    copy->frames[copy->numFrames] = ADDR_TO_PTR(u8, (aram + (dataSize + offset)));
}

void DMAToARAMCallback(u32) {
    end = OSGetTick();
    g_bDMAToARAM = 0;
}

void DMAFromARAMCallback(u32) {
    end = OSGetTick();
    g_bDMAToMRAM = 0;
}

static inline AnimEmotStateDef_t* FindState(AnimFileFormat_t* file, unsigned long state) {
    AnimEmotStateList_t* list = file->states;
    for (u32 i = 0; i < list->count; i++) {
        if (list->states[i].state == state) {
            return &list->states[i];
        }
    }
    return NULL;
}

void* AnimFileGetStateInfo(AnimFileFormat_t* file, unsigned long state) {
    AnimEmotStateList_t* list = file->states;
    for (u32 i = 0; i < list->count; i++) {
        if (list->states[i].state == state) {
            return list->states[i].info;
        }
    }
    return NULL;
}

void AnimFileGetStateTransList(AnimFileFormat_t* file, unsigned long state,
                               AnimTranTransListArray_t* out) {
    if (out->count >= out->capacity) {
        return;
    }
    AnimEmotStateList_t* list = file->states;
    for (u32 i = 0; i < list->count; i++) {
        if (list->states[i].state == state) {
            AnimTranList_t* trans = list->states[i].transitions;
            if (trans == NULL || trans->count == 0) {
                return;
            }
            out->lists[out->count].file = file;
            out->lists[out->count].fileNum = file->fileNum;
            out->lists[out->count].list = file->states->states[i].transitions;
            out->count++;
            return;
        }
    }
}

void AnimFileGetStateAnimList(AnimFileFormat_t* file, unsigned long state,
                              AnimFileStateAnimList_t* out) {
    if (out->count >= out->capacity) {
        return;
    }
    AnimEmotStateList_t* list = file->states;
    for (u32 i = 0; i < list->count; i++) {
        if (list->states[i].state == state) {
            for (u32 j = 0; j < file->states->states[i].anims->count; j++) {
                if (out->count >= out->capacity) {
                    return;
                }
                out->anims[out->count].file = file;
                out->anims[out->count].animIndex =
                    file->states->states[i].anims->anims[j].animIndex;
                out->anims[out->count].plugin = file->states->states[i].anims->anims[j].plugin;
                out->count++;
            }
            return;
        }
    }
}

s32 AnimFileGetStateCallbackID(AnimFileFormat_t* file, unsigned long state) {
    AnimEmotStateList_t* list = file->states;
    for (u32 i = 0; i < list->count; i++) {
        if (list->states[i].state == state) {
            return list->states[i].callback;
        }
    }
    return -1;
}

AnimSeqFrameList_t* AnimFileGetAnimation(AnimFileFormat_t* file, u16 index) {
    return (AnimSeqFrameList_t*)file->anims->anims[index];
}

u16 AnimFileGetFileNum(AnimFileFormat_t* file) { return file->fileNum; }

u16 AnimFileGetObjectType(AnimFileFormat_t* file) { return file->objectType; }

#ifdef FRONTLINE_NATIVE
#define RELOCATE(base, p) NativeRelocate((p), PTR_TO_ADDR(base))
#else
#define RELOCATE(base, p) (*(u32*)&(p) += (u32)(base))
#endif

// Byte-swaps a loaded .emt file and turns its offsets into pointers.
void AnimFileRelocate(void* data, const char* name) {
    AnimFileFormat_t* file = (AnimFileFormat_t*)data;
    EndianSwap(*file);
    if (file->magic != ANIMFILE_MAGIC) {
        if (name == NULL) {
            name = "no_name_specified";
        }
        printf("The EMT file %s was exported with an old version of eMotion\n", name);
    } else if (file->version != ANIMFILE_VERSION) {
        printf("The EMT file %s was exported with an different version of eMotion\n", name);
        printf("EMT version = %d, code version = %d", file->version, ANIMFILE_VERSION);
    }

    RELOCATE(file, file->states);
    RELOCATE(file, file->anims);
    RELOCATE(file, file->frames);
    for (int i = 0; i < file->numFrames; i++) {
        Swap32(&file->frames[i]);
        RELOCATE(file, file->frames[i]);
    }

    // States: their animations (with callback data), state data and
    // transitions.
    Swap32(&file->states->count);
    for (int i = 0; i < (int)file->states->count; i++) {
        AnimEmotStateDef_t* st = &file->states->states[i];
        Swap16(&st->state);
        Swap16(&st->field_0x2);
        Swap32(&st->callback);
        Swap32(&st->anims);
        Swap32(&st->info);
        Swap32(&st->transitions);
        st->callback = AnimStGetCallbackIndexFromID(st->callback);

        RELOCATE(file, st->anims);
        AnimEmotStateAnims_t* anims = st->anims;
        Swap32(&anims->count);
        for (u32 j = 0; j < anims->count; j++) {
            Swap16(&anims->anims[j].animIndex);
            Swap16(&anims->anims[j].field_0x2);
            Swap32(&anims->anims[j].plugin);
        }
        for (int j = 0; j < (int)st->anims->count; j++) {
            if (st->anims->anims[j].plugin != NULL) {
                RELOCATE(file, st->anims->anims[j].plugin);
                EndianSwapAnimPlugIn(*st->anims->anims[j].plugin, st->callback);
            }
        }
        if (st->info != NULL) {
            RELOCATE(file, st->info);
            EndianSwapAnimPlugIn(*st->info, st->callback);
        }

        RELOCATE(file, st->transitions);
        AnimTranList_t* trans = st->transitions;
        Swap32(&trans->count);
        for (u32 j = 0; j < (u32)trans->count; j++) {
            AnimTranEntry_t* e = &trans->entries[j];
            Swap16(&e->key);
            Swap16(&e->sub);
            Swap16(&e->cond);
            Swap16(&e->lo);
            Swap16(&e->hi);
            Swap32(&e->transition);
        }
        for (int j = 0; j < st->transitions->count; j++) {
            if (st->transitions->entries[j].transition != NULL) {
                RELOCATE(file, st->transitions->entries[j].transition);
                u8* t = (u8*)st->transitions->entries[j].transition;
                Swap16(t + 0x0);
                Swap16(t + 0x2);
                Swap16(t + 0x4);
                Swap16(t + 0x6);
                Swap32(t + 0x8);
                Swap16(t + 0xC);
                Swap16(t + 0xE);
            }
        }
    }

    // Animations: programs and frame infos.
    AnimEmotAnimList_t* list = file->anims;
    Swap32(&list->count);
    for (u32 i = 0; i < list->count; i++) {
        Swap32(&list->anims[i]);
    }
    for (u32 i = 0; i < file->anims->count; i++) {
        RELOCATE(file, file->anims->anims[i]);
        AnimEmotAnimation_t* anim = file->anims->anims[i];
        Swap32(&anim->frameInfo);
        if (file->anims->anims[i]->frameInfo == NULL) {
            continue;
        }
        RELOCATE(file, file->anims->anims[i]->frameInfo);
        Swap32(&file->anims->anims[i]->frameInfo->magic);
        anim = file->anims->anims[i];
        int n = ProgramLength(anim, file->anims, i);
        for (int j = 0; j < n; j++) {
            Swap16(&anim->program[j]);
        }
        AnimFrameInfo_t* info = file->anims->anims[i]->frameInfo;
        if (info->magic == ANIMFRAMEINFO_MAGIC) {
            Swap32(&info->basePose);
            Swap32(&info->groups);
            RELOCATE(file, info->basePose);
            RELOCATE(file, info->groups);
            Swap16(&info->groups->count);
            Swap16(info->basePose);
        }
    }
    // Animations without frame info: their programs run up to the next.
    for (u32 i = 0; i < file->anims->count; i++) {
        AnimEmotAnimation_t* anim = file->anims->anims[i];
        if (anim->frameInfo != NULL) {
            continue;
        }
        int n = ProgramLength(anim, file->anims, i);
        for (int j = 0; j < n; j++) {
            Swap16(&anim->program[j]);
        }
    }
}
