// CWeapon: a held gun or throwable: ammo, refire, reloading, firing bullets
// through the bullet factory, muzzle flash, shot sounds and AI noise.
// Hand-maintained.
#ifndef _GAME_WEAPON_H_
#define _GAME_WEAPON_H_

#include "types.h"
#include "game_types.h"
#include "game/static_obj.h"
#include "game/vector.h"
#include "game/BSGO_Dummy.h"

class CSprite;
class CStaticMesh;

class CDrawContext;
class CVector3;
class CWeapon;
class ISceneNode;

// A weapon type's properties (size 0x50, from the level's weapon file).
struct WeaponProperties_struct {
    /* 0x0 */ u32 m_crc;  // CRC of the weapon's name
    /* 0x4 */ s16 m_clipSize;
    /* 0x6 */ s16 m_refireRate;  // frames between shots
    /* 0x8 */ f32 m_speed;  // projectile speed
    /* 0xC */ s16 m_bulletType;
    /* 0xE */ s16 m_isProjectile;  // else thrown (grenades, rockets)
    /* 0x10 */ s16 m_numPellets;  // bullets per shot
    /* 0x12 */ s16 m_maxAmmo;
    /* 0x14 */ char m_modelName[0x14];
    /* 0x28 */ CStaticMesh* m_pMesh;  // (while loading: index of the first type using the model, -1 none)
    /* 0x2C */ char m_iconName[0x14];
    /* 0x40 */ CSprite* m_pIcon;
    /* 0x44 */ f32 m_spread;  // radians
    /* 0x48 */ s16 m_soundType;
    /* 0x4A */ s16 m_hand;  // soldiers hold it in the left (even) or right (odd) hand
    /* 0x4C */ f32 m_noise;  // AI hearing range of a shot
};

// How Update fires (m_shootType).
enum {
    WEAPON_SHOOT_HOLD = 0,  // keep firing at the refire rate
    WEAPON_SHOOT_NONE = 1,
    WEAPON_SHOOT_ONCE = 2,  // one shot (scripts and AI), then a free reload
};

// size 0x300; vtable 0x80189FD8
class CWeapon : public CStaticObject {
public:
    /* 0x280 */ WeaponProperties_struct* m_pProperties;
    /* 0x284 */ s16 m_reserveAmmo;
    /* 0x286 */ s16 m_clipAmmo;
    /* 0x288 */ f32 m_refireTime;  // until the next shot
    /* 0x28C */ s32 m_shootType;  // WEAPON_SHOOT_*
    /* 0x290 */ f32 m_power;  // thrown bullets: throw strength (0..512)
    /* 0x294 */ s32 m_clsnId;  // EClsnId of its bullets (by the holder's id)
    /* 0x298 */ s32 m_weaponType;
    /* 0x29C */ f32 m_fuse;  // thrown bullets: fuse when >= 0 (cooked grenades)
    /* 0x2A0 */ CVector3 m_aimDir;  // the shot direction (set by the holder)
    /* 0x2B0 */ CVector3 m_aimOrigin;  // the muzzle
    /* 0x2C0 */ u8 m_bReloading : 1;
    /* 0x2C0 */ u8 m_fired : 1;  // fired this frame
    /* 0x2C0 */ u8 m_bDropped : 1;  // falling to the ground after a soldier let go of it
    /* 0x2C0 */ u8 m_infiniteAmmo : 1;
    /* 0x2C0 */ u8 m_flashShown : 1;
    /* 0x2C0 */ u8 m_muzzleFlash : 1;  // a muzzle flash is showing
    /* 0x2C0 */ u8 field_0x2C0 : 2;
    /* 0x2C1 */ u8 unk_0x2C1[0x3];
    /* 0x2C4 */ f32 m_flashTime;  // how long the muzzle flash has shown
    /* 0x2C8 */ f32 m_flashDuration;
    /* 0x2CC */ CStaticObject* m_pMuzzleFlash;
    /* 0x2D0 */ f32 m_speedScale;
    /* 0x2D4 */ ISceneNode* m_pHolder;
    /* 0x2D8 */ BSGO_Dummy m_bsgo;  // for proximity triggers once dropped
    virtual CWeapon* AsWeaponObject();  // 0x800D4F28
    virtual void GetPosition(CVector3&) const;  // 0x800D4F2C
    virtual int Init();  // 0x800D4F40
    int GetMuzzleFlashStatus();  // 0x800D4F60
    void SetInfiniteAmo(bool);  // 0x800D4FAC
    bool CanCook() const;  // 0x800D4FBC
    int GetWeaponSoundType() const;  // 0x800D4FF8
    float GetBulletSpeed() const;  // 0x800D5004
    void FireBullet();  // 0x800D5058
    // `paused`: no firing or reload events this frame.
    void Update(float, bool);  // 0x800D5914
    virtual void Draw(CDrawContext&);  // 0x800D605C
    virtual int BeginUpdate(float);  // 0x800D60C4
    void DoneReloading();  // 0x800D611C
    void ReloadSingle();  // 0x800D618C
    void StartReloading();  // 0x800D61C4
    void Empty();  // 0x800D61DC
    void AddAmo(short);  // 0x800D61EC
    void Shoot(EWeaponShootType, float, float);  // 0x800D6238
    CSprite* GetBulletSprite() const;  // 0x800D6248 (the ammo icon)
    void Shutdown();  // 0x800D6284
    void Init(EWeaponTypes, WeaponProperties_struct*, ISceneNode*, short);  // 0x800D6288
    virtual ~CWeapon();  // 0x800D64E8
    CWeapon();  // 0x800D6548
    void* operator new[](unsigned long, char*);  // 0x800D65C8
};
OFFSET_ASSERT(CWeapon, m_flashTime, 0x2C4);
OFFSET_ASSERT(CWeapon, m_bsgo, 0x2D8);

SIZE_ASSERT(CWeapon, 0x300);

#endif
