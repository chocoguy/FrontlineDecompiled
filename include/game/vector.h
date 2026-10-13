// CVector3 / CVector2: the engine's small vector types.
// Hand-maintained (layout and inline methods checked against the retail code).
#ifndef _GAME_VECTOR_H_
#define _GAME_VECTOR_H_

#include "types.h"
#include "math.h"

// 16 bytes: x, y, z and a fourth word (written as 1.0 by some transforms).
// Copies go through two 8-byte FP moves, as in the original.
class CVector3 {
public:
    /* 0x0 */ f32 x;
    /* 0x4 */ f32 y;
    /* 0x8 */ f32 z;
    /* 0xC */ f32 w;

    CVector3() {}
    CVector3(float x, float y, float z) : x(x), y(y), z(z) {}
    CVector3(const CVector3& v) {
        f64 a = ((const f64*)&v)[0];
        f64 b = ((const f64*)&v)[1];
        ((f64*)this)[0] = a;
        ((f64*)this)[1] = b;
    }
    CVector3 operator=(const CVector3& v) {
        f64 a = ((const f64*)&v)[0];
        f64 b = ((const f64*)&v)[1];
        ((f64*)this)[0] = a;
        ((f64*)this)[1] = b;
        return *this;
    }

    // A plain copy (operator= returns by value, which adds a temporary).
    void Copy(const CVector3& v) {
        f64 a = ((const f64*)&v)[0];
        f64 b = ((const f64*)&v)[1];
        ((f64*)this)[0] = a;
        ((f64*)this)[1] = b;
    }

    float Length() const { return sqrtf(x * x + y * y + z * z); }
    // Returns the length before normalizing.
    float Normalize() {
        float len = Length();
        if (len != 0.0f) {
            float inv = 1.0f / len;
            x *= inv;
            y *= inv;
            z *= inv;
        }
        return len;
    }
    void Scale(const CVector3& v, float s) {
        x = s * v.x;
        y = s * v.y;
        z = s * v.z;
    }
    void Add(const CVector3& v) {
        x = x + v.x;
        y = y + v.y;
        z = z + v.z;
    }
    void Sub(const CVector3& a, const CVector3& b) {
        x = a.x - b.x;
        y = a.y - b.y;
        z = a.z - b.z;
    }
    void Cross(const CVector3& a, const CVector3& b) {
        x = a.y * b.z - a.z * b.y;
        y = a.z * b.x - a.x * b.z;
        z = a.x * b.y - a.y * b.x;
    }
    void Abs(const CVector3& v) {
        x = fabs(v.x);
        y = fabs(v.y);
        z = fabs(v.z);
    }
    // Dot products of `v` with three vectors: (v.a, v.b, v.c).
    // Reads everything before writing, so the result may alias an input.
    void DotDotDot(const CVector3& a, const CVector3& b, const CVector3& c, const CVector3& v) {
        float vy = v.y;
        float vx = v.x;
        float vz = v.z;
        float dx = a.x * vx + a.y * vy + a.z * vz;
        float dy = b.x * vx + b.y * vy + b.z * vz;
        float dz = c.x * vx + c.y * vy + c.z * vz;
        x = dx;
        y = dy;
        z = dz;
    }

    // Rotates this unit vector towards the unit vector `axis` until the
    // angle between them is at most acos(cosLimit).
    void Constrain(const CVector3& axis, float cosLimit);  // 0x8009213C
};

class CVector2 {
public:
    /* 0x0 */ f32 x;
    /* 0x4 */ f32 y;
    // this = v rotated by `angle` (MathSinCos units).
    void Rotate(const CVector2& v, long angle);  // 0x8009226C
};

#endif
