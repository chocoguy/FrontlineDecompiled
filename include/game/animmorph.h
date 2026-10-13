#ifndef _GAME_ANIMMORPH_H_
#define _GAME_ANIMMORPH_H_

#include "types.h"
#include "game_types.h"
#include "game/animated.h"

struct AnimMotFrameList_t;
class CAnimObject;
class CDrawContext;
struct CharFrame_t;

#define ANIMMORPH_NUM_WEIGHTS 64

// A morph-target (blend shape) animated mesh, e.g. faces.
class CAnimMorph : public CAnimated {
public:
    /* 0xCC4 */ u8 unk_0xCC4[0xC];
    /* 0xCD0 */ f32 m_weights[ANIMMORPH_NUM_WEIGHTS];  // DMMorphObject_T starts here
    /* 0xDD0 */ u8 unk_0xDD0[0x10];
    /* 0xDE0 */ bool m_weightsDirty;  // weights may be non-zero

    virtual CAnimMorph* AsAnimMorph() { return this; }  // 0x80064C9C
    virtual void UpdateAnimation(float);  // 0x80065198
    void SetBlendTargetWeight(unsigned long, float);  // 0x800652A0
    void Init(CAnimObject*, unsigned short, CharFrame_t*);  // 0x800652E0
    virtual void Draw(CDrawContext&);  // 0x800653C0
    virtual ~CAnimMorph();  // 0x80065568
    CAnimMorph();  // 0x800655C4
};

#endif
