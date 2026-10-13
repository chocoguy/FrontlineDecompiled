// Animation file list structures filled by the AnimFile* queries. Only the
// headers decompiled code uses so far. Hand-maintained.
#ifndef _GAME_ANIMFILE_H_
#define _GAME_ANIMFILE_H_

#include "types.h"

struct AnimEmotStateList_t;
struct AnimEmotAnimList_t;

#define ANIMFILE_MAGIC 0x454D5400  // 'EMT'
#define ANIMFILE_VERSION 1

// A loaded animation file (.emt) header; offsets become pointers in
// AnimFileRelocate.
struct AnimFileFormat_t {
    /* 0x00 */ u32 magic;  // ANIMFILE_MAGIC
    /* 0x04 */ u32 version;
    /* 0x08 */ u16 objectType;
    /* 0x0A */ u16 fileNum;
    /* 0x0C */ u16 numFrames;
    /* 0x0E */ u16 field_0xE;
    /* 0x10 */ AnimEmotStateList_t* states;
    /* 0x14 */ AnimEmotAnimList_t* anims;
    /* 0x18 */ u8** frames;  // numFrames + 1 (the end); ARAM addresses after the upload
};

// Per-animation data for its state's callback (anim_callback.cpp); the
// layout after the header depends on the callback (AnimPlug*_t there).
struct AnimFilePluginData_t {
    /* 0x0 */ u16 version;  // >= 0x100: still byte-swapped
    /* 0x2 */ u16 size;     // 0: no callback data
    /* 0x4 */ u32 data[1];
};

// The animations a state can play, gathered from every loaded file.
struct AnimFileStateAnim_t {
    /* 0x0 */ AnimFileFormat_t* file;
    /* 0x4 */ u16 animIndex;
    /* 0x8 */ AnimFilePluginData_t* plugin;
};

struct AnimFileStateAnimList_t {
    /* 0x0 */ u16 count;
    /* 0x2 */ u16 capacity;
    /* 0x4 */ AnimFileStateAnim_t anims[0xFA];
};

// What to do when leaving one state for another (see animtran.cpp).
struct AnimTransition_t {
    /* 0x0 */ u8 unk_0x0[0x4];
    /* 0x4 */ u16 type;      // ANIMTRAN_*
    /* 0x6 */ u16 duration;  // ANIMTRAN_BLEND: frames
    /* 0x8 */ f32 overlap;   // ANIMTRAN_PLAY: fraction of the clip not cross-faded
    /* 0xC */ u16 fileNum;   // ANIMTRAN_PLAY: clip to play in between
    /* 0xE */ u16 animIndex;
};

// One way into a state: from an animation (key = file | 0x8000, sub = anim)
// or from a state (key = 0xFFFF or a state without bit 15, sub = state),
// optionally only while animation `cond` is between program words lo..hi.
struct AnimTranEntry_t {
    /* 0x0 */ u16 key;
    /* 0x2 */ u16 sub;
    /* 0x4 */ u16 cond;  // 0xFFFF = any
    /* 0x6 */ u16 lo;
    /* 0x8 */ u16 hi;
    /* 0xA */ u8 unk_0xA[0x2];
    /* 0xC */ AnimTransition_t* transition;
};

struct AnimTranList_t {
    /* 0x0 */ s32 count;
    /* 0x4 */ AnimTranEntry_t entries[1];
};

struct AnimTranListRef_t {
    /* 0x0 */ AnimFileFormat_t* file;
    /* 0x4 */ u16 fileNum;
    /* 0x8 */ AnimTranList_t* list;
};

// The transition lists into one state, gathered from every loaded file.
// size 0x68
struct AnimTranTransListArray_t {
    /* 0x0 */ u16 count;
    /* 0x2 */ u16 capacity;
    /* 0x4 */ u16 state;
    /* 0x8 */ AnimTranListRef_t lists[8];
};

// A state's animations in a file.
struct AnimEmotAnimRef_t {
    /* 0x0 */ u16 animIndex;
    /* 0x2 */ u16 field_0x2;
    /* 0x4 */ AnimFilePluginData_t* plugin;
};
struct AnimEmotStateAnims_t {
    /* 0x0 */ u32 count;
    /* 0x4 */ AnimEmotAnimRef_t anims[1];
};

// size 0x14
struct AnimEmotStateDef_t {
    /* 0x00 */ u16 state;
    /* 0x02 */ u16 field_0x2;
    /* 0x04 */ s32 callback;  // callback id; an index after relocation
    /* 0x08 */ AnimEmotStateAnims_t* anims;
    /* 0x0C */ AnimFilePluginData_t* info;  // AnimFileGetStateInfo
    /* 0x10 */ AnimTranList_t* transitions;
};
struct AnimEmotStateList_t {
    /* 0x0 */ u32 count;
    /* 0x4 */ AnimEmotStateDef_t states[1];
};

struct AnimFrameInfo_t;
// An animation as stored in a file (see AnimSeqFrameList_t).
struct AnimEmotAnimation_t {
    /* 0x0 */ u8 frameTime;
    /* 0x1 */ u8 unk_0x1;
    /* 0x2 */ u8 flags;
    /* 0x3 */ u8 unk_0x3;
    /* 0x4 */ AnimFrameInfo_t* frameInfo;
    /* 0x8 */ u16 program[1];
};
struct AnimEmotAnimList_t {
    /* 0x0 */ u32 count;
    /* 0x4 */ AnimEmotAnimation_t* anims[1];
};

// An animation in a file.
// Joints animated by an animation, as runs of consecutive joints.
struct AnimJointGroup_t {
    /* 0x0 */ u8 start;
    /* 0x1 */ u8 count;
};

struct AnimJointGroups_t {
    /* 0x0 */ u16 count;
    /* 0x2 */ AnimJointGroup_t groups[1];
};

// How an animation's packed frames map onto the skeleton.
#define ANIMFRAMEINFO_MAGIC 0x534B454C  // 'SKEL'
struct AnimFrameInfo_t {
    /* 0x0 */ u32 magic;  // ANIMFRAMEINFO_MAGIC
    /* 0x4 */ u8* basePose;  // 2-byte header, then 3 little-endian s16 per joint
    /* 0x8 */ AnimJointGroups_t* groups;  // joints whose frames carry deltas
};

// An animation: a program of frame indices and opcodes (see animseq.cpp).
struct AnimSeqFrameList_t {
    /* 0x0 */ u8 frameTime;  // ticks per frame, in 1/16ths
    /* 0x1 */ u8 unk_0x1;
    /* 0x2 */ u8 flags;
    /* 0x3 */ u8 unk_0x3;
    /* 0x4 */ AnimFrameInfo_t* frameInfo;
    /* 0x8 */ u16 program[1];
};

struct AnimSeqLoop_t {
    /* 0x0 */ u16 pc;        // first opcode of the loop body
    /* 0x2 */ u16 count;     // iterations left (0 = not started)
    /* 0x4 */ f32 position;  // position at the start of the body
};

// Playback state of one animation sequence (embedded in a channel).
struct AnimSeqAnimInfo_t {
    /* 0x00 */ AnimSeqFrameList_t* anim;
    /* 0x04 */ f32 rate;      // ticks per frame
    /* 0x08 */ f32 position;  // ticks
    /* 0x0C */ f32 timeLeft;  // until frame1 is reached
    /* 0x10 */ u16 frame0;      // the two frames around position,
    /* 0x12 */ u16 frame1;      // blended by frameFrac toward frame1
    /* 0x14 */ f32 frameFrac;
    /* 0x18 */ u16 pc;          // next opcode in anim->program
    /* 0x1A */ u16 frameCount;  // cached, 0xFFFF when unknown
    /* 0x1C */ u16 framesParsed;
    /* 0x1E */ u8 motionFlags;  // ANIMMOT_* (root motion reset/wrap state)
    /* 0x1F */ u8 loopDepth;
    /* 0x20 */ AnimSeqLoop_t loops[2];  // loops[loopDepth - 1] is innermost
};

extern "C" {
// Pointer to the packed data of one frame (may stream it from ARAM).
void* AnimFileGetFrame(AnimFileFormat_t* file, u16 frame);  // 0x80063238 (animemot.cpp)
}

#endif
