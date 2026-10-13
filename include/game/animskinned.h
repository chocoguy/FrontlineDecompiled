// CAnimSkinned: an animated object drawn as a skinned (clustered) mesh.
// Hand-maintained.
#ifndef _GAME_ANIMSKINNED_H_
#define _GAME_ANIMSKINNED_H_

#include "types.h"
#include "game/animated.h"
#include "game/skeleton.h"
#include "game/vector.h"

class CAnimObject;
class CDrawContext;
class CMatrix;

#define ANIMSKINNED_HAS_ROTATION 0x1
#define ANIMSKINNED_HAS_TRANSLATION 0x2

// size 0xF08; vtable 0x8018391C
class CAnimSkinned : public CAnimated {
public:
    /* 0xCC4 */ u8 unk_0xCC4[0xC];
    /* 0xCD0 */ CharFrame_t m_frame;               // blended pose (m_jointData follows)
    /* 0xD10 */ s16 m_jointData[0x50][3];
    /* 0xEF0 */ u32 m_motionFlags;                // ANIMSKINNED_HAS_*
    /* 0xEF4 */ long m_motionRot;
    /* 0xEF8 */ CVector3 m_motionTrans;

    CAnimSkinned();  // 0x80066E98
    virtual ~CAnimSkinned();  // 0x80066E3C
    static void InitClass();  // 0x80066E38
    virtual void Draw(CDrawContext& ctx);  // 0x80066DAC
    void Init(CAnimObject* owner, unsigned short arg, CharFrame_t* baseFrame);  // 0x80066CF0
    virtual void UpdateAnimation(float dt);  // 0x80066BEC
    virtual int ExtractRotation(long* out);  // 0x80066BC8
    virtual int ExtractTranslation(CVector3* out);  // 0x80066B9C
    virtual void SetRotation(long rot);  // 0x80066B94
    virtual void BindSkeleton(CharSkel_t* skel);  // 0x80066ADC
    // Joint matrices for the current pose; also widens min/max by 0.1.
    void UpdateSkeletonMatrices(CMatrix* matrices, CVector3* min, CVector3* max);  // 0x80066A08
    virtual CAnimSkinned* AsAnimSkinned() { return this; }  // 0x80066A04
};

#endif
