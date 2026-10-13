// CThrownBullet: thrown or lobbed rounds (hand grenades, stick grenades,
// knives, wrenches, rockets and rifle grenades). Types 3, 4, 5 and 9 bounce
// and roll until their fuse runs out (5 and 9 hurt on contact); the others
// explode on impact. Hand-maintained.
#ifndef _GAME_THROWNBULLET_H_
#define _GAME_THROWNBULLET_H_

#include "types.h"
#include "game_types.h"
#include "game/ISceneNode.h"
#include "game/bullet.h"
#include "game/BSGO_Dummy.h"
#include "game/lightvolman.h"
#include "game/fiber.h"

struct BPDLightVolume;
class CBullet;
class CCollision;
class CDrawContext;
class CLight;
class CMatrix;
class CParticleSystem;
class CThrownBullet;
class CVector3;
class IObserver;
class ISceneNode;
class ISubject;
struct ThrownBulletProperties_struct;

// size 0x200; vtable 0x80189D10
class CThrownBullet : public CBullet {
public:
    virtual int IsCollisionEnabled() const;  // 0x800D1C88
    /* 0x98 */ u8 unk_0x98[0x8];
    /* 0xA0 */ CBullet* m_next;  // the factory's free or active list
    /* 0xA4 */ int m_type;  // EThrownBulletTypes (3, 4: kinds that only hurt tanks)
    /* 0xA8 */ ThrownBulletProperties_struct* m_pProps;
    /* 0xAC */ ISceneNode* m_pFiredBy;  // subscribed: cleared when it goes away
    /* 0xB0 */ BSGO_Dummy m_bsgo;  // for proximity triggers
    /* 0xD4 */ CLightVolumeManager m_lightVolumes;
    /* 0xEC */ u8 unk_0xEC[0x4];
    /* 0xF0 */ CVector3 m_velocity;  // per frame
    /* 0x100 */ CVector3 m_prevVelocity;
    /* 0x110 */ CVector3 m_normal;  // sum of this frame's contact normals
    /* 0x120 */ CVector3 m_angles;  // tumble (x, y, z rotations)
    /* 0x130 */ CVector3 m_prevAngles;
    /* 0x140 */ CVector3 m_spin;  // tumble per unit travelled
    /* 0x150 */ CVolFiber m_localFiber;
    /* 0x1A0 */ CVolFiber m_worldFiber;  // this frame's sweep
    /* 0x1F0 */ CLight* m_pLight;  // subscribed
    /* 0x1F4 */ CParticleSystem* m_pTrail;
    /* 0x1F8 */ u8 m_onGround : 1;  // a contact faced up (normal z > 0.7)
    /* 0x1F8 */ u8 m_collided : 1;  // touched something this frame
    /* 0x1F8 */ u8 m_gravity : 1;
    /* 0x1F8 */ u8 m_explode : 1;  // go off at the next commit
    /* 0x1F8 */ u8 m_dud : 1;  // won't explode (untimed type, or caught by a script)
    /* 0x1F8 */ u8 f1F8_2 : 1;
    /* 0x1F8 */ u8 m_noticed : 1;  // an enemy soldier reacted to it
    /* 0x1F8 */ u8 m_dead : 1;  // marked for destruction
    /* 0x1F9 */ u8 unk_0x1F9[0x3];
    /* 0x1FC */ u32 field_0x1FC;
    virtual ISceneNode* GetFiredBy() const;  // 0x800AF258 (defined in static_obj.cpp)
    virtual int Init();  // 0x800D1C58
    virtual const IVolume* GetLocalBoundingVolume(ISceneNode::EVolumeType) const;  // 0x800D1C78
    virtual const IVolume* GetWorldBoundingVolume(ISceneNode::EVolumeType) const;  // 0x800D1C80
    virtual int GetAttachedLight() const;  // 0x800D1C90
    virtual float GetDamage() const;  // 0x800D1C98
    virtual CThrownBullet* AsThrown();  // 0x800D1CA4
    virtual void GetVelocity(CVector3&);  // 0x800D1CA8
    virtual BPDLightVolume* GetLightVolume();  // 0x800D1CBC
    virtual void ExitLightVolume(BPDLightVolume*);  // 0x800D1CE0
    virtual void EnterLightVolume(BPDLightVolume*);  // 0x800D1D04
    // A script defused it: no explosion, just goes away.
    void SomebodyCaughtYou();  // 0x800D1D28
    // Lets the nearest enemy soldier react (events 0xA3 near the head,
    // 0xAC/0xAD on the ground within 3/6 units; 0xAE to the others nearby).
    void CheckEnemies();  // 0x800D1D88
    virtual void ApplyForceTo(CVector3, float);  // 0x800D2384
    virtual void Halt();  // 0x800D2498
    virtual void HandleBulletCollision(CBullet*, const CCollision&);  // 0x800D24B8
    void SetDetonationTime(float);  // 0x800D2510
    void Explode();  // 0x800D2528
    virtual int GetScriptBulletType() const;  // 0x800D2540
    virtual int IsVisible(CDrawContext&) const;  // 0x800D25D4
    virtual void Draw(CDrawContext&);  // 0x800D2600
    virtual int GetCollisionId() const;  // 0x800D27D4
    virtual int CommitUpdate();  // 0x800D27DC
    virtual int AttemptUpdate(float);  // 0x800D324C
    virtual int BeginUpdate(float);  // 0x800D3474
    virtual void ConstrainVelocity();  // 0x800D38E4
    virtual int Penetrate(const CCollision&);  // 0x800D3DE8
    virtual void OnCollision(const CCollision&);  // 0x800D3F5C
    virtual void Destroy();  // 0x800D4134
    virtual void MarkForDestruction(int);  // 0x800D415C
    virtual void Shutdown();  // 0x800D4228
    void Init(EThrownBulletTypes, ThrownBulletProperties_struct*, const CVector3&, const CVector3&, float, EClsnId, float, ISceneNode*);  // 0x800D425C
    virtual ~CThrownBullet();  // 0x800D4DE0
    CThrownBullet();  // 0x800D4E68
    void* operator new[](unsigned long);  // 0x800D4EFC
};
OFFSET_ASSERT(CThrownBullet, m_bsgo, 0xB0);
OFFSET_ASSERT(CThrownBullet, m_lightVolumes, 0xD4);
OFFSET_ASSERT(CThrownBullet, m_velocity, 0xF0);
OFFSET_ASSERT(CThrownBullet, m_localFiber, 0x150);
OFFSET_ASSERT(CThrownBullet, m_pLight, 0x1F0);

// The blast's particles and flash: dust when it went off on the ground,
// the bullet-cam cheat's set otherwise.
void CreateExplosionEffect(const CMatrix&, bool, bool);  // 0x800D301C

// Multiplayer: thrown bullets of players are drawn only after this long.
extern float MP_THROWNBULLET_DRAW_DELAY;  // 0x8034E228

SIZE_ASSERT(CThrownBullet, 0x200);

#endif
