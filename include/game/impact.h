// CImpact: a scripted melee-style hit (BIFunc_CreateImpact). Visiting the
// nodes inside a sphere, it picks the nearest one the source faces and can
// see; DamageClosestObject then hits it. Hand-maintained.
#ifndef _GAME_IMPACT_H_
#define _GAME_IMPACT_H_

#include "types.h"
#include "game_types.h"
#include "game/sphere.h"
#include "game/vector.h"

class CImpact;
class ISceneNode;

// size 0x60; vtable 0x80189974
class CImpact : public dwi::IVisitor<ISceneNode> {
public:
    CImpact(const CVector3& center, float radius, float damage, float a, float b, EScriptBulletType type,
            ISceneNode* source)
        : m_volume(center, radius) {
        m_damage = damage;
        field_0x34 = a;
        field_0x38 = b;
        m_bulletType = type;
        m_pSource = source;
        m_pClosest = NULL;
        m_closestDist = 9999.0f;
    }

    /* 0x04 */ u8 unk_0x4[0xC];
    /* 0x10 */ CVolSphere m_volume;
    /* 0x30 */ f32 m_damage;  // unused by the hit itself
    /* 0x34 */ f32 field_0x34;  // the damage dealt (doubled from behind)
    /* 0x38 */ f32 field_0x38;
    /* 0x3C */ s32 m_bulletType;  // EScriptBulletType
    /* 0x40 */ ISceneNode* m_pSource;
    /* 0x44 */ ISceneNode* m_pClosest;  // found by Visit
    /* 0x48 */ f32 m_closestDist;
    /* 0x4C */ u8 unk_0x4C[0x4];
    /* 0x50 */ CVector3 m_hitPos;  // where the line from the source meets m_pClosest
    virtual bool Visit(ISceneNode&);  // 0x800CF794
    int DamageClosestObject();  // 0x800D0164
    virtual ~CImpact();  // 0x800D0568
};
OFFSET_ASSERT(CImpact, m_hitPos, 0x50);
SIZE_ASSERT(CImpact, 0x60);

#endif
