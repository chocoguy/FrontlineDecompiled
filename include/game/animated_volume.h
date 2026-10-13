// From tools/gen_headers.py, now hand-maintained.
// Skeleton for game/animated_volume: names, parameter types, virtual order,
// bases and sizes come from the retail binary; fields, return types and
// `static` are inferred from code and need checking.
#ifndef _GAME_ANIMATED_VOLUME_H_
#define _GAME_ANIMATED_VOLUME_H_

#include "types.h"
#include "game_types.h"
#include "game/sphere.h"
#include "game/box.h"
#include "game/location_collision.h"

class CAnimObject;

class CAnimatedVolume;
class CCDBObject;
class CCSGVolume;
class CCollision;
class CLine3;
class CMatrix;
struct CObjLocationCollisionData;
class CObjLocationCollisionMap;
class CPlane;
struct CTriangle;
class CVector3;
class CVolBox;
class CVolCapsule;
class CVolFiber;
class CVolSphere;
class CWorldVolume;
class IVolume;

// A character's hit volume: a bounding sphere around per-bone boxes (from
// the character's location collision data), rebuilt from the bones the
// first time a line test needs them. size 0x7C0; vtable 0x80189480
class CAnimatedVolume : public CVolSphere {
public:
    /* 0x020 */ mutable bool m_boxesValid;
    /* 0x024 */ CObjLocationCollisionData* m_pData;
    /* 0x028 */ CObjLocationCollisionMap* m_pMap;
    /* 0x02C */ u8 unk_0x2C[0x4];
    /* 0x030 */ mutable CVolBox m_boxes[LOCATION_COLLISION_MAX_SHAPES];
    /* 0x7B0 */ CAnimObject* m_pOwner;
    /* 0x7B4 */ u8 unk_0x7B4[0xC];
    virtual void TransformedCopy(const IVolume&, const CMatrix&);  // 0x800BFA2C
    virtual int Create() const;  // 0x800BFA84
    virtual int TestCollision(const CAnimatedVolume&, CCollision&, bool) const;  // 0x800BFB28
    virtual int TestCollision(const CCSGVolume&, CCollision&, bool) const;  // 0x800BFB48
    virtual int TestCollision(const CWorldVolume&, CCollision&, bool) const;  // 0x800BFBBC
    virtual int TestCollision(const CVolFiber&, CCollision&, bool) const;  // 0x800BFC30
    virtual int TestCollision(const CTriangle&, CCollision&, bool) const;  // 0x800BFCA8
    virtual int TestCollision(const CPlane&, CCollision&, bool) const;  // 0x800BFCC8
    virtual int TestCollision(CVector3, CCollision&, bool) const;  // 0x800BFCE8
    virtual int TestCollision(const CCDBObject&, CCollision&, bool) const;  // 0x800BFD1C
    virtual int TestCollision(const CVolCapsule&, CCollision&, bool) const;  // 0x800BFD90
    virtual int TestCollision(const CVolSphere&, CCollision&, bool) const;  // 0x800BFDB0
    virtual int TestCollision(const CVolBox&, CCollision&, bool) const;  // 0x800BFDD0
    virtual int TestCollision(const IVolume&, CCollision&, bool) const;  // 0x800BFDF0
    virtual int TestCollision(const CLine3&, CCollision&, bool) const;  // 0x800BFE64
    void Set(CObjLocationCollisionData*, CObjLocationCollisionMap*);  // 0x800C0208
    virtual ~CAnimatedVolume();  // 0x800C021C
    CAnimatedVolume();  // 0x800C0298
};

#endif
