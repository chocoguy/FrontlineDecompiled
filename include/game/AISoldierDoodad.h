// CAISoldierDoodad: AI adapter for a CSoldierObject. Hand-maintained.
#ifndef _GAME_AISOLDIERDOODAD_H_
#define _GAME_AISOLDIERDOODAD_H_

#include "types.h"
#include "game/AIDoodad.h"

class CSoldierObject;

// size 0x54; vtable 0x8018366C
class CAISoldierDoodad : public CAIDoodad {
public:
    /* 0x48 */ u8 unk_0x48[0x8];
    /* 0x50 */ CSoldierObject* m_pSoldier;

    CAISoldierDoodad();
    virtual ~CAISoldierDoodad();
    virtual ISceneNode* GetSceneNode() const { return (ISceneNode*)(void*)m_pSoldier; }
    virtual void FireAtCurrentTarget(float accuracy);  // 0x80059418
    virtual void FireMMGAtCurrentTarget(CStaticObject* gun);  // 0x80059344
    void Init(CSoldierObject* obj);
    void CleanUp();
    void PreUpdate();
    void Update(float dt);
};

#ifdef FRONTLINE_NATIVE
// Bytes a soldier's factory slot needs: the object, its AI filter and the
// filter's AI object (see CAISoldierDoodad::Init).
int NativeSoldierSlotSize();
#endif

#endif
