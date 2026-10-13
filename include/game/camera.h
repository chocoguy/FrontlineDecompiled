#ifndef _GAME_CAMERA_H_
#define _GAME_CAMERA_H_

#include "types.h"
#include "game_types.h"
#include "game/IMovingSceneNode.h"
#include "game/matrix.h"
#include "game/plane.h"

class CCamera;
class CDrawContext;
class CScreen;
class CVector3;
class CViewport;
struct BSObject;

#define FRUSTUM_NUM_PLANES 10

// View frustum: near, far, left, right, top, bottom, then four planes one
// texel inside the screen edges (size 0x140).
class CFrustum {
public:
    /* 0x0 */ CPlane m_planes[FRUSTUM_NUM_PLANES];

    void Transform(const CMatrix&);  // 0x800793E0
    void Set(const CCamera&, CScreen&, CViewport&);  // 0x800794F8
};

// A scene-graph extent entry (sorted per axis); the camera keeps its own
// position in them.
struct CSceneExtentNode {
    /* 0x0 */ u8 unk_0x0[0x8];
    /* 0x8 */ f32 m_value;
};

class CCamera : public IMovingSceneNode {
public:
    /* 0x00C */ CSceneExtentNode* m_extentMinX;
    /* 0x010 */ CSceneExtentNode* m_extentMaxX;
    /* 0x014 */ CSceneExtentNode* m_extentMinY;
    /* 0x018 */ CSceneExtentNode* m_extentMaxY;
    /* 0x01C */ u32 field_0x1C;
    /* 0x020 */ u32 field_0x20;
    /* 0x024 */ f32 m_near;
    /* 0x028 */ f32 m_far;
    /* 0x02C */ f32 m_xScale;  // tan of half the horizontal field of view
    /* 0x030 */ f32 m_yScale;
    /* 0x034 */ u8 unk_0x34[0xC];
    /* 0x040 */ CMatrix m_localToWorld;
    /* 0x080 */ CMatrix m_worldToCamera;
    /* 0x0C0 */ CMatrix m_projection;
    /* 0x100 */ CMatrix m_worldToClip;
    /* 0x140 */ bool m_perspective;
    /* 0x141 */ bool m_worldToCameraValid;
    /* 0x142 */ bool m_projectionValid;
    /* 0x143 */ bool m_frustumValid;
    /* 0x144 */ u8 unk_0x144[0xC];
    /* 0x150 */ CFrustum m_frustum;
    /* 0x290 */ BSObject* m_script;
    /* 0x294 */ u32 field_0x294;

    CCamera() {
        field_0x4 = 0;
        field_0x8 = 0;
        m_extentMinX = NULL;
        m_extentMaxX = NULL;
        m_extentMinY = NULL;
        m_extentMaxY = NULL;
        field_0x1C = 0;
        field_0x20 = 0;
        m_localToWorld.Ident();
        m_worldToCamera.Ident();
        m_worldToCameraValid = true;
        m_projectionValid = false;
        m_frustumValid = false;
        m_script = NULL;
        field_0x294 = 0;
    }
    void UpdateWorldToCamera();  // 0x80079C68
    int IsPointVisible(CVector3, CDrawContext&) const;  // 0x80079E6C
    void SetPerspective(float, float);  // 0x80079E7C
    virtual void Orthonormalize();  // 0x80079EE8
    virtual int Yaw(float);  // 0x80079F24
    virtual void Roll(float);  // 0x80079F60
    virtual void Pitch(float);  // 0x80079F9C
    virtual void Rotate(CVector3, float);  // 0x80079FD8
    virtual void Move(CVector3);  // 0x8007A028
    virtual void SetBasis(CVector3, CVector3, CVector3);  // 0x8007A078
    virtual void SetPosition(CVector3);  // 0x8007A118
    virtual void SetTMLocalToWorld(const CMatrix&);  // 0x8007A168
    virtual void Transform(const CMatrix&);  // 0x8007A1A4
    virtual void Reset();  // 0x8007A1E8
    virtual void PreTransform(const CMatrix&);  // 0x8007A224
    virtual int IsVisible(CDrawContext&) const;  // 0x8007A264
    virtual int IsDrawEnabled() const;  // 0x8007A26C
    virtual void GetUpward(CVector3&) const;  // 0x8007A274
    virtual void GetForward(CVector3&) const;  // 0x8007A298
    virtual void GetRightward(CVector3&) const;  // 0x8007A2BC
    virtual void GetPosition(CVector3&) const;  // 0x8007A2E0
    virtual void GetTMLocalToWorld(CMatrix&) const;  // 0x8007A304
    virtual void Draw(CDrawContext&);  // 0x8007A330
    virtual int CommitUpdate();  // 0x8007A334
    virtual int AttemptUpdate(float);  // 0x8007A338
    virtual int BeginUpdate(float);  // 0x8007A534
    virtual ~CCamera();  // 0x8007A538

    static const float ASPECT_4x3;  // 0x8034FEC8
};

#endif
