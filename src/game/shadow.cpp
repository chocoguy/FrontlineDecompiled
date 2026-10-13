#include "game/shadow.h"

#include <math.h>
#include "game/ISceneNode.h"
#include "game/compartment.h"
#include "game/draw_context.h"
#include "game/gameconfig.h"
#include "game/texture.h"
#include "game/toilet.h"

// Tunables; overridden from the level's resource 0xD when present.
static float g_FadeDistance = 30.0f;      // shadows vanish beyond this camera distance
static float g_ShadowAlpha = 0.25f;
static float g_ProjectionDistance = 3.0f;  // height of the projection box
static float g_ProjectionHeight = 1.5f;
static float g_ShadowScale = 1.5f;         // box footprint relative to the character's
static float g_zDeltaMax = 1.0f;           // camera-relative heights for fading out
static float g_zDeltaMin;

static CTexture g_texture;
static void* g_pImageData;

static inline float ResourceFloat(LevelFileContentsStruct_* res, int field) {
    return *(float*)&res->fields[field].value;
}

void CCharacterShadow::InitClass() {}

CCharacterShadow::CCharacterShadow() {
    m_enabled = true;
    m_pOwner = NULL;
    m_alphaScale = 1.0f;
    m_alpha = 0.0f;
    if (g_pImageData == NULL) {
        LevelFileContentsStruct_* res = TLT_FindNextResourceByType((TLTResourceID)0xB, NULL);
        g_pImageData = TLT_LoadFileFromLevelBigFile((const char*)res->fields[50].value, NULL);
        res = TLT_FindNextResourceByType((TLTResourceID)0xD, NULL);
        if (res->fields[65].value != 0) {
            g_pImageData = TLT_LoadFileFromLevelBigFile((const char*)res->fields[66].value, NULL);
            g_FadeDistance = ResourceFloat(res, 67);
            g_ShadowAlpha = ResourceFloat(res, 68);
            g_ProjectionDistance = ResourceFloat(res, 69);
            g_ShadowScale = ResourceFloat(res, 70);
            g_ProjectionHeight = ResourceFloat(res, 71);
            g_zDeltaMin = ResourceFloat(res, 72);
            g_zDeltaMax = ResourceFloat(res, 73);
        }
        if (g_pImageData != NULL) {
            g_texture.Set((ShapeFile*)g_pImageData, false);
            CCompartment::SetShadowTexture(g_texture);
        }
    }
}

CCharacterShadow::~CCharacterShadow() {}

void CCharacterShadow::Draw(CDrawContext& ctx) {
    if (g_config.noCharacterShadows || !IsDrawEnabled()) {
        return;
    }
    m_box = *(const CVolBox*)m_pOwner->GetWorldBoundingVolume((ISceneNode::EVolumeType)3);

    CVector3 pos = m_box.m_center;
    CVector3 camPos;
    ctx.m_pCamera->GetPosition(camPos);
    float dx = pos.x - camPos.x;
    float dy = pos.y - camPos.y;
    float dz0 = pos.z - camPos.z;
    float dist = sqrtf(dx * dx + dy * dy + dz0 * dz0);
    if (dist >= g_FadeDistance) {
        return;
    }
    m_alpha = m_alphaScale * (g_ShadowAlpha * (1.0f - dist / g_FadeDistance));

    // Flatten the box into a slab under the character.
    float halfHeight = m_box.m_halfSize[2];
    m_box.SetWidth(g_ShadowScale * m_box.GetWidth());
    m_box.SetDepth(g_ShadowScale * m_box.GetDepth());
    m_box.SetHeight(g_ProjectionDistance);
    pos.z = g_ProjectionHeight + ((pos.z - halfHeight) - 0.5f * g_ProjectionDistance);
    m_box.SetCenter(pos);

    float dz = (pos.z - 0.5f * g_ProjectionDistance) - camPos.z;
    if (dz > g_zDeltaMax) {
        return;
    }
    if (dz > g_zDeltaMin) {
        m_alpha = m_alpha * ((g_zDeltaMax - dz) / (g_zDeltaMax - g_zDeltaMin));
    }
    if (m_box.TestVisibility(ctx)) {
        CShadowDrawList* list = ctx.m_pShadows;
        list->items[list->count++] = this;
    }
}
