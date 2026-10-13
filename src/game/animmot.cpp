// Animation motion: collects the key frames contributing to the pose and
// blends the root motion they carry.
#include "game/animmot.h"
#include "game/animchan.h"
#include "game/animfile.h"
#include "game/animst.h"

static AnimMotFrameList_t _AnimMot_FrameList;

// Two key frames per running channel (the frames around its position), each
// weighted by the channel and state weights. The blend values are then made
// cumulative, so blending in list order with blend[i] gives the weighted mean.
AnimMotFrameList_t* AnimMotCreateWeightList(AnimObjHdr_t* hdr, AnimChannel_t* chans,
                                            AnimStInfo_t* stateInfo) {
    AnimMotFrameList_t* list = &_AnimMot_FrameList;
    u16 count = 0;
    list->max = ANIMMOT_MAX_FRAMES;

    AnimChannel_t* chan = chans;
    for (int i = 0; i < hdr->numChannels; chan++, i++) {
        if (count >= list->max) {
            break;
        }
        if (chan->status == ANIMCHAN_FREE || chan->delay != 0.0f) {
            continue;
        }

        float weight = AnimStGetStateWeight(stateInfo, chan->state) * chan->weight.weight;
        AnimMotWeightFrame_t* f = &list->frames[count];
        f->weight = weight;
        f->blend = weight * chan->seq.frameFrac;
        if (f->blend != 0.0f) {
            f->frame = (u8*)AnimFileGetFrame(chan->file, chan->seq.frame1);
            count++;
            f->file = chan->file;
            f->frameIndex = chan->seq.frame1;
            f->frameInfo = chan->seq.anim->frameInfo;
            f->flags = chan->animFlags;
            f->channel = i;
        }

        if (count < list->max) {
            f = &list->frames[count];
            f->weight = weight;
            f->blend = weight * (1.0f - chan->seq.frameFrac);
            if (f->blend != 0.0f) {
                f->frame = (u8*)AnimFileGetFrame(chan->file, chan->seq.frame0);
                count++;
                f->file = chan->file;
                f->frameIndex = chan->seq.frame0;
                f->frameInfo = chan->seq.anim->frameInfo;
                f->flags = chan->animFlags;
                f->channel = i;
            }
        }
    }

    list->count = count;
    float total = list->frames[0].blend;
    list->frames[0].blend = 1.0f;
    for (int i = 1; i < count; i++) {
        total += list->frames[i].blend;
        list->frames[i].blend = list->frames[i].blend / total;
    }
    return list;
}

// Root rotation since the last update, scaled by the channel's rotation scale.
bool AnimMotCalcAnimRot(AnimObjHdr_t* hdr, AnimChannel_t* chans, AnimMotFrameList_t* list,
                        long* out, u16 state, u8 all) {
    float total = 0.0f;
    long rot = 0;
    bool found = false;

    for (int i = 0; i < list->count; i++) {
        AnimMotWeightFrame_t* f = &list->frames[i];
        if (!(f->flags & ANIMMOT_ROTATION)) {
            continue;
        }
        u8 ch = f->channel;
        u8 use = all;
        AnimChannel_t* chan = &chans[ch];
        if (state == chan->state || chan->state == 0xFFFF || state == 0xFFFF) {
            found = true;
            use = 1;
        }
        if (use != 1) {
            continue;
        }

        if (chan->seq.motionFlags & ANIMMOT_ROT_RESET) {
            chan->motionHeading = hdr->field_0x8;
            chan->motionRot = 0;
            chan->seq.motionFlags &= ~ANIMMOT_ROT_RESET;
            chan->seq.motionFlags |= ANIMMOT_ROT_WRAPPED;
        }
        if (chan->seq.motionFlags & ANIMMOT_ROT_CLEAR) {
            chan->seq.motionFlags &= ~(ANIMMOT_ROT_RESET | ANIMMOT_ROT_WRAPPED | ANIMMOT_ROT_CLEAR);
        }

        // Lerp between the channel's two frames.
        long target;
        if (i + 1 < list->count && list->frames[i + 1].channel == ch) {
            if (!(chan->seq.motionFlags & ANIMMOT_ROT_WRAPPED)) {
                OrientLerpAngle(&target, f->rot, list->frames[i + 1].rot, chan->seq.frameFrac);
            } else {
                OrientLerpAngle(&target, f->rot, 0, chan->seq.frameFrac);
            }
            i++;
        } else {
            target = f->rot;
        }

        long delta = target - chan->motionRot;
        chan->motionRot = chan->motionRot + delta;
        total += f->weight;
        OrientLerpAngle(&rot, (long)(((s64)delta * chan->motionRotScale) >> 24), rot,
                        f->weight / total);
    }

    if (found) {
        *out = rot;
    }
    return found;
}

// Root translation since the last update, scaled and rotated into the
// channel's heading.
bool AnimMotCalcAnimPos(AnimObjHdr_t* hdr, AnimChannel_t* chans, AnimMotFrameList_t* list,
                        CVector3* out, u16 state, u8 all) {
    float total = 0.0f;
    CVector3 pos(0.0f, 0.0f, 0.0f);
    bool found = false;

    for (int i = 0; i < list->count; i++) {
        AnimMotWeightFrame_t* f = &list->frames[i];
        if (!(f->flags & ANIMMOT_POS_XY) && !(f->flags & ANIMMOT_POS_Z)) {
            continue;
        }
        u8 ch = f->channel;
        u8 use = all;
        AnimChannel_t* chan = &chans[ch];
        if (state == chan->state || chan->state == 0xFFFF || state == 0xFFFF) {
            found = true;
            use = 1;
        }
        if (use != 1) {
            continue;
        }

        if (chan->seq.motionFlags & ANIMMOT_POS_RESET) {
            chan->motionPos[2] = 0.0f;
            chan->motionPos[1] = 0.0f;
            chan->motionPos[0] = 0.0f;
            chan->seq.motionFlags &= ~ANIMMOT_POS_RESET;
            chan->seq.motionFlags |= ANIMMOT_POS_WRAPPED;
        }
        if (chan->seq.motionFlags & ANIMMOT_POS_CLEAR) {
            chan->seq.motionFlags &= ~(ANIMMOT_POS_RESET | ANIMMOT_POS_WRAPPED | ANIMMOT_POS_CLEAR);
        }

        CVector3 target;
        if (i + 1 < list->count && list->frames[i + 1].channel == ch) {
            AnimMotWeightFrame_t* next = &list->frames[i + 1];
            if (!(chan->seq.motionFlags & ANIMMOT_POS_WRAPPED)) {
                float t = chan->seq.frameFrac;
                target.x = t * (f->pos.x - next->pos.x) + next->pos.x;
                target.y = t * (f->pos.y - next->pos.y) + next->pos.y;
                target.z = t * (f->pos.z - next->pos.z) + next->pos.z;
            } else {
                float t = chan->seq.frameFrac;
                target.x = t * f->pos.x;
                target.y = t * f->pos.y;
                target.z = t * f->pos.z;
            }
            i++;
        } else {
            target = f->pos;
        }
        if (!(f->flags & ANIMMOT_POS_XY)) {
            target.x = 0.0f;
            target.y = 0.0f;
        }
        if (!(f->flags & ANIMMOT_POS_Z)) {
            target.z = 0.0f;
        }

        CVector3 delta = target;
        delta.x = delta.x - chan->motionPos[0];
        chan->motionPos[0] = chan->motionPos[0] + delta.x;
        delta.y = delta.y - chan->motionPos[1];
        chan->motionPos[1] = chan->motionPos[1] + delta.y;
        delta.z = delta.z - chan->motionPos[2];
        chan->motionPos[2] = chan->motionPos[2] + delta.z;

        float scale = chan->motionPosScale;
        CVector2 xy;
        xy.x = delta.x = delta.x * scale;
        xy.y = delta.y = delta.y * scale;
        delta.z = delta.z * scale;
        xy.Rotate(xy, chan->motionHeading);

        total += f->weight;
        float t = f->weight / total;
        delta.x = xy.x;
        delta.y = xy.y;
        pos.x = t * (delta.x - pos.x) + pos.x;
        pos.y = t * (delta.y - pos.y) + pos.y;
        pos.z = t * (delta.z - pos.z) + pos.z;
    }

    if (found) {
        *out = pos;
    }
    return found;
}

// The floating bone's position and orientation (absolute, not a delta).
bool AnimMotCalcFloatingBone(AnimObjHdr_t* hdr, AnimChannel_t* chans, AnimMotFrameList_t* list,
                             CVector3* outPos, Orient3_t* outRot, u16 state, u8 all) {
    float total = 0.0f;
    CVector3 pos(0.0f, 0.0f, 0.0f);
    Orient3_t rot;
    bool found = false;

    for (int i = 0; i < list->count; i++) {
        AnimMotWeightFrame_t* f = &list->frames[i];
        if (!(f->flags & ANIMMOT_FLOATING_BONE)) {
            continue;
        }
        u8 ch = f->channel;
        u8 use = all;
        AnimChannel_t* chan = &chans[ch];
        if (state == chan->state || chan->state == 0xFFFF || state == 0xFFFF) {
            found = true;
            use = 1;
        }
        if (use != 1) {
            continue;
        }

        CVector3 framePos;
        Orient3_t frameRot;
        if (i + 1 < list->count && list->frames[i + 1].channel == ch) {
            AnimMotWeightFrame_t* next = &list->frames[i + 1];
            float t = chan->seq.frameFrac;
            framePos.x = t * (f->floatPos.x - next->floatPos.x) + next->floatPos.x;
            framePos.y = t * (f->floatPos.y - next->floatPos.y) + next->floatPos.y;
            framePos.z = t * (f->floatPos.z - next->floatPos.z) + next->floatPos.z;
            Orient3Lerp(&frameRot, &f->floatRot, &next->floatRot, t);
            i++;
        } else {
            framePos = f->floatPos;
            frameRot = f->floatRot;
        }

        total += f->weight;
        float t = f->weight / total;
        pos.x = t * (framePos.x - pos.x) + pos.x;
        pos.y = t * (framePos.y - pos.y) + pos.y;
        pos.z = t * (framePos.z - pos.z) + pos.z;
        Orient3Lerp(&rot, &frameRot, &rot, t);
    }

    if (found) {
        *outPos = pos;
        *outRot = rot;
    }
    return found;
}
