// CAnimObjectFactory: the level's character definitions (skeleton, meshes,
// textures, helmet, hit locations) and a pool of fixed-size slots the
// animated objects are built in. Hand-maintained.
#ifndef _GAME_ANIMOBJFACTORY_H_
#define _GAME_ANIMOBJFACTORY_H_

#include "types.h"
#include "game_types.h"
#include "game/vector.h"

class CAnimObject;
class CAnimObjectFactory;
class CDMesh;
class CObjLocationCollisionMap;
class CStaticMesh;
class CTexture;
class CTexturePack;
struct CharSkel_t;
struct CObjLocationCollisionBox;
struct CObjLocationCollisionData;
struct LevelFileContentsStruct_;

// What AllocateObject builds in the slot (EObjectClass).
enum {
    OBJCLASS_ANIM = 0,
    OBJCLASS_SOLDIER = 1,
    OBJCLASS_RAW = 2,  // nothing constructed
    OBJCLASS_PLAYER_WEAPON = 3,
    OBJCLASS_ANIMATED_PLAYER = 4,
};

// One character definition from the level contents (size 0x50), sorted by
// m_crc.
class CAnimObjDef {
public:
    CAnimObjDef() {}  // 0x800971B8 (weak)
    /* 0x00 */ CharSkel_t* m_pSkel;
    /* 0x04 */ CDMesh* m_pMesh;
    /* 0x08 */ CDMesh* m_pFaceMesh;
    /* 0x0C */ CTexture* m_pBodyTexture;
    /* 0x10 */ u8 m_bodyPart;  // the mesh part using "mohf_body"
    /* 0x11 */ u8 unk_0x11[0x3];
    /* 0x14 */ CTexture* m_pFaceTexture;
    /* 0x18 */ CStaticMesh* m_pHelmet;
    /* 0x1C */ u32 unk_0x1C;
    /* 0x20 */ CVector3 m_helmetOffset;
    /* 0x30 */ int m_moodTarget;  // face morph target held on (-1 none)
    /* 0x34 */ int m_nodeState;
    /* 0x38 */ CObjLocationCollisionData* m_pLocData;
    /* 0x3C */ CObjLocationCollisionMap* m_pLocMap;
    /* 0x40 */ bool m_locShared;  // the data was loaded by another definition
    /* 0x41 */ u8 unk_0x41[0x3];
    /* 0x44 */ u32 m_crc;
    /* 0x48 */ LevelFileContentsStruct_* m_pResource;
    /* 0x4C */ u32 unk_0x4C;
};

// size 0x1C
class CAnimObjectFactory {
public:
    /* 0x0 */ CAnimObjDef* m_defs;
    /* 0x4 */ u8* m_slots;
    /* 0x8 */ CAnimObject* m_pFree;  // linked through m_pNextFree
    /* 0xC */ CTexturePack* m_pTextures;  // the character textures
    /* 0x10 */ int m_slotSize;  // 16-aligned
    /* 0x14 */ u32 m_numDefs;
    /* 0x18 */ u32 m_numSlots;
    void SetLargestObjectSize(int);  // 0x80095BA0
    void ReleaseObject(CAnimObject*);  // 0x80095BB8
    CAnimObject* AllocateObject(unsigned long, EObjectClass);  // 0x80095BC8
    void InitPlayerWeaponFromLevelContentsStruct(LevelFileContentsStruct_*, int);  // 0x80096120
    void InitAnimObjectFromLevelContentsStruct(LevelFileContentsStruct_*, int);  // 0x80096328
    void Shutdown();  // 0x80096E6C
    void Reset();  // 0x80096F04
    void Init(unsigned long, char*);  // 0x80096F50
    CAnimObjectFactory();  // 0x800971BC
};

// Free functions
void EndianSwap(CObjLocationCollisionData&);  // 0x80096A74
void EndianSwap(CObjLocationCollisionBox&);  // 0x80096B34
int CompareObjectDefinitions(const void*, const void*);  // 0x800971C8

#endif
