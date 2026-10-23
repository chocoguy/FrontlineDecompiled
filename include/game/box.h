// CVolBox: oriented box collision volume. Hand-maintained.
#ifndef _GAME_BOX_H_
#define _GAME_BOX_H_

#include "types.h"
#include "game_types.h"
#include "game/IVolume.h"
#include "game/vector.h"
#include "game/memory.h"

class CAnimatedVolume;
class CCDBObject;
class CCSGVolume;
class CCollision;
class CDrawContext;
class CLine3;
class CMatrix;
class CPlane;
struct CTriangle;
class CVector3;
class CVolBox;
class CVolCapsule;
class CVolFiber;
class CVolSphere;
class CWorldVolume;
class IVolume;

// size >= 0x50 (from field accesses); vtable 0x801894F4
class CVolBox : public IVolume {
public:
    // Declared first (slot order is fixed by IVolume; this new slot comes last).
    virtual int TestCollision(const CCSGVolume&, CCollision&, bool) const;  // 0x800C27B0
    /* 0x04 */ f32 m_halfSize[3];  // half extents along the basis axes
    /* 0x10 */ CVector3 m_axisX;
    /* 0x20 */ CVector3 m_axisY;
    /* 0x30 */ CVector3 m_axisZ;
    /* 0x40 */ CVector3 m_center;
    CVolBox() {}  // 0x80074BEC
    // Boxes come from the default heap (inlined into Create()).
    static void* operator new(unsigned long size) { return DWI_alloc(NULL, size, 0x400); }
    static void* operator new(unsigned long, void* where) { return where; }
    static void operator delete(void* p) { ::operator delete(p); }
    void EndianSwap();  // 0x80096B90
    virtual void GetExtents(CVector3&, CVector3&) const;  // 0x800C13B4
    virtual bool TestVisibility(CDrawContext&) const;  // 0x800C14CC
    virtual int TestCollision(const CTriangle&, CCollision&, bool) const;  // 0x800C15E4
    virtual int TestCollision(const CPlane&, CCollision&, bool) const;  // 0x800C15EC
    virtual int TestCollision(const CLine3&, CCollision&, bool) const;  // 0x800C1EE0
    virtual int TestCollision(const CAnimatedVolume&, CCollision&, bool) const;  // 0x800C2630
    virtual int TestCollision(const CVolFiber&, CCollision&, bool) const;  // 0x800C26A4
    virtual int TestCollision(CVector3, CCollision&, bool) const;  // 0x800C271C
    virtual int TestCollision(const CWorldVolume&, CCollision&, bool) const;  // 0x800C2824
    virtual int TestCollision(const CCDBObject&, CCollision&, bool) const;  // 0x800C2898
    virtual int TestCollision(const CVolCapsule&, CCollision&, bool) const;  // 0x800C290C
    virtual int TestCollision(const CVolSphere&, CCollision&, bool) const;  // 0x800C3210
    virtual int TestCollision(const CVolBox&, CCollision&, bool) const;  // 0x800C37D0
    virtual int TestCollision(const IVolume&, CCollision&, bool) const;  // 0x800C50D8
    float GetDepth() const;  // 0x800C514C
    float GetWidth() const;  // 0x800C515C
    void SetCorners(CVector3, CVector3);  // 0x800C516C
    void SetDepth(float);  // 0x800C5258
    void SetHeight(float);  // 0x800C5268
    void SetWidth(float);  // 0x800C5278
    void SetBasis(CVector3, CVector3, CVector3);  // 0x800C5288
    void SetCenter(CVector3);  // 0x800C52BC
    virtual void TransformedCopy(const IVolume&, const CMatrix&);  // 0x800C52D0
    virtual int Create() const;  // 0x800C53A8
    void operator=(const CVolBox&);  // 0x800C53F4
    virtual ~CVolBox();  // 0x800C5458
};

// Squared distance from an infinite line (start + t dir) to the box: t and
// the closest point in box coordinates (Magic Software, Eberly).
float BoxLineSqrDistance(const CLine3& line, const CVolBox& box, float* t, float* b0, float* b1, float* b2);  // 0x800C0320
// Squared distance from a point to the box and the closest point in box
// coordinates.
float BoxPointSqrDistance(const CVector3& point, const CVolBox& box, float* b0, float* b1, float* b2);  // 0x800C11D8

#ifdef FRONTLINE_NATIVE
#include <string.h>
// A box stored with the GameCube layout, in file data (location collision
// boxes, a static mesh's root node): the vtable pointer slot at 0, the half
// sizes at 4, the axes at 0x10, 0x20, 0x30 and the center at 0x40. Natively a
// CVolBox's fields sit after an 8-byte vtable pointer, so code that viewed
// those bytes as a CVolBox gets a copy instead.
inline CVolBox GCVolBox(const void* gc) {
    const u8* p = (const u8*)gc;
    CVolBox box;
    memcpy(box.m_halfSize, p + 0x04, 0xC);
    memcpy(&box.m_axisX, p + 0x10, 0xC);
    memcpy(&box.m_axisY, p + 0x20, 0xC);
    memcpy(&box.m_axisZ, p + 0x30, 0xC);
    memcpy(&box.m_center, p + 0x40, 0xC);
    return box;
}
#endif

#endif
