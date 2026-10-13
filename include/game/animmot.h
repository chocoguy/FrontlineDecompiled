// Animation motion: the list of weighted key frames contributing to the
// current pose, and the root motion (rotation, translation, floating bone)
// blended from it. Hand-maintained.
#ifndef _GAME_ANIMMOT_H_
#define _GAME_ANIMMOT_H_

#include "types.h"
#include "game/vector.h"
#include "game/orient.h"

struct AnimChannel_t;
struct AnimFileFormat_t;
struct AnimFrameInfo_t;
struct AnimObjHdr_t;
struct AnimStInfo_t;

#define ANIMMOT_MAX_FRAMES 24

// AnimMotWeightFrame_t::flags (the channel's animation flags)
#define ANIMMOT_MIRROR 0x1  // negate x translation and the y/z angles
#define ANIMMOT_ROTATION 0x2
#define ANIMMOT_POS_XY 0x4
#define ANIMMOT_POS_Z 0x8
#define ANIMMOT_FLOATING_BONE 0x10

// AnimSeqAnimInfo_t::motionFlags: set when the sequence restarts or wraps
#define ANIMMOT_POS_RESET 0x1    // forget the translation already applied
#define ANIMMOT_POS_WRAPPED 0x2  // lerp the next frame's translation from zero
#define ANIMMOT_POS_CLEAR 0x4
#define ANIMMOT_ROT_RESET 0x8
#define ANIMMOT_ROT_WRAPPED 0x10
#define ANIMMOT_ROT_CLEAR 0x20

// One key frame of one channel, with its share of the pose. Filled in by
// AnimMotCreateWeightList; the motion fields are unpacked by skelblend.cpp.
// size 0x50
struct AnimMotWeightFrame_t {
    /* 0x00 */ f32 weight;  // channel weight x state weight
    /* 0x04 */ f32 blend;   // share of the pose (cumulative after normalizing)
    /* 0x08 */ u8 flags;    // ANIMMOT_ROTATION etc.
    /* 0x09 */ u8 channel;
    /* 0x0C */ long rot;
    /* 0x10 */ CVector3 pos;
    /* 0x20 */ CVector3 floatPos;
    /* 0x30 */ Orient3_t floatRot;
    /* 0x3C */ AnimFileFormat_t* file;
    /* 0x40 */ u16 frameIndex;
    /* 0x44 */ AnimFrameInfo_t* frameInfo;
    /* 0x48 */ u8* frame;  // packed little-endian key frame
    /* 0x4C */ u8 unk_0x4C[0x4];

    AnimMotWeightFrame_t() {}
};

// size 0x788
struct AnimMotFrameList_t {
    /* 0x0 */ u16 count;
    /* 0x2 */ u16 max;
    /* 0x4 */ u8 unk_0x4[0x4];
    /* 0x8 */ AnimMotWeightFrame_t frames[ANIMMOT_MAX_FRAMES];
};

// Each Calc function only uses channels playing `state` (any when either is
// 0xFFFF) unless `all` is 1, and returns whether any channel matched.
extern "C" {
AnimMotFrameList_t* AnimMotCreateWeightList(AnimObjHdr_t* hdr, AnimChannel_t* chans,
                                            AnimStInfo_t* stateInfo);  // 0x80065DB0
bool AnimMotCalcAnimRot(AnimObjHdr_t* hdr, AnimChannel_t* chans, AnimMotFrameList_t* list,
                        long* out, u16 state, u8 all);  // 0x80065BD4
bool AnimMotCalcAnimPos(AnimObjHdr_t* hdr, AnimChannel_t* chans, AnimMotFrameList_t* list,
                        CVector3* out, u16 state, u8 all);  // 0x800658C4
bool AnimMotCalcFloatingBone(AnimObjHdr_t* hdr, AnimChannel_t* chans, AnimMotFrameList_t* list,
                             CVector3* pos, Orient3_t* rot, u16 state, u8 all);  // 0x800656B8
}

#endif
