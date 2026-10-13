// Animation weights: a value driven toward a target by one of a few
// per-frame functions. Hand-maintained.
#ifndef _GAME_ANIMWGT_H_
#define _GAME_ANIMWGT_H_

#include "types.h"

#define ANIMWGT_CONSTANT 0
#define ANIMWGT_LINEAR 1
#define ANIMWGT_WAIT_TO_STOP 2
#define ANIMWGT_LINEAR_IN_AND_OUT 3

// Pass to keep the current weight or target.
#define ANIMWGT_KEEP (-1.0f)

struct AnimWgtLinear_t {
    f32 start;
    u32 frames;
};

// Ramps from `from` to `to` over rampFrames, holds, then ramps back.
struct AnimWgtLinearInAndOut_t {
    /* 0x0 */ f32 from;
    /* 0x4 */ f32 to;
    /* 0x8 */ s32 holdFrames;
    /* 0xC */ s8 rampFrames;
    /* 0xD */ s8 counter;
    /* 0xE */ s8 phase;  // 0 ramp in, 1 hold, 2 ramp out
};

// size 0x1C
struct AnimWgtInfo_t {
    /* 0x00 */ f32 weight;
    /* 0x04 */ f32 target;
    /* 0x08 */ s32 type;
    /* 0x0C */ union {
        AnimWgtLinear_t linear;
        AnimWgtLinearInAndOut_t inOut;
        u8 raw[0x10];
    } data;
};

extern "C" {
s32 AnimWgtInit(void);  // 0x80069010
s32 AnimWgtShutdown(void);  // 0x80069008
void AnimWgtSet(AnimWgtInfo_t* info, float weight, float target, s32 type, void* data);  // 0x80068F9C
// Runs the weight function `frames` times.
void AnimWgtProcess(AnimWgtInfo_t* info, float frames);  // 0x80068EEC
void AnimWgtSetLinear(AnimWgtInfo_t* info, float weight, float target, u32 frames);  // 0x80068E0C
void AnimWgtSetLinearInAndOut(AnimWgtInfo_t* info, float from, float to, u8 rampFrames,
                              u8 holdFrames);  // 0x80068CF8
}

#endif
