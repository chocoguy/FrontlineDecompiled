#include "game/vector.h"

#include <math.h>
#include "game/trig.h"

// Functions are written in reverse address order: the game was compiled with
// deferred inlining, which emits them last to first.

void CVector2::Rotate(const CVector2& v, long angle) {
    float vx = v.x;
    float vy = v.y;
    float s;
    float c;
    MathSinCos(angle, &s, &c);
    x = c * vx - s * vy;
    y = c * vy + s * vx;
}

void CVector3::Constrain(const CVector3& axis, float cosLimit) {
    float cosAngle = x * axis.x + y * axis.y + z * axis.z;
    if (cosAngle < cosLimit) {
        float limit = acos(cosLimit);
        float angle = acos(cosAngle);
        float sinAngle = sin(angle);
        float axisScale = (float)sin(angle - limit) / sinAngle;
        float scale = (float)sin(limit) / sinAngle;
        x *= scale;
        y *= scale;
        z *= scale;
        CVector3 t;
        t.Scale(axis, axisScale);
        Add(t);
    }
}
