// State callbacks for character animation: start, update and stop the
// animations of a state (pathing, turning, aiming, dying, locomotion...).
// Hand-maintained.
#ifndef _GAME_ANIM_CALLBACK_H_
#define _GAME_ANIM_CALLBACK_H_

#include "types.h"
#include "game_types.h"
#include "game/animfile.h"

struct AnimChannel_t;
struct AnimObjHdr_t;
class CAnimObject;

// Callback data of the animations (AnimFileStateAnim_t::plugin).
struct AnimPlugRandom_t {  // random_cb
    /* 0x0 */ u16 version;
    /* 0x2 */ u16 size;
    /* 0x4 */ u32 weight;
    /* 0x8 */ u32 total;  // sum of the weights (read from the first animation)
};

struct AnimPlugTurn_t {  // turn_cb; angles are 24-bit
    /* 0x00 */ u16 version;
    /* 0x02 */ u16 size;
    /* 0x04 */ u8 unk_0x4[0x8];
    /* 0x0C */ s32 angle;     // how far the animation turns
    /* 0x10 */ s32 minAngle;  // range of turns it is used for
    /* 0x14 */ s32 maxAngle;
    /* 0x18 */ f32 duration;  // frames
    /* 0x1C */ f32 field_0x1C;
    /* 0x20 */ f32 field_0x20;
};

struct AnimPlugDeath_t {  // death_cb; masks: bit n allows value n, 0 allows all
    /* 0x00 */ u16 version;
    /* 0x02 */ u16 size;
    /* 0x04 */ u32 locations;
    /* 0x08 */ u32 field_0x8;
    /* 0x0C */ u32 postures[2];  // both 0 allows all
    /* 0x14 */ u32 intensities;
    /* 0x18 */ u32 bullets;
    /* 0x1C */ u32 variants;     // possible follow-ups (AnimDeathInfo_t::variant)
    /* 0x20 */ u32 variantChance;  // percent
    /* 0x24 */ u32 weight;
};

struct AnimPlugWeapon_t {  // weapon_cb
    /* 0x0 */ u16 version;
    /* 0x2 */ u16 size;
    /* 0x4 */ u32 weapon;
};

struct AnimPlugAim_t {  // aim_cb; degrees
    /* 0x0 */ u16 version;
    /* 0x2 */ u16 size;
    /* 0x4 */ u32 field_0x4;
    /* 0x8 */ s32 yaw;
    /* 0xC */ s32 pitch;
};

// Locomotion speeds (CAnimDatabase::GetStateInfo of the floco state).
struct AnimFlocoSpeed_t {
    /* 0x0 */ u32 field_0x0;
    /* 0x4 */ f32 speed;  // per frame
    /* 0x8 */ u32 field_0x8;
};
struct AnimPlugFloco_t {
    /* 0x00 */ u16 version;
    /* 0x02 */ u16 size;
    /* 0x04 */ u32 count;
    /* 0x08 */ u32 field_0x8;
    /* 0x0C */ u32 field_0xC;
    /* 0x10 */ u32 field_0x10;
    /* 0x14 */ AnimFlocoSpeed_t speeds[1];
};

// What killed the character (the data passed to the death state).
struct AnimDeathInfo_t {
    /* 0x00 */ u32 intensity;
    /* 0x04 */ u32 field_0x4;
    /* 0x08 */ u32 bullet;
    /* 0x0C */ u32 location;
    /* 0x10 */ s32 posture;
    /* 0x14 */ u32 variant;  // out: 0, or 1 + the chosen follow-up
};

class CAnimCallbacks {
public:
    static u16 _TurnFindTurnAnimInfo(AnimFileStateAnimList_t*, CAnimObject*, unsigned short, long*, float*, float*, float*, float*, float);  // 0x8005E6D4
    static int animfinish_cb(unsigned short, void*);  // 0x8005E9BC
    static int aim_cb(AnimFileStateAnimList_t*, unsigned short, AnimObjHdr_t*, AnimChannel_t*, void*, unsigned long, void*, float);  // 0x8005E9EC
    static int floco_cb(AnimFileStateAnimList_t*, unsigned short, AnimObjHdr_t*, AnimChannel_t*, void*, unsigned long, void*, float);  // 0x8005F2C4
    static int weapon_cb(AnimFileStateAnimList_t*, unsigned short, AnimObjHdr_t*, AnimChannel_t*, void*, unsigned long, void*, float);  // 0x8005FAB4
    static int death_cb(AnimFileStateAnimList_t*, unsigned short, AnimObjHdr_t*, AnimChannel_t*, void*, unsigned long, void*, float);  // 0x8005FB58
    static int turn_cb(AnimFileStateAnimList_t*, unsigned short, AnimObjHdr_t*, AnimChannel_t*, void*, unsigned long, void*, float);  // 0x8005FF80
    static int pathing_cb(AnimFileStateAnimList_t*, unsigned short, AnimObjHdr_t*, AnimChannel_t*, void*, unsigned long, void*, float);  // 0x80060554
    static int random_cb(AnimFileStateAnimList_t*, unsigned short, AnimObjHdr_t*, AnimChannel_t*, void*, unsigned long, void*, float);  // 0x80060788
    static int face_cb(AnimFileStateAnimList_t*, unsigned short, AnimObjHdr_t*, AnimChannel_t*, void*, unsigned long, void*, float);  // 0x80060858
    static int _AnimIntfOpcodeSkip(void*, unsigned short, unsigned long);  // 0x80064570
    static int _AnimIntfOpcodeProcess(void*, unsigned short, unsigned long);  // 0x800646C8
};

// Free functions
// Byte-swaps an animation's callback data; `callback` indexes the callback
// table (AnimCallback_Init).
void EndianSwapAnimPlugIn(AnimFilePluginData_t&, int callback);  // 0x8005DE58
// Blend weights of the 5 aim animations (up-left, up-right, down-right,
// down-left, center) for a target at (x, y) in view units.
void _find_aim_blend_values(float x, float y, float* weights);  // 0x8005E494
void AnimCallback_Init();  // 0x800608B4

#endif
