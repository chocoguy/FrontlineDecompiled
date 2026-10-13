#include "game/quaternion.h"

#include <math.h>
#include "game/endian.h"
#include "game/matrix.h"

// Functions are written in reverse address order: the game was compiled with
// deferred inlining, which emits them last to first.

void CQuaternion::Set(float qx, float qy, float qz, float qw) {
    x = qx;
    y = qy;
    z = qz;
    w = qw;
}

void CQuaternion::Normalize() {
    float inv = 1.0f / sqrtf(x * x + y * y + z * z + w * w);
    x *= inv;
    y *= inv;
    z *= inv;
    w *= inv;
}

void CQuaternion::GetMatrix(CMatrix& m) const {
    float x2 = 2.0f * x;
    float y2 = 2.0f * y;
    float w2 = 2.0f * w;
    float yy = y2 * y;
    float zz = 2.0f * z * z;
    float xy = x2 * y;
    float wz = w2 * z;
    float xz = x2 * z;
    float wy = w2 * y;
    float xx = x2 * x;
    float yz = y2 * z;
    float wx = w2 * x;

    m.m[0][0] = 1.0f - (yy + zz);
    m.m[0][1] = xy - wz;
    m.m[0][2] = xz + wy;
    m.m[0][3] = 0.0f;
    m.m[1][0] = xy + wz;
    m.m[1][1] = 1.0f - (xx + zz);
    m.m[1][2] = yz - wx;
    m.m[1][3] = 0.0f;
    m.m[2][0] = xz - wy;
    m.m[2][1] = yz + wx;
    m.m[2][2] = 1.0f - (xx + yy);
    m.m[2][3] = 0.0f;
    m.m[3][0] = 0.0f;
    m.m[3][1] = 0.0f;
    m.m[3][2] = 0.0f;
    m.m[3][3] = 1.0f;
}

void CQuaternion::SetFromMatrix(const CMatrix& m) {
    float trace = m.m[0][0] + m.m[1][1] + m.m[2][2];
    if (trace > 0.0f) {
        float s = sqrtf(trace + 1.0f);
        w = 0.5f * s;
        s = 0.5f / s;
        x = s * (m.m[2][1] - m.m[1][2]);
        y = s * (m.m[0][2] - m.m[2][0]);
        z = s * (m.m[1][0] - m.m[0][1]);
    } else {
        static int next[3] = {1, 2, 0};
        int i = 0;
        if (m.m[1][1] > m.m[0][0]) {
            i = 1;
        }
        if (m.m[2][2] > m.m[i][i]) {
            i = 2;
        }
        int j = next[i];
        int k = next[j];
        float s = sqrtf(m.m[i][i] - m.m[j][j] - m.m[k][k] + 1.0f);
        float* q[3] = {&x, &y, &z};
        *q[i] = 0.5f * s;
        s = 0.5f / s;
        *q[j] = s * (m.m[j][i] + m.m[i][j]);
        *q[k] = s * (m.m[k][i] + m.m[i][k]);
        w = s * (m.m[k][j] - m.m[j][k]);
    }
    Normalize();
}

void CQuaternion::Slerp(CQuaternion to, CQuaternion from, float t) {
    float cosom = from.x * to.x + from.y * to.y + from.z * to.z + from.w * to.w;
    if (cosom < 0.0f) {
        cosom = -cosom;
        x = -to.x;
        y = -to.y;
        z = -to.z;
        w = -to.w;
    } else {
        x = to.x;
        y = to.y;
        z = to.z;
        w = to.w;
    }

    float scaleFrom;
    float scaleTo;
    if (1.0f - cosom > 0.0005f) {
        float omega = acos(cosom);
        float sinom = sin(omega);
        scaleFrom = (float)sin((1.0f - t) * omega) / sinom;
        scaleTo = (float)sin(t * omega) / sinom;
    } else {
        scaleFrom = 1.0f - t;
        scaleTo = t;
    }
    x = scaleFrom * from.x + scaleTo * x;
    y = scaleFrom * from.y + scaleTo * y;
    z = scaleFrom * from.z + scaleTo * z;
    w = scaleFrom * from.w + scaleTo * w;
}

void CQuaternion::Multiply(CQuaternion q) {
    float tx = x;
    float ty = y;
    float tz = z;
    float tw = w;
    float qx = q.x;
    float qy = q.y;
    float qz = q.z;
    float qw = q.w;
    // The products are separate temporaries (no fused multiply-add).
    float xw = tx * qw;
    float yw = ty * qw;
    float zw = tz * qw;
    float wx = qx * tw;
    float wy = qy * tw;
    float wz = qz * tw;
    w = tw * qw - (tx * qx + ty * qy + tz * qz);
    x = (ty * qz - tz * qy) + xw + wx;
    y = (tz * qx - tx * qz) + yw + wy;
    z = (tx * qy - ty * qx) + zw + wz;
}

void CQuaternion::Rotate(CVector3& out, CVector3 v) {
    float x2 = 2.0f * x;
    float y2 = 2.0f * y;
    float w2 = 2.0f * w;
    float yy = y2 * y;
    float zz = 2.0f * z * z;
    float xy = x2 * y;
    float wz = w2 * z;
    float xz = x2 * z;
    float wy = w2 * y;
    float xx = x2 * x;
    float yz = y2 * z;
    float wx = w2 * x;

    out.x = (1.0f - (yy + zz)) * v.x + (xy + wz) * v.y + (xz - wy) * v.z;
    out.y = (xy - wz) * v.x + (1.0f - (xx + zz)) * v.y + (yz + wx) * v.z;
    out.z = (xz + wy) * v.x + (yz - wx) * v.y + (1.0f - (xx + yy)) * v.z;
}

void CQuaternion::Rotate(CVector3& v) {
    Rotate(v, v);
}

void CQuaternion::EndianSwap() {
    // Swapped through integer registers: a byte-swapped float can be any bit
    // pattern.
    ::EndianSwap(*(u32*)&x);
    ::EndianSwap(*(u32*)&y);
    ::EndianSwap(*(u32*)&z);
    ::EndianSwap(*(u32*)&w);
}
