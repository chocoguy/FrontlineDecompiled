// Pose blending: unpacks the key frames of an AnimMotFrameList_t and blends
// them into one CharFrame_t. Hand-maintained.
#ifndef _GAME_SKELBLEND_H_
#define _GAME_SKELBLEND_H_

#include "types.h"

struct AnimMotFrameList_t;
struct AnimMotWeightFrame_t;
struct CharFrame_t;

extern "C" {
// dst->jointData must hold dst->numJoints joints.
void BlendSkelFrames(CharFrame_t* dst, AnimMotFrameList_t* frames);  // 0x8006A39C
}

#endif
