// CAnimatedPlayer: a player's third-person body (multiplayer), a soldier
// moved by the player object rather than physics or AI. Hand-maintained.
#ifndef _GAME_ANIMPLAYER_H_
#define _GAME_ANIMPLAYER_H_

#include "types.h"
#include "game_types.h"
#include "game/ISceneNode.h"
#include "game/Soldier_object.h"

class CAnimatedPlayer;
class CPlayerObject;
class CBullet;
class CCollision;
class CDrawContext;
class CVector3;

// size 0x44C0; vtable 0x8018C614
class CAnimatedPlayer : public CSoldierObject {
public:
    /* 0x44B0 */ CPlayerObject* m_pPlayer;
    /* 0x44B4 */ f32 m_flocoTime;   // the animation callbacks' footstep state
    /* 0x44B8 */ long m_flocoState;
    virtual void UpdateAI(float);  // 0x801099C4
    virtual const CAnimatedPlayer* AsAnimatedPlayerObject() const;  // 0x801099C8
    virtual CAnimatedPlayer* AsAnimatedPlayerObject();  // 0x801099CC
    virtual int CommitUpdate();  // 0x801099D0
    virtual int AttemptUpdate(float);  // 0x80109A58
    virtual int BeginUpdate(float);  // 0x80109ED0
    virtual void UpdateLocalBoundingVolume(ISceneNode::EVolumeType, const CVector3&, const CVector3&);  // 0x80109FA4
    virtual const IVolume* GetWorldBoundingVolume(ISceneNode::EVolumeType) const;  // 0x8010A00C
    virtual const IVolume* GetLocalBoundingVolume(ISceneNode::EVolumeType) const;  // 0x8010A050
    void GetFlocoParams(float&, long&);  // 0x8010A094
    void SetFlocoParams(float, long);  // 0x8010A0A8
    virtual void UpdateAnimation(float);  // 0x8010A0B4
    virtual void Draw(CDrawContext&);  // 0x8010A39C
    virtual void HandleBulletCollision(CBullet*, const CCollision&);  // 0x8010A3D0
    virtual void InitAI();  // 0x8010A8CC
    virtual int Init();  // 0x8010A8D0
    virtual ~CAnimatedPlayer();  // 0x8010AC30
    CAnimatedPlayer();  // 0x8010AC90
};

SIZE_ASSERT(CAnimatedPlayer, 0x44C0);

#endif
