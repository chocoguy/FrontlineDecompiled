// CLight: dynamic point lights, plus the default level lights (an ambient and
// three directional lights) and the pools of animated lights placed in the
// level data (property lights) or created on demand by id (instanced).
#include "game/light.h"
#include "game/memory.h"
#include "game/scene.h"
#include "game/sphere.h"
#include "math.h"

#define ANIMLIGHT_POOL_SIZE 10

BPDLight g_DefaultLights[4] = {
    {BPD_LIGHT_AMBIENT, {0}, {0.0f, 0.0f, 0.0f}, {16.0f, 16.0f, 16.0f}, 1.0f, {0}},
    {BPD_LIGHT_DIRECTIONAL, {0}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 1.0f, {0}},
    {BPD_LIGHT_DIRECTIONAL, {0}, {0.0f, 0.0f, 1.0f}, {64.0f, 64.0f, 64.0f}, 1.0f, {0}},
    {BPD_LIGHT_DIRECTIONAL, {0}, {0.0f, 1.0f, 0.0f}, {128.0f, 128.0f, 128.0f}, 1.0f, {0}},
};

BPDLightVolume g_DefaultLightVolume = {{0}, 0, NULL, 4, g_DefaultLights, 0, {0}};

static CAnimLightManager g_AnimLightManager;
static bool g_bReady;

// Chains every node of the pool onto its free list.
template <class T>
static inline void ResetPool(CAnimLightPool<T>& pool) {
    pool.m_used = NULL;
    pool.m_count = 0;
    pool.m_free = pool.m_base;
    pool.m_base[pool.m_capacity - 1].next = NULL;
    for (int i = 0; i < pool.m_capacity - 1; i++) {
        pool.m_free[i].next = &pool.m_free[i + 1];
    }
}

template <class T>
static inline void InitPool(CAnimLightPool<T>& pool, int capacity) {
    pool.m_capacity = capacity;
    pool.m_base = (typename CAnimLightPool<T>::node*)DWI_alloc(
        NULL, capacity * sizeof(typename CAnimLightPool<T>::node), 0x400);
    ResetPool(pool);
}

// Takes a node off the free list onto the used list, or NULL when full.
template <class T>
static inline T* AllocFromPool(CAnimLightPool<T>& pool) {
    typename CAnimLightPool<T>::node* n = pool.m_free;
    if (n == NULL) {
        return NULL;
    }
    pool.m_free = n->next;
    n->next = pool.m_used;
    pool.m_used = n;
    pool.m_count++;
    return &n->obj;
}

template <class T>
static inline void ReturnToPool(CAnimLightPool<T>& pool, T* obj) {
    typename CAnimLightPool<T>::node* node = (typename CAnimLightPool<T>::node*)obj;
    typename CAnimLightPool<T>::node* n = pool.m_used;
    if (node == n) {
        pool.m_used = node->next;
    } else {
        for (; n != NULL; n = n->next) {
            if (n->next == node) {
                n->next = node->next;
                break;
            }
        }
    }
    pool.m_count--;
    node->next = pool.m_free;
    pool.m_free = node;
}

void CLight::InitClass(CScene* scene) {
    g_AnimLightManager.m_pScene = scene;
    InitPool(g_AnimLightManager.m_property, ANIMLIGHT_POOL_SIZE);
    InitPool(g_AnimLightManager.m_instanced, ANIMLIGHT_POOL_SIZE);
    g_bReady = true;
}

// Takes every animated light out of the scene and empties the pools.
void CLight::ResetClass() {
    CAnimLightPool<CInstancedAnimLight>::node* i;
    for (i = g_AnimLightManager.m_instanced.m_used; i != NULL; i = i->next) {
        g_AnimLightManager.m_pScene->Remove(i->obj);
    }
    CAnimLightPool<CPropertyAnimLight>::node* p;
    for (p = g_AnimLightManager.m_property.m_used; p != NULL; p = p->next) {
        g_AnimLightManager.m_pScene->Remove(p->obj);
    }
    ResetPool(g_AnimLightManager.m_property);
    ResetPool(g_AnimLightManager.m_instanced);
}

// Sets the level's animated-light table (for Create(u32)).
void CLight::Register(void* lights, int count) {
    g_AnimLightManager.m_pLights = (MOH_animatedLight_Struct*)lights;
    g_AnimLightManager.m_numLights = count;
}

CLight* CLight::Create(u32 id) {
    return g_AnimLightManager.Create(id);
}

CLight* CLight::Create(void* info) {
    return g_AnimLightManager.Create((MOH_animatedLight_Struct*)info);
}

void CLight::Destroy() {}

void CLight::MarkForDestruction(int arg) {
    if (g_scene.IsNodeInScene(this)) {
        g_scene.Remove(*this);
    }
    ISubject::MarkForDestruction(arg);
}

BPDLightVolume* CLight::GetDefaultLightVolume() {
    return &g_DefaultLightVolume;
}

// Sets default light `index`: its direction (normalized) and colour, with the
// colour's luma boosted by half (via YCbCr) and scaled to 0..1.
void CLight::SetDefaultLight(CLight::EDefaultLights index, CVector3 dir, CColor color, float intensity) {
    float r = color.r;
    float g = color.g;
    float b = color.b;
    float y = (float)(0.114 * b + (0.299 * r + 0.587 * g)) * 1.5f;
    float cb = (float)(128.0 + (0.5 * b + (-0.1687 * r - 0.3313 * g))) - 128.0f;
    float cr = (float)(128.0 + ((0.5 * r - 0.4187 * g) - 0.0813 * b)) - 128.0f;

    float len = dir.z * dir.z + (dir.x * dir.x + dir.y * dir.y);
    if (len > 0.0f) {
        len = sqrtf(len);
    }
    if (len != 0.0f) {
        float inv = 1.0f / len;
        dir.x = dir.x * inv;
        dir.y = dir.y * inv;
        dir.z = dir.z * inv;
    }

    BPDLight& light = g_DefaultLights[index];
    light.dir[0] = dir.x;
    light.dir[1] = dir.y;
    light.dir[2] = dir.z;
    light.color[0] = (float)(1.402 * cr + y) / 255.0f;
    light.color[1] = (float)((y - 0.34414 * cb) - 0.71414 * cr) / 255.0f;
    light.color[2] = (float)(1.772 * cb + y) / 255.0f;
    light.intensity = intensity;
}

CLight::~CLight() {}

int CLight::BeginUpdate(float) {
    return 0;
}

int CLight::AttemptUpdate(float) {
    return 0;
}

int CLight::CommitUpdate() {
    return 0;
}

int CLight::IsVisible(CDrawContext& ctx) const {
    CVector3 pos;
    GetPosition(pos);
    CVolSphere sphere(pos, m_radius);
    return sphere.TestVisibility(ctx);
}

void CLight::Reset() {
    m_localToWorld.Ident();
}

void CLight::PreTransform(const CMatrix& m) {
    m_localToWorld.Multiply(m, m_localToWorld);
}

void CLight::Transform(const CMatrix& m) {
    m_localToWorld.Multiply(m_localToWorld, m);
}

void CLight::SetTMLocalToWorld(const CMatrix& m) {
    m_localToWorld = m;
}

void CLight::SetPosition(CVector3 pos) {
    m_localToWorld.SetPos(pos);
}

void CLight::SetBasis(CVector3 right, CVector3 front, CVector3 up) {
    m_localToWorld.SetRight(right);
    m_localToWorld.SetFront(front);
    m_localToWorld.SetUp(up);
}

void CLight::Move(CVector3 delta) {
    m_localToWorld.Translate(delta);
}

void CLight::Rotate(CVector3 axis, float angle) {
    m_localToWorld.Rotate(axis, angle);
}

void CLight::Pitch(float angle) {
    m_localToWorld.RotateX(angle);
}

void CLight::Roll(float angle) {
    m_localToWorld.RotateY(angle);
}

int CLight::Yaw(float angle) {
    m_localToWorld.RotateZ(angle);
    return 0;
}

void CLight::Orthonormalize() {
    m_localToWorld.Orthonormalize();
}

void CLight::GetTMLocalToWorld(CMatrix& out) const {
    out = m_localToWorld;
}

void CLight::GetPosition(CVector3& out) const {
    out = *(const CVector3*)m_localToWorld.m[3];
}

void CLight::GetRightward(CVector3& out) const {
    out = *(const CVector3*)m_localToWorld.m[0];
}

void CLight::GetForward(CVector3& out) const {
    out = *(const CVector3*)m_localToWorld.m[1];
}

void CLight::GetUpward(CVector3& out) const {
    out = *(const CVector3*)m_localToWorld.m[2];
}

// Places the light at its level position with its radius (1/16 units).
CPropertyAnimLight::CPropertyAnimLight(MOH_animatedLight_Struct* info) {
    m_pInfo = info;
    m_frame = 0;
    m_reverse = false;
    SetPosition(CVector3(info->pos[0], info->pos[1], info->pos[2]));
    m_radius = info->radius * 0.0625f;
    GXInitLightDistAttn(&m_lightObj, 0.5f * m_radius, 0.15f, 2);
}

CPropertyAnimLight::~CPropertyAnimLight() {}

void CPropertyAnimLight::Destroy() {
    ReturnToPool(g_AnimLightManager.m_property, this);
}

// Advances the animation by `dt` frames (play once then destroy, loop, or
// ping-pong) and sets the colour from the frame's rgba times its intensity.
int CPropertyAnimLight::BeginUpdate(float dt) {
    int frames = dt;
    switch (m_pInfo->mode) {
    case ANIMLIGHT_ONCE:
        m_frame += frames;
        if (m_frame >= m_pInfo->numFrames) {
            m_frame = m_pInfo->numFrames - 1;
            MarkForDestruction(0);
        }
        break;
    case ANIMLIGHT_LOOP:
        m_frame += frames;
        while (m_frame >= m_pInfo->numFrames) {
            m_frame -= m_pInfo->numFrames;
        }
        break;
    case ANIMLIGHT_PINGPONG:
        for (int i = frames; i != 0; i--) {
            if (m_reverse) {
                m_frame--;
                if (m_frame <= 0) {
                    m_frame = 1;
                    m_reverse = false;
                }
            } else {
                m_frame++;
                if (m_frame >= m_pInfo->numFrames) {
                    m_frame = m_pInfo->numFrames - 2;
                    m_reverse = true;
                }
            }
        }
        break;
    }
    u8* rgba = m_pInfo->colors[m_frame];
    float intensity = m_pInfo->intensities[m_frame];
    m_color[0] = rgba[0] * intensity;
    m_color[1] = rgba[1] * intensity;
    m_color[2] = rgba[2] * intensity;
    m_color[3] = rgba[3] * intensity;
    return 0;
}

CInstancedAnimLight::CInstancedAnimLight(MOH_animatedLight_Struct* info) : CPropertyAnimLight(info) {}

void CInstancedAnimLight::Destroy() {
    ReturnToPool(g_AnimLightManager.m_instanced, this);
}

// Starts an instance of the level light with this id, or returns NULL.
CInstancedAnimLight* CAnimLightManager::Create(u32 id) {
    for (int i = 0; i < m_numLights; i++) {
        if (id == m_pLights[i].id) {
            CInstancedAnimLight* light = AllocFromPool(m_instanced);
            if (light == NULL) {
                return NULL;
            }
            new (light) CInstancedAnimLight(&m_pLights[i]);
            m_pScene->Add(*light);
            return light;
        }
    }
    return NULL;
}

CPropertyAnimLight* CAnimLightManager::Create(MOH_animatedLight_Struct* info) {
    CPropertyAnimLight* light = AllocFromPool(m_property);
    if (light == NULL) {
        return NULL;
    }
    new (light) CPropertyAnimLight(info);
    m_pScene->Add(*light);
    return light;
}
