// CVolCapsule: capsule (swept sphere) collision volume. Hand-maintained.
#ifndef _GAME_CAPSULE_H_
#define _GAME_CAPSULE_H_

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

// size 0x50; vtable 0x80189594
class CVolCapsule : public IVolume {
public:
    // Declared first (slot order is fixed by IVolume; this new slot comes last).
    virtual int TestCollision(const CCSGVolume&, CCollision&, bool) const;  // 0x800C6824
    /* 0x4 */ u8 unk_0x4[0x8];
    /* 0xC */ f32 m_radius;
    /* 0x10 */ CLine3 m_line;  // the capsule's axis
    /* 0x4C */ u8 unk_0x4C[0x4];
    virtual void GetExtents(CVector3&, CVector3&) const;  // 0x800C555C
    virtual bool TestVisibility(CDrawContext&) const;  // 0x800C568C
    virtual int TestCollision(const CTriangle&, CCollision&, bool) const;  // 0x800C5694
    virtual int TestCollision(const CPlane&, CCollision&, bool) const;  // 0x800C569C
    virtual int TestCollision(const CLine3&, CCollision&, bool) const;  // 0x800C5810
    virtual int TestCollision(CVector3, CCollision&, bool) const;  // 0x800C64E0
    virtual int TestCollision(const CWorldVolume&, CCollision&, bool) const;  // 0x800C67B0
    virtual int TestCollision(const CAnimatedVolume&, CCollision&, bool) const;  // 0x800C6898
    virtual int TestCollision(const CVolFiber&, CCollision&, bool) const;  // 0x800C68C4
    virtual int TestCollision(const CCDBObject&, CCollision&, bool) const;  // 0x800C693C
    virtual int TestCollision(const CVolCapsule&, CCollision&, bool) const;  // 0x800C69B0
    virtual int TestCollision(const CVolSphere&, CCollision&, bool) const;  // 0x800C6ED8
    virtual int TestCollision(const CVolBox&, CCollision&, bool) const;  // 0x800C71EC
    virtual int TestCollision(const IVolume&, CCollision&, bool) const;  // 0x800C7260
    virtual void TransformedCopy(const IVolume&, const CMatrix&);  // 0x800C72D4
    virtual int Create() const;  // 0x800C7428
    virtual ~CVolCapsule();  // 0x800C746C
};


#endif
