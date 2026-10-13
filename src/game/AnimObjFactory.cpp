// CAnimObjectFactory: character definitions and the animated object pool
// (see AnimObjFactory.h).
#include "game/AnimObjFactory.h"
#include "game/anim_obj.h"
#include "game/Soldier_object.h"
#include "game/animplayer.h"
#include "game/player_weapon_object.h"
#include "game/location_collision.h"
#include "game/skeleton.h"
#include "game/dmesh.h"
#include "game/staticmesh.h"
#include "game/texpack.h"
#include "game/sprite.h"
#include "game/box.h"
#include "game/toilet.h"
#include "game/endian.h"
#include "game/memory.h"
#include "game/MathFun.h"
#include "game/ShellMenu.h"
#include "game/Moh2.h"
#include "ea/real.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

// Level resource types.
#define RES_TEXTURES 0xD
#define RES_CHARACTER 9
#define RES_PLAYER_WEAPON 0x13
#define RES_STATIC_OBJECT 0xA

// The value of field `i` of a level resource.
#define RES_FIELD(res, i) ((res)->fields[i].value)
#define RES_FLOAT(res, i) (*(f32*)&(res)->fields[i].value)
#define RES_STRING(res, i) ((char*)(res)->fields[i].value)

// Static object meshes (for the random helmets cheat).
static int g_staticObjCount;

// A mesh file this executable can use.
static inline bool MeshFormatOK(CDMesh* mesh) {
    return mesh->m_version == 6 && mesh->field_0x5 == 1 && mesh->m_format == 3;
}

// Is `res` a static object with a .msh mesh?
static inline bool IsMeshObject(LevelFileContentsStruct_* res) {
    char* file = RES_STRING(res, 0);
    return file != NULL && stricmp(&file[strlen(file)] - 3, "msh") == 0;
}

void CAnimObjectFactory::SetLargestObjectSize(int size) {
    int rem = size & 0xF;
    if (rem != 0) {
        size += 0x10 - rem;
    }
    m_slotSize = size;
}

void CAnimObjectFactory::ReleaseObject(CAnimObject* obj) {
    obj->m_pNextFree = m_pFree;
    m_pFree = obj;
}

// Takes a free slot and builds an object of class `cls` from definition
// `crc` in it (NULL when the pool is empty).
CAnimObject* CAnimObjectFactory::AllocateObject(unsigned long crc, EObjectClass cls) {
    CAnimObject* obj = NULL;
    CAnimObject* slot = m_pFree;
    if (slot == NULL) {
        return obj;
    }
    obj = slot;
    m_pFree = slot->m_pNextFree;
    CAnimObjDef key;
    key.m_crc = crc;
    CAnimObjDef* def = (CAnimObjDef*)bsearch(&key, m_defs, m_numDefs, sizeof(CAnimObjDef), CompareObjectDefinitions);
    switch ((int)cls) {
    case OBJCLASS_RAW:
        break;
    case OBJCLASS_ANIM:
        obj = new (slot) CAnimObject;
        break;
    case OBJCLASS_SOLDIER:
        obj = new (slot) CSoldierObject;
        break;
    case OBJCLASS_ANIMATED_PLAYER:
        obj = new (slot) CAnimatedPlayer;
        break;
    case OBJCLASS_PLAYER_WEAPON: {
        CPlayerWeaponObject* weapon = new (slot) CPlayerWeaponObject;
        LevelFileContentsStruct_* res = def->m_pResource;
        obj = weapon;
        CVector3 v;
        v.x = RES_FLOAT(res, 9);
        v.y = RES_FLOAT(res, 10);
        v.z = RES_FLOAT(res, 11);
        weapon->m_offset = v;
        v.x = RES_FLOAT(res, 12);
        v.y = RES_FLOAT(res, 13);
        v.z = RES_FLOAT(res, 14);
        *(CVector3*)&weapon->m_pitch = v;
        v.x = RES_FLOAT(res, 15);
        v.y = RES_FLOAT(res, 16);
        v.z = RES_FLOAT(res, 17);
        if (!(v.x != 0.0f)) {
            v.x = 1.0f;
        }
        if (v.y == 0.0f) {
            v.y = 1.0f;
        }
        if (v.z == 0.0f) {
            v.z = 1.0f;
        }
        weapon->m_scale = v;
        weapon->m_fovScale = RES_FLOAT(res, 19) > 0.0f ? RES_FLOAT(res, 19) : 1.0f;
        // Its sprites are built in the slot right after the object.
        u8* sprite = (u8*)obj + sizeof(CPlayerWeaponObject);
        weapon->field_0x31A4 = RES_FIELD(res, 18);
        weapon->m_hideMode = RES_FIELD(res, 20);
        weapon->m_pSprite0 = NULL;
        if (RES_STRING(res, 3) != NULL) {
            weapon->m_pSprite0 = ::new (sprite) CSprite(RES_STRING(res, 3), (CSprite::ESpriteLoadType)1, (ERenderPriority)0xA);
            sprite += sizeof(CSprite);
        }
        weapon->m_pSprite1 = NULL;
        if (RES_STRING(res, 4) != NULL) {
            weapon->m_pSprite1 = ::new (sprite) CSprite(RES_STRING(res, 4), (CSprite::ESpriteLoadType)1, (ERenderPriority)7);
            sprite += sizeof(CSprite);
        }
        weapon->m_pSprite2 = NULL;
        if (RES_STRING(res, 5) != NULL) {
            weapon->m_pSprite2 = ::new (sprite) CSprite(RES_STRING(res, 5), (CSprite::ESpriteLoadType)1, (ERenderPriority)0xA);
        }
        float f24 = RES_FLOAT(res, 24);
        if (f24 == 0.0f) {
            f24 = 1.0f;
        }
        weapon->field_0x318C = RES_FLOAT(res, 23);
        weapon->field_0x3190 = RES_FLOAT(res, 22);
        weapon->field_0x3194 = f24;
        weapon->m_flashScale = RES_FLOAT(res, 21);
        weapon->field_0x3184 = RES_FLOAT(res, 25);
        weapon->field_0x3188 = RES_FLOAT(res, 26);
        weapon->m_flashParticles = RES_FIELD(res, 8);
        weapon->SetWeaponName(RES_STRING(res, 6));
        break;
    }
    }
    obj->Init();
    obj->m_anim.BindSkeleton(def->m_pSkel);
    obj->m_anim.BindMesh(def->m_pMesh);
    obj->BindLocationCollision(def->m_pLocData, def->m_pLocMap);
    if ((int)cls == OBJCLASS_SOLDIER) {
        CSoldierObject* soldier = (CSoldierObject*)obj;
        soldier->m_face.BindMesh(def->m_pFaceMesh);
        soldier->m_face.BindSkeleton(def->m_pSkel);
        soldier->m_moodTarget = def->m_moodTarget;
        soldier->SetNodeState(1, def->m_nodeState);
        soldier->AttachMesh(def->m_pHelmet, "head", 0);
        soldier->SetHelmetOffset(def->m_helmetOffset);
        soldier->m_pBodyTexture = def->m_pBodyTexture;
        soldier->m_pFaceTexture = def->m_pFaceTexture;
        soldier->m_bodyTexturePart = def->m_bodyPart;
        LevelFileContentsStruct_* res = def->m_pResource;
        if (RES_FIELD(res, 15) != 0) {
            soldier->m_helmetHitScale = (float)RES_FIELD(res, 17) / 100.0f;
            soldier->m_helmetChance = RES_FIELD(def->m_pResource, 18);
            soldier->field_0x43A4 = RES_FIELD(def->m_pResource, 16);
        }
        soldier->m_shadow.m_enabled = RES_FIELD(def->m_pResource, 19) == 0;
    } else if ((int)cls == OBJCLASS_ANIMATED_PLAYER) {
        CSoldierObject* soldier = (CSoldierObject*)obj;
        soldier->m_face.BindMesh(def->m_pFaceMesh);
        soldier->m_face.BindSkeleton(def->m_pSkel);
        soldier->m_moodTarget = def->m_moodTarget;
        soldier->SetNodeState(1, def->m_nodeState);
        soldier->AttachMesh(def->m_pHelmet, "head", 0);
        soldier->SetHelmetOffset(def->m_helmetOffset);
        soldier->m_pBodyTexture = def->m_pBodyTexture;
        soldier->m_pFaceTexture = def->m_pFaceTexture;
        soldier->m_bodyTexturePart = def->m_bodyPart;
        LevelFileContentsStruct_* res = def->m_pResource;
        if (RES_FIELD(res, 15) != 0) {
            soldier->m_helmetHitScale = (float)RES_FIELD(res, 17) / 100.0f;
            soldier->m_helmetChance = RES_FIELD(def->m_pResource, 18);
            soldier->field_0x43A4 = RES_FIELD(def->m_pResource, 16);
        }
    }
    return obj;
}

// A first-person weapon: skeleton and mesh only.
void CAnimObjectFactory::InitPlayerWeaponFromLevelContentsStruct(LevelFileContentsStruct_* res, int index) {
    CAnimObjDef* def;
    int size;
    bool loaded = false;
    m_defs[index].m_pResource = res;
    m_defs[index].m_crc = RES_FIELD(res, 7);
    m_defs[index].m_nodeState = -1;
    char* meshFile = RES_STRING(res, 1);
    m_defs[index].m_pMesh = (CDMesh*)TLT_LoadFromRepository(meshFile, &size, &loaded);
    CDMesh* mesh = m_defs[index].m_pMesh;
    if (mesh != NULL && !MeshFormatOK(mesh)) {
        printf("WARNING! \"%s\" has an incompatible format for this executable. File ignored.\n", meshFile);
        m_defs[index].m_pMesh = NULL;
    }
    if (!loaded && (mesh = m_defs[index].m_pMesh) != NULL) {
        mesh->Init(1);
        m_defs[index].m_pSkel = (CharSkel_t*)TLT_LoadFromRepository(RES_STRING(res, 0), &size, &loaded);
        if (!loaded) {
            CharRelocateSkeleton(m_defs[index].m_pSkel);
        }
        m_defs[index].m_pMesh->BindTextures(m_pTextures, NULL);
        def = &m_defs[index];
        def->m_pMesh->BindSkeleton(def->m_pSkel);
    } else {
        m_defs[index].m_pSkel = (CharSkel_t*)TLT_LoadFromRepository(RES_STRING(res, 0), &size, &loaded);
        def = &m_defs[index];
        if (def->m_pMesh == NULL && !loaded) {
            CharRelocateSkeleton(def->m_pSkel);
        }
    }
    m_defs[index].m_moodTarget = -1;
    m_defs[index].m_pFaceMesh = NULL;
    m_defs[index].m_pLocData = NULL;
    m_defs[index].m_pLocMap = NULL;
}

// A character: skeleton, body and face meshes and textures, helmet, and
// the hit locations for the current difficulty.
void CAnimObjectFactory::InitAnimObjectFromLevelContentsStruct(LevelFileContentsStruct_* res, int index) {
    int size;
    bool loaded = false;
    CAnimObjDef* def;
    m_defs[index].m_pResource = res;
    m_defs[index].m_crc = RES_FIELD(res, 14);
    m_defs[index].m_nodeState = RES_FIELD(res, 6);
    m_defs[index].m_pMesh = (CDMesh*)TLT_LoadFromRepository(RES_STRING(res, 1), &size, &loaded);
    if (m_defs[index].m_pMesh != NULL) {
        if (m_pTextures != NULL && RES_STRING(res, 2) != NULL) {
            m_defs[index].m_pBodyTexture = m_pTextures->FindTexture(RES_STRING(res, 2));
        } else {
            m_defs[index].m_pBodyTexture = NULL;
        }
        if (m_pTextures != NULL && RES_STRING(res, 4) != NULL) {
            m_defs[index].m_pFaceTexture = m_pTextures->FindTexture(RES_STRING(res, 4));
        } else {
            m_defs[index].m_pFaceTexture = NULL;
        }
        if (!MeshFormatOK(m_defs[index].m_pMesh)) {
            printf("WARNING! \"%s\" has an incompatible format for this executable. File ignored.\n",
                   RES_STRING(res, 1));
            m_defs[index].m_pMesh = NULL;
            m_defs[index].m_pBodyTexture = NULL;
            m_defs[index].m_pFaceTexture = NULL;
        }
    } else {
        m_defs[index].m_pBodyTexture = NULL;
        m_defs[index].m_pFaceTexture = NULL;
    }
    CDMesh* mesh;
    if (!loaded && (mesh = m_defs[index].m_pMesh) != NULL) {
        mesh->Init(0);
        m_defs[index].m_pSkel = (CharSkel_t*)TLT_LoadFromRepository(RES_STRING(res, 0), &size, &loaded);
        if (!loaded) {
            CharRelocateSkeleton(m_defs[index].m_pSkel);
        }
        m_defs[index].m_pMesh->BindTextures(m_pTextures, RES_STRING(res, 2));
        def = &m_defs[index];
        def->m_pMesh->BindSkeleton(def->m_pSkel);
    } else {
        m_defs[index].m_pSkel = (CharSkel_t*)TLT_LoadFromRepository(RES_STRING(res, 0), &size, &loaded);
        def = &m_defs[index];
        if (def->m_pMesh == NULL && !loaded) {
            CharRelocateSkeleton(def->m_pSkel);
        }
    }

    // The face (a morph mesh).
    if (RES_STRING(res, 3) != NULL) {
        m_defs[index].m_moodTarget = RES_FIELD(res, 5);
        m_defs[index].m_pFaceMesh = (CDMesh*)TLT_LoadFromRepository(RES_STRING(res, 3), &size, &loaded);
        if (!MeshFormatOK(m_defs[index].m_pFaceMesh)) {
            printf("WARNING! \"%s\" has an incompatible format for this executable. File ignored.\n",
                   RES_STRING(res, 3));
            m_defs[index].m_moodTarget = -1;
            m_defs[index].m_pFaceMesh = NULL;
        }
        if (!loaded) {
            CDMesh* face = m_defs[index].m_pFaceMesh;
            if (face != NULL) {
                face->Init(0);
                m_defs[index].m_pFaceMesh->BindTextures(m_pTextures, RES_STRING(res, 4));
                def = &m_defs[index];
                def->m_pFaceMesh->BindSkeleton(def->m_pSkel);
            }
        }
    } else {
        m_defs[index].m_moodTarget = -1;
        m_defs[index].m_pFaceMesh = NULL;
    }

    // The helmet: with the cheat, any static object of the level.
    if (RES_STRING(res, 7) != NULL) {
        u8 chosen = 0;
        if (g_cheats.field_0x9 && g_staticObjCount > 0) {
            int n = 1;
            int pick = MathFunRandomI64(1, g_staticObjCount);
            LevelFileContentsStruct_* obj = TLT_FindNextResourceByType((TLTResourceID)RES_STATIC_OBJECT, NULL);
            while (obj != NULL) {
                if (IsMeshObject(obj)) {
                    if (n >= pick) {
                        break;
                    }
                    n++;
                }
                obj = TLT_FindNextResourceByType((TLTResourceID)RES_STATIC_OBJECT, obj);
            }
            if (obj != NULL) {
                StaticMesh* data = (StaticMesh*)TLT_LoadFromRepository(RES_STRING(obj, 0), &size, &loaded);
                CStaticMesh* helmet = new CStaticMesh(data, !loaded);
                chosen = 1;
                m_defs[index].m_pHelmet = helmet;
            }
        }
        if (!chosen) {
            StaticMesh* data = (StaticMesh*)TLT_LoadFromRepository(RES_STRING(res, 7), &size, &loaded);
            CStaticMesh* helmet = new CStaticMesh(data, !loaded);
            m_defs[index].m_pHelmet = helmet;
        }
    } else {
        m_defs[index].m_pHelmet = NULL;
    }
    m_defs[index].m_helmetOffset.x = RES_FLOAT(res, 8);
    m_defs[index].m_helmetOffset.y = RES_FLOAT(res, 9);
    m_defs[index].m_helmetOffset.z = RES_FLOAT(res, 10);

    // The body texture goes on the part using "mohf_body".
    if (m_defs[index].m_pMesh != NULL) {
        for (u32 i = 0; i < m_defs[index].m_pMesh->m_numParts; i++) {
            CDMesh* body = m_defs[index].m_pMesh;
            u32 tex = (u32)body->m_parts[i].m_texture;
            if (tex < body->m_numTextureNames && stricmp(body->m_textureNames[tex], "mohf_body") == 0) {
                m_defs[index].m_bodyPart = i;
                i = m_defs[index].m_pMesh->m_numParts;
            }
        }
    }

    // Hit locations, per difficulty (fields 12, 13 and 11 for easy, medium
    // and hard).
    if (RES_STRING(res, 11) != NULL) {
        char* file;
        switch (g_Shell.Get_currentDifficulty()) {
        case 1:
            file = RES_STRING(res, 12);
            break;
        case 2:
            file = RES_STRING(res, 13);
            break;
        case 3:
            file = RES_STRING(res, 11);
            break;
        default:
            DebugMsg("Unknown difficulty level.  Assuming Level Hard\n");
            file = RES_STRING(res, 11);
            break;
        }
        if (file == NULL) {
            file = RES_STRING(res, 11);
        }
        CObjLocationCollisionData* data = (CObjLocationCollisionData*)TLT_LoadFromRepository(file, &size, &loaded);
        m_defs[index].m_pLocData = data;
        if (!loaded) {
            EndianSwap(*data);
            m_defs[index].m_pLocMap = new CObjLocationCollisionMap(*data);
        } else {
            // Share the map of the definition that loaded it.
            m_defs[index].m_pLocMap = NULL;
            for (int i = 0; i < index; i++) {
                if (m_defs[i].m_pLocData == data) {
                    m_defs[index].m_pLocMap = m_defs[i].m_pLocMap;
                    break;
                }
            }
        }
        m_defs[index].m_locShared = loaded;
        return;
    }
    m_defs[index].m_pLocData = NULL;
    m_defs[index].m_pLocMap = NULL;
    m_defs[index].m_locShared = 0;
}

void EndianSwap(CObjLocationCollisionData& data) {
    EndianSwap(data.numShapes);
    EndianSwap(data.field_0xC04);
    for (int i = 0; i < data.numShapes; i++) {
        EndianSwap(data.shapes[i]);
    }
}

void EndianSwap(CObjLocationCollisionBox& box) {
    ((CVolBox*)box.box)->EndianSwap();
    EndianSwap(box.bone);
}

void CVolBox::EndianSwap() {
    ::EndianSwap(m_halfSize[0]);
    ::EndianSwap(m_halfSize[1]);
    ::EndianSwap(m_halfSize[2]);
    ::EndianSwap(m_axisX.x);
    ::EndianSwap(m_axisX.y);
    ::EndianSwap(m_axisX.z);
    ::EndianSwap(m_axisY.x);
    ::EndianSwap(m_axisY.y);
    ::EndianSwap(m_axisY.z);
    ::EndianSwap(m_axisZ.x);
    ::EndianSwap(m_axisZ.y);
    ::EndianSwap(m_axisZ.z);
    ::EndianSwap(m_center.x);
    ::EndianSwap(m_center.y);
    ::EndianSwap(m_center.z);
}

void CAnimObjectFactory::Shutdown() {
    for (u32 i = 0; i < m_numDefs; i++) {
        CAnimObjDef* def = &m_defs[i];
        if (def->m_pLocMap != NULL && !def->m_locShared) {
            delete def->m_pLocMap;
        }
    }
    MEM_free(m_slots);
    delete[] m_defs;
}

// Puts every slot back on the free list.
void CAnimObjectFactory::Reset() {
    CAnimObject* last = NULL;
    m_pFree = (CAnimObject*)m_slots;
    u8* slot = m_slots;
    for (int i = 0; i < (int)(m_numSlots - 1); i++) {
        u8* cur = slot;
        slot += m_slotSize;
        ((CAnimObject*)cur)->m_pNextFree = (CAnimObject*)slot;
        last = (CAnimObject*)slot;
    }
    last->m_pNextFree = NULL;
}

// Loads the character textures and every character and player weapon
// definition of the level, then makes `numSlots` object slots.
void CAnimObjectFactory::Init(unsigned long numSlots, char*) {
    int size;
    bool loaded;
    m_numSlots = numSlots;
    LevelFileContentsStruct_* res = TLT_FindNextResourceByType((TLTResourceID)RES_TEXTURES, NULL);
    m_pTextures = (CTexturePack*)TLT_LoadFromRepository(RES_STRING(res, 28), &size, &loaded);
    if (m_pTextures != NULL) {
        m_pTextures = new (m_pTextures) CTexturePack;
    }
    int count = 0;
    for (res = TLT_FindNextResourceByType((TLTResourceID)RES_CHARACTER, NULL); res != NULL;
         res = TLT_FindNextResourceByType((TLTResourceID)RES_CHARACTER, res)) {
        count++;
    }
    for (res = TLT_FindNextResourceByType((TLTResourceID)RES_PLAYER_WEAPON, NULL); res != NULL;
         res = TLT_FindNextResourceByType((TLTResourceID)RES_PLAYER_WEAPON, res)) {
        count++;
    }
    for (res = TLT_FindNextResourceByType((TLTResourceID)RES_STATIC_OBJECT, NULL); res != NULL;
         res = TLT_FindNextResourceByType((TLTResourceID)RES_STATIC_OBJECT, res)) {
        if (IsMeshObject(res)) {
            g_staticObjCount++;
        }
    }
    m_numDefs = count;
    if (count > 0) {
        m_defs = new CAnimObjDef[count];
        int index = 0;
        for (res = TLT_FindNextResourceByType((TLTResourceID)RES_CHARACTER, NULL); res != NULL;
             res = TLT_FindNextResourceByType((TLTResourceID)RES_CHARACTER, res), index++) {
            InitAnimObjectFromLevelContentsStruct(res, index);
        }
        for (res = TLT_FindNextResourceByType((TLTResourceID)RES_PLAYER_WEAPON, NULL); res != NULL;
             res = TLT_FindNextResourceByType((TLTResourceID)RES_PLAYER_WEAPON, res), index++) {
            InitPlayerWeaponFromLevelContentsStruct(res, index);
        }
    }
    qsort(m_defs, m_numDefs, sizeof(CAnimObjDef), CompareObjectDefinitions);
    m_slots = (u8*)DWI_alloc("Anim object factory", m_slotSize * m_numSlots, 0x400);
    m_pFree = (CAnimObject*)m_slots;
    CAnimObject* last = NULL;
    u8* slot = m_slots;
    for (int i = 0; i < (int)(m_numSlots - 1); i++) {
        u8* cur = slot;
        slot += m_slotSize;
        ((CAnimObject*)cur)->m_pNextFree = (CAnimObject*)slot;
        last = (CAnimObject*)slot;
    }
    last->m_pNextFree = NULL;
}

CAnimObjectFactory::CAnimObjectFactory() {
    m_slotSize = 0x44C0;
}

int CompareObjectDefinitions(const void* a, const void* b) {
    u32 ka = ((const CAnimObjDef*)a)->m_crc;
    u32 kb = ((const CAnimObjDef*)b)->m_crc;
    if ((s32)ka < (s32)kb) {
        return -1;
    }
    return (s32)kb < (s32)ka;
}
