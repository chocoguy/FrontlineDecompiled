// CQuaternion: rotation quaternion stored w, x, y, z. Hand-maintained.
#ifndef _GAME_QUATERNION_H_
#define _GAME_QUATERNION_H_

#include "types.h"
#include "game_types.h"

class CMatrix;
class CVector3;

// size 0x10
class CQuaternion {
public:
    /* 0x0 */ f32 w;
    /* 0x4 */ f32 x;
    /* 0x8 */ f32 y;
    /* 0xC */ f32 z;
    void EndianSwap();  // 0x80091420
    // v = v * R (row vector times the rotation matrix GetMatrix builds).
    void Rotate(CVector3& v);  // 0x800914EC
    void Rotate(CVector3& out, CVector3 v);  // 0x80091620
    // this = this * q
    void Multiply(CQuaternion q);  // 0x8009174C
    // Spherical interpolation: t = 0 gives `from` (the SECOND argument),
    // t = 1 gives `to`.
    void Slerp(CQuaternion to, CQuaternion from, float t);  // 0x800917F0
    void SetFromMatrix(const CMatrix&);  // 0x80091990
    void GetMatrix(CMatrix&) const;  // 0x80091C9C
    void Normalize();  // 0x80091D88
    void Set(float x, float y, float z, float w);  // 0x80091E50
};

#endif
