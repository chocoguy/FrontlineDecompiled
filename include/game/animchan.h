// Animation channels: each plays one animation sequence with a blend weight.
// Hand-maintained.
#ifndef _GAME_ANIMCHAN_H_
#define _GAME_ANIMCHAN_H_

#include "types.h"
#include "game/animfile.h"
#include "game/animwgt.h"

#define ANIMCHAN_FREE 0
#define ANIMCHAN_DONE 1
#define ANIMCHAN_RUNNING 2

// Header of an animated object: owns the channel array.
struct AnimObjHdr_t {
    /* 0x0 */ u16 field_0x0;  // non-zero selects channel type 4 (else 1)
    /* 0x2 */ u16 objectType;  // animation database group
    /* 0x4 */ u16 numChannels;
    /* 0x6 */ u8 unk_0x6[0x2];
    /* 0x8 */ u32 field_0x8;
};

// size 0x88
struct AnimChannel_t {
    /* 0x00 */ u8 type;
    /* 0x01 */ u8 status;  // ANIMCHAN_*
    /* 0x02 */ u16 animIndex;
    /* 0x04 */ u16 fileNum;
    /* 0x06 */ u8 unk_0x6[0x2];
    /* 0x08 */ AnimFileFormat_t* file;
    /* 0x0C */ u8 animFlags;
    /* 0x0E */ u16 state;  // bit 15: may be stolen by a new animation
    // Root motion already applied this cycle (see animmot.cpp).
    /* 0x10 */ s32 motionHeading;   // rotates the root translation
    /* 0x14 */ s32 motionRot;
    /* 0x18 */ s32 motionRotScale;  // 8.24 fixed point
    /* 0x1C */ u8 unk_0x1C[0x4];
    /* 0x20 */ f32 motionPos[3];
    /* 0x2C */ u8 unk_0x2C[0x4];
    /* 0x30 */ f32 motionPosScale;
    /* 0x34 */ AnimWgtInfo_t weight;
    /* 0x50 */ f32 delay;  // ticks before the channel starts advancing
    /* 0x54 */ f32 speed;
    /* 0x58 */ AnimSeqAnimInfo_t seq;

    AnimChannel_t() {}  // 0x80065680
};

extern "C" {
void AnimChanInitChannels(AnimObjHdr_t* hdr, AnimChannel_t* chans);  // 0x80061BF8
void AnimChanProcess(AnimObjHdr_t* hdr, AnimChannel_t* chans, void* userData, float dt);  // 0x80061ACC
// Returns the channel used, or -1.
int AnimChanStartAnim(AnimObjHdr_t* hdr, AnimChannel_t* chans, AnimFileFormat_t* file,
                      u16 animIndex, u16 state, void* userData, float weight);  // 0x800618FC
void AnimChanStopByStateIndex(AnimObjHdr_t* hdr, AnimChannel_t* chans, u16 state);  // 0x800618A8
// Switches a running channel to another animation, keeping its phase.
void AnimChanSwitchAnimByDuration(AnimObjHdr_t* hdr, AnimChannel_t* chans, int index,
                                  AnimFileFormat_t* file, u16 animIndex, u16 state,
                                  void* userData);  // 0x80061700
int AnimChanGetNumActiveByState(AnimObjHdr_t* hdr, AnimChannel_t* chans, u16 state);  // 0x800616C0
int AnimChanGetNumRunningOrDoneByState(AnimObjHdr_t* hdr, AnimChannel_t* chans, u16 state);  // 0x80061678
// The `index`th running-or-done channel for `state`, or 0xFFFF.
u16 AnimChanGetStateChannelByIndex(AnimObjHdr_t* hdr, AnimChannel_t* chans, u16 state, u16 index);  // 0x80061608
void AnimChanSetDelayByState(AnimObjHdr_t* hdr, AnimChannel_t* chans, u16 state, float delay);  // 0x800615C8
void AnimChanSetSpeed(AnimChannel_t* chan, float speed);  // 0x800615C0
}

#endif
