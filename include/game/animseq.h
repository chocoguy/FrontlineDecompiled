// Animation sequences: steps an AnimSeqAnimInfo_t through an animation's
// program of frame indices and opcodes. Hand-maintained.
//
// Program words: a value without both top bits set is a frame index.
// Otherwise it is an opcode:
//   0xC000 / 0xC001  end
//   0xC002           loop start
//   0xC003 n         loop end: repeat the body n times
//   0xC004 n         hold the current frame n/16 ticks longer
//   0xC005 / 0xC006  reset / clear the channel's root motion (ANIMMOT_*)
//   0x?8?? (bit 11)  user opcode, handled by the AnimSetUserOpcodeCallbacks
//                    callbacks; may read a parameter with AnimSeqGetOpcodeParameter
// A parameter word holds a 12-bit signed value; bit 12 means a second word
// follows with the low 16 bits.
#ifndef _GAME_ANIMSEQ_H_
#define _GAME_ANIMSEQ_H_

#include "types.h"

struct AnimSeqAnimInfo_t;
struct AnimSeqFrameList_t;

#define ANIMSEQ_OP_MASK 0xC000
#define ANIMSEQ_OP_END 0xC000
#define ANIMSEQ_OP_STOP 0xC001
#define ANIMSEQ_OP_LOOP_START 0xC002
#define ANIMSEQ_OP_LOOP_END 0xC003
#define ANIMSEQ_OP_DELAY 0xC004
#define ANIMSEQ_OP_RESET_MOTION 0xC005
#define ANIMSEQ_OP_CLEAR_MOTION 0xC006
#define ANIMSEQ_OP_USER 0x800
#define ANIMSEQ_PARAM_LONG 0x1000

extern "C" {
// The current user opcode's parameter (from inside an opcode callback).
s32 AnimSeqGetOpcodeParameter(void);  // 0x80066094
// Stretches the sequence so it lasts `scale` times its normal length.
void AnimSeqScaleDuration(AnimSeqAnimInfo_t* seq, float scale);  // 0x800660F8
void AnimSeqSetRate(AnimSeqAnimInfo_t* seq, float rate);  // 0x800661E0
u16 AnimSeqGetFrameCount(AnimSeqFrameList_t* anim);  // 0x80066210
// Length in ticks.
float AnimSeqGetLife(AnimSeqFrameList_t* anim);  // 0x80066244
// Advances by `ticks`; returns true once the sequence has ended.
bool AnimSeqGrow(AnimSeqAnimInfo_t* seq, float ticks);  // 0x80066278
// Returns true if the animation has a single frame.
bool AnimSeqStart(AnimSeqAnimInfo_t* seq, AnimSeqFrameList_t* anim);  // 0x80066388
}

#endif
