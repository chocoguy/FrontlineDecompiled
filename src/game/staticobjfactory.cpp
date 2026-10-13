// Static object factory: the level's static object templates (mesh, motion
// clip and kind per object CRC and part index), sorted for binary search,
// and fixed pools of each object class with free lists. Plain static
// objects borrow from the other pools in turn when their own pool is empty.
#include "game/staticobjfactory.h"

#include "game/static_obj.h"
#include "game/attachobject.h"
#include "game/hierobject.h"
#include "game/Tank_object.h"
#include "game/thrown_obj.h"
#include "game/staticmesh.h"
#include "game/objcreate.h"
#include "game/toilet.h"
#include "game/quaternion.h"
#include "game/Moh2.h"
#include "game/memory.h"
#include "stdlib.h"

// Template kind flags (StaticObjectTemplate::m_flags).
#define TEMPLATE_ATTACHABLE 1
#define TEMPLATE_ANIMATED 2  // the mesh is a CStaticAnimMesh
#define TEMPLATE_THROWN 4
#define TEMPLATE_HIER 8

// Level resource types.
#define RES_STATIC_OBJECT ((TLTResourceID)0xA)
#define RES_THROWN_OBJECT ((TLTResourceID)9)
#define RES_POOL_SIZES_B ((TLTResourceID)0xB)
#define RES_POOL_SIZES ((TLTResourceID)0xD)
#define RES_STATIC_MESH ((TLTResourceID)0x13)
#define RES_HIER_OBJECT ((TLTResourceID)0x15)

// The thrown-object property resource every thrown prop uses.
#define THROWN_PROPERTIES_CRC 0x7D06EB90

// A tank and its AI filter (TANK_AI_FILTER_OFFSET) share one 0x1000 slot.
struct STankSlot {
    CTankObject m_tank;
    u8 m_ai[0x1000 - sizeof(CTankObject)];
};

// One object definition (size 0x2C). The mesh is constructed in place.
struct StaticObjectTemplate {
    StaticObjectTemplate() { m_meshType = -1; }
    ~StaticObjectTemplate() {
        if (GetMesh() != NULL) {
            GetMesh()->~CStaticMesh();
        }
        m_meshType = -1;
    }

    CStaticMesh* GetMesh() { return m_meshType >= 0 ? (CStaticMesh*)m_mesh : NULL; }
    int NumFrames() {
        return (m_meshType >= 0 && (m_meshType & TEMPLATE_ANIMATED)) ? ((CStaticAnimMesh*)m_mesh)->numFrames() : 0;
    }
    int TicksPerFrame() {
        return (m_meshType >= 0 && (m_meshType & TEMPLATE_ANIMATED)) ? ((CStaticAnimMesh*)m_mesh)->ticksPerFrame() : 0;
    }

    // Loads the mesh file and builds the mesh (animated when the flags say so).
    void LoadMesh(const char* file) {
        int size;
        bool loaded;
        void* data = TLT_LoadFromRepository(file, &size, &loaded);
        if (m_flags & TEMPLATE_ANIMATED) {
            m_meshType = TEMPLATE_ANIMATED;
            CStaticAnimMesh* mesh = new (m_mesh) CStaticAnimMesh;
            mesh->Init(data, !loaded);
        } else {
            m_meshType = 0;
            CStaticMesh* mesh = new (m_mesh) CStaticMesh;
            mesh->Init(data, !loaded);
        }
    }

    void LoadMotion(const char* file) {
        if (file != NULL) {
            int size = 0;
            bool loaded = false;
            m_pMotion = (MotionData*)TLT_LoadFromRepository(file, &size, &loaded);
            if (!loaded) {
                CStaticObjectFactory::PatchUpMVDFile(m_pMotion);
            }
        } else {
            m_pMotion = NULL;
        }
    }

    /* 0x00 */ int m_meshType;  // -1 none, 0 CStaticMesh, 2 CStaticAnimMesh
    /* 0x04 */ u8 m_mesh[0x10];
    /* 0x14 */ int m_crc;
    /* 0x18 */ int m_flags;
    /* 0x1C */ MotionData* m_pMotion;
    /* 0x20 */ LevelFileContentsStruct_* m_pResource;
    /* 0x24 */ LevelFileContentsStruct_* m_pThrownResource;  // thrown props
    /* 0x28 */ int m_index;  // part index of hier objects, else -1
};

static CStaticObject** g_ppAllocatorFreeLists[3];
CStaticObject* g_pStaticObjectGrenade;
CStaticObject* g_pStaticObjectBazooka;
CStaticObject* g_pStaticObjectStickGrenade;
CStaticObject* g_pStaticObjectKnife;
CStaticObject* g_pStaticObjectWrench;
static int g_iStaticAIObjectPoolSize;
static int g_iStaticObjectPoolSize;
static int g_iAttachObjectPoolSize;
static int g_iHierObjectPoolSize;
static int g_numTemplates;
static int g_iMaxThrownObjects;
static int g_iMaxRecycledThrownObjects;
static int g_iRecycledThrownObjectsIndex;
static StaticObjectTemplate* g_pStaticObjectTemplates;
static int g_currentAlternateAllocatorArrayIndex;
static CStaticObject* g_pStaticObjectArray;
static CHierObject* g_pHierObjectArray;
static CStaticObject* g_pHierObjectFreeList;
static STankSlot* g_pStaticAIObjectArray;
static CStaticObject* g_pStaticAIObjectFreeList;
static CAttachableObject* g_pAttachableObjectArray;
static CThrownObject* g_pThrownObjectArray;
static CThrownObject* g_pRecycledThrownObjectArray;
static CStaticObject* g_pStaticObjectFreeList;
static CStaticObject* g_pAttachableObjectFreeList;
static CStaticObject* g_pThrownObjectFreeList;

static inline u32 Swap32(u32 v) {
    return (v << 24) | ((v & 0xFF00) << 8) | ((v >> 8) & 0xFF00) | (v >> 24);
}

static inline void SwapWord(void* p) {
    *(u32*)p = Swap32(*(u32*)p);
}

// Byte-swaps a motion clip; relative clips store per-frame deltas, which
// become positions.
void CStaticObjectFactory::PatchUpMVDFile(MotionData* motion) {
    SwapWord(&motion->m_numFrames);
    SwapWord(&motion->m_frameTime);
    SwapWord(&motion->m_relative);
    for (int i = 0; i < motion->m_numFrames; i++) {
        CStaticObject::MotionFrame& frame = motion->m_frames[i];
        SwapWord(&frame.m_pos.x);
        SwapWord(&frame.m_pos.y);
        SwapWord(&frame.m_pos.z);
        frame.m_rot.EndianSwap();
    }
    if (motion->m_relative) {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        for (int i = 0; i < motion->m_numFrames; i++) {
            CStaticObject::MotionFrame& frame = motion->m_frames[i];
            z += frame.m_pos.z;
            x += frame.m_pos.x;
            y += frame.m_pos.y;
            frame.m_pos.x = x;
            frame.m_pos.y = y;
            frame.m_pos.z = z;
        }
    }
}

static inline StaticObjectTemplate* FindTemplate(StaticObjectTemplate& key) {
    return (StaticObjectTemplate*)bsearch(&key, g_pStaticObjectTemplates, g_numTemplates,
                                          sizeof(StaticObjectTemplate), CompareStaticObjectDefinitions);
}

// Helmets whose thrown-object properties don't say otherwise stop bullets.
int CStaticObjectFactory::DoesHelmetTypeStopBullets(int crc) {
    StaticObjectTemplate key;
    key.m_crc = crc;
    key.m_index = -1;
    return FindTemplate(key)->m_pThrownResource->fields[20].value == 0;
}

void CStaticMesh::EnableFogging(bool on) {
    m_fogging = on;
}

static inline void SetMeshOptions(StaticObjectTemplate& t, LevelFileContentsStruct_* res) {
    if (res->fields[3].value != 0) {
        t.GetMesh()->EnableLighting(false);
    }
    if (res->fields[4].value != 0) {
        t.GetMesh()->EnableFogging(false);
    }
}

// Links a pool's objects into its free list.
template <class T>
static inline CStaticObject* LinkPool(T* pool, int count) {
    for (int i = 0; i < count - 1; i++) {
        pool[i].m_pNextFree = &pool[i + 1];
    }
    pool[count - 1].m_pNextFree = NULL;
    return pool;
}

static inline void LinkPools() {
    g_pStaticObjectFreeList = g_pStaticObjectArray;
    g_pHierObjectFreeList = g_pHierObjectArray;
    g_pStaticAIObjectFreeList = &g_pStaticAIObjectArray->m_tank;
    g_pAttachableObjectFreeList = g_pAttachableObjectArray;
    g_pThrownObjectFreeList = g_pThrownObjectArray;
    LinkPool(g_pStaticObjectArray, g_iStaticObjectPoolSize);
    LinkPool(g_pHierObjectArray, g_iHierObjectPoolSize);
    LinkPool(g_pAttachableObjectArray, g_iAttachObjectPoolSize);
    LinkPool(g_pThrownObjectArray, g_iMaxThrownObjects);
    for (int i = 0; i < g_iStaticAIObjectPoolSize - 1; i++) {
        g_pStaticAIObjectArray[i].m_tank.m_pNextFree = &g_pStaticAIObjectArray[i + 1].m_tank;
    }
    g_pStaticAIObjectArray[g_iStaticAIObjectPoolSize - 1].m_tank.m_pNextFree = NULL;
}

// The weapon props every level keeps ready.
static inline void AllocateSpecialObjects() {
    g_currentAlternateAllocatorArrayIndex = 0;
    g_ppAllocatorFreeLists[0] = &g_pHierObjectFreeList;
    g_ppAllocatorFreeLists[1] = &g_pAttachableObjectFreeList;
    g_ppAllocatorFreeLists[2] = &g_pThrownObjectFreeList;
    g_pStaticObjectGrenade = CStaticObjectFactory::AllocateStaticObject(0xA55DDA2D, -1);
    g_pStaticObjectStickGrenade = CStaticObjectFactory::AllocateStaticObject(0x83674487, -1);
    g_pStaticObjectKnife = CStaticObjectFactory::AllocateStaticObject(0x7B81B72C, -1);
    g_pStaticObjectWrench = CStaticObjectFactory::AllocateStaticObject(0x60531A56, -1);
    g_pStaticObjectBazooka = CStaticObjectFactory::AllocateStaticObject(0xD23D51CF, -1);
}

// Builds the templates from the level's resources and allocates the pools.
void CStaticObjectFactory::Init() {
    LevelFileContentsStruct_* res;
    for (res = TLT_FindNextResourceByType(RES_STATIC_OBJECT, NULL); res != NULL;
         res = TLT_FindNextResourceByType(RES_STATIC_OBJECT, res)) {
        if (res->fields[0].value != 0) {
            g_numTemplates++;
        }
    }
    for (res = TLT_FindNextResourceByType(RES_STATIC_MESH, NULL); res != NULL;
         res = TLT_FindNextResourceByType(RES_STATIC_MESH, res)) {
        g_numTemplates++;
    }
    for (res = TLT_FindNextResourceByType(RES_THROWN_OBJECT, NULL); res != NULL;
         res = TLT_FindNextResourceByType(RES_THROWN_OBJECT, res)) {
        if (res->fields[7].value != 0) {
            g_numTemplates++;
        }
    }
    for (res = TLT_FindNextResourceByType(RES_HIER_OBJECT, NULL); res != NULL;
         res = TLT_FindNextResourceByType(RES_HIER_OBJECT, res)) {
        g_numTemplates += res->fields[0].value;
    }

    if (g_numTemplates > 0) {
        g_pStaticObjectTemplates = new StaticObjectTemplate[g_numTemplates];
        int n = 0;
        for (res = TLT_FindNextResourceByType(RES_STATIC_OBJECT, NULL); res != NULL;
             res = TLT_FindNextResourceByType(RES_STATIC_OBJECT, res)) {
            if (res->fields[0].value == 0) {
                continue;
            }
            StaticObjectTemplate& t = g_pStaticObjectTemplates[n];
            t.m_crc = res->fields[1].value;
            t.m_flags = res->fields[2].value;
            t.m_pResource = res;
            t.m_index = -1;
            t.LoadMotion((const char*)res->fields[5].value);
            t.LoadMesh((const char*)res->fields[0].value);
            SetMeshOptions(t, res);
            n++;
        }
        for (res = TLT_FindNextResourceByType(RES_STATIC_MESH, NULL); res != NULL;
             res = TLT_FindNextResourceByType(RES_STATIC_MESH, res)) {
            StaticObjectTemplate& t = g_pStaticObjectTemplates[n];
            t.m_crc = res->fields[7].value;
            t.m_flags = 0;
            t.m_pMotion = NULL;
            t.m_pResource = res;
            t.m_index = -1;
            DebugMsg("Loading static mesh file %s\n", (const char*)res->fields[2].value);
            t.LoadMesh((const char*)res->fields[2].value);
            n++;
        }
        LevelFileContentsStruct_* thrownProps =
            TLT_FindNextResourceByTypeAndVal(RES_STATIC_OBJECT, 1, THROWN_PROPERTIES_CRC, NULL);
        for (res = TLT_FindNextResourceByType(RES_THROWN_OBJECT, NULL); res != NULL;
             res = TLT_FindNextResourceByType(RES_THROWN_OBJECT, res)) {
            if (res->fields[7].value == 0) {
                continue;
            }
            StaticObjectTemplate& t = g_pStaticObjectTemplates[n];
            t.m_crc = res->fields[14].value;
            t.m_flags = TEMPLATE_THROWN;
            t.m_pMotion = NULL;
            t.m_pResource = thrownProps;
            t.m_pThrownResource = res;
            t.m_index = -1;
            int size;
            bool loaded;
            void* data = TLT_LoadFromRepository((const char*)res->fields[7].value, &size, &loaded);
            t.m_meshType = 0;
            CStaticMesh* mesh = new (t.m_mesh) CStaticMesh;
            mesh->Init(data, !loaded);
            n++;
        }
        // Hier objects: one template per part (three fields each).
        for (res = TLT_FindNextResourceByType(RES_HIER_OBJECT, NULL); res != NULL;
             res = TLT_FindNextResourceByType(RES_HIER_OBJECT, res)) {
            for (int i = 0; i < res->fields[0].value; i++) {
                StaticObjectTemplate& t = g_pStaticObjectTemplates[n];
                t.m_crc = res->fields[1].value;
                t.m_flags = res->fields[6 + 3 * i].value;
                t.m_flags = t.m_flags | TEMPLATE_HIER;
                t.m_pResource = res;
                t.m_index = i;
                if (res->fields[7 + 3 * i].value != 0) {
                    t.LoadMotion((const char*)res->fields[7 + 3 * i].value);
                } else {
                    t.m_pMotion = NULL;
                }
                t.LoadMesh((const char*)res->fields[5 + 3 * i].value);
                SetMeshOptions(t, res);
                n++;
            }
        }
    }

    g_iMaxRecycledThrownObjects = TLT_FindNextResourceByType(RES_POOL_SIZES_B, NULL)->fields[58].value;
    LevelFileContentsStruct_* sizes = TLT_FindNextResourceByType(RES_POOL_SIZES, NULL);
    g_iStaticObjectPoolSize = sizes->fields[60].value;
    g_iAttachObjectPoolSize = sizes->fields[61].value;
    g_iHierObjectPoolSize = sizes->fields[62].value;
    g_iStaticAIObjectPoolSize = sizes->fields[63].value;
    g_iMaxThrownObjects = sizes->fields[64].value;
    g_pStaticObjectArray = new CStaticObject[g_iStaticObjectPoolSize];
    g_pHierObjectArray = new CHierObject[g_iHierObjectPoolSize];
    g_pStaticAIObjectArray = new STankSlot[g_iStaticAIObjectPoolSize];
    g_pAttachableObjectArray = new CAttachableObject[g_iAttachObjectPoolSize];
    g_pThrownObjectArray = new CThrownObject[g_iMaxThrownObjects];
    g_pRecycledThrownObjectArray = new CThrownObject[g_iMaxRecycledThrownObjects];
    LinkPools();
    qsort(g_pStaticObjectTemplates, g_numTemplates, sizeof(StaticObjectTemplate),
          CompareStaticObjectDefinitions);
    AllocateSpecialObjects();
}

// Empties the pools for a new level.
void CStaticObjectFactory::Reset() {
    LinkPools();
    g_iRecycledThrownObjectsIndex = 0;
    AllocateSpecialObjects();
}

MotionData* CStaticObjectFactory::GetMotionData(int crc, int index) {
    StaticObjectTemplate key;
    key.m_crc = crc;
    key.m_index = index;
    StaticObjectTemplate* t = FindTemplate(key);
    if (t == NULL) {
        return NULL;
    }
    return t->m_pMotion;
}

CStaticMesh* CStaticObjectFactory::GetStaticMesh(int crc, int index) {
    StaticObjectTemplate key;
    key.m_crc = crc;
    key.m_index = index;
    StaticObjectTemplate* t = FindTemplate(key);
    if (t == NULL) {
        return NULL;
    }
    return t->GetMesh();
}

// Returns the object to its pool. Plain static objects may live in any
// pool's memory, so they are put back by address.
void CStaticObjectFactory::DeallocateStaticObject(CStaticObject* obj) {
    DebugMsg("Deallocating static object %x <<<d %x>>>\n", obj, obj);
    switch (obj->GetObjectType()) {
    case 8:
        if (obj->AsHierTankObject() != NULL) {
            obj->m_pNextFree = g_pStaticAIObjectFreeList;
            g_pStaticAIObjectFreeList = obj;
            return;
        }
        obj->m_pNextFree = g_pHierObjectFreeList;
        g_pHierObjectFreeList = obj;
        return;
    case 0:
        if ((u8*)obj >= (u8*)g_pStaticObjectArray &&
            (u8*)obj < (u8*)(g_pStaticObjectArray + g_iStaticObjectPoolSize)) {
            obj->m_pNextFree = g_pStaticObjectFreeList;
            g_pStaticObjectFreeList = obj;
            return;
        }
        if ((u8*)obj >= (u8*)g_pAttachableObjectArray &&
            (u8*)obj < (u8*)(g_pAttachableObjectArray + g_iAttachObjectPoolSize)) {
            obj->m_pNextFree = g_pAttachableObjectFreeList;
            g_pAttachableObjectFreeList = obj;
            return;
        }
        if ((u8*)obj >= (u8*)g_pThrownObjectArray &&
            (u8*)obj < (u8*)(g_pThrownObjectArray + g_iMaxThrownObjects)) {
            obj->m_pNextFree = g_pThrownObjectFreeList;
            g_pThrownObjectFreeList = obj;
            return;
        }
        if ((u8*)obj >= (u8*)g_pHierObjectArray &&
            (u8*)obj < (u8*)(g_pHierObjectArray + g_iHierObjectPoolSize)) {
            obj->m_pNextFree = g_pHierObjectFreeList;
            g_pHierObjectFreeList = obj;
            return;
        }
        return;
    case 1:
        obj->m_pNextFree = g_pAttachableObjectFreeList;
        g_pAttachableObjectFreeList = obj;
        return;
    case 4: {
        CThrownObject* thrown = obj->AsThrownObject();
        if (thrown->m_recyclable) {
            thrown->SetDeleted(true);
            return;
        }
        obj->m_pNextFree = g_pThrownObjectFreeList;
        g_pThrownObjectFreeList = obj;
        break;
    }
    }
}

static inline CStaticObject* TakeFree(CStaticObject*& list) {
    CStaticObject* obj = list;
    list = obj->m_pNextFree;
    return obj;
}

// Recyclable thrown props reuse the pool round robin, skipping ones still
// in use; when all are in use the next one is destroyed and NULL returned.
static inline CStaticObject* TakeRecycledThrown() {
    int max = g_iMaxRecycledThrownObjects;
    int index = g_iRecycledThrownObjectsIndex % max;
    if (g_iRecycledThrownObjectsIndex >= max) {
        int stop = max - 1;
        if (index > 0) {
            stop = index - 1;
        }
        while (index != stop && !g_pRecycledThrownObjectArray[index].m_deleted) {
            g_iRecycledThrownObjectsIndex++;
            index = index == max - 1 ? 0 : index + 1;
        }
        if (index == stop) {
            g_pRecycledThrownObjectArray[index].MarkForDestruction(0);
            return NULL;
        }
    } else {
        g_iRecycledThrownObjectsIndex++;
    }
    return &g_pRecycledThrownObjectArray[index];
}

// Constructs an object of class T from template `t` in `memory`.
template <class T>
static inline T* Construct(CStaticObject* memory, StaticObjectTemplate* t) {
    if (memory == NULL) {
        return NULL;
    }
    CStaticMesh* mesh = t->GetMesh();
    int frames = t->NumFrames();
    int ticks = t->TicksPerFrame();
    MotionData* motion = t->m_pMotion;
    if (motion != NULL) {
        return new (memory) T(mesh, frames, ticks, motion->m_numFrames, motion->m_frameTime,
                              motion->m_relative, motion->m_frames);
    }
    return new (memory) T(mesh, frames, ticks, 0, 0, 0, NULL);
}

CStaticObject* CStaticObjectFactory::AllocateStaticObject(int crc, int index) {
    StaticObjectTemplate key;
    key.m_crc = crc;
    key.m_index = index;
    StaticObjectTemplate* t = FindTemplate(key);
    CStaticObject* obj;

    if (t->m_flags & TEMPLATE_HIER) {
        if (index == 0 && t->m_pResource->fields[2].value != 0) {
            if (g_pStaticAIObjectFreeList == NULL) {
                DebugMsg("Static object factory memory is used up.  Unable to allocate hierobject with AI 0x%x\n", crc);
                return NULL;
            }
            obj = TakeFree(g_pStaticAIObjectFreeList);
            DebugMsg("Allocating static object CRC: %x, obj num %d, returned %x <<<a AIObj %x,%x>>>\n", crc,
                     index, obj, crc, obj);
            return Construct<CTankObject>(obj, t);
        }
        if (g_pHierObjectFreeList == NULL) {
            DebugMsg("Static object factory memory is used up.  Unable to allocate hierobject without AI 0x%x\n", crc);
            return NULL;
        }
        obj = TakeFree(g_pHierObjectFreeList);
        DebugMsg("Allocating static object CRC: %x, obj num %d, returned %x <<<a Hier %x,%x>>>\n", crc, index,
                 obj, crc, obj);
        return Construct<CHierObject>(obj, t);
    }
    if (t->m_flags & TEMPLATE_ATTACHABLE) {
        if (g_pAttachableObjectFreeList == NULL) {
            DebugMsg("Attachable static object factory memory is used up.  Unable to allocate object 0x%x\n", crc);
            return NULL;
        }
        obj = TakeFree(g_pAttachableObjectFreeList);
        DebugMsg("Allocating static object CRC: %x, obj num %d, returned %x <<<a Attach %x,%x>>>\n", crc, index,
                 obj, crc, obj);
        return Construct<CAttachableObject>(obj, t);
    }
    if (t->m_flags & TEMPLATE_THROWN) {
        if (t->m_pResource->fields[9].value != 0) {
            obj = TakeRecycledThrown();
            if (obj == NULL) {
                return NULL;
            }
        } else {
            // Like the original, no check for an empty pool.
            obj = TakeFree(g_pThrownObjectFreeList);
        }
        DebugMsg("Allocating static object CRC: %x, obj num %d, returned %x <<<a Thrown %x,%x>>>\n", crc, index,
                 obj, crc, obj);
        if (obj == NULL) {
            return NULL;
        }
        return new (obj) CThrownObject(t->GetMesh(), t->NumFrames(), t->TicksPerFrame(), 0, 0, 0, NULL,
                                       t->m_pResource);
    }

    // Plain objects: their own pool, then the other pools in turn, then
    // after destroying the oldest dump object.
    CStaticObject** list;
    if (g_pStaticObjectFreeList != NULL) {
        list = &g_pStaticObjectFreeList;
    } else if (*(list = g_ppAllocatorFreeLists[g_currentAlternateAllocatorArrayIndex]) != NULL) {
    } else if (*(list = g_ppAllocatorFreeLists[(g_currentAlternateAllocatorArrayIndex + 1) % 3]) != NULL) {
    } else if (*(list = g_ppAllocatorFreeLists[(g_currentAlternateAllocatorArrayIndex + 2) % 3]) != NULL) {
    } else {
        DestroyOldestDumpObject();
        list = g_pStaticObjectFreeList != NULL ? &g_pStaticObjectFreeList : NULL;
    }
    if (list != NULL) {
        obj = TakeFree(*list);
        g_currentAlternateAllocatorArrayIndex++;
        if (g_currentAlternateAllocatorArrayIndex >= 3) {
            g_currentAlternateAllocatorArrayIndex = 0;
        }
    } else {
        obj = NULL;
    }
    if (obj == NULL) {
        DebugMsg("Static object factory memory is used up.  Unable to allocate object 0x%x\n", crc);
        return NULL;
    }
    DebugMsg("Allocating static object CRC: %x, obj num %d, returned %x <<<a Normal %x,%x>>>\n", crc, index, obj,
             crc, obj);
    return Construct<CStaticObject>(obj, t);
}

// Templates are sorted by CRC, then part index.
int CompareStaticObjectDefinitions(const void* a, const void* b) {
    const StaticObjectTemplate* ta = (const StaticObjectTemplate*)a;
    const StaticObjectTemplate* tb = (const StaticObjectTemplate*)b;
    if (ta->m_crc < tb->m_crc) {
        return -1;
    }
    if (ta->m_crc > tb->m_crc) {
        return 1;
    }
    if (ta->m_index < tb->m_index) {
        return -1;
    }
    return ta->m_index > tb->m_index;
}
