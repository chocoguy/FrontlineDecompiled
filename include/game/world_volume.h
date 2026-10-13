// CWorldVolume. Hand-maintained.
#ifndef _GAME_WORLD_VOLUME_H_
#define _GAME_WORLD_VOLUME_H_

#include "types.h"
#include "game_types.h"
#include "game/IVolume.h"
#include "game/vector.h"
#include "dwi/list.h"

class CAnimatedVolume;
class CCDBObject;
class CCSGVolume;
class CCollision;
class CLine3;
class CPlane;
struct CTriangle;
class CVector3;
class CVolBox;
class CVolCapsule;
class CVolFiber;
class CVolSphere;
class CWorldVolume;
class IVolume;

// The level's collision: a list of collision-database objects tested in
// turn. size 0x4C; vtable 0x80189714
class CWorldVolume : public IVolume {
public:
    // Declared first (slot order is fixed by IVolume; this new slot comes last).
    virtual int TestCollision(const CCSGVolume&, CCollision&, bool) const;  // 0x800CBBCC
    /* 0x04 */ dwi::list<const CCDBObject*> m_subVolumes;
    /* 0x24 */ u8 unk_0x24[0x4];
    /* 0x28 */ CVector3 m_min;  // extents of all sub volumes
    /* 0x38 */ CVector3 m_max;
    /* 0x48 */ u32 field_0x48;
    virtual int TestCollision(const CTriangle&, CCollision&, bool) const;  // 0x800CBB80
    virtual int TestCollision(const CPlane&, CCollision&, bool) const;  // 0x800CBB88
    virtual int TestCollision(CVector3, CCollision&, bool) const;  // 0x800CBB90
    virtual int TestCollision(const CWorldVolume&, CCollision&, bool) const;  // 0x800CBB98
    virtual int TestCollision(const CCDBObject&, CCollision&, bool) const;  // 0x800CBBA0
    virtual void GetExtents(CVector3&, CVector3&) const;  // 0x800CBBA8
    virtual int TestCollision(const CAnimatedVolume&, CCollision&, bool) const;  // 0x800CBC40
    virtual int TestCollision(const CVolFiber&, CCollision&, bool) const;  // 0x800CBC60
    virtual int TestCollision(const CLine3&, CCollision&, bool) const;  // 0x800CBC80
    virtual int TestCollision(const CVolCapsule&, CCollision&, bool) const;  // 0x800CBCA0
    virtual int TestCollision(const CVolSphere&, CCollision&, bool) const;  // 0x800CBCC0
    virtual int TestCollision(const CVolBox&, CCollision&, bool) const;  // 0x800CBCE0
    virtual int TestCollision(const IVolume&, CCollision&, bool) const;  // 0x800CBD00
    void RemoveSubVolume(dwi::list_iter<const CCDBObject*>);  // 0x800CBD74
    void RemoveSubVolume(const CCDBObject&);  // 0x800CBF1C
    void AddSubVolume(const CCDBObject&);  // 0x800CBF6C
    virtual ~CWorldVolume();  // 0x800CC124
    CWorldVolume(int capacity);  // 0x800CC29C

private:
    void UpdateExtents();
    template <class T>
    int TestCollisionWithSubVolumes(const T& volume, CCollision& collision, bool record) const;
};

#endif
