// CAIPlayerDoodad: AI adapter for a CPlayerObject. Hand-maintained.
#ifndef _GAME_AIPLAYERDOODAD_H_
#define _GAME_AIPLAYERDOODAD_H_

#include "types.h"
#include "game/AIDoodad.h"

class CPlayerObject;

// size 0x54; vtable 0x80183644
class CAIPlayerDoodad : public CAIDoodad {
public:
    /* 0x48 */ u8 unk_0x48[0x8];
    /* 0x50 */ CPlayerObject* m_pPlayer;

    CAIPlayerDoodad();
    virtual ~CAIPlayerDoodad();
    virtual ISceneNode* GetSceneNode() const { return (ISceneNode*)(void*)m_pPlayer; }
    void Init(CPlayerObject* obj);
    void CleanUp();
    void PreUpdate();
    void Update(float dt);
};

#endif
