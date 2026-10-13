// From tools/gen_headers.py, now hand-maintained.
// Skeleton for game/IVolume: names, parameter types, virtual order,
// bases and sizes come from the retail binary; fields, return types and
// `static` are inferred from code and need checking.
#ifndef _GAME_IVOLUME_H_
#define _GAME_IVOLUME_H_

#include "types.h"
#include "game_types.h"

class CAnimatedVolume;
class CCDBObject;
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

// size >= 0x4 (from field accesses); vtable 0x80181E28
class IVolume {
public:
    // vtable pointer at 0x0
    IVolume() {}
    virtual ~IVolume();  // 0x8001C078
    virtual int Create() const;  // 0x8001C0C0
    virtual void TransformedCopy(const IVolume&, const CMatrix&);  // 0x8001C0C8
    virtual int TestCollision(const IVolume&, CCollision&, bool) const;  // 0x8001C124
    virtual int TestCollision(const CVolBox&, CCollision&, bool) const;  // 0x8001C11C
    virtual int TestCollision(const CVolSphere&, CCollision&, bool) const;  // 0x8001C114
    virtual int TestCollision(const CVolCapsule&, CCollision&, bool) const;  // 0x8001C10C
    virtual int TestCollision(const CCDBObject&, CCollision&, bool) const;  // 0x8001C104
    virtual int TestCollision(const CVolFiber&, CCollision&, bool) const;  // 0x8001C0FC
    virtual int TestCollision(const CAnimatedVolume&, CCollision&, bool) const;  // 0x8001C0F4
    virtual int TestCollision(const CWorldVolume&, CCollision&, bool) const;  // 0x8001C0EC
    virtual int TestCollision(CVector3, CCollision&, bool) const;  // 0x8001C0E4
    virtual int TestCollision(const CLine3&, CCollision&, bool) const;  // 0x8001C0DC
    virtual int TestCollision(const CPlane&, CCollision&, bool) const;  // 0x8001C0D4
    virtual int TestCollision(const CTriangle&, CCollision&, bool) const;  // 0x8001C0CC
    virtual bool TestVisibility(CDrawContext&) const;  // 0x8001C12C
    virtual void GetExtents(CVector3&, CVector3&) const;  // 0x8001C134
};

#endif
