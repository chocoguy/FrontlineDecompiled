#include "game/matrix.h"

#include <math.h>
#include <string.h>
#include "game/CVector4.h"
#include "game/endian.h"
#include "game/trig.h"

extern "C" void C_MTXFrustum(float m[4][4], f32 t, f32 b, f32 l, f32 r, f32 n, f32 f);

// MathSinCos angle units per radian (2^24 per turn).
#define RAD_TO_ANGLE 2670176.8f
// Radians per 1/65536 turn, as the game computes it (6.28 rather than 2*pi).
#define SHORT_ANGLE_TO_RAD 0.0000958252f

// A matrix that sets itself to the identity when constructed.
class CUnitMatrix : public CMatrix {
public:
    CUnitMatrix() { Ident(); }
};

// Functions are written in reverse address order: the game was compiled with
// deferred inlining, which emits them last to first.

CMatrix CMatrix::s_TempMat;
static CUnitMatrix _Mat_Unit;
u8 CMatrix::s_ClassInit;

static f32 _Mat_Data[16] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f,
};

void CMatrix::InitClass() {
    memcpy(&_Mat_Unit, _Mat_Data, sizeof(CMatrix));
    s_ClassInit = 1;
}

asm void CMatrix::operator=(const CMatrix& src) {
    nofralloc
    psq_l f0, 0x0(r4), 0, 0
    psq_l f1, 0x8(r4), 0, 0
    psq_l f2, 0x10(r4), 0, 0
    psq_l f3, 0x18(r4), 0, 0
    psq_st f0, 0x0(r3), 0, 0
    psq_st f1, 0x8(r3), 0, 0
    psq_st f2, 0x10(r3), 0, 0
    psq_st f3, 0x18(r3), 0, 0
    psq_l f0, 0x20(r4), 0, 0
    psq_l f1, 0x28(r4), 0, 0
    psq_l f2, 0x30(r4), 0, 0
    psq_l f3, 0x38(r4), 0, 0
    psq_st f0, 0x20(r3), 0, 0
    psq_st f1, 0x28(r3), 0, 0
    psq_st f2, 0x30(r3), 0, 0
    psq_st f3, 0x38(r3), 0, 0
    blr
}

void CMatrix::BuildScale(float s) {
    Ident();
    m[0][0] = m[1][1] = m[2][2] = s;
}

void CMatrix::BuildScale(CVector3 s) {
    Ident();
    m[0][0] = s.x;
    m[1][1] = s.y;
    m[2][2] = s.z;
}

// Rotation of `angle` radians about a unit `axis`.
void CMatrix::BuildRot(CVector3 axis, float angle) {
    float s, c;
    MathSinCos((long)(RAD_TO_ANGLE * angle), &s, &c);
    float t = 1.0f - c;
    float tx = t * axis.x;
    float ty = t * axis.y;
    float tz = t * axis.z;
    float txx = tx * axis.x;
    float txy = tx * axis.y;
    float txz = tx * axis.z;
    float tyy = ty * axis.y;
    float tyz = ty * axis.z;
    float tzz = tz * axis.z;
    float sx = s * axis.x;
    float sy = s * axis.y;
    float sz = s * axis.z;
    m[0][0] = txx + c;
    m[0][1] = txy + sz;
    m[0][2] = txz - sy;
    m[0][3] = 0.0f;
    m[1][0] = txy - sz;
    m[1][1] = tyy + c;
    m[1][2] = tyz + sx;
    m[1][3] = 0.0f;
    m[2][0] = txz + sy;
    m[2][1] = tyz - sx;
    m[2][2] = tzz + c;
    m[2][3] = 0.0f;
    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = 0.0f;
    m[3][3] = 1.0f;
}

void CMatrix::BuildRotX(float angle) {
    float s, c;
    MathSinCos((long)(RAD_TO_ANGLE * angle), &s, &c);
    Ident();
    m[1][1] = c;
    m[1][2] = s;
    m[2][1] = -s;
    m[2][2] = c;
}

void CMatrix::BuildRotZ(float angle) {
    float s, c;
    MathSinCos((long)(RAD_TO_ANGLE * angle), &s, &c);
    Ident();
    m[0][0] = c;
    m[0][1] = s;
    m[1][0] = -s;
    m[1][1] = c;
}

void CMatrix::BuildTrans(CVector3 v) {
    Ident();
    SetPos(v);
}

void CMatrix::SetRight(CVector3 v) {
    m[0][0] = v.x;
    m[0][1] = v.y;
    m[0][2] = v.z;
}

void CMatrix::SetFront(CVector3 v) {
    m[1][0] = v.x;
    m[1][1] = v.y;
    m[1][2] = v.z;
}

void CMatrix::SetUp(CVector3 v) {
    m[2][0] = v.x;
    m[2][1] = v.y;
    m[2][2] = v.z;
}

void CMatrix::SetPos(CVector3 v) {
    m[3][0] = v.x;
    m[3][1] = v.y;
    m[3][2] = v.z;
}

// Translate in local space: pos += v * rotation.
void CMatrix::PreTranslate(CVector3 v) {
    m[3][0] += m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z;
    m[3][1] += m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z;
    m[3][2] += m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z;
}

void CMatrix::Translate(CVector3 v) {
    m[3][0] += v.x;
    m[3][1] += v.y;
    m[3][2] += v.z;
}

void CMatrix::Rotate(CVector3 axis, float angle) {
    CMatrix rot;
    rot.BuildRot(axis, angle);
    Multiply(*this, rot);
}

void CMatrix::RotateX(float angle) {
    float s, c;
    MathSinCos((long)(RAD_TO_ANGLE * -angle), &s, &c);
    float a0 = m[1][0], a1 = m[1][1], a2 = m[1][2], a3 = m[1][3];
    float b0 = m[2][0], b1 = m[2][1], b2 = m[2][2], b3 = m[2][3];
    m[1][0] = c * a0 + s * b0;
    m[1][1] = c * a1 + s * b1;
    m[1][2] = c * a2 + s * b2;
    m[1][3] = c * a3 + s * b3;
    m[2][0] = -s * a0 + c * b0;
    m[2][1] = -s * a1 + c * b1;
    m[2][2] = -s * a2 + c * b2;
    m[2][3] = -s * a3 + c * b3;
}

void CMatrix::RotateY(float angle) {
    float s, c;
    MathSinCos((long)(RAD_TO_ANGLE * -angle), &s, &c);
    float a0 = m[0][0], a1 = m[0][1], a2 = m[0][2], a3 = m[0][3];
    float b0 = m[2][0], b1 = m[2][1], b2 = m[2][2], b3 = m[2][3];
    m[0][0] = c * a0 + -s * b0;
    m[0][1] = c * a1 + -s * b1;
    m[0][2] = c * a2 + -s * b2;
    m[0][3] = c * a3 + -s * b3;
    m[2][0] = s * a0 + c * b0;
    m[2][1] = s * a1 + c * b1;
    m[2][2] = s * a2 + c * b2;
    m[2][3] = s * a3 + c * b3;
}

void CMatrix::RotateZ(float angle) {
    float s, c;
    MathSinCos((long)(RAD_TO_ANGLE * -angle), &s, &c);
    float a0 = m[0][0], a1 = m[0][1], a2 = m[0][2], a3 = m[0][3];
    float b0 = m[1][0], b1 = m[1][1], b2 = m[1][2], b3 = m[1][3];
    m[0][0] = c * a0 + s * b0;
    m[0][1] = c * a1 + s * b1;
    m[0][2] = c * a2 + s * b2;
    m[0][3] = c * a3 + s * b3;
    m[1][0] = -s * a0 + c * b0;
    m[1][1] = -s * a1 + c * b1;
    m[1][2] = -s * a2 + c * b2;
    m[1][3] = -s * a3 + c * b3;
}

// Angles in 1/65536 turn, in the high 16 of 24 bits.
void CMatrix::RotateX(long angle) {
    RotateX((float)(angle >> 8) * SHORT_ANGLE_TO_RAD);
}

void CMatrix::RotateY(long angle) {
    RotateY((float)(angle >> 8) * SHORT_ANGLE_TO_RAD);
}

void CMatrix::RotateZ(long angle) {
    RotateZ((float)(angle >> 8) * SHORT_ANGLE_TO_RAD);
}

void CMatrix::Scale(CVector3 s) {
    for (int i = 0; i < 4; i++) {
        m[i][0] *= s.x;
        m[i][1] *= s.y;
        m[i][2] *= s.z;
    }
}

void CMatrix::Scale(float s) {
    for (int i = 0; i < 4; i++) {
        m[i][0] *= s;
        m[i][1] *= s;
        m[i][2] *= s;
    }
}

void CMatrix::Multiply(const CMatrix& a, const CMatrix& b) {
    PSMultMatrix(&m[0][0], &a.m[0][0], &b.m[0][0]);
}

void CMatrix::Perspective(float xScale, float yScale, float zNear, float unused) {
    C_MTXFrustum(m, yScale * zNear, -yScale * zNear, -xScale * zNear, xScale * zNear, zNear,
                 1000.0f);
}

void CMatrix::Orthographic(float xScale, float yScale, float zNear, float zFar) {
    float invDepth = 1.0f / (zFar - zNear);
    Ident();
    m[0][0] = 1.0f / xScale;
    m[1][1] = 1.0f / yScale;
    m[2][2] = 2.0f * invDepth;
    m[3][2] = invDepth * (zFar + zNear);
    m[3][3] = 0.0f;
}

void CMatrix::Transform(CVector4& out, CVector3 v) const {
    out.x = v.x * m[0][0] + v.y * m[1][0] + v.z * m[2][0] + m[3][0];
    out.y = v.x * m[0][1] + v.y * m[1][1] + v.z * m[2][1] + m[3][1];
    out.z = v.x * m[0][2] + v.y * m[1][2] + v.z * m[2][2] + m[3][2];
    out.w = v.x * m[0][3] + v.y * m[1][3] + v.z * m[2][3] + m[3][3];
}

void CMatrix::TransformPoint(CVector3& out, CVector3 v) const {
    PSTransformPoint(&out.x, &m[0][0], &v.x);
}

void CMatrix::TransformVector(CVector3& out, CVector3 v) const {
    PSTransformVector(&out.x, &m[0][0], &v.x);
}

// Inverse-transform a world point into this matrix's space (rotation must be
// orthonormal).
CVector3 CMatrix::TransformToLocal(CVector3 v) const {
    CVector3 out;
    CVector3 d(v.x - m[3][0], v.y - m[3][1], v.z - m[3][2]);
    PSTransformToLocal(&out.x, &m[0][0], &d.x);
    out.w = 1.0f;
    return out;
}

// Inverse of a rigid transform: transpose the rotation, rotate the position.
void CMatrix::FastInverse(const CMatrix& src) {
    CVector3 right = *(const CVector3*)src.m[0];
    CVector3 front = *(const CVector3*)src.m[1];
    CVector3 up = *(const CVector3*)src.m[2];
    CVector3 pos = *(const CVector3*)src.m[3];
    m[0][0] = src.m[0][0];
    m[0][1] = src.m[1][0];
    m[0][2] = src.m[2][0];
    m[0][3] = 0.0f;
    m[1][0] = src.m[0][1];
    m[1][1] = src.m[1][1];
    m[1][2] = src.m[2][1];
    m[1][3] = 0.0f;
    m[2][0] = src.m[0][2];
    m[2][1] = src.m[1][2];
    m[2][2] = src.m[2][2];
    m[2][3] = 0.0f;
    m[3][0] = -(pos.x * right.x + pos.y * right.y + pos.z * right.z);
    m[3][1] = -(pos.x * front.x + pos.y * front.y + pos.z * front.z);
    m[3][2] = -(pos.x * up.x + pos.y * up.y + pos.z * up.z);
    m[3][3] = 1.0f;
}

// General affine inverse (Graphics Gems II, "Fast Matrix Inversion").
// Leaves the matrix unchanged if `in` is singular.
void CMatrix::Inverse(const CMatrix& in) {
    float pos = 0.0f;
    float neg = 0.0f;
    float t;

#define ACCUMULATE                                                                                \
    if (t > 0.0f) {                                                                               \
        pos += t;                                                                                 \
    }                                                                                             \
    if (!(t > 0.0f)) {                                                                            \
        neg += t;                                                                                 \
    }

    t = in.m[0][0] * in.m[1][1] * in.m[2][2];
    ACCUMULATE
    t = in.m[0][1] * in.m[1][2] * in.m[2][0];
    ACCUMULATE
    t = in.m[0][2] * in.m[1][0] * in.m[2][1];
    ACCUMULATE
    t = -in.m[0][2] * in.m[1][1] * in.m[2][0];
    ACCUMULATE
    t = -in.m[0][1] * in.m[1][0] * in.m[2][2];
    ACCUMULATE
    t = -in.m[0][0] * in.m[1][2] * in.m[2][1];
    ACCUMULATE
#undef ACCUMULATE

    float det = pos + neg;
    if (det == 0.0) {
        return;
    }
    if ((float)fabs(det / (pos - neg)) < 1e-15f) {
        return;
    }

    det = 1.0f / det;
    m[0][0] = (in.m[1][1] * in.m[2][2] - in.m[1][2] * in.m[2][1]) * det;
    m[0][1] = -(in.m[0][1] * in.m[2][2] - in.m[0][2] * in.m[2][1]) * det;
    m[0][2] = (in.m[0][1] * in.m[1][2] - in.m[0][2] * in.m[1][1]) * det;
    m[0][3] = 0.0f;
    m[1][0] = -(in.m[1][0] * in.m[2][2] - in.m[1][2] * in.m[2][0]) * det;
    m[1][1] = (in.m[0][0] * in.m[2][2] - in.m[0][2] * in.m[2][0]) * det;
    m[1][2] = -(in.m[0][0] * in.m[1][2] - in.m[0][2] * in.m[1][0]) * det;
    m[1][3] = 0.0f;
    m[2][0] = (in.m[1][0] * in.m[2][1] - in.m[1][1] * in.m[2][0]) * det;
    m[2][1] = -(in.m[0][0] * in.m[2][1] - in.m[0][1] * in.m[2][0]) * det;
    m[2][2] = (in.m[0][0] * in.m[1][1] - in.m[0][1] * in.m[1][0]) * det;
    m[2][3] = 0.0f;
    m[3][0] = -(in.m[3][0] * m[0][0] + in.m[3][1] * m[1][0] + in.m[3][2] * m[2][0]);
    m[3][1] = -(in.m[3][0] * m[0][1] + in.m[3][1] * m[1][1] + in.m[3][2] * m[2][1]);
    m[3][2] = -(in.m[3][0] * m[0][2] + in.m[3][1] * m[1][2] + in.m[3][2] * m[2][2]);
    m[3][3] = 1.0f;
}

void CMatrix::Transpose() {
    for (int i = 1; i < 4; i++) {
        for (int j = 0; j < i; j++) {
            float t = m[i][j];
            m[i][j] = m[j][i];
            m[j][i] = t;
        }
    }
}

// Gram-Schmidt on the three rotation rows (right, then front, then up).
void CMatrix::Orthonormalize() {
    CVector3 right = *(CVector3*)m[0];
    CVector3 front = *(CVector3*)m[1];
    CVector3 up = *(CVector3*)m[2];

    right.Normalize();

    CVector3 t;
    float d = front.x * right.x + front.y * right.y + front.z * right.z;
    t.Scale(right, d);
    CVector3 f;
    f.Sub(front, t);
    f.Normalize();

    float d0 = up.x * right.x + up.y * right.y + up.z * right.z;
    float d1 = up.x * f.x + up.y * f.y + up.z * f.z;
    CVector3 u;
    t.Scale(right, d0);
    u.Sub(up, t);
    t.Scale(f, d1);
    u.Sub(u, t);
    u.Normalize();

    m[0][0] = right.x;
    m[0][1] = right.y;
    m[0][2] = right.z;
    m[1][0] = f.x;
    m[1][1] = f.y;
    m[1][2] = f.z;
    m[2][0] = u.x;
    m[2][1] = u.y;
    m[2][2] = u.z;
}

void CMatrix::Ident() {
    register f32 zero = 0.0f;
    register f32 one = 1.0f;
    register f32 zero_one, one_zero;
    register CMatrix* p = this;
    asm {
        psq_st zero, 0x8(p), 0, 0
        ps_merge01 zero_one, zero, one
        psq_st zero, 0x18(p), 0, 0
        ps_merge10 one_zero, one, zero
        psq_st zero, 0x20(p), 0, 0
        psq_st zero, 0x30(p), 0, 0
        psq_st zero_one, 0x10(p), 0, 0
        psq_st one_zero, 0x0(p), 0, 0
        psq_st one_zero, 0x28(p), 0, 0
        psq_st zero_one, 0x38(p), 0, 0
        nop
    }
}

void CMatrix::ToEulerXYZ(float& x, float& y, float& z) const {
    if (m[0][2] < 1.0f) {
        if (m[0][2] > -1.0f) {
            x = asin(-m[1][2]);
            y = atan2(-m[0][2], m[2][2]);
            z = atan2(m[0][1], m[0][0]);
        } else {
            x = -1.5707964f;
            y = -atan2(m[1][0], m[1][1]);
            z = 0.0f;
        }
    } else {
        x = 1.5707964f;
        y = atan2(m[1][0], m[1][1]);
        z = 0.0f;
    }
}

void CMatrix::ToEulerXZY(float& x, float& y, float& z) const {
    if (m[0][1] < 1.0f) {
        if (m[0][1] > -1.0f) {
            y = atan2(-m[0][2], m[0][0]);
            z = asin(m[0][1]);
            x = atan2(-m[2][1], m[1][1]);
        } else {
            x = -atan2(m[1][2], m[2][2]);
            z = -1.5707964f;
            y = 0.0f;
        }
    } else {
        x = atan2(m[1][2], m[2][2]);
        z = 1.5707964f;
        y = 0.0f;
    }
}

// Convert from Tibet (the art tool) axes: flip the right axis and swap the
// front and up axes.
void CMatrix::TibToMOHFL() {
    m[0][0] = -m[0][0];
    m[0][1] = -m[0][1];
    m[0][2] = -m[0][2];
    for (int j = 0; j < 3; j++) {
        float t = m[1][j];
        m[1][j] = m[2][j];
        m[2][j] = t;
    }
}

// Transposed 3x4 copy, the layout GX expects.
void CMatrix::GetGAMECUBEMatrix34(float (&out)[3][4]) const {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            out[i][j] = m[j][i];
        }
    }
}

void CMatrix::EndianSwap() {
    ::EndianSwap(*(u32*)&m[0][0]);
    ::EndianSwap(*(u32*)&m[1][0]);
    ::EndianSwap(*(u32*)&m[2][0]);
    ::EndianSwap(*(u32*)&m[3][0]);
    ::EndianSwap(*(u32*)&m[0][1]);
    ::EndianSwap(*(u32*)&m[1][1]);
    ::EndianSwap(*(u32*)&m[2][1]);
    ::EndianSwap(*(u32*)&m[3][1]);
    ::EndianSwap(*(u32*)&m[0][2]);
    ::EndianSwap(*(u32*)&m[1][2]);
    ::EndianSwap(*(u32*)&m[2][2]);
    ::EndianSwap(*(u32*)&m[3][2]);
    ::EndianSwap(*(u32*)&m[0][3]);
    ::EndianSwap(*(u32*)&m[1][3]);
    ::EndianSwap(*(u32*)&m[2][3]);
    ::EndianSwap(*(u32*)&m[3][3]);
}

asm void PSTransformToLocal(float* out, const float* m, const float* v) {
    nofralloc
    psq_l f0, 0x0(r4), 0, 0
    psq_l f2, 0x10(r4), 0, 0
    psq_l f4, 0x20(r4), 0, 0
    lfs f1, 0x8(r4)
    lfs f3, 0x18(r4)
    lfs f5, 0x28(r4)
    ps_merge10 f8, f0, f0
    ps_merge10 f9, f2, f2
    ps_merge01 f0, f0, f9
    ps_merge01 f2, f8, f2
    ps_mr f8, f1
    ps_merge01 f1, f4, f1
    ps_merge01 f4, f8, f4
    ps_merge10 f8, f4, f4
    ps_merge10 f9, f3, f3
    ps_merge01 f4, f4, f9
    ps_merge01 f3, f8, f3
    psq_l f8, 0x0(r5), 0, 0
    ps_merge11 f9, f8, f8
    ps_merge00 f8, f8, f8
    lfs f10, 0x8(r5)
    ps_muls0 f11, f0, f8
    ps_madds1 f11, f2, f9, f11
    ps_madds0 f11, f4, f10, f11
    fmuls f12, f1, f8
    fmadds f12, f3, f9, f12
    fmadds f12, f5, f10, f12
    psq_st f11, 0x0(r3), 0, 0
    stfs f12, 0x8(r3)
    blr
}

asm void PSTransformVector(float* out, const float* m, const float* v) {
    nofralloc
    psq_l f0, 0x0(r4), 0, 0
    psq_l f2, 0x10(r4), 0, 0
    psq_l f4, 0x20(r4), 0, 0
    lfs f1, 0x8(r4)
    lfs f3, 0x18(r4)
    lfs f5, 0x28(r4)
    psq_l f8, 0x0(r5), 0, 0
    ps_merge11 f9, f8, f8
    ps_merge00 f8, f8, f8
    lfs f10, 0x8(r5)
    ps_muls0 f11, f0, f8
    ps_madds1 f11, f2, f9, f11
    ps_madds0 f11, f4, f10, f11
    fmuls f12, f1, f8
    fmadds f12, f3, f9, f12
    fmadds f12, f5, f10, f12
    psq_st f11, 0x0(r3), 0, 0
    stfs f12, 0x8(r3)
    blr
}

asm void PSTransformPoint(float* out, const float* m, const float* v) {
    nofralloc
    psq_l f0, 0x0(r4), 0, 0
    psq_l f2, 0x10(r4), 0, 0
    psq_l f4, 0x20(r4), 0, 0
    psq_l f6, 0x30(r4), 0, 0
    lfs f1, 0x8(r4)
    lfs f3, 0x18(r4)
    lfs f5, 0x28(r4)
    lfs f7, 0x38(r4)
    psq_l f8, 0x0(r5), 0, 0
    ps_merge11 f9, f8, f8
    ps_merge00 f8, f8, f8
    lfs f10, 0x8(r5)
    ps_muls0 f11, f0, f8
    ps_madds1 f11, f2, f9, f11
    ps_madds0 f11, f4, f10, f11
    ps_add f11, f6, f11
    fmuls f12, f1, f8
    fmadds f12, f3, f9, f12
    fmadds f12, f5, f10, f12
    fadds f12, f7, f12
    psq_st f11, 0x0(r3), 0, 0
    stfs f12, 0x8(r3)
    blr
}

// out = a * b for row-major 4x4 matrices.
//
// The original uses f14-f19 without saving them, breaking the PowerPC ABI;
// the retail callers happen to keep nothing live in those registers across
// the call, but our recompiled callers do (e.g. CParticleSystem::RenderSystem
// lost its particle age to it). This version saves and restores them.
asm void PSMultMatrix(float* out, const float* a, const float* b) {
    nofralloc
    stwu r1, -0x70(r1)
    stfd f14, 0x8(r1)
    psq_st f14, 0x10(r1), 0, 0
    stfd f15, 0x18(r1)
    psq_st f15, 0x20(r1), 0, 0
    stfd f16, 0x28(r1)
    psq_st f16, 0x30(r1), 0, 0
    stfd f17, 0x38(r1)
    psq_st f17, 0x40(r1), 0, 0
    stfd f18, 0x48(r1)
    psq_st f18, 0x50(r1), 0, 0
    stfd f19, 0x58(r1)
    psq_st f19, 0x60(r1), 0, 0
    psq_l f0, 0x0(r4), 0, 0
    psq_l f8, 0x0(r5), 0, 0
    ps_muls0 f16, f8, f0
    psq_l f9, 0x10(r5), 0, 0
    psq_l f10, 0x20(r5), 0, 0
    ps_madds1 f16, f9, f0, f16
    psq_l f1, 0x8(r4), 0, 0
    psq_l f11, 0x30(r5), 0, 0
    ps_madds0 f16, f10, f1, f16
    ps_madds1 f16, f11, f1, f16
    psq_st f16, 0x0(r3), 0, 0
    psq_l f2, 0x10(r4), 0, 0
    psq_l f3, 0x18(r4), 0, 0
    ps_muls0 f17, f8, f2
    ps_madds1 f17, f9, f2, f17
    ps_madds0 f17, f10, f3, f17
    ps_madds1 f17, f11, f3, f17
    psq_st f17, 0x10(r3), 0, 0
    psq_l f4, 0x20(r4), 0, 0
    psq_l f5, 0x28(r4), 0, 0
    ps_muls0 f18, f8, f4
    ps_madds1 f18, f9, f4, f18
    ps_madds0 f18, f10, f5, f18
    ps_madds1 f18, f11, f5, f18
    psq_st f18, 0x20(r3), 0, 0
    psq_l f6, 0x30(r4), 0, 0
    psq_l f7, 0x38(r4), 0, 0
    ps_muls0 f19, f8, f6
    ps_madds1 f19, f9, f6, f19
    ps_madds0 f19, f10, f7, f19
    ps_madds1 f19, f11, f7, f19
    psq_st f19, 0x30(r3), 0, 0
    psq_l f12, 0x8(r5), 0, 0
    ps_muls0 f16, f12, f0
    psq_l f13, 0x18(r5), 0, 0
    psq_l f14, 0x28(r5), 0, 0
    ps_madds1 f16, f13, f0, f16
    psq_l f15, 0x38(r5), 0, 0
    ps_madds0 f16, f14, f1, f16
    ps_madds1 f16, f15, f1, f16
    psq_st f16, 0x8(r3), 0, 0
    ps_muls0 f17, f12, f2
    ps_madds1 f17, f13, f2, f17
    ps_madds0 f17, f14, f3, f17
    ps_madds1 f17, f15, f3, f17
    psq_st f17, 0x18(r3), 0, 0
    ps_muls0 f18, f12, f4
    ps_madds1 f18, f13, f4, f18
    ps_madds0 f18, f14, f5, f18
    ps_madds1 f18, f15, f5, f18
    psq_st f18, 0x28(r3), 0, 0
    ps_muls0 f19, f12, f6
    ps_madds1 f19, f13, f6, f19
    ps_madds0 f19, f14, f7, f19
    ps_madds1 f19, f15, f7, f19
    psq_st f19, 0x38(r3), 0, 0
    psq_l f14, 0x10(r1), 0, 0
    lfd f14, 0x8(r1)
    psq_l f15, 0x20(r1), 0, 0
    lfd f15, 0x18(r1)
    psq_l f16, 0x30(r1), 0, 0
    lfd f16, 0x28(r1)
    psq_l f17, 0x40(r1), 0, 0
    lfd f17, 0x38(r1)
    psq_l f18, 0x50(r1), 0, 0
    lfd f18, 0x48(r1)
    psq_l f19, 0x60(r1), 0, 0
    lfd f19, 0x58(r1)
    addi r1, r1, 0x70
    blr
}
