// CCSGVolume: a union of a hierarchical object's sub volumes. Hand-maintained.
#ifndef _GAME_CSG_VOLUME_H_
#define _GAME_CSG_VOLUME_H_

#include "types.h"
#include "game_types.h"
#include "game/IVolume.h"

class CAnimatedVolume;
class CCDBObject;
class CCSGVolume;
class CCollision;
class CHierObject;
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

// size 0xC; vtable 0x801895F4
class CCSGVolume : public IVolume {
public:
    // Declared first (slot order is fixed by IVolume; this new slot comes last).
    virtual int TestCollision(const CCSGVolume&, CCollision&, bool) const;  // 0x800C80CC
    /* 0x4 */ CHierObject* m_pObject;
    /* 0x8 */ int m_numSubVolumes;
    virtual void TransformedCopy(const IVolume&, const CMatrix&);  // 0x800C7F54
    virtual void GetExtents(CVector3&, CVector3&) const;  // 0x800C7F58
    virtual int Create() const;  // 0x800C807C
    virtual int TestCollision(const CAnimatedVolume&, CCollision&, bool) const;  // 0x800C8210
    virtual int TestCollision(const CWorldVolume&, CCollision&, bool) const;  // 0x800C8354
    virtual int TestCollision(const CVolFiber&, CCollision&, bool) const;  // 0x800C8498
    virtual int TestCollision(const CTriangle&, CCollision&, bool) const;  // 0x800C85DC
    virtual int TestCollision(const CPlane&, CCollision&, bool) const;  // 0x800C8720
    virtual int TestCollision(CVector3, CCollision&, bool) const;  // 0x800C8864
    virtual int TestCollision(const CCDBObject&, CCollision&, bool) const;  // 0x800C89B8
    virtual int TestCollision(const CVolCapsule&, CCollision&, bool) const;  // 0x800C8AFC
    virtual int TestCollision(const CVolSphere&, CCollision&, bool) const;  // 0x800C8C40
    virtual int TestCollision(const CVolBox&, CCollision&, bool) const;  // 0x800C8D84
    virtual int TestCollision(const IVolume&, CCollision&, bool) const;  // 0x800C8EC8
    // Merges the nearest of `count` sub-volume hits into `collision`.
    int CheckContacts(int* hits, float* fractions, CCollision* collisions, int count,
                      CCollision& collision, bool record) const;  // 0x800C8F3C
    virtual int TestCollision(const CLine3&, CCollision&, bool) const;  // 0x800C90BC
    void Init(int numSubVolumes, CHierObject* object);  // 0x800C9200
    virtual ~CCSGVolume();  // 0x800C920C
    CCSGVolume();  // 0x800C9268

private:
    template <class T>
    int TestSubVolumes(const T& volume, CCollision& collision, bool record) const;
};

#endif
