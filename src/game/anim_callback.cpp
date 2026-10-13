// State callbacks for character animation (see animst.cpp): each starts the
// animations of its state (event 0), updates them (2) and handles the end
// (1). userData is the CAnimated playing the state; `arg` is the data given
// to SetAnimationState.
#include "game/anim_callback.h"

#include <math.h>
#include <stdlib.h>

#include "game/AIDoodad.h"
#include "game/AIFilter.h"
#include "game/AIObject.h"
#include "game/Moh2.h"
#include "game/Soldier_object.h"
#include "game/anim.h"
#include "game/anim_obj.h"
#include "game/animated.h"
#include "game/animchan.h"
#include "game/animdb.h"
#include "game/animplayer.h"
#include "game/animseq.h"
#include "game/animst.h"
#include "game/animwgt.h"
#include "game/matrix.h"
#include "game/player.h"
#include "game/trig.h"

#define ANGLE_FULL 0x1000000  // 24-bit angles
#define ANGLE_MASK 0xFFFFFF

// Callback table (AnimStSetCallbackTable), by the database's callback id.
static AnimStCallbackEntry_t _AnimObj_StateSelFuncArray[9];
static AnimStCallbackTable_t _AnimObj_StateDef = {9, _AnimObj_StateSelFuncArray};

// Locomotion animation ranges: [dir], [dir + 1] standing; [dir + 4],
// [dir + 5] crouching; [8] jumping.
static const s8 _floco_anim_ranges[10] = {0, 3, 6, 9, 12, 14, 16, 18, 20, 25};

// State user memory (AnimStGetStateUserMemory) of each callback.
struct PathingMem {
    /* 0x0 */ u8 unk_0x0[0x4];
    /* 0x4 */ s32 channel;
};

struct TurnMem {
    /* 0x00 */ u8 unk_0x0[0x4];
    /* 0x04 */ s32 channel;
    /* 0x08 */ s32 rate;      // 24-bit angle per frame; 0 when done
    /* 0x0C */ f32 moveLeft;  // frames of the moving turn left
    /* 0x10 */ f32 moveTime;
    /* 0x14 */ u8 trackTarget;  // turning to face the AI's target
    /* 0x15 */ u8 moving;       // also moving to `dest`
    /* 0x16 */ u8 unk_0x16[0x2];
    /* 0x18 */ f32 dest[3];
};

struct FlocoMem {
    /* 0x00 */ u8 unk_0x0[0x4];
    /* 0x04 */ CAnimatedPlayer* player;
    /* 0x08 */ s32 dir;
    /* 0x0C */ s32 first;  // animation range of dir
    /* 0x10 */ s32 last;
    /* 0x14 */ f32 speedLo;  // speeds of the two blended animations
    /* 0x18 */ f32 speedHi;
    /* 0x1C */ f32 rateLo;
    /* 0x20 */ f32 rateHi;
    /* 0x24 */ u8 channelLo;
    /* 0x25 */ u8 channelHi;
    /* 0x26 */ u8 animLo;
    /* 0x27 */ u8 animHi;
    /* 0x28 */ u8 unk_0x28[0x2];
    /* 0x2A */ u8 crouching;
};

#define AIM_NUM_ANIMS 5
struct AimMem {
    /* 0x00 */ u8 unk_0x0[0x4];
    /* 0x04 */ f32 target[3];
    struct {
        /* 0x0 */ f32 weight;
        /* 0x4 */ s32 channel;
    } anims[AIM_NUM_ANIMS];  // 0x10
};

static inline AnimStInfo_t* StateInfo(CAnimated* anim) { return (AnimStInfo_t*)anim->m_stateInfo; }

static inline AnimFileStateAnim_t& Anim(AnimFileStateAnimList_t* list, int i) {
    return list->anims[i];
}

static inline int StartAnim(AnimObjHdr_t* hdr, AnimChannel_t* chans, AnimFileStateAnimList_t* list,
                            int i, u16 state, void* userData, float weight) {
    return AnimChanStartAnim(hdr, chans, list->anims[i].file, list->anims[i].animIndex, state,
                             userData, weight);
}

static inline void SwitchAnim(AnimObjHdr_t* hdr, AnimChannel_t* chans, int channel,
                              AnimFileStateAnimList_t* list, int i, u16 state, void* userData) {
    AnimChanSwitchAnimByDuration(hdr, chans, channel, list->anims[i].file, list->anims[i].animIndex,
                                 state, userData);
}

// Heading (24-bit) from the object towards its goal.
static inline int GoalAngle(CAnimObject* obj) {
    float d[3];
    d[0] = obj->m_goal[0] - obj->m_tm.m[3][0];
    d[1] = obj->m_goal[1] - obj->m_tm.m[3][1];
    d[2] = obj->m_goal[2] - obj->m_tm.m[3][2];
    return MathArcTan2(-d[0], d[1]);
}

// The turn towards the goal in degrees (-180..180], limited to 6 degrees
// per frame.
static inline float GoalTurnDegrees(CAnimObject* obj, float frames) {
    int delta = ((GoalAngle(obj) & ANGLE_MASK) - obj->m_heading) & ANGLE_MASK;
    float deg = 360.0f * (float)delta / 16777216.0f;
    if (deg > 180.0f) {
        deg -= 360.0f;
    }
    if (deg > 6.0f * frames) {
        deg = 6.0f * frames;
    } else if (deg < -6.0f * frames) {
        deg = -6.0f * frames;
    }
    return deg;
}

static inline CAIObject* SoldierAI(CSoldierObject* soldier) {
    return soldier->GetAIDoodad()->m_pFilter->m_pAIObject;
}

// Where an AI aims: its target, or a metre above where it last saw it.
static inline void AIAimPoint(CAIObject* ai, float* out) {
    if (ai->m_pTarget == NULL) {
        out[0] = ai->m_lastSeenPosition.field_0x0;
        out[1] = ai->m_lastSeenPosition.field_0x4;
        out[2] = ai->m_lastSeenPosition.field_0x8 + 1.0f;
    } else {
        out[0] = ai->m_targetPosition.field_0x0;
        out[1] = ai->m_targetPosition.field_0x4;
        out[2] = ai->m_targetPosition.field_0x8;
    }
}

void AnimCallback_Init() {
    _AnimObj_StateSelFuncArray[0].id = 1000;
    _AnimObj_StateSelFuncArray[0].callback = (AnimStCallback)CAnimCallbacks::pathing_cb;
    _AnimObj_StateSelFuncArray[1].id = 1100;
    _AnimObj_StateSelFuncArray[1].callback = (AnimStCallback)CAnimCallbacks::random_cb;
    _AnimObj_StateSelFuncArray[2].id = 1001;
    _AnimObj_StateSelFuncArray[2].callback = (AnimStCallback)CAnimCallbacks::turn_cb;
    _AnimObj_StateSelFuncArray[3].id = 1200;
    _AnimObj_StateSelFuncArray[3].callback = (AnimStCallback)CAnimCallbacks::death_cb;
    _AnimObj_StateSelFuncArray[4].id = 1300;
    _AnimObj_StateSelFuncArray[4].callback = (AnimStCallback)CAnimCallbacks::weapon_cb;
    _AnimObj_StateSelFuncArray[5].id = 1002;
    _AnimObj_StateSelFuncArray[5].callback = (AnimStCallback)CAnimCallbacks::aim_cb;
    _AnimObj_StateSelFuncArray[6].id = 1010;
    _AnimObj_StateSelFuncArray[6].callback = (AnimStCallback)CAnimCallbacks::face_cb;
    _AnimObj_StateSelFuncArray[7].id = 1020;
    _AnimObj_StateSelFuncArray[7].callback = (AnimStCallback)CAnimCallbacks::pathing_cb;
    _AnimObj_StateSelFuncArray[8].id = 1400;
    _AnimObj_StateSelFuncArray[8].callback = (AnimStCallback)CAnimCallbacks::floco_cb;
    AnimStSetCallbackTable(&_AnimObj_StateDef);
    AnimStRegisterFinishCallback((AnimStFinishCallback)CAnimCallbacks::animfinish_cb);
}

// Plays the state's first animation.
int CAnimCallbacks::face_cb(AnimFileStateAnimList_t* anims, unsigned short state, AnimObjHdr_t* hdr,
                            AnimChannel_t* chans, void* userData, unsigned long event, void* arg,
                            float frames) {
    if (event == ANIMST_EVENT_START) {
        StartAnim(hdr, chans, anims, 0, state, userData, 1.0f);
    }
    return 0;
}

// Plays one of the state's animations, picked by weight.
int CAnimCallbacks::random_cb(AnimFileStateAnimList_t* anims, unsigned short state,
                              AnimObjHdr_t* hdr, AnimChannel_t* chans, void* userData,
                              unsigned long event, void* arg, float frames) {
    if (event == ANIMST_EVENT_START) {
        AnimPlugRandom_t* plug = (AnimPlugRandom_t*)anims->anims[0].plugin;
        u32 roll = (u32)rand() % plug->total;
        int i = 0;
        u32 sum = 0;
        while (i < anims->count - 1 && plug->weight + sum <= roll) {
            sum += plug->weight;
            plug = (AnimPlugRandom_t*)anims->anims[i + 1].plugin;
            i++;
        }
        StartAnim(hdr, chans, anims, i, state, userData, 1.0f);
    }
    return 0;
}

CAnimSkinned* CAnimated::AsAnimSkinned() { return NULL; }

// Walking along a path: plays the first animation and, when asked
// (m_pathTurn), turns the character towards its goal.
int CAnimCallbacks::pathing_cb(AnimFileStateAnimList_t* anims, unsigned short state,
                               AnimObjHdr_t* hdr, AnimChannel_t* chans, void* userData,
                               unsigned long event, void* arg, float frames) {
    CAnimated* anim = (CAnimated*)userData;
    CAnimated* skinned = (CAnimated*)anim->AsAnimSkinned();
    CAnimObject* owner;
    if (skinned != NULL) {
        owner = skinned->m_pOwner;
    } else {
        owner = NULL;
    }
    PathingMem* mem = (PathingMem*)AnimStGetStateUserMemory(StateInfo(anim), state);
    switch (event) {
    case ANIMST_EVENT_START:
        if (owner != NULL) {
            AnimStSetStateFlags(StateInfo(anim), state, ANIMST_UPDATE_CALLBACK);
        }
        mem->channel = StartAnim(hdr, chans, anims, 0, state, userData, 1.0f);
        break;
    case ANIMST_EVENT_UPDATE:
        if (owner == NULL || state != AnimStGetCurrentState(StateInfo(anim))) {
            break;
        }
        if (owner->m_pathTurn != 0 && owner->m_facingTarget == 0 && owner->m_yawDelta == 0.0f) {
            float deg = GoalTurnDegrees(owner, frames);
            owner->m_yawDelta = 0.017453292f * deg + owner->m_yawDelta;
            skinned->m_channels[mem->channel].motionHeading = owner->m_heading;
        }
        owner->m_pathTurn = 0;
        break;
    }
    return 0;
}

// Turning on the spot (or while stepping to the goal): picks the animation
// for the turn and spreads the rotation over it. With a zero angle the
// character turns to face its goal, tracking the AI's target if it has one.
int CAnimCallbacks::turn_cb(AnimFileStateAnimList_t* anims, unsigned short state, AnimObjHdr_t* hdr,
                            AnimChannel_t* chans, void* userData, unsigned long event, void* arg,
                            float frames) {
    CAnimated* anim = (CAnimated*)userData;
    CAnimObject* owner = anim->m_pOwner;
    float angle = *(float*)&arg;
    float unused1 = 0.0f;
    float unused2 = 0.0f;
    TurnMem* mem = (TurnMem*)AnimStGetStateUserMemory(StateInfo(anim), state);
    switch (event) {
    case ANIMST_EVENT_START: {
        long turn;
        float duration;
        float scale;
        AnimStSetStateFlags(StateInfo(anim), state, ANIMST_UPDATE_CALLBACK);
        u16 i = _TurnFindTurnAnimInfo(anims, owner, state, &turn, &duration, &scale, &unused1,
                                      &unused2, angle);
        mem->channel = StartAnim(hdr, chans, anims, i, state, userData, 1.0f);
        AnimSeqScaleDuration(&anim->m_channels[mem->channel].seq, 1.0f / scale);
        mem->rate = (s32)((float)turn / duration);
        anim->m_animFlags &= ~0x10;
        if (angle == 0.0f) {
            mem->trackTarget = 1;
        } else {
            mem->trackTarget = 0;
        }
        if (owner->field_0x2458 != 0) {
            mem->moving = 1;
            owner->field_0x2458 = 0;
            mem->dest[0] = owner->m_goal[0];
            mem->dest[1] = owner->m_goal[1];
            mem->dest[2] = owner->m_goal[2];
            float dx = mem->dest[0] - owner->m_turnFrom[0];
            float dy = mem->dest[1] - owner->m_turnFrom[1];
            float dz = mem->dest[2] - owner->m_turnFrom[2];
            mem->moveTime = duration;
            mem->moveLeft = duration;
            float left = mem->moveLeft;
            float step = frames;
            if (frames > left) {
                step = left;
            }
            float frac = step / left;
            mem->moveLeft -= step;
            float t = mem->moveLeft / mem->moveTime;
            owner->m_turnLeft = t;
            float k = (1.0f - t) * frac;
            owner->m_turnMove[0] = k * dx;
            owner->m_turnMove[1] = k * dy;
            owner->m_turnMove[2] = k * dz;
        } else {
            mem->moving = 0;
        }
        break;
    }
    case ANIMST_EVENT_UPDATE:
        if (state != AnimStGetCurrentState(StateInfo(anim))) {
            break;
        }
        if (mem->trackTarget != 0) {
            CSoldierObject* soldier = owner->AsSoldierObject();
            if (soldier == NULL) {
                return 0;
            }
            CAIObject* ai = SoldierAI(soldier);
            bool hasTarget = false;
            if (ai != NULL && (ai->m_nonAITarget != 0 || ai->m_pTarget != NULL)) {
                hasTarget = true;
            }
            if (hasTarget) {
                float aim[3];
                AIAimPoint(ai, aim);
                owner->m_goal[0] = aim[0];
                owner->m_goal[1] = aim[1];
                owner->m_goal[2] = aim[2];
                float deg = GoalTurnDegrees(owner, frames);
                owner->m_yawDelta = 0.017453292f * deg + owner->m_yawDelta;
                anim->m_channels[mem->channel].motionHeading = owner->m_heading;
                owner->m_facingTarget = 1;
            }
        } else {
            owner->m_yawDelta = ((6.2831855f * (float)mem->rate) / 16777216.0f) * frames + owner->m_yawDelta;
            AnimChannel_t* chan = &anim->m_channels[mem->channel];
            chan->motionHeading = (chan->motionHeading + (s32)((float)mem->rate * frames)) & ANGLE_MASK;
            if (mem->moving != 0) {
                float d[3];
                d[0] = mem->dest[0] - owner->m_turnFrom[0];
                d[1] = mem->dest[1] - owner->m_turnFrom[1];
                d[2] = mem->dest[2] - owner->m_turnFrom[2];
                float left = mem->moveLeft;
                if (left > 0.0f) {
                    float step = frames;
                    if (frames > left) {
                        step = left;
                    }
                    float frac = step / left;
                    mem->moveLeft -= step;
                    float t = mem->moveLeft / mem->moveTime;
                    owner->m_turnLeft = t;
                    float k = (1.0f - t) * frac;
                    owner->m_turnMove[0] = k * d[0];
                    owner->m_turnMove[1] = k * d[1];
                    owner->m_turnMove[2] = k * d[2];
                } else {
                    owner->m_turnLeft = 0.0f;
                    owner->m_turnMove[0] = d[0];
                    owner->m_turnMove[1] = d[1];
                    owner->m_turnMove[2] = d[2];
                }
            }
        }
        if (anim->m_animFlags & 0x10) {
            mem->rate = 0;
            mem->moveLeft = 0.0f;
        }
        break;
    case ANIMST_EVENT_FINISHED:
        if (state == AnimStGetCurrentState(StateInfo(anim))) {
            mem->rate = 0;
        }
        break;
    }
    return 0;
}

#define DEATH_MAX_ANIMS 256

// Plays a death animation matching how the character was killed (the
// AnimDeathInfo_t passed as `arg`), picked by weight, and maybe chooses a
// follow-up variant.
int CAnimCallbacks::death_cb(AnimFileStateAnimList_t* anims, unsigned short state,
                             AnimObjHdr_t* hdr, AnimChannel_t* chans, void* userData,
                             unsigned long event, void* arg, float frames) {
    if (event != ANIMST_EVENT_START) {
        return 0;
    }
    AnimDeathInfo_t* info = (AnimDeathInfo_t*)arg;
    u32 weights[DEATH_MAX_ANIMS];
    u32 list[DEATH_MAX_ANIMS];
    u32 n = 0;
    u32 i;

    // By bullet type.
    switch (info->bullet) {
    case 2:
    case 6:
    case 18:
    case 19:
    case 20:
        for (i = 0; i < anims->count; i++) {
            if (((AnimPlugDeath_t*)anims->anims[i].plugin)->bullets & 0x40) {
                list[n++] = i;
            }
        }
        break;
    case 3:
    case 24:
        for (i = 0; i < anims->count; i++) {
            if (((AnimPlugDeath_t*)anims->anims[i].plugin)->bullets & 0x8) {
                list[n++] = i;
            }
        }
        break;
    default:
        for (i = 0; i < anims->count; i++) {
            u32 mask = ((AnimPlugDeath_t*)anims->anims[i].plugin)->bullets;
            if (mask == 0 || (mask & (1 << info->bullet))) {
                list[n++] = i;
            }
        }
        break;
    }

    // By posture.
    u32 m = 0;
    for (i = 0; i < n; i++) {
        u32 a = list[i];
        AnimPlugDeath_t* plug = (AnimPlugDeath_t*)anims->anims[a].plugin;
        if ((plug->postures[0] == 0 && plug->postures[1] == 0) ||
            (plug->postures[info->posture / 32] & (1 << (info->posture % 32)))) {
            m++;
            list[m - 1] = a;
        }
    }
    n = m;

    // By hit location.
    m = 0;
    for (i = 0; i < n; i++) {
        u32 a = list[i];
        u32 mask = ((AnimPlugDeath_t*)anims->anims[a].plugin)->locations;
        if (mask == 0 || (mask & (1 << info->location))) {
            list[m++] = a;
        }
    }
    n = m;

    // By intensity, collecting the weights.
    m = 0;
    u32 total = 0;
    for (i = 0; i < n; i++) {
        u32 a = list[i];
        AnimPlugDeath_t* plug = (AnimPlugDeath_t*)anims->anims[a].plugin;
        u32 mask = plug->intensities;
        if (mask == 0 || (mask & (1 << info->intensity))) {
            u32 w = plug->weight;
            list[m] = a;
            total += w;
            weights[m] = w;
            m++;
        }
    }
    n = m;

    if (n == 0 || total == 0) {
        DebugMsg("ANIM WARNING***\nBullet: %d, Posture: %d, Location: %d, Intensity: %d\nEND OF "
                 "WARNING***\n",
                 info->bullet, info->posture, info->location, info->intensity);
        n = 1;
        list[0] = 0;
    }
    int pick;
    if (n == 1) {
        pick = 0;
    } else {
        u32 roll = (u32)rand() % total;
        u32 sum = 0;
        pick = 0;
        while (roll >= sum + weights[pick]) {
            sum += weights[pick];
            pick++;
        }
    }

    AnimPlugDeath_t* chosen = (AnimPlugDeath_t*)anims->anims[list[pick]].plugin;
    info->variant = 0;
    if (chosen->variants != 0 && (u32)(rand() % 100) < chosen->variantChance) {
        int b = 0;
        while (!(chosen->variants & (1 << b))) {
            b++;
        }
        info->variant = b + 1;
    }
    StartAnim(hdr, chans, anims, list[pick], state, userData, 1.0f);
    return 0;
}

// Plays the animation for the weapon in `arg` (else the first).
int CAnimCallbacks::weapon_cb(AnimFileStateAnimList_t* anims, unsigned short state,
                              AnimObjHdr_t* hdr, AnimChannel_t* chans, void* userData,
                              unsigned long event, void* arg, float frames) {
    if (event == ANIMST_EVENT_START) {
        u32 i;
        for (i = 0; i < anims->count; i++) {
            if ((u32)arg == ((AnimPlugWeapon_t*)anims->anims[i].plugin)->weapon) {
                break;
            }
        }
        if (i >= anims->count) {
            i = 0;
        }
        StartAnim(hdr, chans, anims, i, state, userData, 1.0f);
    }
    return 0;
}

// Finds the two locomotion animations of [first, last) whose speeds bracket
// `speed` (clamping it into the range); returns the index of the faster one
// relative to `first`.
static inline int FlocoFindSpeeds(AnimPlugFloco_t* info, int first, int last, float& speed,
                                  float frames, float* speeds) {
    int n = last - first;
    speeds[0] = info->speeds[first].speed * frames;
    int i;
    for (i = 1; i < n; i++) {
        speeds[i] = info->speeds[first + i].speed * frames;
        if (speed < speeds[i]) {
            break;
        }
    }
    if (speed < speeds[i - 1]) {
        speed = speeds[i - 1];
    }
    if (i >= n) {
        i = n - 1;
        speed = speeds[i];
    }
    return i;
}

// Player locomotion: blends the two animations whose speeds bracket the
// player's speed for its direction (and crouch), switching pairs as the
// speed changes.
int CAnimCallbacks::floco_cb(AnimFileStateAnimList_t* anims, unsigned short state,
                             AnimObjHdr_t* hdr, AnimChannel_t* chans, void* userData,
                             unsigned long event, void* arg, float frames) {
    CAnimated* anim = (CAnimated*)userData;
    AnimPlugFloco_t* info = (AnimPlugFloco_t*)g_AnimDB.GetStateInfo(state, hdr->field_0x0);
    FlocoMem* mem = (FlocoMem*)AnimStGetStateUserMemory(StateInfo(anim), state);
    float speed;
    long dir;
    float speeds[16];
    switch (event) {
    case ANIMST_EVENT_START: {
        mem->player = (CAnimatedPlayer*)arg;
        mem->player->GetFlocoParams(speed, dir);
        mem->player->SetFlocoParams(0.0f, 0);
        CPlayerObject* player = mem->player->m_pPlayer;
        mem->crouching = player->IsPlayerCrouching();
        speed *= frames;
        if ((u8)player->IsPlayerJumping() && !mem->crouching) {
            int i = 0;
            player->m_jumping = 0;
            if (speed > 0.1f) {
                i = dir + 1;
            }
            StartAnim(hdr, chans, anims, _floco_anim_ranges[8] + i, state, userData, 1.0f);
            break;
        }
        int first;
        int last;
        if (mem->crouching) {
            first = _floco_anim_ranges[dir + 4];
            last = _floco_anim_ranges[dir + 5];
        } else {
            first = _floco_anim_ranges[dir];
            last = _floco_anim_ranges[dir + 1];
        }
        AnimStSetStateFlags(StateInfo(anim), state, ANIMST_UPDATE_CALLBACK);
        int i = FlocoFindSpeeds(info, first, last, speed, frames, speeds);
        int k = first + i;
        float hi = speeds[i];
        float lo = speeds[i - 1];
        mem->speedLo = info->speeds[k - 1].speed;
        mem->speedHi = info->speeds[k].speed;
        mem->animLo = k - 1;
        mem->animHi = k;
        mem->first = first;
        mem->last = last;
        mem->dir = dir;
        float wLo;
        float wHi;
        if (hi <= lo) {
            wLo = 0.0f;
            wHi = 1.0f;
        } else {
            wHi = (speed - lo) / (hi - lo);
            wLo = 1.0f - wHi;
        }
        mem->channelLo = StartAnim(hdr, chans, anims, k - 1, state, userData, wLo);
        mem->channelHi = StartAnim(hdr, chans, anims, k, state, userData, wHi);
        mem->rateLo = 1.0f + wLo;
        mem->rateHi = 0.5f * wHi + 0.5f;
        anim->SetAnimSpeed(mem->channelLo, mem->rateLo);
        anim->SetAnimSpeed(mem->channelHi, mem->rateHi);
        break;
    }
    case ANIMST_EVENT_UPDATE: {
        mem->player->GetFlocoParams(speed, dir);
        mem->player->SetFlocoParams(0.0f, 0);
        speed *= frames;
        float lo = mem->speedLo * frames;
        float hi = mem->speedHi * frames;
        CPlayerObject* player = mem->player->m_pPlayer;
        if (mem->dir != dir || mem->crouching != (u8)player->IsPlayerCrouching()) {
            // New direction or posture: a new pair.
            mem->crouching = player->IsPlayerCrouching();
            int first;
            int last;
            if (mem->crouching) {
                first = _floco_anim_ranges[dir + 4];
                last = _floco_anim_ranges[dir + 5];
            } else {
                first = _floco_anim_ranges[dir];
                last = _floco_anim_ranges[dir + 1];
            }
            int i = FlocoFindSpeeds(info, first, last, speed, frames, speeds);
            int k = first + i;
            mem->speedLo = info->speeds[k - 1].speed;
            mem->speedHi = info->speeds[k].speed;
            mem->animLo = k - 1;
            mem->animHi = k;
            mem->first = first;
            mem->last = last;
            mem->dir = dir;
            lo = mem->speedLo * frames;
            hi = mem->speedHi * frames;
            SwitchAnim(hdr, chans, mem->channelLo, anims, mem->animLo, state, userData);
            SwitchAnim(hdr, chans, mem->channelHi, anims, mem->animHi, state, userData);
        } else if (speed < lo) {
            // Slower than the pair: step down.
            if (mem->animLo == mem->first) {
                speed = lo;
            } else {
                mem->animLo--;
                mem->animHi--;
                mem->speedLo = info->speeds[mem->animLo].speed;
                mem->speedHi = info->speeds[mem->animHi].speed;
                lo = mem->speedLo * frames;
                hi = mem->speedHi * frames;
                if (speed < lo) {
                    speed = lo;
                }
                SwitchAnim(hdr, chans, mem->channelLo, anims, mem->animLo, state, userData);
                SwitchAnim(hdr, chans, mem->channelHi, anims, mem->animHi, state, userData);
            }
        } else if (speed > hi) {
            // Faster: step up.
            if (mem->animHi == mem->last - 1) {
                speed = hi;
            } else {
                mem->animLo++;
                mem->animHi++;
                mem->speedLo = info->speeds[mem->animLo].speed;
                mem->speedHi = info->speeds[mem->animHi].speed;
                hi = mem->speedHi * frames;
                lo = mem->speedLo * frames;
                if (speed > hi) {
                    speed = hi;
                }
                SwitchAnim(hdr, chans, mem->channelLo, anims, mem->animLo, state, userData);
                SwitchAnim(hdr, chans, mem->channelHi, anims, mem->animHi, state, userData);
            }
        }
        float wLo;
        float wHi;
        if (hi <= lo) {
            wLo = 0.0f;
            wHi = 1.0f;
        } else {
            wHi = (speed - lo) / (hi - lo);
            wLo = 1.0f - wHi;
        }
        AnimWgtSet(&chans[mem->channelLo].weight, wLo, wLo, 0, NULL);
        AnimWgtSet(&chans[mem->channelHi].weight, wHi, wHi, 0, NULL);
        mem->rateLo = 1.0f + wLo;
        mem->rateHi = 0.5f * wHi + 0.5f;
        anim->SetAnimSpeed(mem->channelLo, mem->rateLo);
        anim->SetAnimSpeed(mem->channelHi, mem->rateHi);
        break;
    }
    }
    return 0;
}

// Index of the first animation aimed at (yaw, pitch), else 0.
static inline u16 FindAimAnim(AnimFileStateAnimList_t* anims, s32 pitch, s32 yaw) {
    for (u16 i = 0; i < anims->count; i++) {
        AnimPlugAim_t* plug = (AnimPlugAim_t*)anims->anims[i].plugin;
        if (plug->pitch == pitch && plug->yaw == yaw) {
            return i;
        }
    }
    return 0;
}

// Index of the first level (pitch 0) aim animation, else 0.
static inline u16 FindAimCenter(AnimFileStateAnimList_t* anims) {
    for (u16 i = 0; i < anims->count; i++) {
        if (((AnimPlugAim_t*)anims->anims[i].plugin)->pitch == 0) {
            return i;
        }
    }
    return 0;
}

// The aim blend weights for a target from the owner's view (its matrix
// rows: 0 side, 1 forward, 2 up), the view tilted by the center
// animation's yaw.
static inline void AimWeights(CAnimObject* owner, AnimFileStateAnimList_t* anims, u16 center,
                              const float* target, float tanHalfFov, float* weights) {
    CMatrix m;
    m = owner->m_tm;
    float d[3];
    d[0] = target[0] - m.m[3][0];
    d[1] = target[1] - m.m[3][1];
    d[2] = target[2] - m.m[3][2];
    float side[3];
    float fwd[3];
    float up[3];
    fwd[0] = m.m[1][0];
    fwd[1] = m.m[1][1];
    fwd[2] = m.m[1][2];
    side[0] = m.m[0][0];
    side[1] = m.m[0][1];
    side[2] = m.m[0][2];
    up[0] = m.m[2][0];
    up[1] = m.m[2][1];
    up[2] = m.m[2][2];
    s32 tilt = ((AnimPlugAim_t*)anims->anims[center].plugin)->yaw;
    if (tilt != 0) {
        float a = 0.017453 * tilt;
        float c = cos(a);
        float s = sin(a);
        float t = tan(a);
        float s0 = s * up[0];
        float s1 = s * up[1];
        float s2 = s * up[2];
        fwd[0] = c * fwd[0] + s0;
        fwd[1] = c * fwd[1] + s1;
        fwd[2] = c * fwd[2] + s2;
        up[0] = up[0] * c;
        up[1] = up[1] * c;
        up[2] = up[2] * c;
        up[0] = up[0] - (fwd[0] - s0) * t;
        up[1] = up[1] - (fwd[1] - s1) * t;
        up[2] = up[2] - (fwd[2] - s2) * t;
    }
    float dist = fwd[0] * d[0] + fwd[1] * d[1] + fwd[2] * d[2];
    if (dist < 1.0f) {
        dist = 1.0f;
    }
    float inv = 1.0f / (tanHalfFov * dist);
    float x = side[0] * d[0] + side[1] * d[1] + side[2] * d[2];
    float y = up[0] * d[0] + up[1] * d[1] + up[2] * d[2];
    _find_aim_blend_values(inv * (x - 0.2f), inv * (y - 1.15f), weights);
}

// Aiming at the AI's target: blends the four corner and the center aim
// animations.
int CAnimCallbacks::aim_cb(AnimFileStateAnimList_t* anims, unsigned short state, AnimObjHdr_t* hdr,
                           AnimChannel_t* chans, void* userData, unsigned long event, void* arg,
                           float frames) {
    CAnimated* anim = (CAnimated*)userData;
    CAnimObject* owner = anim->m_pOwner;
    CSoldierObject* soldier = owner->AsSoldierObject();
    if (soldier == NULL) {
        return 0;
    }
    CAIFilterObject* filter = soldier->GetAIDoodad()->m_pFilter;
    filter->m_pAIObject->m_flags |= 0x10;
    float target[3];
    AIAimPoint(filter->m_pAIObject, target);
    float tanHalfFov = tan(0.5235987901687622);
    AimMem* mem = (AimMem*)AnimStGetStateUserMemory(StateInfo(anim), state);
    float weights[AIM_NUM_ANIMS];
    switch (event) {
    case ANIMST_EVENT_START: {
        AnimStSetStateFlags(StateInfo(anim), state, ANIMST_UPDATE_CALLBACK);
        mem->target[0] = target[0];
        mem->target[1] = target[1];
        mem->target[2] = target[2];
        u16 index[AIM_NUM_ANIMS];
        index[0] = FindAimAnim(anims, -45, 45);
        index[1] = FindAimAnim(anims, 45, 45);
        index[2] = FindAimAnim(anims, 45, -45);
        index[3] = FindAimAnim(anims, -45, -45);
        index[4] = FindAimCenter(anims);
        AimWeights(owner, anims, index[4], target, tanHalfFov, weights);
        for (int i = 0; i < AIM_NUM_ANIMS; i++) {
            mem->anims[i].channel = StartAnim(hdr, chans, anims, index[i], state, userData, weights[i]);
            mem->anims[i].weight = weights[i];
        }
        anim->m_animFlags &= ~0x10;
        break;
    }
    case ANIMST_EVENT_UPDATE:
        // The stored target is never updated: once the target moves, the
        // weights are recomputed every frame.
        if (state == AnimStGetCurrentState(StateInfo(anim)) &&
            (mem->target[0] != target[0] || mem->target[1] != target[1] ||
             mem->target[2] != target[2])) {
            AimWeights(owner, anims, FindAimCenter(anims), target, tanHalfFov, weights);
            for (int i = 0; i < AIM_NUM_ANIMS; i++) {
                AnimWgtSet(&chans[mem->anims[i].channel].weight, weights[i], weights[i], 0, NULL);
                mem->anims[i].weight = weights[i];
            }
        }
        break;
    case ANIMST_EVENT_FINISHED:
        filter->m_pAIObject->m_flags &= ~0x10;
        break;
    }
    return 0;
}

int CAnimCallbacks::animfinish_cb(unsigned short state, void* userData) {
    ((CAnimated*)userData)->FinishAnimationState(state);
    return 0;
}

// Picks the turn animation whose range holds the turn (`angle` in radians,
// or 0 to turn towards the owner's goal). Out: the turn left over after the
// animation's own (24-bit), its duration and scale, and two more values.
u16 CAnimCallbacks::_TurnFindTurnAnimInfo(AnimFileStateAnimList_t* anims, CAnimObject* owner,
                                          unsigned short state, long* rest, float* duration,
                                          float* scale, float* out1, float* out2, float angle) {
    *rest = 0;
    *duration = 1.0f;
    *scale = 1.0f;
    AnimPlugTurn_t* found = NULL;
    u16 index = 0;
    s32 delta;
    if (0.0 == angle) {
        delta = ((GoalAngle(owner) & ANGLE_MASK) - (owner->m_heading & ANGLE_MASK)) & ANGLE_MASK;
    } else {
        delta = (s32)(2670176.8f * angle);
    }
    s32 d;
    s32 lo;
    for (u16 i = 0; i < anims->count; i++) {
        AnimPlugTurn_t* plug = (AnimPlugTurn_t*)anims->anims[i].plugin;
        d = delta;
        lo = plug->minAngle;
        s32 hi = plug->maxAngle;
        if (lo > hi) {
            hi += ANGLE_FULL;
        }
        if (delta < lo) {
            d += ANGLE_FULL;
        }
        if (d >= lo && d <= hi) {
            found = plug;
            index = i;
            break;
        }
    }
    if (found == NULL) {
        DebugMsg("Can't find turn Animation for %f!!\n", 360.0f * (float)delta / 16777216.0f);
        for (int i = 0; i < anims->count; i++) {
            AnimPlugTurn_t* plug = (AnimPlugTurn_t*)anims->anims[i].plugin;
            DebugMsg("Anim %d (%x) min %f max %f\n", i, plug,
                     360.0f * (float)plug->minAngle / 16777216.0f,
                     360.0f * (float)plug->maxAngle / 16777216.0f);
        }
    }
    if (found != NULL && index < anims->count) {
        s32 start = found->angle;
        if (start < lo) {
            start += ANGLE_FULL;
        }
        *rest = d - start;
        if (*rest > 0x800000) {
            *rest -= ANGLE_FULL;
        } else if (*rest < -0x800000) {
            *rest += ANGLE_FULL;
        }
        *out1 = -found->field_0x1C;
        *out2 = found->field_0x20;
        *duration = found->duration;
        *duration = *duration / *scale;
    } else {
        index = 0;
    }
    return index;
}

void _find_aim_blend_values(float x, float y, float* w) {
    w[4] = 0.0f;
    w[3] = 0.0f;
    w[2] = 0.0f;
    w[1] = 0.0f;
    w[0] = 0.0f;
    if (x == 0.0f && y == 0.0f) {
        w[4] = 1.0f;
        return;
    }
    if (x > y) {
        float ny = -y;
        if (x > ny) {
            if (x > 1.0f) {
                w[1] = 0.5f * (1.0f + y / x);
                w[2] = 1.0f - w[1];
                return;
            }
            float a = 1.0f - x;
            float b = 1.0f - y;
            float c = -1.0f - y;
            float nx = -x;
            float e = a * ny;
            w[4] = -0.5f * (a * c - a * b);
            w[1] = -0.5f * -(nx * c - e);
            w[2] = -0.5f * (nx * b - e);
            return;
        }
        if (y < -1.0f) {
            w[2] = 0.5f * (1.0f + x / ny);
            w[3] = 1.0f - w[2];
            return;
        }
        float a = -1.0f - y;
        float b = -1.0f - x;
        float c = 1.0f - x;
        float e = -x * a;
        w[4] = -0.5f * (c * a - b * a);
        w[2] = -0.5f * (b * ny - e);
        w[3] = -0.5f * -(c * ny - e);
        return;
    }
    float nx = -x;
    if (nx < y) {
        if (y > 1.0f) {
            w[1] = 0.5f * (1.0f + x / y);
            w[0] = 1.0f - w[1];
            return;
        }
        float a = 1.0f - y;
        float b = 1.0f - x;
        float c = -1.0f - x;
        float ny = -y;
        float e = nx * a;
        w[4] = -0.5f * (c * a - b * a);
        w[0] = -0.5f * (b * ny - e);
        w[1] = -0.5f * -(c * ny - e);
        return;
    }
    if (x < -1.0f) {
        w[0] = 0.5f * (1.0f + -y / x);
        w[3] = 1.0f - w[0];
        return;
    }
    float a = -1.0f - x;
    float b = -1.0f - y;
    float c = 1.0f - y;
    float e = a * -y;
    w[4] = -0.5f * (a * c - a * b);
    w[3] = -0.5f * -(nx * c - e);
    w[0] = -0.5f * (nx * b - e);
}

static inline void Swap16(u16& v) {
    u8* p = (u8*)&v;
    u8 t = p[0];
    p[0] = p[1];
    p[1] = t;
}

static inline void Swap32(u32& v) {
    u32 x = v;
    u8* p = (u8*)&x;
    u8 b0 = p[0];
    u8 b3 = p[3];
    u8 b1 = p[1];
    u8 b2 = p[2];
    p[0] = b3;
    p[3] = b0;
    p[1] = b2;
    p[2] = b1;
    v = x;
}

void EndianSwapAnimPlugIn(AnimFilePluginData_t& data, int callback) {
    if (data.version < 0x100) {
        return;
    }
    Swap16(data.version);
    Swap16(data.size);
    if (data.size == 0) {
        return;
    }
    AnimStCallback fnc = _AnimObj_StateSelFuncArray[callback].callback;
    u32* w = (u32*)&data;
    if (fnc == (AnimStCallback)CAnimCallbacks::pathing_cb) {
        return;
    }
    if (fnc == (AnimStCallback)CAnimCallbacks::random_cb) {
        Swap32(w[1]);
        Swap32(w[2]);
    } else if (fnc == (AnimStCallback)CAnimCallbacks::turn_cb) {
        for (int i = 3; i <= 8; i++) {
            Swap32(w[i]);
        }
    } else if (fnc == (AnimStCallback)CAnimCallbacks::death_cb) {
        for (int i = 1; i <= 9; i++) {
            Swap32(w[i]);
        }
    } else if (fnc == (AnimStCallback)CAnimCallbacks::weapon_cb) {
        Swap32(w[1]);
    } else if (fnc == (AnimStCallback)CAnimCallbacks::aim_cb) {
        Swap32(w[2]);
        Swap32(w[3]);
    } else if (fnc == (AnimStCallback)CAnimCallbacks::face_cb) {
        return;
    } else if (fnc == (AnimStCallback)CAnimCallbacks::floco_cb) {
        AnimPlugFloco_t* floco = (AnimPlugFloco_t*)&data;
        Swap32(w[1]);
        Swap32(w[2]);
        Swap32(w[3]);
        Swap32(w[4]);
        for (u32 i = 0; i < floco->count; i++) {
            Swap32(*(u32*)&floco->speeds[i].field_0x0);
            Swap32(*(u32*)&floco->speeds[i].speed);
            Swap32(*(u32*)&floco->speeds[i].field_0x8);
        }
    }
}
