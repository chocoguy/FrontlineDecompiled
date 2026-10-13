// CCharacterShadow: a projected blob shadow under a character, faded by
// camera distance. Hand-maintained.
#ifndef _GAME_SHADOW_H_
#define _GAME_SHADOW_H_

#include "types.h"
#include "game/box.h"

class CDrawContext;
class ISceneNode;

// size 0x70; vtable 0x80184CE0
class CCharacterShadow {
public:
    virtual ~CCharacterShadow();  // 0x800816D8
    virtual void Draw(CDrawContext& ctx);  // 0x80081454
    virtual bool IsDrawEnabled() const { return m_enabled; }  // 0x8008144C
    /* 0x04 */ u8 unk_0x4[0xC];
    /* 0x10 */ CVolBox m_box;  // projection volume, rebuilt each draw
    /* 0x60 */ bool m_enabled;
    /* 0x64 */ ISceneNode* m_pOwner;
    /* 0x68 */ f32 m_alphaScale;
    /* 0x6C */ f32 m_alpha;

    CCharacterShadow();  // 0x8008173C
    static void InitClass();  // 0x8008188C
    CVolBox& GetBox() { return m_box; }  // 0x80074BD0
};

#endif
