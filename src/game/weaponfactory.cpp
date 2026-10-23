// CWeaponFactory: the level's weapon types and the weapon pool.
#include "game/weaponfactory.h"
#include "game/weapon.h"
#include "game/staticmesh.h"
#include "game/staticobjfactory.h"
#include "game/sprite.h"
#include "game/toilet.h"
#include "game/memory.h"
#include "game/ShellMenu.h"
#include "game/Moh2.h"
#include "string.h"

enum {
    WEAPON_POOL_SIZE = 150,
};

CStaticObject* g_pStaticObjectMuzzleFlashNPC;
CStaticObject* g_pStaticObjectMuzzleFlashPlayer;

// Weak in the original (the mesh array is built here).
CStaticMesh::CStaticMesh() {
    m_pData = NULL;
    m_lighting = true;
    m_fogging = true;
}

// Converts a little-endian value (the weapon files are) to the CPU's order.
template <class T>
static inline void SwapBytes(T& value) {
#ifndef FRONTLINE_NATIVE  // natively the CPU is little-endian too
    u8* b = (u8*)&value;
    for (int i = 0; i < (int)sizeof(T) / 2; i++) {
        u8 t = b[i];
        b[i] = b[sizeof(T) - 1 - i];
        b[sizeof(T) - 1 - i] = t;
    }
#endif
}

// Reads a little-endian value at `at`.
template <class T>
static inline T ReadLE(const u8* at) {
    T value;
    memcpy(&value, at, sizeof(T));
    SwapBytes(value);
    return value;
}

// Each distinct model is loaded once; types sharing a model share its mesh.
void CWeaponFactory::LoadWeaponObjects() {
    int numModels = 0;
    for (int i = 0; i < m_numTypes; i++) {
        char* name = m_props[i].m_modelName;
        if (name[0] != 0) {
            int j;
            for (j = 0; j < i; j++) {
                if (strcmp(name, m_props[j].m_modelName) == 0) {
                    *(int*)&m_props[i].m_pMesh = j;
                    break;
                }
            }
            if (j == i) {
                numModels++;
                *(int*)&m_props[i].m_pMesh = i;
            }
        } else {
            *(int*)&m_props[i].m_pMesh = -1;
        }
    }
    m_meshes = new CStaticMesh[numModels];
    int next = 0;
    for (int i = 0; i < m_numTypes; i++) {
        int first = *(int*)&m_props[i].m_pMesh;
        if (first == -1) {
            m_props[i].m_pMesh = NULL;
        } else if (first < i) {
            m_props[i].m_pMesh = m_props[first].m_pMesh;
        } else if (first == i) {
            m_props[i].m_pMesh = NULL;
            bool loaded;
            void* data = TLT_LoadFromRepository(m_props[i].m_modelName, NULL, &loaded);
            if (data != NULL) {
                m_meshes[next].Init(data, !loaded);
                m_props[i].m_pMesh = &m_meshes[next];
                next++;
            }
        }
    }
}

// The level's weapon file for the difficulty (multiplayer has its own): a
// count, then records of a kind (1 weapon, 2 and 3 skipped), the type, and
// that type's parameters.
void CWeaponFactory::LoadWeaponParameters() {
    LevelFileContentsStruct_* level = TLT_FindNextResourceByType((TLTResourceID)0xB, NULL);
    u8* file;
    if (g_bInMultiplayerMode) {
        file = (u8*)TLT_LoadFileFromLevelBigFile(ADDR_TO_PTR(char, level->fields[65].value), NULL);
    } else {
        switch (g_Shell.Get_currentDifficulty()) {
        case 1:
            file = (u8*)TLT_LoadFileFromLevelBigFile(ADDR_TO_PTR(char, level->fields[4].value), NULL);
            break;
        case 2:
            file = (u8*)TLT_LoadFileFromLevelBigFile(ADDR_TO_PTR(char, level->fields[5].value), NULL);
            break;
        case 3:
            file = (u8*)TLT_LoadFileFromLevelBigFile(ADDR_TO_PTR(char, level->fields[6].value), NULL);
            break;
        default:
            DebugMsg("Unknown difficulty level.  Assuming Level TE\n");
            file = (u8*)TLT_LoadFileFromLevelBigFile(ADDR_TO_PTR(char, level->fields[4].value), NULL);
            break;
        }
    }
    m_numTypes = ReadLE<int>(file);
    m_props = (WeaponProperties_struct*)new u8[m_numTypes * sizeof(WeaponProperties_struct)];
    s16 at = 0xC;
    int loaded = 0;
    while (loaded < m_numTypes) {
        s16 kind = ReadLE<s16>(file + at);
        at += 2;
        switch (kind) {
        case 1: {
            s16 type = ReadLE<s16>(file + at);
            memcpy(&m_props[type], file + (s16)(at + 2), sizeof(WeaponProperties_struct));
            EndianSwap(m_props[type]);
            at += 0x52;
            loaded++;
            break;
        }
        case 2:
            at += 0x42;
            break;
        case 3:
            at += 0x4E;
            break;
        }
    }
    TLT_CloseFile(file);
}

void EndianSwap(WeaponProperties_struct& p) {
    SwapBytes(p.m_crc);
    SwapBytes(p.m_clipSize);
    SwapBytes(p.m_refireRate);
    SwapBytes(p.m_bulletType);
    SwapBytes(p.m_isProjectile);
    SwapBytes(p.m_numPellets);
    SwapBytes(p.m_maxAmmo);
    SwapBytes(*(u32*)&p.m_pMesh);
    SwapBytes(*(u32*)&p.m_pIcon);
    SwapBytes(p.m_soundType);
    SwapBytes(p.m_hand);
    SwapBytes(*(u32*)&p.m_speed);
    SwapBytes(*(u32*)&p.m_spread);
    SwapBytes(*(u32*)&p.m_noise);
}

void CWeaponFactory::DestroyWeapon(CWeapon* weapon) {
    weapon->Shutdown();
    CWeapon** link = &m_active;
    CWeapon* w = m_active;
    while (w != weapon) {
        link = (CWeapon**)&w->m_pNextFree;
        w = (CWeapon*)w->m_pNextFree;
    }
    *link = (CWeapon*)weapon->m_pNextFree;
    weapon->m_pNextFree = m_free;
    m_free = weapon;
}

// Some weapons' CRCs stand for others.
static inline int RemapWeaponCRC(int crc) {
    switch ((u32)crc) {
    case 0xC320DF3B:
        return (int)0x8EFB8E12;
    case 0xD8541A59:
        return 0x392805E6;
    case 0xCC0DC130:
        return (int)0xF45E50D5;
    case 0x4804D932:
        return (int)0xADC42548;
    case 0xF6CFC618:
        return 0x6E584C98;
    case 0x88AC7990:
        return (int)0xC186CA3C;
    case 0x8E32A26E:
        return (int)0xC662E91E;
    case 0x87038E92:
        return 0x36ADF7F0;
    case 0x585FF990:
        return (int)0x83674487;
    case 0xD1F65C59:
        return 0x13DB5201;
    }
    return crc;
}

static inline WeaponProperties_struct* FindByCRC(CWeaponFactory* f, int crc, int* index) {
    crc = RemapWeaponCRC(crc);
    for (int i = 0; i < f->m_numTypes; i++) {
        if ((u32)crc == f->m_props[i].m_crc) {
            *index = i;
            return &f->m_props[i];
        }
    }
    *index = -1;
    return NULL;
}

static inline CWeapon* TakeWeapon(CWeaponFactory* f) {
    CWeapon* weapon = f->m_free;
    f->m_free = (CWeapon*)weapon->m_pNextFree;
    weapon->m_pNextFree = f->m_active;
    f->m_active = weapon;
    return weapon;
}

// Unknown weapons fall back to type 0 (none for CRC 0).
CWeapon* CWeaponFactory::CreateWeaponByCRC(int crc, ISceneNode* holder, int ammo) {
    int index;
    WeaponProperties_struct* props = FindByCRC(this, crc, &index);
    if (props == NULL) {
        if (crc == 0) {
            return NULL;
        }
        props = m_props;
        index = 0;
    }
    CWeapon* weapon = TakeWeapon(this);
    weapon->Init((EWeaponTypes)index, props, holder, ammo);
    return weapon;
}

WeaponProperties_struct* CWeaponFactory::GetWeaponPropertiesByCRC(int crc, int* index) {
    return FindByCRC(this, crc, index);
}

CWeapon* CWeaponFactory::CreateWeapon(EWeaponTypes type, ISceneNode* holder, int ammo) {
    CWeapon* weapon = TakeWeapon(this);
    weapon->Init(type, &m_props[type], holder, ammo);
    return weapon;
}

// Types sharing an icon free it once.
void CWeaponFactory::Shutdown() {
    for (int i = 0; i < m_numTypes; i++) {
        char* name = m_props[i].m_iconName;
        if (name[0] != 0) {
            for (int j = 0; j < i; j++) {
                if (strcmp(name, m_props[j].m_iconName) == 0) {
                    m_props[i].m_pIcon = NULL;
                    break;
                }
            }
            if (m_props[i].m_pIcon != NULL) {
                delete m_props[i].m_pIcon;
                m_props[i].m_pIcon = NULL;
            }
        }
    }
    if (m_meshes != NULL) {
        delete[] m_meshes;
    }
    delete[] (u8*)m_props;
    delete[] m_pool;
    m_props = NULL;
    m_pool = NULL;
    m_ready = false;
}

static inline void BuildPool(CWeaponFactory* f) {
    f->m_pool = new ("Weapon factory") CWeapon[WEAPON_POOL_SIZE];
    f->m_free = f->m_pool;
    f->m_active = NULL;
    for (int i = 0; i < WEAPON_POOL_SIZE - 1; i++) {
        f->m_pool[i].m_pNextFree = &f->m_pool[i + 1];
    }
    f->m_pool[WEAPON_POOL_SIZE - 1].m_pNextFree = NULL;
}

void CWeaponFactory::Reset() {
    delete[] m_pool;
    BuildPool(this);
    g_pStaticObjectMuzzleFlashNPC = CStaticObjectFactory::AllocateStaticObject((int)0xB095BE01, -1);
    g_pStaticObjectMuzzleFlashPlayer = CStaticObjectFactory::AllocateStaticObject((int)0xAE37DA9C, -1);
}

// Types sharing an icon share the sprite.
void CWeaponFactory::Init() {
    LoadWeaponParameters();
    for (int i = 0; i < m_numTypes; i++) {
        m_props[i].m_pIcon = NULL;
        char* name = m_props[i].m_iconName;
        if (name[0] != 0) {
            for (int j = 0; j < i; j++) {
                if (strcmp(name, m_props[j].m_iconName) == 0) {
                    m_props[i].m_pIcon = m_props[j].m_pIcon;
                    break;
                }
            }
            if (m_props[i].m_pIcon == NULL) {
                // The original builds a "data\" path but loads by the bare name.
                char path[0x40];
                strcpy(path, "data\\");
                strcpy(path + strlen("data\\"), name);
                void* mem = DWI_alloc(NULL, sizeof(CSprite), 0x400);
                CSprite* sprite = NULL;
                if (mem != NULL) {
                    sprite = ::new (mem) CSprite(name, (CSprite::ESpriteLoadType)1, (ERenderPriority)0x6F);
                }
                m_props[i].m_pIcon = sprite;
            }
        }
    }
    LoadWeaponObjects();
    BuildPool(this);
    m_ready = true;
    g_pStaticObjectMuzzleFlashNPC = CStaticObjectFactory::AllocateStaticObject((int)0xB095BE01, -1);
    g_pStaticObjectMuzzleFlashPlayer = CStaticObjectFactory::AllocateStaticObject((int)0xAE37DA9C, -1);
}

CWeaponFactory::~CWeaponFactory() {}

CWeaponFactory::CWeaponFactory() {
    m_props = NULL;
    m_pool = NULL;
    m_free = NULL;
    m_active = NULL;
    m_ready = false;
}
