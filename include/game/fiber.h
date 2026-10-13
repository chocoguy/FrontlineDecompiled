// CVolFiber: a line-segment collision volume (rays: bullets, sight lines).
// Every test with a fiber reports only the nearest hit. Hand-maintained.
#ifndef _GAME_FIBER_H_
#define _GAME_FIBER_H_

#include "types.h"
#include "game_types.h"
#include "game/IVolume.h"
#include "game/line.h"

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

// size 0x50; vtable 0x80189654
class CVolFiber : public IVolume {
public:
    // Declared first (slot order is fixed by IVolume; this new slot comes last).
    virtual int TestCollision(const CCSGVolume&, CCollision&, bool) const;  // 0x800C9F68
    /* 0x4 */ u8 unk_0x4[0xC];
    /* 0x10 */ CLine3 m_line;
    /* 0x4C */ u8 unk_0x4C[0x4];
    CVolFiber() {}
    virtual void GetExtents(CVector3&, CVector3&) const;  // 0x800C9290
    virtual bool TestVisibility(CDrawContext&) const;  // 0x800C9440
    virtual int TestCollision(const CTriangle&, CCollision&, bool) const;  // 0x800C9504
    virtual int TestCollision(const CPlane&, CCollision&, bool) const;  // 0x800C951C
    virtual int TestCollision(const CLine3&, CCollision&, bool) const;  // 0x800C9668
    virtual int TestCollision(const CVolFiber&, CCollision&, bool) const;  // 0x800C9B64
    virtual int TestCollision(CVector3, CCollision&, bool) const;  // 0x800C9BA4
    virtual int TestCollision(const CWorldVolume&, CCollision&, bool) const;  // 0x800C9EE4
    virtual int TestCollision(const CAnimatedVolume&, CCollision&, bool) const;  // 0x800C9FEC
    virtual int TestCollision(const CCDBObject&, CCollision&, bool) const;  // 0x800CA070
    virtual int TestCollision(const CVolCapsule&, CCollision&, bool) const;  // 0x800CA0F4
    virtual int TestCollision(const CVolSphere&, CCollision&, bool) const;  // 0x800CA178
    virtual int TestCollision(const CVolBox&, CCollision&, bool) const;  // 0x800CA1FC
    virtual int TestCollision(const IVolume&, CCollision&, bool) const;  // 0x800CA280
    const CLine3& GetLine() const;  // 0x800CA2F4
    void Set(CVector3 start, CVector3 end);  // 0x800CA2FC
    virtual void TransformedCopy(const IVolume&, const CMatrix&);  // 0x800CA374
    virtual int Create() const;  // 0x800CA4C0
    virtual ~CVolFiber();  // 0x800CA504
    CVolFiber(const CLine3&);  // 0x800CA560
    CVolFiber(CVector3, CVector3);  // 0x800CA5CC
    CVolFiber(const CVolFiber&);  // 0x800CA65C
};

#endif
