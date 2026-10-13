#include "game/lightvolman.h"

#include <math.h>
#include "game/light.h"

// Output of GetVolume while blending: slot 0 is the ambient light.
static BPDLight g_BlendLights[4] = {
    {1, {0}, {0.0f, 0.0f, 0.0f}, {16.0f, 16.0f, 16.0f}, 1.0f},
    {2, {0}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 1.0f},
    {2, {0}, {0.0f, 0.0f, 1.0f}, {64.0f, 64.0f, 64.0f}, 1.0f},
    {2, {0}, {0.0f, 1.0f, 0.0f}, {128.0f, 128.0f, 128.0f}, 1.0f},
};
static BPDLightVolume g_BlendLightVolume = {{0}, 0, NULL, 4, g_BlendLights};

CLightVolumeManager::CLightVolumeManager() {
    m_volumes[0] = NULL;
    m_volumes[1] = NULL;
    m_volumes[2] = NULL;
    m_volumes[3] = NULL;
    m_blendFrom = NULL;
    m_blendTime = 0.0f;
}

CLightVolumeManager::~CLightVolumeManager() {
    m_volumes[0] = NULL;
    m_volumes[1] = NULL;
    m_volumes[2] = NULL;
    m_volumes[3] = NULL;
    m_blendFrom = NULL;
    m_blendTime = 0.0f;
}

void CLightVolumeManager::Reset() {
    m_volumes[0] = NULL;
    m_volumes[1] = NULL;
    m_volumes[2] = NULL;
    m_volumes[3] = NULL;
    m_blendFrom = NULL;
    m_blendTime = 0.0f;
}

void CLightVolumeManager::Update(float dt) {
    if (m_blendFrom != NULL) {
        m_blendTime -= dt;
        if (m_blendTime <= 0.0f) {
            m_blendFrom = NULL;
        }
    }
}

// A change during a blend back to the previous volume reverses the blend.
inline void CLightVolumeManager::StartBlend(BPDLightVolume* from) {
    if (m_blendFrom != NULL && m_blendFrom == m_volumes[0]) {
        m_blendTime = LIGHT_BLEND_TIME - m_blendTime;
    } else {
        m_blendTime = LIGHT_BLEND_TIME;
    }
    m_blendFrom = from != NULL ? from : CLight::GetDefaultLightVolume();
}

// Ignored when all four slots are in use.
void CLightVolumeManager::AddVolume(BPDLightVolume* volume) {
    if (m_volumes[3] != NULL) {
        return;
    }
    BPDLightVolume* active = m_volumes[0];
    for (int i = 0; i < 4; i++) {
        if (m_volumes[i] == NULL) {
            m_volumes[i] = volume;
            break;
        }
        if (m_volumes[i]->priority < volume->priority) {
            BPDLightVolume* carry = m_volumes[i];
            for (int j = i + 1; j < 4; j++) {
                BPDLightVolume* next = m_volumes[j];
                m_volumes[j] = carry;
                carry = next;
            }
            m_volumes[i] = volume;
            break;
        }
    }
    if (active != m_volumes[0]) {
        StartBlend(active);
    }
}

void CLightVolumeManager::RemoveVolume(BPDLightVolume* volume) {
    BPDLightVolume* active = m_volumes[0];
    for (int i = 0; i < 4; i++) {
        if (m_volumes[i] == volume) {
            m_volumes[i] = NULL;
            for (int j = i + 1; j < 4 && m_volumes[j] != NULL; j++) {
                m_volumes[j - 1] = m_volumes[j];
                m_volumes[j] = NULL;
            }
            if (active != m_volumes[0]) {
                StartBlend(active);
            }
            return;
        }
    }
}

static inline void ClearLight(BPDLight* l) {
    l->intensity = 0.0f;
    l->color[0] = 0.0f;
    l->color[1] = 0.0f;
    l->color[2] = 0.0f;
    l->dir[0] = 0.0f;
    l->dir[1] = 0.0f;
    l->dir[2] = 0.0f;
}

BPDLightVolume* CLightVolumeManager::GetVolume() {
    if (m_blendFrom == NULL) {
        return m_volumes[0] != NULL ? m_volumes[0] : CLight::GetDefaultLightVolume();
    }

    BPDLight* ambient = &g_BlendLights[0];
    ClearLight(&g_BlendLights[0]);
    ClearLight(&g_BlendLights[1]);
    ClearLight(&g_BlendLights[2]);
    ClearLight(&g_BlendLights[3]);

    // The outgoing volume, fading out.
    BPDLight* out = &g_BlendLights[1];
    BPDLight* l = m_blendFrom->lights;
    for (int i = 0; i < m_blendFrom->numLights; i++, l++) {
        float w = (l->intensity * m_blendTime) / LIGHT_BLEND_TIME;
        if (l->type == BPD_LIGHT_AMBIENT) {
            ambient->intensity = 1.0f;
            ambient->color[0] = l->color[0] * w;
            ambient->color[1] = l->color[1] * w;
            ambient->color[2] = l->color[2] * w;
        } else {
            out->intensity = 1.0f;
            out->color[0] = l->color[0] * w;
            out->color[1] = l->color[1] * w;
            out->color[2] = l->color[2] * w;
            out->dir[0] = l->dir[0];
            out->dir[1] = l->dir[1];
            out->dir[2] = l->dir[2];
            out++;
        }
    }

    // The incoming volume, fading in, added on top.
    BPDLightVolume* current = m_volumes[0] != NULL ? m_volumes[0] : CLight::GetDefaultLightVolume();
    BPDLight* dst = &g_BlendLights[1];
    l = current->lights;
    for (int i = 0; i < current->numLights; i++, l++) {
        float w = l->intensity * (1.0f - m_blendTime / LIGHT_BLEND_TIME);
        if (l->type == BPD_LIGHT_AMBIENT) {
            if (ambient->intensity != 0.0f) {
                ambient->color[0] = l->color[0] * w + ambient->color[0];
                ambient->color[1] = l->color[1] * w + ambient->color[1];
                ambient->color[2] = l->color[2] * w + ambient->color[2];
            } else {
                ambient->intensity = 1.0f;
                ambient->color[0] = l->color[0] * w;
                ambient->color[1] = l->color[1] * w;
                ambient->color[2] = l->color[2] * w;
            }
        } else {
            if (dst->intensity != 0.0f) {
                dst->color[0] = l->color[0] * w + dst->color[0];
                dst->color[1] = l->color[1] * w + dst->color[1];
                dst->color[2] = l->color[2] * w + dst->color[2];
                // Interpolate the direction and renormalize.
                float dx = l->dir[0] - dst->dir[0];
                float dy = l->dir[1] - dst->dir[1];
                float dz = l->dir[2] - dst->dir[2];
                dx *= w;
                dy *= w;
                dz *= w;
                dx += dst->dir[0];
                dy += dst->dir[1];
                dz += dst->dir[2];
                float len = sqrtf(dx * dx + dy * dy + dz * dz);
                if (len != 0.0f) {
                    float inv = 1.0f / len;
                    dx *= inv;
                    dy *= inv;
                    dz *= inv;
                }
                dst->dir[0] = dx;
                dst->dir[1] = dy;
                dst->dir[2] = dz;
            } else {
                dst->intensity = 1.0f;
                dst->color[0] = l->color[0] * w;
                dst->color[1] = l->color[1] * w;
                dst->color[2] = l->color[2] * w;
                dst->dir[0] = l->dir[0];
                dst->dir[1] = l->dir[1];
                dst->dir[2] = l->dir[2];
            }
            dst++;
        }
    }
    return &g_BlendLightVolume;
}
