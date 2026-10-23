#include "game/bullet.h"
#include "game/matrix.h"

// FLT_MAX
#define BIG_FLOAT 3.4028235e38f

int CBullet::IsDrawEnabled() const {
    return 1;
}

void CBullet::GetRightward(CVector3& v) const {
    v = m_rightward;
}

void CBullet::GetUp(CVector3& v) const {
    v = m_up;
}

void CBullet::GetTMLocalToWorld(CMatrix& tm) const {
    tm = *(const CMatrix*)&m_rightward;
}

const CBullet* CBullet::AsBullet() const {
    return this;
}

CBullet* CBullet::AsBullet() {
    return this;
}

void CBullet::Draw(CDrawContext&) {}

void CBullet::Shutdown() {
    field_0x90 = 0.0f;
    m_collisionId = -1;
    field_0x94 = BIG_FLOAT;
}

int CBullet::Init() {
    ((CMatrix*)&m_rightward)->Ident();
    field_0x90 = 0.0f;
    m_collisionId = -1;
    field_0x94 = BIG_FLOAT;
    field_0x8 = 0;
    field_0x4 = 0;
    END_RETURN_NOTHING
}

CBullet::~CBullet() {}

CBullet::CBullet() {
    ClearSceneNodeData();
    // (The original's matrix member constructor.)
    if (!CMatrix::s_ClassInit) {
        CMatrix::InitClass();
    }
    field_0x28 = 0;
}
