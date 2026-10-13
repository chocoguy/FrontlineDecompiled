// CExplosion: a blast. Constructing one makes AI noise, visits every node
// inside the sphere (damage events, pushes, rumble, setting off grenades)
// and advances the world's texture swaps. Hand-maintained.
#ifndef _GAME_EXPLOSION_H_
#define _GAME_EXPLOSION_H_

#include "types.h"
#include "game_types.h"
#include "game/sphere.h"

class CExplosion;
class CScene;
class CVector3;
class ISceneNode;

// size 0x44; vtable 0x80189954
class CExplosion : public dwi::IVisitor<ISceneNode> {
public:
    /* 0x4 */ u8 unk_0x4[0xC];
    /* 0x10 */ CVolSphere m_sphere;  // the blast
    /* 0x30 */ f32 m_loudness;  // AI noise range / 6
    /* 0x34 */ f32 m_damage;  // at the center (half at the edge)
    /* 0x38 */ s32 m_bulletType;  // EScriptBulletType
    /* 0x3C */ ISceneNode* m_pSource;  // who set it off, or NULL
    /* 0x40 */ u8 m_fromScript;  // pushes the player by the level's script explosion scales
    /* 0x41 */ u8 unk_0x41[0x3];
    // True when level geometry blocks all three lines from the blast to
    // the node (at its feet, middle and head).
    bool CheckForObstructions(ISceneNode&);  // 0x800CE450
    virtual bool Visit(ISceneNode&);  // 0x800CEDD4
    virtual ~CExplosion();  // 0x800CF634
    CExplosion(CScene&, CVector3, float, float, float, EScriptBulletType, ISceneNode*, bool);  // 0x800CF6AC
};
SIZE_ASSERT(CExplosion, 0x44);

#endif
