// CVolSphere: sphere collision volume. Hand-maintained.
#ifndef _GAME_SPHERE_H_
#define _GAME_SPHERE_H_

#include "types.h"
#include "game_types.h"
#include "game/vector.h"
#include "game/IVolume.h"
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

// size 0x20; vtable 0x801896B4
class CVolSphere : public IVolume {
public:
    // Declared first (slot order is fixed by IVolume; this new slot comes last).
    virtual int TestCollision(const CCSGVolume&, CCollision&, bool) const;  // 0x800CB6C4
    CVolSphere() {}
    // Spheres come from the default heap (inlined into Create()).
    static void* operator new(unsigned long size) { return DWI_alloc(NULL, size, 0x400); }
    static void* operator new(unsigned long, void* where) { return where; }
    static void operator delete(void* p) { ::operator delete(p); }
    CVolSphere(const CVector3& center, float radius) : m_radius(radius), m_center(center) {}

    /* 0x4 */ u8 unk_0x4[0x8];
    /* 0xC */ f32 m_radius;
    /* 0x10 */ CVector3 m_center;
    virtual void GetExtents(CVector3&, CVector3&) const;  // 0x800CA6C8
    virtual bool TestVisibility(CDrawContext&) const;  // 0x800CA718
    virtual int TestCollision(const CTriangle&, CCollision&, bool) const;  // 0x800CAB00
    virtual int TestCollision(const CPlane&, CCollision&, bool) const;  // 0x800CAB08
    virtual int TestCollision(const CLine3&, CCollision&, bool) const;  // 0x800CAC48
    virtual int TestCollision(CVector3, CCollision&, bool) const;  // 0x800CB31C
    virtual int TestCollision(const CCDBObject&, CCollision&, bool) const;  // 0x800CB4F0
    virtual int TestCollision(const CVolFiber&, CCollision&, bool) const;  // 0x800CB564
    virtual int TestCollision(const CVolCapsule&, CCollision&, bool) const;  // 0x800CB5DC
    virtual int TestCollision(const CWorldVolume&, CCollision&, bool) const;  // 0x800CB650
    virtual int TestCollision(const CAnimatedVolume&, CCollision&, bool) const;  // 0x800CB738
    virtual int TestCollision(const CVolSphere&, CCollision&, bool) const;  // 0x800CB764
    virtual int TestCollision(const CVolBox&, CCollision&, bool) const;  // 0x800CB940
    virtual int TestCollision(const IVolume&, CCollision&, bool) const;  // 0x800CB9B4
    float GetRadius() const;  // 0x800CBA28
    CVector3 GetCenter() const;  // 0x800CBA30
    void SetRadius(float);  // 0x800CBA44
    void SetCenter(CVector3);  // 0x800CBA4C
    virtual void TransformedCopy(const IVolume&, const CMatrix&);  // 0x800CBA60
    virtual int Create() const;  // 0x800CBABC
    void operator=(const CVolSphere&);  // 0x800CBB08
    virtual ~CVolSphere();  // 0x800CBB24
};

#endif
