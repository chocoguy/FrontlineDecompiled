// CPlane: normal . p = d. Hand-maintained.
#ifndef _GAME_PLANE_H_
#define _GAME_PLANE_H_

#include "types.h"
#include "game_types.h"
#include "game/vector.h"

class CLine3;
class CMatrix;
class CPlane;
class CVector3;

// size >= 0x14 (from field accesses)
class CPlane {
public:
    /* 0x0 */ f32 m_normal[3];
    /* 0xC */ f32 m_w;  // the normal's CVector3 w (copied along with it)
    /* 0x10 */ f32 m_d;
    /* 0x14 */ u8 unk_0x14[0xC];
    CVector3& Normal() { return *(CVector3*)m_normal; }
    const CVector3& Normal() const { return *(const CVector3*)m_normal; }
    ~CPlane();  // 0x8001ACF0
    CPlane();  // 0x8001AD2C
    // Intersection with the segment: t along it (or +-1 when parallel) and
    // the point; true when the segment crosses the plane.
    bool GetIntersection(const CLine3&, float&, CVector3*) const;  // 0x800910AC
    void Transform(const CMatrix&);  // 0x80091344
    CPlane(CVector3, float);  // 0x80091408
};

#endif
