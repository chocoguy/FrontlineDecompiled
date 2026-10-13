// CAITankDoodad: AI adapter for a CTankObject. Hand-maintained.
#ifndef _GAME_AITANKDOODAD_H_
#define _GAME_AITANKDOODAD_H_

#include "types.h"
#include "game/AIDoodad.h"

class CTankObject;

// size 0x54; vtable 0x80183694
class CAITankDoodad : public CAIDoodad {
public:
    /* 0x48 */ u8 unk_0x48[0x8];
    /* 0x50 */ CTankObject* m_pTank;

    CAITankDoodad();
    virtual ~CAITankDoodad();
    virtual ISceneNode* GetSceneNode() const { return (ISceneNode*)(void*)m_pTank; }
    void Init(CTankObject* obj);
    void CleanUp();
    void PreUpdate();
    void Update(float dt);
};

#endif
