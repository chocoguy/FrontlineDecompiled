// CMatrix: 4x4 row-major float matrix (rows are right, front, up, position).
// Hand-maintained (layout and inline methods checked against the retail code).
#ifndef _GAME_MATRIX_H_
#define _GAME_MATRIX_H_

#include "types.h"
#include "game/vector.h"

class CVector4;

// 16-byte aligned: functions with a local CMatrix realign the stack.
class CMatrix {
public:
    /* 0x0 */ f32 m[4][4];

    // The constructor leaves the matrix uninitialized; it only makes sure the
    // class statics are set up.
    CMatrix() {
        if (!s_ClassInit) {
            InitClass();
        }
    }

    void EndianSwap();  // 0x8008EBF0
    void GetGAMECUBEMatrix34(float (&out)[3][4]) const;  // 0x8008EEFC
    void TibToMOHFL();  // 0x8008EF60
    void ToEulerXZY(float& x, float& y, float& z) const;  // 0x8008EFB8
    void ToEulerXYZ(float& x, float& y, float& z) const;  // 0x8008F0B4
    void Ident();  // 0x8008F1B0
    void Orthonormalize();  // 0x8008F1E8
    void Transpose();  // 0x8008F498
    void Inverse(const CMatrix& src);  // 0x8008F5C0
    void FastInverse(const CMatrix& src);  // 0x8008F8FC
    CVector3 TransformToLocal(CVector3 v) const;  // 0x8008FA08
    void TransformVector(CVector3& out, CVector3 v) const;  // 0x8008FA84
    void TransformPoint(CVector3& out, CVector3 v) const;  // 0x8008FAB0
    void Transform(CVector4& out, CVector3 v) const;  // 0x8008FADC
    void Orthographic(float xScale, float yScale, float zNear, float zFar);  // 0x8008FBA0
    void Perspective(float xScale, float yScale, float zNear, float unused);  // 0x8008FC0C
    void Multiply(const CMatrix& a, const CMatrix& b);  // 0x8008FC50
    void Scale(float s);  // 0x8008FC70
    void Scale(CVector3 s);  // 0x8008FD04
    void RotateZ(long angle);  // 0x8008FDC8
    void RotateY(long angle);  // 0x8008FF0C
    void RotateX(long angle);  // 0x80090050
    void RotateZ(float angle);  // 0x80090194
    void RotateY(float angle);  // 0x800902B0
    void RotateX(float angle);  // 0x800903CC
    void Rotate(CVector3 axis, float angle);  // 0x800904E8
    void Translate(CVector3 v);  // 0x80090660
    void PreTranslate(CVector3 v);  // 0x80090694
    void SetPos(CVector3 v);  // 0x80090728
    void SetUp(CVector3 v);  // 0x80090744
    void SetFront(CVector3 v);  // 0x80090760
    void SetRight(CVector3 v);  // 0x8009077C
    void BuildTrans(CVector3 v);  // 0x80090798
    void BuildRotZ(float angle);  // 0x80090800
    void BuildRotX(float angle);  // 0x800908C0
    void BuildRot(CVector3 axis, float angle);  // 0x80090980
    void BuildScale(CVector3 s);  // 0x80090AA0
    void BuildScale(float s);  // 0x80090AF0
    void operator=(const CMatrix& src);  // 0x80090B34
    static void InitClass();  // 0x80090B78
    void PostMultiply(const CMatrix&);  // 0x8009B91C

    static CMatrix s_TempMat;  // 0x801EEED0
    static u8 s_ClassInit;  // 0x8034EAB0
} __attribute__((aligned(16)));

// Paired-single helpers (matrix.cpp, hand-written assembly)
void PSMultMatrix(float* out, const float* a, const float* b);  // 0x8008E9EC
void PSTransformPoint(float* out, const float* m, const float* v);  // 0x8008EAD0
void PSTransformVector(float* out, const float* m, const float* v);  // 0x8008EB2C
void PSTransformToLocal(float* out, const float* m, const float* v);  // 0x8008EB78

#endif
