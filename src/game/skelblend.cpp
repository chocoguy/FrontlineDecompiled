// Pose blending: key frames are stored little-endian as s16 values, with
// joint angles as deltas from the animation's base pose.
#include "game/skelblend.h"
#include "game/animfile.h"
#include "game/animmot.h"
#include "game/endian.h"
#include "game/orient.h"
#include "game/skeleton.h"

#define MAX_JOINTS 0x50

static inline s16 GetS16(const u8* p) {
    s16 v = *(const s16*)p;
    EndianSwap(v);
    return v;
}

// Packed key frame layout (s16 unless noted):
//   0x00 u8 numJoints   0x02 yaw         0x04 x, y, z     0x0A z offset
//   0x0E floating bone angles x, y, z    0x14 floating bone position x, y, z
//   0x1A deltas for the joints in frameInfo->groups (3 per joint)
// Root motion the channel extracts (rotation, translation, floating bone)
// goes into `f`; the rest stays in the pose.
static void _unpack_skel_frame(CharFrame_t* out, AnimMotWeightFrame_t* f) {
    u8* data = f->frame;
    u8 flags = f->flags;
    AnimFrameInfo_t* info = f->frameInfo;
    u32 numJoints = data[0];
    u32 rotation = flags & ANIMMOT_ROTATION;
    u32 posXY = flags & ANIMMOT_POS_XY;
    u32 posZ = flags & ANIMMOT_POS_Z;
    u32 floating = flags & ANIMMOT_FLOATING_BONE;
    u32 mirror = flags & ANIMMOT_MIRROR;
    out->numJoints = numJoints;

    s16 yaw = GetS16(data + 0x2);
    s16 x = GetS16(data + 0x4);
    s16 y = GetS16(data + 0x6);
    s16 z = GetS16(data + 0x8);
    s16 zOffset = GetS16(data + 0xA);
    s16 floatRotX = GetS16(data + 0xE);
    s16 floatRotY = GetS16(data + 0x10);
    s16 floatRotZ = GetS16(data + 0x12);
    s16 floatX = GetS16(data + 0x14);
    s16 floatY = GetS16(data + 0x16);
    s16 floatZ = GetS16(data + 0x18);
    if (numJoints > MAX_JOINTS) {
        numJoints = MAX_JOINTS;
    }
    u8* deltas = data + 0x1A;

    if (posXY) {
        out->pos[0] = 0.0f;
        out->pos[1] = 0.0f;
        out->pos[2] = 0.0f;
        f->pos.x = (x / 4096.0f) * (mirror ? -1 : 1);
        f->pos.y = y / 4096.0f;
    } else {
        out->pos[0] = (x / 4096.0f) * (mirror ? -1 : 1);
        out->pos[1] = y / 4096.0f;
        out->pos[2] = 0.0f;
    }
    if (posZ) {
        f->pos.z = z / 4096.0f;
        out->pos[2] = zOffset / 4096.0f;
    } else {
        out->pos[2] = (z + zOffset) / 4096.0f;
    }

    if (floating) {
        f->floatRot.x = floatRotX << 8;
        f->floatRot.y = floatRotY << 8;
        f->floatRot.z = floatRotZ << 8;
        f->floatPos.x = floatX / 4096.0f;
        f->floatPos.y = floatY / 4096.0f;
        f->floatPos.z = floatZ / 4096.0f;
    } else {
        f->floatRot.z = 0;
        f->floatRot.y = 0;
        f->floatRot.x = 0;
        f->floatPos.x = 0.0f;
        f->floatPos.y = 0.0f;
        f->floatPos.z = 0.0f;
    }

    // Base pose (the root joint stays zero), then the frame's deltas.
    s16* joints = out->jointData;
    joints[0] = 0;
    joints[1] = 0;
    joints[2] = 0;
    for (u32 j = 1; j < numJoints; j++) {
        u8* base = info->basePose + 2 + j * 6;
        joints[j * 3 + 0] = GetS16(base);
        joints[j * 3 + 1] = GetS16(base + 2);
        joints[j * 3 + 2] = GetS16(base + 4);
    }
    for (u32 g = 0; g < info->groups->count; g++) {
        AnimJointGroup_t* group = &info->groups->groups[g];
        for (u32 j = group->start; j < (u32)(group->start + group->count); j++) {
            out->jointData[j * 3 + 0] += GetS16(deltas);
            out->jointData[j * 3 + 1] += GetS16(deltas + 2);
            out->jointData[j * 3 + 2] += GetS16(deltas + 4);
            deltas += 6;
        }
    }

    if (rotation) {
        f->rot = yaw << 8;
        out->rot = 0;
    } else {
        out->rot = -yaw << 8;
    }

    out->floatPos.x = floatX / 4096.0f;
    out->floatPos.y = floatY / 4096.0f;
    out->floatPos.z = floatZ / 4096.0f;
    out->floatPos.x *= (mirror ? -1 : 1);
    out->floatRot.x = floatRotX << 8;
    out->floatRot.y = (floatRotY * (mirror ? -1 : 1)) << 8;
    out->floatRot.z = (floatRotZ * (mirror ? -1 : 1)) << 8;
}

// frames->frames[i].blend is cumulative (see AnimMotCreateWeightList).
void BlendSkelFrames(CharFrame_t* dst, AnimMotFrameList_t* frames) {
    CharFrame_t frame;
    s16 joints[MAX_JOINTS * 3];

    u16 count = frames->count;
    if (count == 0) {
        return;
    }
    frame.jointData = joints;
    _unpack_skel_frame(dst, &frames->frames[0]);

    for (u32 i = 1; i < count; i++) {
        float t = frames->frames[i].blend;
        _unpack_skel_frame(&frame, &frames->frames[i]);
        dst->pos[0] = t * (frame.pos[0] - dst->pos[0]) + dst->pos[0];
        int ti = 4095.0f * t;
        dst->pos[1] = t * (frame.pos[1] - dst->pos[1]) + dst->pos[1];
        dst->pos[2] = t * (frame.pos[2] - dst->pos[2]) + dst->pos[2];

        s16* d = dst->jointData;
        s16* s = frame.jointData;
        for (u32 j = 0; j < dst->numJoints; j++) {
            d[0] = d[0] + ((ti * (s16)(s[0] - d[0])) >> 12);
            d[1] = d[1] + ((ti * (s16)(s[1] - d[1])) >> 12);
            d[2] = d[2] + ((ti * (s16)(s[2] - d[2])) >> 12);
            d += 3;
            s += 3;
        }
        OrientLerpAngle(&dst->rot, frame.rot, dst->rot, t);
    }
}
