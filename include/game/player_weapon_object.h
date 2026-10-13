// CPlayerWeaponObject: a first-person weapon (arms and gun) of a player.
// Hand-maintained.
#ifndef _GAME_PLAYER_WEAPON_OBJECT_H_
#define _GAME_PLAYER_WEAPON_OBJECT_H_

#include "types.h"
#include "game_types.h"
#include "game/anim_obj.h"

struct BSObject;
class CDrawContext;
class CPlayerObject;
class CPlayerWeaponObject;
class CSprite;
class CStaticObject;
class CTexture;
class CWeapon;

// size 0x31E0; vtable 0x80187EE8
// Drawn with its own field of view, positioned relative to the player's
// eyes; spawns the muzzle flash (2D sprite and particles) and ejected
// shell casings.
class CPlayerWeaponObject : public CAnimObject {
public:
    /* 0x30E0 */ CPlayerObject* m_pPlayer;
    /* 0x30E4 */ CWeapon* m_pWeapon;
    // HUD sprites named by the weapon resource (fields 3 to 5), built in
    // the factory slot right after the object. With the first (a scope),
    // the model is hidden while zoomed.
    /* 0x30E8 */ CSprite* m_pSprite0;
    /* 0x30EC */ CSprite* m_pSprite1;
    /* 0x30F0 */ CSprite* m_pSprite2;
    /* 0x30F4 */ CStaticObject* m_pMuzzleFlash;  // weapons with a flash (else NULL)
    /* 0x30F8 */ CVector3 m_offset;  // from the eyes (y is offset by 1)
    /* 0x3108 */ f32 m_pitch;
    /* 0x310C */ f32 m_yaw;
    /* 0x3110 */ f32 m_roll;
    /* 0x3114 */ u8 unk_0x3114[0x4];
    /* 0x3118 */ CVector3 m_scale;
    /* 0x3128 */ CVector3 m_flashPos;  // the "Fire" point in camera space
    /* 0x3138 */ u8 unk_0x3138[0x8];
    /* 0x3140 */ CMatrix m_fireTM;  // the "Fire" attach point's offset
    /* 0x3180 */ f32 m_fovScale;
    /* 0x3184 */ f32 field_0x3184;  // resource fields 25 to 26
    /* 0x3188 */ f32 field_0x3188;
    /* 0x318C */ f32 field_0x318C;  // resource fields 23, 22 and 24 (0: 1)
    /* 0x3190 */ f32 field_0x3190;
    /* 0x3194 */ f32 field_0x3194;
    /* 0x3198 */ f32 m_flashScale;
    /* 0x319C */ f32 m_flashSpin;  // random each shot
    /* 0x31A0 */ u32 m_flashParticles;  // particle system CRC (0: the default)
    /* 0x31A4 */ int field_0x31A4;  // resource field 18
    /* 0x31A8 */ u32 m_firePoint;  // attach point of the muzzle
    /* 0x31AC */ int m_hideMode;  // 3: hide while reloading or out of ammo
    /* 0x31B0 */ int m_hideFrames;  // skip drawing this many frames
    /* 0x31B4 */ char m_name[0x1E];
    /* 0x31D2 */ u8 m_inView : 1;  // copied from the player each frame
    /* 0x31D2 */ u8 m_active : 1;  // updated and drawn
    /* 0x31D2 */ u8 m_selectable : 1;
    /* 0x31D2 */ u8 m_hasFlash : 1;
    /* 0x31D2 */ u8 m_flags31D2 : 4;
    /* 0x31D4 */ int m_lastReserve;  // reserve ammo when dump pickups were last updated
    /* 0x31D8 */ CTexture* m_pSleeveTexture;  // multiplayer

    virtual int Init() { return CAnimObject::Init(); }  // 0x800A7970 (weak)
    virtual const CPlayerWeaponObject* AsPlayerWeaponObject() const { return this; }  // 0x800A7990 (weak)
    virtual CPlayerWeaponObject* AsPlayerWeaponObject() { return this; }  // 0x800A7994 (weak)
    virtual BSObject* GetScriptObject() const { return NULL; }  // 0x800A7998 (weak)
    virtual int IsVisible(CDrawContext&) const;  // 0x800A79A0
    void SetSleeveTexture(char*);  // 0x800A79CC
    virtual void Destroy();  // 0x800A7A34
    static void InitClass();  // 0x800A7A60
    void SetWeaponName(const char*);  // 0x800A7AA4
    void CreateEjectedShell(int);  // 0x800A7ADC
    virtual int IsDrawEnabled() const;  // 0x800A7F80
    void AddAmo(int);  // 0x800A7FAC
    void DoneReloading();  // 0x800A800C
    void Shoot(EWeaponShootType, int);  // 0x800A8030
    bool IsSelectable() const;  // 0x800A8074
    virtual void Draw(CDrawContext&);  // 0x800A80B4
    virtual int CommitUpdate();  // 0x800A8630
    virtual int AttemptUpdate(float);  // 0x800A8634
    virtual int BeginUpdate(float);  // 0x800A8638
    void UpdateWeaponTransforms();  // 0x800A8874
    void Shutdown();  // 0x800A8B7C
    void Init(EWeaponTypes, int, CPlayerObject*);  // 0x800A8BCC
    virtual ~CPlayerWeaponObject();  // 0x800A8CD0
    CPlayerWeaponObject();  // 0x800A8D30
};

SIZE_ASSERT(CPlayerWeaponObject, 0x31E0);
OFFSET_ASSERT(CPlayerWeaponObject, m_offset, 0x30F8);
OFFSET_ASSERT(CPlayerWeaponObject, m_scale, 0x3118);
OFFSET_ASSERT(CPlayerWeaponObject, m_fireTM, 0x3140);
OFFSET_ASSERT(CPlayerWeaponObject, m_fovScale, 0x3180);
OFFSET_ASSERT(CPlayerWeaponObject, m_name, 0x31B4);
OFFSET_ASSERT(CPlayerWeaponObject, m_lastReserve, 0x31D4);
OFFSET_ASSERT(CPlayerWeaponObject, m_pSleeveTexture, 0x31D8);

// Makes the current weapon of player 1 fire along its aim (set by the holder).
void SetMuzzleStatusForPlayer(bool);  // 0x800A7A64

#endif
