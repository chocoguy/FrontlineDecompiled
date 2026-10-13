// CWeaponFactory: loads the level's weapon types (by difficulty), their
// models and ammo icons, and hands out weapons from a pool. Hand-maintained.
#ifndef _GAME_WEAPONFACTORY_H_
#define _GAME_WEAPONFACTORY_H_

#include "types.h"
#include "game_types.h"

class CStaticObject;
class CStaticMesh;
class CWeapon;
class CWeaponFactory;
class ISceneNode;
struct WeaponProperties_struct;

// The weapon types of the level and a pool of 150 weapons. size 0x1C
class CWeaponFactory {
public:
    /* 0x0 */ WeaponProperties_struct* m_props;  // by EWeaponTypes
    /* 0x4 */ CStaticMesh* m_meshes;  // one per distinct model
    /* 0x8 */ CWeapon* m_pool;
    /* 0xC */ CWeapon* m_free;  // linked through CStaticObject::m_pNextFree
    /* 0x10 */ CWeapon* m_active;
    /* 0x14 */ u8 m_ready;
    /* 0x15 */ u8 unk_0x15[0x3];
    /* 0x18 */ int m_numTypes;
    void LoadWeaponObjects();  // 0x800D65F8
    void LoadWeaponParameters();  // 0x800D67FC
    void DestroyWeapon(CWeapon*);  // 0x800D6C88
    CWeapon* CreateWeaponByCRC(int, ISceneNode*, int);  // 0x800D6CF4
    WeaponProperties_struct* GetWeaponPropertiesByCRC(int, int*);  // 0x800D6EFC
    CWeapon* CreateWeapon(EWeaponTypes, ISceneNode*, int);  // 0x800D7090
    void Shutdown();  // 0x800D70F0
    void Reset();  // 0x800D7218
    void Init();  // 0x800D73F4
    ~CWeaponFactory();  // 0x800D76C0
    CWeaponFactory();  // 0x800D76FC
};

// Free functions
void EndianSwap(WeaponProperties_struct&);  // 0x800D6A5C

// Muzzle flash objects drawn on the weapons that have one.
extern CStaticObject* g_pStaticObjectMuzzleFlashNPC;  // 0x8034EC78
extern CStaticObject* g_pStaticObjectMuzzleFlashPlayer;  // 0x8034EC7C

#endif
