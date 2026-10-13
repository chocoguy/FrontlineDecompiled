#include "game/orient.h"

// Functions are written in reverse address order: the game was compiled with
// deferred inlining, which emits them last to first.

#define ANGLE_MASK 0xFFFFFF
#define FULL_TURN 0x1000000
#define HALF_TURN 0x800000

extern "C" {

void Orient3Lerp(Orient3_t* out, const Orient3_t* a, const Orient3_t* b, float t) {
    long ft = (long)(16777216.0f * t);
    long* o = &out->x;
    const long* pa = &a->x;
    const long* pb = &b->x;
    for (int i = 0; i < 3; i++) {
        long from = *pb++ & ANGLE_MASK;
        long to = *pa++ & ANGLE_MASK;
        long d = to - from;
        if (d > HALF_TURN) {
            from += FULL_TURN;
        } else if (d < -HALF_TURN) {
            to += FULL_TURN;
        }
        *o++ = (long)(((s64)ft * (s64)(to - from)) >> 24) + from;
    }
}

void OrientLerpAngle(long* out, long a, long b, float t) {
    long from = b & ANGLE_MASK;
    long to = a & ANGLE_MASK;
    long d = to - from;
    if (d > HALF_TURN) {
        from += FULL_TURN;
    } else if (d < -HALF_TURN) {
        to += FULL_TURN;
    }
    d = to - from;
    *out = (long)(((s64)d * (s64)(long)(16777216.0f * t)) >> 24) + from;
}

}
