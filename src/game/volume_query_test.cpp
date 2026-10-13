// The volume query test itself is compiled out of the retail game; the unit
// kept the out-of-line default implementations of IVolume and the
// dwi::IVisitor<ISceneNode> destructor (and with them both vtables).
#include "game/IVolume.h"
#include "game/vector.h"

class ISceneNode;

IVolume::~IVolume() {}

int IVolume::Create() const {
    return 0;
}

void IVolume::TransformedCopy(const IVolume&, const CMatrix&) {}

int IVolume::TestCollision(const CTriangle&, CCollision&, bool) const {
    return 0;
}

int IVolume::TestCollision(const CPlane&, CCollision&, bool) const {
    return 0;
}

int IVolume::TestCollision(const CLine3&, CCollision&, bool) const {
    return 0;
}

int IVolume::TestCollision(CVector3, CCollision&, bool) const {
    return 0;
}

int IVolume::TestCollision(const CWorldVolume&, CCollision&, bool) const {
    return 0;
}

int IVolume::TestCollision(const CAnimatedVolume&, CCollision&, bool) const {
    return 0;
}

int IVolume::TestCollision(const CVolFiber&, CCollision&, bool) const {
    return 0;
}

int IVolume::TestCollision(const CCDBObject&, CCollision&, bool) const {
    return 0;
}

int IVolume::TestCollision(const CVolCapsule&, CCollision&, bool) const {
    return 0;
}

int IVolume::TestCollision(const CVolSphere&, CCollision&, bool) const {
    return 0;
}

int IVolume::TestCollision(const CVolBox&, CCollision&, bool) const {
    return 0;
}

int IVolume::TestCollision(const IVolume&, CCollision&, bool) const {
    return 0;
}

bool IVolume::TestVisibility(CDrawContext&) const {
    return false;
}

void IVolume::GetExtents(CVector3&, CVector3&) const {}

namespace dwi {
template <class T0>
IVisitor<T0>::~IVisitor() {}
}

template class dwi::IVisitor<ISceneNode>;
