// CCamera: the view. Keeps its local-to-world matrix, derives the
// world-to-camera, projection and frustum lazily in AttemptUpdate. Camera
// space has y forward; the world-to-camera matrix swaps y and z.
#include "game/camera.h"
#include "game/Screen.h"
#include "game/ShellMenu.h"
#include "game/bsobject.h"
#include "game/draw_context.h"
#include "game/vector.h"
#include "math.h"

const float CCamera::ASPECT_4x3 = 0.75f;

static inline void NormalizePlane(CPlane& plane) {
    float x = plane.m_normal[0] * plane.m_normal[0];
    float y = plane.m_normal[1] * plane.m_normal[1];
    float z = plane.m_normal[2] * plane.m_normal[2];
    float len = sqrtf(z + (x + y));
    if (len != 0.0f) {
        float inv = 1.0f / len;
        plane.m_normal[0] = plane.m_normal[0] * inv;
        plane.m_normal[1] = plane.m_normal[1] * inv;
        plane.m_normal[2] = plane.m_normal[2] * inv;
    }
}

// A plane through the camera with normal (x, y, z), normalized.
static inline void SetSidePlane(CPlane& plane, float x, float y, float z) {
    plane.m_normal[0] = x;
    plane.m_normal[1] = y;
    plane.m_normal[2] = z;
    NormalizePlane(plane);
    plane.m_d = 0.0f;
}

void CFrustum::Transform(const CMatrix& m) {
    for (int i = 0; i < FRUSTUM_NUM_PLANES; i++) {
        m_planes[i].Transform(m);
        NormalizePlane(m_planes[i]);
    }
}

// Camera-space frustum planes for `camera` on `screen`.
void CFrustum::Set(const CCamera& camera, CScreen& screen, CViewport&) {
    m_planes[0].m_normal[0] = 0.0f;
    m_planes[0].m_normal[1] = -1.0f;
    m_planes[0].m_normal[2] = 0.0f;
    m_planes[0].m_d = -camera.m_near;
    m_planes[1].m_normal[0] = 0.0f;
    m_planes[1].m_normal[1] = 1.0f;
    m_planes[1].m_normal[2] = 0.0f;
    m_planes[1].m_d = camera.m_far;
    SetSidePlane(m_planes[2], -1.0f, -camera.m_xScale, 0.0f);
    SetSidePlane(m_planes[3], 1.0f, -camera.m_xScale, 0.0f);
    SetSidePlane(m_planes[4], 0.0f, -camera.m_yScale, 1.0f);
    SetSidePlane(m_planes[5], 0.0f, -camera.m_yScale, -1.0f);

    // one screen texel (in 1/16 units of a 4096-wide guard band) inside
    float xScale = camera.m_xScale;
    float x = 2047.9375f * (2.0f * xScale) / (float)screen.GetWidth();
    float yScale = camera.m_yScale;
    float y = 2047.9375f * (2.0f * yScale) / (float)screen.GetHeight();
    SetSidePlane(m_planes[6], -1.0f, -x, 0.0f);
    SetSidePlane(m_planes[7], 1.0f, -x, 0.0f);
    SetSidePlane(m_planes[8], 0.0f, -y, 1.0f);
    SetSidePlane(m_planes[9], 0.0f, -y, -1.0f);
}

void CCamera::UpdateWorldToCamera() {
    CMatrix m;
    m = m_localToWorld;
    m.RotateZ(3.1415927f);
    CVector3 right(m.m[0][0], m.m[0][1], m.m[0][2]);
    CVector3 forward(m.m[1][0], m.m[1][1], m.m[1][2]);
    CVector3 up(m.m[2][0], m.m[2][1], m.m[2][2]);
    CVector3 pos(m.m[3][0], m.m[3][1], m.m[3][2]);
    right.x = right.x * -1.0f;
    right.y = right.y * -1.0f;
    right.z = right.z * -1.0f;
    m_worldToCamera.SetRight(CVector3(right.x, up.x, forward.x));
    m_worldToCamera.SetFront(CVector3(right.y, up.y, forward.y));
    m_worldToCamera.SetUp(CVector3(right.z, up.z, forward.z));
    float px = right.x * pos.x + right.y * pos.y + right.z * pos.z;
    float py = up.x * pos.x + up.y * pos.y + up.z * pos.z;
    float pz = forward.x * pos.x + forward.y * pos.y + forward.z * pos.z;
    m_worldToCamera.SetPos(CVector3(-px, -py, -pz));
    m_worldToCameraValid = true;
}

int CCamera::IsPointVisible(CVector3, CDrawContext& ctx) const {
    return ctx.m_2dReject == 0;
}

// Field of view as horizontal and vertical half angles (radians).
void CCamera::SetPerspective(float halfX, float halfY) {
    m_xScale = tan(halfX);
    m_yScale = tan(halfY);
    m_projectionValid = false;
    m_frustumValid = false;
    m_perspective = true;
}

void CCamera::Orthonormalize() {
    m_localToWorld.Orthonormalize();
    m_worldToCameraValid = false;
    m_frustumValid = false;
}

int CCamera::Yaw(float angle) {
    m_localToWorld.RotateZ(angle);
    m_worldToCameraValid = false;
    m_frustumValid = false;
    return 0;
}

void CCamera::Roll(float angle) {
    m_localToWorld.RotateY(angle);
    m_worldToCameraValid = false;
    m_frustumValid = false;
}

void CCamera::Pitch(float angle) {
    m_localToWorld.RotateX(angle);
    m_worldToCameraValid = false;
    m_frustumValid = false;
}

void CCamera::Rotate(CVector3 axis, float angle) {
    m_localToWorld.Rotate(axis, angle);
    m_worldToCameraValid = false;
    m_frustumValid = false;
}

void CCamera::Move(CVector3 delta) {
    m_localToWorld.Translate(delta);
    m_worldToCameraValid = false;
    m_frustumValid = false;
}

void CCamera::SetBasis(CVector3 right, CVector3 front, CVector3 up) {
    m_localToWorld.SetRight(right);
    m_localToWorld.SetFront(front);
    m_localToWorld.SetUp(up);
    m_worldToCameraValid = false;
    m_frustumValid = false;
}

void CCamera::SetPosition(CVector3 pos) {
    m_localToWorld.SetPos(pos);
    m_worldToCameraValid = false;
    m_frustumValid = false;
}

void CCamera::SetTMLocalToWorld(const CMatrix& m) {
    m_localToWorld = m;
    m_worldToCameraValid = false;
    m_frustumValid = false;
}

void CCamera::Transform(const CMatrix& m) {
    m_localToWorld.Multiply(m_localToWorld, m);
    m_worldToCameraValid = false;
    m_frustumValid = false;
}

void CCamera::Reset() {
    m_localToWorld.Ident();
    m_worldToCameraValid = false;
    m_frustumValid = false;
}

void CCamera::PreTransform(const CMatrix& m) {
    m_localToWorld.Multiply(m, m_localToWorld);
    m_worldToCameraValid = false;
    m_frustumValid = false;
}

int CCamera::IsVisible(CDrawContext&) const {
    return 0;
}

int CCamera::IsDrawEnabled() const {
    return 0;
}

void CCamera::GetUpward(CVector3& out) const {
    out = *(const CVector3*)m_localToWorld.m[2];
}

void CCamera::GetForward(CVector3& out) const {
    out = *(const CVector3*)m_localToWorld.m[1];
}

void CCamera::GetRightward(CVector3& out) const {
    out = *(const CVector3*)m_localToWorld.m[0];
}

void CCamera::GetPosition(CVector3& out) const {
    out = *(const CVector3*)m_localToWorld.m[3];
}

void CCamera::GetTMLocalToWorld(CMatrix& out) const {
    out = m_localToWorld;
}

void CCamera::Draw(CDrawContext&) {}

int CCamera::CommitUpdate() {
    return 0;
}

// Rebuilds whatever was invalidated, then keeps the camera's scene extents
// at its position (single player only).
int CCamera::AttemptUpdate(float) {
    if (!m_worldToCameraValid) {
        UpdateWorldToCamera();
    }
    if (!m_projectionValid) {
        if (m_perspective) {
            m_projection.Perspective(m_xScale, m_yScale, m_near, 100.0f * m_far);
        } else {
            m_projection.Orthographic(m_xScale, m_yScale, m_near, m_far);
        }
        m_projectionValid = true;
    }
    if (!m_frustumValid) {
        m_worldToClip.Multiply(m_worldToCamera, m_projection);
        m_frustum.Transform(m_localToWorld);
        m_frustumValid = true;
    }
    if (!g_bInMultiplayerMode) {
        CVector3 pos;
        GetPosition(pos);
        m_extentMinX->m_value = pos.x;
        m_extentMinY->m_value = pos.y;
        m_extentMaxX->m_value = pos.x;
        m_extentMaxY->m_value = pos.y;
    }
    return 0;
}

int CCamera::BeginUpdate(float) {
    return 0;
}

CCamera::~CCamera() {
    if (m_script != NULL) {
        DestroyBSObject(m_script);
        m_script = NULL;
    }
}
