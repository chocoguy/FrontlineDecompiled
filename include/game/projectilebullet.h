// CProjectileBullet: rounds that fly straight (rifle and MG bullets,
// shells), swept each frame as a fiber from the last position to the new
// one; tracers are drawn as camera-facing quads by one shared render bin.
// Hand-maintained.
#ifndef _GAME_PROJECTILEBULLET_H_
#define _GAME_PROJECTILEBULLET_H_

#include "types.h"
#include "game_types.h"
#include "game/ISceneNode.h"
#include "game/bullet.h"
#include "game/fiber.h"
#include "game/renderbin.h"
#include "game/texture.h"

class CCollision;
class CDrawContext;
class CParticleSystem;
class CProjectileBullet;
class CProjectileBulletRenderBin;
class CRenderList;
class CScreen;
class CVector3;
class ISceneNode;
struct ProjectileBulletProperties_struct;

// Draws the frame's tracers. size 0x64; vtable 0x801899A8
class CProjectileBulletRenderBin : public CRenderBin {
public:
    CProjectileBulletRenderBin() {
        m_type = 7;
        m_loaded = false;
    }
    virtual ~CProjectileBulletRenderBin() {}  // 0x800D05E0
    virtual void Link(CDmaTag*, CDmaPacket&);  // 0x800D063C
    virtual int Render(CDmaPacket&, void*);  // 0x800D0B3C

    /* 0x20 */ bool m_loaded;  // m_texture set (first bullet of the level)
    /* 0x24 */ CTexture m_texture;  // the level's first texture resource
};

// size 0x170; vtable 0x80189AFC
class CProjectileBullet : public CBullet {
public:
    enum {
        FLAG_HIT = 0x80,       // collided (or marked for destruction) this frame
        FLAG_SHUTDOWN = 0x40,
    };

    /* 0x98 */ u8 unk_0x98[0x8];
    /* 0xA0 */ CBullet* m_next;  // the factory's free or active list
    /* 0xA4 */ s32 m_type;  // EProjectileBulletTypes
    /* 0xA8 */ ProjectileBulletProperties_struct* m_pProps;
    /* 0xAC */ ISceneNode* m_pFiredBy;
    /* 0xB0 */ ISceneNode* m_pExcluded;  // node collisions are excluded with
    /* 0xB4 */ f32 m_speed;
    /* 0xB8 */ f32 m_tracerOffset;  // how far back along the sweep the tracer starts
    /* 0xBC */ u8 m_flags;  // FLAG_*
    /* 0xBD */ u8 unk_0xBD[0x3];
    /* 0xC0 */ CVolFiber m_localFiber;  // this frame's sweep, in bullet space
    /* 0x110 */ CVolFiber m_worldFiber;
    /* 0x160 */ CParticleSystem* m_pTrail;  // bullet-cam cheat trail
    /* 0x164 */ u8 unk_0x164[0xC];
    virtual void GetVelocity(CVector3&);  // 0x800D0C40
    virtual void SetExclusionPair(ISceneNode*);  // 0x800D0C84
    virtual int GetScriptBulletType() const;  // 0x800D0CD8
    virtual int IsVisible(CDrawContext&) const;  // 0x800D0DBC
    virtual void Draw(CDrawContext&);  // 0x800D0DE8
    virtual int GetCollisionId() const;  // 0x800D0FF8
    virtual int Penetrate(const CCollision&);  // 0x800D1000
    virtual int CommitUpdate();  // 0x800D1168
    virtual int AttemptUpdate(float);  // 0x800D11D4
    virtual int BeginUpdate(float);  // 0x800D1340
    virtual void OnCollision(const CCollision&);  // 0x800D13A4
    virtual void Shutdown();  // 0x800D13F8
    virtual void Destroy();  // 0x800D1428
    virtual void MarkForDestruction(int);  // 0x800D1450
    void Init(EProjectileBulletTypes, ProjectileBulletProperties_struct*, const CVector3&, const CVector3&, float, EClsnId, float, ISceneNode*);  // 0x800D14C8
    static void InitClass(CRenderList*, CScreen*);  // 0x800D1A44
    virtual ~CProjectileBullet();  // 0x800D1A78
    CProjectileBullet();  // 0x800D1AF4
    void* operator new[](unsigned long);  // 0x800D1B58
    virtual int Init();  // 0x800D1B84
    virtual const IVolume* GetLocalBoundingVolume(ISceneNode::EVolumeType) const;  // 0x800D1BA4
    virtual const IVolume* GetWorldBoundingVolume(ISceneNode::EVolumeType) const;  // 0x800D1BAC
    virtual float GetDamage() const;  // 0x800D1BB4
    virtual ISceneNode* GetFiredBy() const;  // 0x800D1BC0
    virtual CProjectileBullet* AsProjectile();  // 0x800D1BC8
};
OFFSET_ASSERT(CProjectileBullet, m_localFiber, 0xC0);
OFFSET_ASSERT(CProjectileBullet, m_pTrail, 0x160);
SIZE_ASSERT(CProjectileBullet, 0x170);

// A queued tracer, as Render writes it into the packet (size 0x28).
class STracer {
public:
    /* 0x00 */ f32 m_start[3];
    /* 0x0C */ u8 unk_0xC[0x4];
    /* 0x10 */ f32 m_width;
    /* 0x14 */ u8 unk_0x14[0x4];
    /* 0x18 */ f32 m_end[3];
    /* 0x24 */ u8 unk_0x24[0x4];
    // The quad's corners: start +/- side, end -/+ side.
    void GetVerts(CVector3*);  // 0x800D0950
};

// Multiplayer: other players' bullets are drawn only after this long.
extern float MP_PROJBULLET_DRAW_DELAY;  // 0x8034E1D0

#endif
