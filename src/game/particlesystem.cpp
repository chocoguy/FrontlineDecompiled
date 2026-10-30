// Particle systems: emitters placed in the level (property systems) or
// started on demand by id (instanced). Particles are never stored; each
// frame RenderSystem replays every live emission from the emitter's seed,
// drawing camera-facing quads sorted back to front by emitter.
#include "game/particlesystem.h"
#include "game/CFog.h"
#include "game/Moh2.h"
#include "game/ShellMenu.h"
#include "game/box.h"
#include "game/camera.h"
#include "game/color.h"
#include "game/draw_context.h"
#include "game/gameconfig.h"
#include "game/memory.h"
#include "game/plane.h"
#include "game/renderlist.h"
#include "game/scene.h"
#include "game/Screen.h"
#include "math.h"
#include "stdlib.h"
#include <dolphin/gx.h>


#define PARTICLE_POOL_SIZE 0x80
#define PARTICLE_RANDOM_PER_PARTICLE 11
#define PARTICLE_NO_SYSTEM_ID 0x47F5DF95  // "no effect"

static CMatrix g_ModelToScreenMatrix;
static CVector3 g_XAxis;  // camera axes, set each Draw
static CVector3 g_YAxis;
static CVector3 g_ZAxis;
static CMatrix g_WorldToCamera;
static CParticleSystemManager g_ParticleSystemManager;
static CVector3 vCameraForward(0.0f, 0.0f, -1.0f);

static bool g_bAllowRotation = true;
static CRenderList* g_pRenderList;
static CScreen* g_pScreen;
static CScene* g_pScene;
static CDrawContext* g_pContext;
static float g_WidthScale;
static float g_HeightScale;
static bool g_bReady;
static bool g_bPaused;
static CFog* g_pFog;
int g_RenderTypeCache;
static int g_ParticleCounter;
static int g_ParticleCounterMax;
int g_CurrentSeed;

static int s_identInit = (g_ModelToScreenMatrix.Ident(), 0);

static inline int ParticleRand() {
    g_CurrentSeed = g_CurrentSeed * 0x41C64E6D + 0x3039;
    return g_CurrentSeed & 0x7FFF;
}

static inline float ParticleRandUnit() {
    return ParticleRand() * 0.00003051851f;
}

template <class T>
static inline void ResetPool(CParticlePool<T>& pool) {
    pool.m_used = NULL;
    pool.m_count = 0;
    pool.m_free = pool.m_base;
    pool.m_base[pool.m_capacity - 1].next = NULL;
    for (int i = 0; i < pool.m_capacity - 1; i++) {
        pool.m_free[i].next = &pool.m_free[i + 1];
    }
}

template <class T>
static inline void InitPool(CParticlePool<T>& pool, int capacity) {
    pool.m_capacity = capacity;
    pool.m_base = (typename CParticlePool<T>::node*)DWI_alloc(
        NULL, capacity * sizeof(typename CParticlePool<T>::node), 0x400);
    ResetPool(pool);
}

template <class T>
static inline T* AllocFromPool(CParticlePool<T>& pool) {
    typename CParticlePool<T>::node* n = pool.m_free;
    if (n == NULL) {
        return NULL;
    }
    pool.m_free = n->next;
    n->next = pool.m_used;
    pool.m_used = n;
    pool.m_count++;
    return &n->obj;
}

// Updates the systems of `pool`, returning finished ones to the free list.
template <class T>
static inline void UpdatePool(CParticlePool<T>& pool, float dt) {
    typename CParticlePool<T>::node* n = pool.m_used;
    while (n != NULL) {
        if (n->obj.IsActive()) {
            n->obj.UpdateSystem(dt);
            n = n->next;
            continue;
        }
        typename CParticlePool<T>::node* next = n->next;
        typename CParticlePool<T>::node* prev = pool.m_used;
        if (n == prev) {
            pool.m_used = next;
        } else {
            for (; prev != NULL; prev = prev->next) {
                if (prev->next == n) {
                    prev->next = n->next;
                    break;
                }
            }
        }
        pool.m_count--;
        n->next = pool.m_free;
        pool.m_free = n;
        n = next;
    }
}

// Inserts `sys` into the manager's draw list, farthest first.
static inline void InsertSorted(CParticleSystemManager& mgr, CParticleSystem* sys) {
    sys->m_pNext = NULL;
    sys->m_pPrev = NULL;
    CParticleSystem* cur = (CParticleSystem*)mgr.m_pChildren;
    if (cur == NULL) {
        mgr.m_pChildren = sys;
        return;
    }
    if (cur->m_distSq < sys->m_distSq) {
        sys->m_pPrev = cur->m_pPrev;
        if (cur->m_pPrev != NULL) {
            cur->m_pPrev->m_pNext = sys;
        }
        sys->m_pNext = cur;
        cur->m_pPrev = sys;
        mgr.m_pChildren = sys;
        return;
    }
    CParticleSystem* next;
    while ((next = (CParticleSystem*)cur->m_pNext) != NULL && !(sys->m_distSq > next->m_distSq)) {
        cur = next;
    }
    sys->m_pNext = next;
    if (cur->m_pNext != NULL) {
        cur->m_pNext->m_pPrev = sys;
    }
    sys->m_pPrev = cur;
    cur->m_pNext = sys;
}

// Links `bin` after the first texture bin.
static inline void AddTextureBin(CParticleSystemTextureBin* bin) {
    CParticleSystemTextureBin* first = g_ParticleSystemManager.m_pBins;
    if (first != NULL) {
        bin->m_pNext = first->m_pNext;
        if (first->m_pNext != NULL) {
            first->m_pNext->m_pPrev = bin;
        }
        bin->m_pPrev = first;
        first->m_pNext = bin;
    } else {
        g_ParticleSystemManager.m_pBins = bin;
    }
}

static inline CParticleSystemTextureBin* FindTextureBin(ShapeFile* shape) {
    for (CParticleSystemTextureBin* bin = g_ParticleSystemManager.m_pBins; bin != NULL;
         bin = (CParticleSystemTextureBin*)bin->m_pNext) {
        if (bin->m_pShape == shape) {
            return bin;
        }
    }
    return NULL;
}

static inline void Min(CVector3& a, const CVector3& b) {
    a.x = a.x < b.x ? a.x : b.x;
    a.y = a.y < b.y ? a.y : b.y;
    a.z = a.z < b.z ? a.z : b.z;
}

static inline void Max(CVector3& a, const CVector3& b) {
    a.x = a.x > b.x ? a.x : b.x;
    a.y = a.y > b.y ? a.y : b.y;
    a.z = a.z > b.z ? a.z : b.z;
}

// Widens [lo, hi] by the turning point of p(t) = v t + a t^2 inside (0, end).
static inline void AddTurningPoint(float v, float a, float end, float& lo, float& hi) {
    float t = (-0.5f * v) / a;
    if (t > 0.0f && t < end) {
        float p = v * t + t * (a * t);
        if (p < lo) {
            lo = p;
        } else if (p > hi) {
            hi = p;
        }
    }
}

static inline void SetCenter(CParticleSystem* sys) {
    sys->m_center.x = sys->m_boxMax.x - sys->m_boxMin.x;
    sys->m_center.y = sys->m_boxMax.y - sys->m_boxMin.y;
    sys->m_center.z = sys->m_boxMax.z - sys->m_boxMin.z;
    sys->m_center.x = sys->m_center.x * 0.5f;
    sys->m_center.y = sys->m_center.y * 0.5f;
    sys->m_center.z = sys->m_center.z * 0.5f;
    sys->m_center.x = sys->m_center.x + sys->m_boxMin.x;
    sys->m_center.y = sys->m_center.y + sys->m_boxMin.y;
    sys->m_center.z = sys->m_center.z + sys->m_boxMin.z;
}

// Moving: unless both the emitter's velocity and acceleration are ~zero.
static inline void UpdateMoving(CPropertyParticleSystem* sys) {
    bool moving = true;
    if (fabsf(sys->m_sysVel.z * sys->m_sysVel.z +
              (sys->m_sysVel.x * sys->m_sysVel.x + sys->m_sysVel.y * sys->m_sysVel.y)) < 0.000001f &&
        fabsf(sys->m_sysAccel.z * sys->m_sysAccel.z +
              (sys->m_sysAccel.x * sys->m_sysAccel.x + sys->m_sysAccel.y * sys->m_sysAccel.y)) < 0.000001f) {
        moving = false;
    }
    sys->m_moving = moving;
}

// Rotates `v` by `angle` about the unit axis `axis` (Rodrigues).
static inline void RotateAboutAxis(CVector3& out, const CVector3& v, const CVector3& axis, float s, float c) {
    float k = 1.0f - c;
    float m00 = k * axis.x * axis.x + c;
    float m01 = k * axis.x * axis.y - s * axis.z;
    float m02 = s * axis.y + k * axis.x * axis.z;
    float m10 = s * axis.z + k * axis.x * axis.y;
    float m11 = axis.y * (k * axis.y) + c;
    float m12 = axis.z * (k * axis.y) - s * axis.x;
    float m20 = k * axis.x * axis.z - s * axis.y;
    float m21 = s * axis.x + axis.z * (k * axis.y);
    float m22 = axis.z * (k * axis.z) + c;
    out.x = v.x * m00 + v.y * m01 + v.z * m02;
    out.y = v.x * m10 + v.y * m11 + v.z * m12;
    out.z = v.x * m20 + v.y * m21 + v.z * m22;
}

inline CParticleSystem::CParticleSystem() {
    m_pBin = NULL;
    m_stopTime = 0.0f;
    m_active = true;
    m_emitting = true;
    m_bboxValid = false;
    m_moving = false;
    m_hasRotation = false;
    m_startTime = g_ParticleSystemManager.m_time;
    m_emitTime = g_ParticleSystemManager.m_time;
    m_time = g_ParticleSystemManager.m_time;
}

void CParticleSystem::SetSystemAcceleration(CVector3) {}

void CParticleSystem::SetFogEnable(bool) {}

void CParticleSystem::SetParticleAcceleration(CVector3) {}

void CParticleSystem::SetParticleVelocity(CVector3, CVector3) {}

void CParticleSystem::SetParticlePosition(CVector3, CVector3) {}

void CParticleSystem::SetParticleSize(CVector3, CVector3, float) {}

void CParticleSystem::SetParticleColor(CColor, CColor) {}

void CParticleSystem::SetParticleLifetime(float) {}

void CParticleSystem::SetSystemLifetime(float) {}

void CParticleSystem::SetEmmisionDelay(float) {}

void CParticleSystem::SetEmmisionRate(float) {}

void CParticleSystem::SetTexture(ShapeFile*) {}

void CParticleSystem::SetRenderType(CParticleSystem::ERenderType) {}

int CParticleSystem::IsEmitting() const {
    return m_emitting;
}

CInstancedParticleSystem::~CInstancedParticleSystem() {}

CParticleSystem::~CParticleSystem() {}

void CParticleSystem::InitClass(CRenderList* renderList, CScreen* screen, CScene* scene) {
    g_bReady = true;
    g_pRenderList = renderList;
    g_pScreen = screen;
    g_pScene = scene;
    g_WidthScale = 0.5f * screen->GetWidth();
    g_HeightScale = 0.5f * g_pScreen->GetHeight();
    renderList->Register(g_ParticleSystemManager);
    scene->Add(g_ParticleSystemManager);
    InitPool(g_ParticleSystemManager.m_property, PARTICLE_POOL_SIZE);
    InitPool(g_ParticleSystemManager.m_instanced, PARTICLE_POOL_SIZE);
}

// Drops every system and rejoins the scene.
void CParticleSystem::Reset() {
    g_ParticleSystemManager.m_sceneLinks[1] = 0;
    g_ParticleSystemManager.m_sceneLinks[0] = 0;
    g_pScene->Add(g_ParticleSystemManager);
    ResetPool(g_ParticleSystemManager.m_property);
    ResetPool(g_ParticleSystemManager.m_instanced);
}

// Adds a texture bin for `shape` (once).
void CParticleSystem::Register(ShapeFile* shape) {
    if (FindTextureBin(shape) != NULL) {
        DebugMsg("CParticleSystemManager::Register():  ALREADY REGISTERED");
        return;
    }
    AddTextureBin(new CParticleSystemTextureBin(shape));
}

// Sets the level's particle table (for Create(u32)).
void CParticleSystem::Register(void* particles, int count) {
    g_ParticleSystemManager.m_pParticles = (MOH_particle_Struct*)particles;
    g_ParticleSystemManager.m_numParticles = count;
}

// Starts an instance of the level system with this id, or returns NULL.
CParticleSystem* CParticleSystem::Create(u32 id) {
    if (id == PARTICLE_NO_SYSTEM_ID) {
        return NULL;
    }
    MOH_particle_Struct* info = g_ParticleSystemManager.m_pParticles;
    for (int i = 0; i < g_ParticleSystemManager.m_numParticles; i++, info++) {
        if (id == info->id) {
            CInstancedParticleSystem* sys = AllocFromPool(g_ParticleSystemManager.m_instanced);
            if (sys == NULL) {
                return NULL;
            }
            new (sys) CInstancedParticleSystem(&g_ParticleSystemManager.m_pParticles[i]);
            return sys;
        }
    }
    return NULL;
}

// Starts a level-placed system: places it from its orientation and position
// and converts its particle acceleration to system space.
CParticleSystem* CParticleSystem::Create(void* data) {
    CPropertyParticleSystem* sys = AllocFromPool(g_ParticleSystemManager.m_property);
    if (sys == NULL) {
        return NULL;
    }
    new (sys) CPropertyParticleSystem((MOH_particle_Struct*)data);
    sys->m_pInfo->orient.GetMatrix(sys->m_localToWorld);
    MOH_particle_Struct* info = sys->m_pInfo;
    sys->m_localToWorld.SetPos(CVector3(info->pos[0], info->pos[1], info->pos[2]));
    CMatrix worldToLocal;
    worldToLocal.FastInverse(sys->m_localToWorld);
    CVector3 accel;
    sys->GetParticleAcceleration(accel);
    worldToLocal.TransformVector(accel, accel);
    sys->m_pInfo->accel[0] = accel.x;
    sys->m_pInfo->accel[1] = accel.y;
    sys->m_pInfo->accel[2] = accel.z;
    return sys;
}

ShapeFile* CParticleSystem::GetTexture() const {
    return m_pBin->m_pShape;
}

// Advances the system: drops emissions whose particles have all died
// (skipping their random numbers), stops emitting after the system lifetime
// and finishes once the last particles have died.
void CParticleSystem::UpdateSystem(float dt) {
    float lifeFrames = 60.0f * GetParticleLifetime();
    m_time += dt;
    float lastEmit = m_emitTime;
    if (m_time - lastEmit > lifeFrames) {
        int delay = GetEmmisionDelay();
        m_emitTime = m_emitTime + (float)(delay * ((int)dt / delay + 1));
        int expired = (int)(m_emitTime - lastEmit) / delay;
        expired *= GetEmmisionRate();
        int seed = GetSeed();
        g_CurrentSeed = seed;
        for (int i = 0; i < expired; i++) {
            for (int j = 0; j < PARTICLE_RANDOM_PER_PARTICLE; j++) {
                seed = ParticleRand();
            }
        }
        SetSeed(seed);
    }
    if (m_emitting) {
        float systemLifetime = GetSystemLifetime();
        if (systemLifetime != 0.0f &&
            g_ParticleSystemManager.m_time >= m_startTime + 60.0f * systemLifetime) {
            DeActivate();
        }
    } else if (g_ParticleSystemManager.m_time > m_stopTime + lifeFrames) {
        Terminate();
    }
    UpdateBoundingBox();
}

void CParticleSystem::DeActivate() {
    m_emitting = false;
    m_stopTime = g_ParticleSystemManager.m_time;
}

void CParticleSystem::Terminate() {
    m_active = false;
    m_emitting = false;
}

int CParticleSystem::IsBoundingBoxValid() const {
    return m_bboxValid;
}

int CParticleSystem::IsMoving() const {
    return m_moving;
}

// Bounds of the particles around the emitter (computed once), then, for a
// moving emitter, those bounds swept along its path since the oldest live
// emission.
void CParticleSystem::UpdateBoundingBox() {
    if (!IsBoundingBoxValid()) {
        m_boxMin.x = 3.4028235e38f;
        m_boxMin.y = 3.4028235e38f;
        m_boxMin.z = 3.4028235e38f;
        m_boxMax.x = -3.4028235e38f;
        m_boxMax.y = -3.4028235e38f;
        m_boxMax.z = -3.4028235e38f;
        float life = GetParticleLifetime();
        CVector3 vel0;
        CVector3 vel1;
        GetParticleVelocity(vel0, vel1);
        CVector3 accel;
        GetParticleAcceleration(accel);

        // where the extreme velocities end up
        float life2 = life * life;
        float ax = life2 * accel.x;
        float ay = life2 * accel.y;
        float az = life2 * accel.z;
        CVector3 end0(life * vel0.x + ax, life * vel0.y + ay, life * vel0.z + az);
        CVector3 end1(life * vel1.x + ax, life * vel1.y + ay, life * vel1.z + az);
        Min(m_boxMin, end0);
        Max(m_boxMax, end1);

        // and how far they turn back on the way
        CVector3 lo(0.0f, 0.0f, 0.0f);
        CVector3 hi(0.0f, 0.0f, 0.0f);
        if (fabsf(accel.x) > 0.00001f) {
            AddTurningPoint(vel0.x, accel.x, life, lo.x, hi.x);
            AddTurningPoint(vel1.x, accel.x, life, lo.x, hi.x);
        }
        if (fabsf(accel.y) > 0.00001f) {
            AddTurningPoint(vel0.y, accel.y, life, lo.y, hi.y);
            AddTurningPoint(vel1.y, accel.y, life, lo.y, hi.y);
        }
        if (fabsf(accel.z) > 0.00001f) {
            AddTurningPoint(vel0.z, accel.z, life, lo.z, hi.z);
            AddTurningPoint(vel1.z, accel.z, life, lo.z, hi.z);
        }
        Min(m_boxMin, lo);
        Max(m_boxMax, hi);

        CVector3 pos0;
        CVector3 pos1;
        GetParticlePosition(pos0, pos1);
        m_boxMin.x = m_boxMin.x + pos0.x;
        m_boxMin.y = m_boxMin.y + pos0.y;
        m_boxMin.z = m_boxMin.z + pos0.z;
        m_boxMax.x = m_boxMax.x + pos1.x;
        m_boxMax.y = m_boxMax.y + pos1.y;
        m_boxMax.z = m_boxMax.z + pos1.z;

        CVector3 size0;
        CVector3 size1;
        float growth;
        GetParticleSize(size0, size1, growth);
        float radius = size1.x > size1.y ? 0.5f * size1.x : 0.5f * size1.y;
        if (growth > 1.0f) {
            radius *= growth;
        }
        m_boxMin.x = m_boxMin.x - radius;
        m_boxMin.y = m_boxMin.y - radius;
        m_boxMin.z = m_boxMin.z - radius;
        m_boxMax.x = m_boxMax.x + radius;
        m_boxMax.y = m_boxMax.y + radius;
        m_boxMax.z = m_boxMax.z + radius;
        m_localMin = m_boxMin;
        m_localMax = m_boxMax;
        SetCenter(this);
        ValidateBoundingBox(true);
    }

    if (IsMoving()) {
        CVector3 vel;
        GetSystemInitialVelocity(vel);
        CVector3 accel;
        GetSystemAcceleration(accel);
        float t0 = (m_emitTime - m_startTime) / 60.0f;
        float twoT0 = 2.0f * t0;
        float t02 = t0 * t0;
        float t1 = (g_ParticleSystemManager.m_time - m_emitTime) / 60.0f;
        float t12 = t1 * t1;

        // emitter position at the oldest emission and now
        CVector3 p0(t0 * vel.x + t02 * accel.x, t0 * vel.y + t02 * accel.y, t0 * vel.z + t02 * accel.z);
        CVector3 v0(twoT0 * accel.x + vel.x, twoT0 * accel.y + vel.y, twoT0 * accel.z + vel.z);
        CVector3 p1(t1 * v0.x + p0.x + t12 * accel.x, t1 * v0.y + p0.y + t12 * accel.y,
                    t1 * v0.z + p0.z + t12 * accel.z);
        CVector3 lo = p0;
        CVector3 hi = p0;
        Min(lo, p1);
        Max(hi, p1);
        CVector3 turnLo = lo;
        CVector3 turnHi = hi;
        if (fabsf(accel.x) > 0.00001f) {
            AddTurningPoint(v0.x, accel.x, t1, turnLo.x, turnHi.x);
        }
        if (fabsf(accel.y) > 0.00001f) {
            AddTurningPoint(v0.y, accel.y, t1, turnLo.y, turnHi.y);
        }
        if (fabsf(accel.z) > 0.00001f) {
            AddTurningPoint(v0.z, accel.z, t1, turnLo.z, turnHi.z);
        }
        Min(lo, turnLo);
        Max(hi, turnHi);
        m_boxMin.x = lo.x + m_localMin.x;
        m_boxMin.y = lo.y + m_localMin.y;
        m_boxMin.z = lo.z + m_localMin.z;
        m_boxMax.x = hi.x + m_localMax.x;
        m_boxMax.y = hi.y + m_localMax.y;
        m_boxMax.z = hi.z + m_localMax.z;
        SetCenter(this);
    }
}

// Whether an oriented box is inside the camera's six frustum planes.
int IsBoxVisible(const CVolBox& box, CDrawContext& ctx) {
    CVector3 axisX = box.m_axisX;
    CVector3 axisY = box.m_axisY;
    CVector3 axisZ = box.m_axisZ;
    CVector3 center = box.m_center;
    const CPlane* plane = ((CCamera*)ctx.m_pCamera)->m_frustum.m_planes;
    float ex = box.m_halfSize[0];
    float ey = box.m_halfSize[1];
    float ez = box.m_halfSize[2];
    CVector3 x(ex * axisX.x, ex * axisX.y, ex * axisX.z);
    CVector3 y(ey * axisY.x, ey * axisY.y, ey * axisY.z);
    CVector3 z(ez * axisZ.x, ez * axisZ.y, ez * axisZ.z);
    for (int i = 0; i < 6; i++, plane++) {
        float nx = plane->m_normal[0];
        float ny = plane->m_normal[1];
        float nz = plane->m_normal[2];
        float dist = nz * center.z + (nx * center.x + ny * center.y) - plane->m_d;
        float radius = fabsf(z.z * nz + (z.x * nx + z.y * ny)) +
                       (fabsf(x.z * nz + (x.x * nx + x.y * ny)) + fabsf(y.z * nz + (y.x * nx + y.y * ny)));
        if (dist > radius) {
            return 0;
        }
    }
    return 1;
}

// Frustum test of the bounds in world space; also records the distance from
// the camera for sorting.
int CParticleSystem::IsVisible(CDrawContext& ctx) {
    if (!IsActive()) {
        return 0;
    }
    CVolBox box;
    CMatrix localToWorld;
    GetLocalToWorld(localToWorld);
    box.SetBasis(CVector3(1.0f, 0.0f, 0.0f), CVector3(0.0f, 1.0f, 0.0f), CVector3(0.0f, 0.0f, 1.0f));
    box.SetCorners(m_boxMin, m_boxMax);
    box.TransformedCopy(box, localToWorld);
    int visible = IsBoxVisible(box, ctx);
    CVector3 center;
    localToWorld.TransformPoint(center, m_center);
    CVector3 camera;
    ctx.m_pCamera->GetPosition(camera);
    float dx = center.x - camera.x;
    float dy = center.y - camera.y;
    float dz = center.z - camera.z;
    m_distSq = dz * dz + (dx * dx + dy * dy);
    return visible;
}

// Takes min/max of each component pair so `lo` <= `hi`.
static inline void SortRange(f32* lo, f32* hi) {
    CVector3 a(lo[0], lo[1], lo[2]);
    CVector3 b(hi[0], hi[1], hi[2]);
    lo[0] = a.x < b.x ? a.x : b.x;
    lo[1] = a.y < b.y ? a.y : b.y;
    lo[2] = a.z < b.z ? a.z : b.z;
    hi[0] = a.x > b.x ? a.x : b.x;
    hi[1] = a.y > b.y ? a.y : b.y;
    hi[2] = a.z > b.z ? a.z : b.z;
}

// Places the system from its level data, converts its particle acceleration
// to system space (in the shared data) and finds its texture bin.
CPropertyParticleSystem::CPropertyParticleSystem(MOH_particle_Struct* info) {
    m_sysVel.x = 0.0f;
    m_sysVel.y = 0.0f;
    m_sysVel.z = 0.0f;
    m_sysAccel.x = 0.0f;
    m_sysAccel.y = 0.0f;
    m_sysAccel.z = 0.0f;
    m_pInfo = info;
    bool rotates = false;
    if (m_pInfo->rotationMin != 0.0f || m_pInfo->rotationMax != 0.0f) {
        rotates = true;
    }
    m_hasRotation = rotates;
    info->orient.GetMatrix(m_localToWorld);
    m_localToWorld.SetPos(CVector3(info->pos[0], info->pos[1], info->pos[2]));
    CMatrix worldToLocal;
    worldToLocal.FastInverse(m_localToWorld);
    CVector3 accel;
    GetParticleAcceleration(accel);
    worldToLocal.TransformVector(accel, accel);
    m_pInfo->accel[0] = accel.x;
    m_pInfo->accel[1] = accel.y;
    m_pInfo->accel[2] = accel.z;
    SortRange(m_pInfo->posMin, m_pInfo->posMax);
    SortRange(m_pInfo->velMin, m_pInfo->velMax);
    m_pBin = FindTextureBin(m_pInfo->texture);
}

CPropertyParticleSystem::~CPropertyParticleSystem() {}

int CPropertyParticleSystem::GetSeed() const {
    return m_pInfo->seed;
}

int CPropertyParticleSystem::GetRenderType() const {
    return m_pInfo->renderType;
}

int CPropertyParticleSystem::GetEmmisionRate() const {
    return m_pInfo->emissionRate;
}

int CPropertyParticleSystem::GetEmmisionDelay() const {
    return m_pInfo->emissionDelay;
}

float CPropertyParticleSystem::GetSystemLifetime() const {
    return m_pInfo->systemLifetime;
}

float CPropertyParticleSystem::GetParticleLifetime() const {
    return m_pInfo->particleLifetime;
}

void CPropertyParticleSystem::GetLocalToWorld(CMatrix& out) const {
    out = m_localToWorld;
}

void CPropertyParticleSystem::GetSystemInitialVelocity(CVector3& out) const {
    out = m_sysVel;
}

void CPropertyParticleSystem::GetSystemAcceleration(CVector3& out) const {
    out = m_sysAccel;
}

void CPropertyParticleSystem::GetParticleRotation(float& min, float& max) const {
    min = m_pInfo->rotationMin;
    max = m_pInfo->rotationMax;
}

void CPropertyParticleSystem::GetParticleAlpha(float& start, float& startMax, float& end) const {
    start = m_pInfo->startColorMin[3];
    startMax = m_pInfo->endColorMax[3];
    end = m_pInfo->endColorMin[3];
}

int CPropertyParticleSystem::GetFogEnable() const {
    return (m_pInfo->flags & 1) == 0;
}

void CPropertyParticleSystem::GetParticleColor(CColor& startMin, CColor& startMax, CColor& endMin,
                                               CColor& endMax) const {
    startMin.r = m_pInfo->startColorMin[0];
    startMin.g = m_pInfo->startColorMin[1];
    startMin.b = m_pInfo->startColorMin[2];
    startMin.a = m_pInfo->startColorMin[3];
    startMax.r = m_pInfo->startColorMax[0];
    startMax.g = m_pInfo->startColorMax[1];
    startMax.b = m_pInfo->startColorMax[2];
    startMax.a = m_pInfo->endColorMax[0];
    endMin.r = m_pInfo->endColorMin[0];
    endMin.g = m_pInfo->endColorMin[1];
    endMin.b = m_pInfo->endColorMin[2];
    endMin.a = m_pInfo->endColorMin[3];
    endMax.r = m_pInfo->endColorMax[0];
    endMax.g = m_pInfo->endColorMax[1];
    endMax.b = m_pInfo->endColorMax[2];
    endMax.a = m_pInfo->endColorMax[3];
}

void CPropertyParticleSystem::GetParticleSize(CVector3& min, CVector3& max, float& growth) const {
    min.x = m_pInfo->widthMin;
    min.y = m_pInfo->heightMin;
    min.z = 0.0f;
    max.x = m_pInfo->widthMax;
    max.y = m_pInfo->heightMax;
    max.z = 0.0f;
    growth = m_pInfo->growth;
}

void CPropertyParticleSystem::GetParticlePosition(CVector3& min, CVector3& max) const {
    min.x = m_pInfo->posMin[0];
    min.y = m_pInfo->posMin[1];
    min.z = m_pInfo->posMin[2];
    max.x = m_pInfo->posMax[0];
    max.y = m_pInfo->posMax[1];
    max.z = m_pInfo->posMax[2];
}

void CPropertyParticleSystem::GetParticleVelocity(CVector3& min, CVector3& max) const {
    min.x = m_pInfo->velMin[0];
    min.y = m_pInfo->velMin[1];
    min.z = m_pInfo->velMin[2];
    max.x = m_pInfo->velMax[0];
    max.y = m_pInfo->velMax[1];
    max.z = m_pInfo->velMax[2];
}

void CPropertyParticleSystem::GetParticleAcceleration(CVector3& out) const {
    out.x = m_pInfo->accel[0];
    out.y = m_pInfo->accel[1];
    out.z = m_pInfo->accel[2];
}

void CPropertyParticleSystem::SetSeed(int seed) {
    m_pInfo->seed = seed;
}

void CPropertyParticleSystem::SetLocalToWorld(const CMatrix&) {}

void CPropertyParticleSystem::SetSystemInitialVelocity(CVector3 vel) {
    m_sysVel = vel;
    ValidateBoundingBox(false);
    UpdateMoving(this);
}

void CParticleSystem::ValidateBoundingBox(bool valid) {
    m_bboxValid = valid;
}

void CPropertyParticleSystem::SetSystemAcceleration(CVector3 accel) {
    m_sysAccel = accel;
    ValidateBoundingBox(false);
    UpdateMoving(this);
}

int CInstancedParticleSystem::GetSeed() const {
    return m_seed;
}

void CInstancedParticleSystem::GetParticleAcceleration(CVector3& out) const {
    out = m_accel;
}

void CInstancedParticleSystem::SetSeed(int seed) {
    m_seed = seed;
}

// Moves the system; its vectors are kept in system space.
void CInstancedParticleSystem::SetLocalToWorld(const CMatrix& m) {
    m_localToWorld = m;
    CMatrix worldToLocal;
    worldToLocal.FastInverse(m_localToWorld);
    worldToLocal.TransformVector(m_accel, m_accel);
    worldToLocal.TransformVector(m_sysAccel, m_sysAccel);
    worldToLocal.TransformVector(m_sysVel, m_sysVel);
}

CParticleSystemTextureBin::~CParticleSystemTextureBin() {
    m_pShape = NULL;
}

void CParticleSystemManager::Init() {
    field_0xC = 0;
    m_count = 0;
    for (CRenderBin* bin = m_pChildren; bin != NULL; bin = bin->m_pNext) {
        bin->Init();
    }
    m_pChildren = NULL;
}

bool CParticleSystemManager::IsUsed() {
    return m_pChildren != NULL;
}

// Picks up the camera, then queues the visible systems far to near.
void CParticleSystemManager::Draw(CDrawContext& ctx) {
    g_pContext = &ctx;
    ctx.m_pCamera->GetRightward(g_XAxis);
    ctx.m_pCamera->GetUpward(g_YAxis);
    ctx.m_pCamera->GetForward(g_ZAxis);
    g_WorldToCamera = *ctx.m_pView;
    g_pFog = (CFog*)ctx.field_0xD8;
    CParticlePool<CInstancedParticleSystem>::node* i;
    for (i = m_instanced.m_used; i != NULL; i = i->next) {
        if (i->obj.IsVisible(ctx)) {
            InsertSorted(*this, &i->obj);
        }
    }
    CParticlePool<CPropertyParticleSystem>::node* p;
    for (p = m_property.m_used; p != NULL; p = p->next) {
        if (p->obj.IsVisible(ctx)) {
            InsertSorted(*this, &p->obj);
        }
    }
    for (CRenderBin* bin = m_pChildren; bin != NULL; bin = bin->m_pNext) {
        g_pRenderList->Render(*bin, NULL);
    }
    g_bPaused = true;
}

int CParticleSystemManager::IsVisible(CDrawContext&) const {
    return 1;
}

int CParticleSystemManager::IsDrawEnabled() const {
    return g_config.noParticles == 0;
}

int CParticleSystem::IsActive() const {
    return m_active;
}

// Advances time and every system; finished systems go back to their pool.
int CParticleSystemManager::BeginUpdate(float dt) {
    g_bPaused = dt > 0.0f;
    m_time += dt;
    UpdatePool(m_instanced, dt);
    UpdatePool(m_property, dt);
    return 0;
}

int CParticleSystem::HasRotation() const {
    return m_hasRotation;
}

// Draws every live particle as a camera-facing quad. Each emission (every
// `delay` frames) made `rate` particles; each particle takes 11 random
// numbers from the replayed seed: start offset, velocity, colour mix, size
// and spin. Particles fade out with distance when fogged.
int CParticleSystem::RenderSystem(CDmaPacket&) {
    u8 rotate = HasRotation() & g_bAllowRotation;
    int renderType = GetRenderType();
    if (renderType != g_RenderTypeCache) {
        g_RenderTypeCache = renderType;
        switch (renderType) {
        case 0:
            GXSetBlendMode(1, 4, 1, 5);
            break;
        case 1:
            GXSetBlendMode(3, 1, 1, 5);
            break;
        default:
            GXSetBlendMode(1, 4, 5, 5);
            break;
        }
    }
    GXSetCullMode(0);

    CMatrix localToWorld;
    GetLocalToWorld(localToWorld);
    float now = g_ParticleSystemManager.m_time;
    int delay = GetEmmisionDelay();
    int rate = GetEmmisionRate();
    float lifetime = GetParticleLifetime();
    float lifeFrames = 60.0f * lifetime;
    float lastEmit = m_time;
    if (!m_emitting && lastEmit > m_stopTime) {
        lastEmit = m_stopTime;
    }
    if (g_Shell.Get_currentMission() == 6 && !g_bInMultiplayerMode) {
        rate >>= 1;
    }

    CColor startMin(0, 0, 0, 0x80);
    CColor startMax(0, 0, 0, 0x80);
    CColor endMin(0, 0, 0, 0x80);
    CColor endMax(0, 0, 0, 0x80);
    int seed = GetSeed();
    CVector3 size0;
    CVector3 size1;
    float growth;
    GetParticleSize(size0, size1, growth);
    CVector3 pos0;
    CVector3 pos1;
    GetParticlePosition(pos0, pos1);
    CVector3 vel0;
    CVector3 vel1;
    GetParticleVelocity(vel0, vel1);
    CVector3 sysVel;
    GetSystemInitialVelocity(sysVel);
    CVector3 sysAccel;
    GetSystemAcceleration(sysAccel);
    CVector3 accel;
    GetParticleAcceleration(accel);
    GetParticleColor(startMin, startMax, endMin, endMax);
    float rot0;
    float rot1;
    GetParticleRotation(rot0, rot1);
    float alphaStart;
    float alphaStartMax;
    float alphaEnd;
    GetParticleAlpha(alphaStart, alphaStartMax, alphaEnd);

    if (g_pFog->m_enabled != 0) {
        float far = 2.0f * g_pFog->m_range;
        float near = (9.0f * far) / 10.0f;
        float near2 = near * near;
        float far2 = far * far;
        const float* pos = localToWorld.m[3];
        const CMatrix& w = g_WorldToCamera;
        float cz = (w.m[3][2] + (pos[2] * w.m[2][2] + (pos[0] * w.m[0][2] + pos[1] * w.m[1][2]))) * -1.0f;
        float cx = (w.m[3][0] + (pos[2] * w.m[2][0] + (pos[0] * w.m[0][0] + pos[1] * w.m[1][0]))) * -1.0f;
        float d2 = cx * cx + cz * cz;
        if (d2 > near2) {
            float fade;
            if (d2 < far2) {
                fade = 1.0f - (d2 - near2) / (far2 - near2);
            } else {
                fade = 0.0f;
            }
            if (fade == 0.0f) {
                return 1;
            }
            alphaStart *= fade;
            alphaStartMax *= fade;
            alphaEnd *= fade;
        }
    }

    float turns = 3.1415927f * (2.0f * lifetime);
    rot0 = rot0 * turns;
    rot1 = rot1 * turns;
    float rotRange = rot1 - rot0;
    size0.y = size0.y * 0.7f;
    size1.y = size1.y * 0.7f;
    growth -= 1.0f;
    float widthRange = size1.x - size0.x;
    float heightRange = size1.y - size0.y;
    CVector3 posRange(pos1.x - pos0.x, pos1.y - pos0.y, pos1.z - pos0.z);
    CVector3 velRange(vel1.x - vel0.x, vel1.y - vel0.y, vel1.z - vel0.z);

    // colour at birth: startMin..startMax; at death: endMin..endMax
    float start[4] = {startMin.r, startMin.g, startMin.b, alphaStart};
    float startRange[4] = {(float)startMax.r - start[0], (float)startMax.g - start[1],
                           (float)startMax.b - start[2], alphaStartMax - alphaStart};
    float end[4] = {endMin.r, endMin.g, endMin.b, alphaEnd};
    float endRange[4] = {(float)endMax.r - end[0], (float)endMax.g - end[1], (float)endMax.b - end[2],
                         alphaEnd - alphaEnd};

    // the oldest live emission first
    float t = (now - m_emitTime) / lifeFrames;  // 0..1 through a particle's life
    float age = (now - m_emitTime) / 60.0f;     // seconds
    float tStep = -(float)delay / lifeFrames;
    float ageStep = -(float)delay / 60.0f;
    float sysAge = (m_emitTime - m_startTime) / 60.0f;  // emitter age at emission
    float sysAgeStep = (float)delay / 60.0f;

    CMatrix toView = g_WorldToCamera;
    toView.Multiply(localToWorld, toView);
    float mtx[3][4];
    toView.GetGAMECUBEMatrix34(mtx);
    GXLoadPosMtxImm(mtx, 3);
    GXSetCurrentMtx(3);

    // camera axes in system space
    CVector3 x = g_XAxis;
    CVector3 y = g_YAxis;
    CVector3 z = g_ZAxis;
    const float(*l)[4] = localToWorld.m;
    CVector3 right(x.z * l[0][2] + (x.x * l[0][0] + x.y * l[0][1]),
                   x.z * l[1][2] + (x.x * l[1][0] + x.y * l[1][1]),
                   x.z * l[2][2] + (x.x * l[2][0] + x.y * l[2][1]));
    CVector3 up(y.z * l[0][2] + (y.x * l[0][0] + y.y * l[0][1]),
                y.z * l[1][2] + (y.x * l[1][0] + y.y * l[1][1]),
                y.z * l[2][2] + (y.x * l[2][0] + y.y * l[2][1]));
    CVector3 fwd(z.z * l[0][2] + (z.x * l[0][0] + z.y * l[0][1]),
                 z.z * l[1][2] + (z.x * l[1][0] + z.y * l[1][1]),
                 z.z * l[2][2] + (z.x * l[2][0] + z.y * l[2][1]));

    g_CurrentSeed = seed;
    for (int frame = (int)m_emitTime; (float)frame <= lastEmit; frame += delay) {
        float sysAge2 = sysAge * sysAge;
        float age2 = age * age;
        CVector3 sysPos(sysAge * sysVel.x + sysAge2 * sysAccel.x, sysAge * sysVel.y + sysAge2 * sysAccel.y,
                        sysAge * sysVel.z + sysAge2 * sysAccel.z);
        CVector3 accelOffset(age2 * accel.x, age2 * accel.y, age2 * accel.z);
        float halfScale = 0.5f * (growth * t + 1.0f);
        GXBegin(0x80, 0, rate * 4);
        float tInv = 1.0f - t;
        for (int i = 0; i < rate; i++) {
            float px = posRange.x * ParticleRandUnit() + pos0.x;
            float py = posRange.y * ParticleRandUnit() + pos0.y;
            float pz = posRange.z * ParticleRandUnit() + pos0.z;
            float vx = velRange.x * ParticleRandUnit() + vel0.x;
            float vy = velRange.y * ParticleRandUnit() + vel0.y;
            float vz = velRange.z * ParticleRandUnit() + vel0.z;
            float mixStart = ParticleRandUnit();
            float mixEnd = ParticleRandUnit();
            float color[4];
            for (int c = 0; c < 4; c++) {
                color[c] = tInv * (startRange[c] * mixStart + start[c]) + t * (endRange[c] * mixEnd + end[c]);
            }
            float width = widthRange * ParticleRandUnit() + size0.x;
            float height = heightRange * ParticleRandUnit() + size0.y;
            float angle = t * (rotRange * ParticleRandUnit() + rot0);

            CVector3 center(sysPos.x + (px + (vx * age + accelOffset.x)),
                            sysPos.y + (py + (vy * age + accelOffset.y)),
                            sysPos.z + (pz + (vz * age + accelOffset.z)));
            CVector3 r;
            CVector3 u;
            if (rotate == 0) {
                float w = halfScale * width;
                float h = halfScale * height;
                r.x = right.x * w;
                r.y = right.y * w;
                r.z = right.z * w;
                u.x = up.x * h;
                u.y = up.y * h;
                u.z = up.z * h;
            } else {
                float s = sin(angle);
                float c = cos(angle);
                float w = halfScale * width;
                float h = halfScale * height;
                CVector3 rr;
                CVector3 ru;
                RotateAboutAxis(rr, right, fwd, s, c);
                RotateAboutAxis(ru, up, fwd, s, c);
                r.x = w * rr.x;
                r.y = w * rr.y;
                r.z = w * rr.z;
                u.x = h * ru.x;
                u.y = h * ru.y;
                u.z = h * ru.z;
            }
            u8 cr = color[0];
            u8 cg = color[1];
            u8 cb = color[2];
            u8 ca = color[3];
            GXPosition3f32(center.x - r.x + u.x, center.y - r.y + u.y, center.z - r.z + u.z);
            GXColor4u8(cr, cg, cb, ca);
            GXTexCoord2f32(0.0f, 0.0f);
            GXPosition3f32(center.x + r.x + u.x, center.y + r.y + u.y, center.z + r.z + u.z);
            GXColor4u8(cr, cg, cb, ca);
            GXTexCoord2f32(1.0f, 0.0f);
            GXPosition3f32(center.x + r.x - u.x, center.y + r.y - u.y, center.z + r.z - u.z);
            GXColor4u8(cr, cg, cb, ca);
            GXTexCoord2f32(1.0f, 1.0f);
            GXPosition3f32(center.x - r.x - u.x, center.y - r.y - u.y, center.z - r.z - u.z);
            GXColor4u8(cr, cg, cb, ca);
            GXTexCoord2f32(0.0f, 1.0f);
            g_ParticleCounter++;
        }
        age += ageStep;
        t += tStep;
        sysAge += sysAgeStep;
    }
    GXSetCurrentMtx(0);
    return 1;
}

int CPropertyParticleSystem::Render(CDmaPacket&, void*) {
    return 0;
}

// Loads the texture unless the previous system drawn used the same one.
void CPropertyParticleSystem::Link(CDmaTag*, CDmaPacket& packet) {
    bool load = true;
    CPropertyParticleSystem* prev = (CPropertyParticleSystem*)m_pPrev;
    if (prev != NULL && m_pBin == prev->m_pBin) {
        load = false;
    }
    if (load) {
        m_pBin->m_texture.Load(packet, NULL);
    }
    RenderSystem(packet);
}

// Shared GX state, then every queued system; logs the particle count every
// ten frames.
void CParticleSystemManager::Link(CDmaTag*, CDmaPacket& packet) {
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xB, 1);
    GXSetVtxDesc(0xD, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 0xB, 1, 5, 0);
    GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);
    GXSetNumChans(1);
    GXSetNumTexGens(1);
    GXSetTexCoordGen2(0, 1, 4, 0x3C, 0, 0x7D);
    GXSetNumTevStages(1);
    GXSetZMode(1, 3, 0);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetTevColorIn(0, 0xF, 8, 0xA, 0xF);
    GXSetTevColorOp(0, 0, 0, 1, 1, 0);
    GXSetTevAlphaIn(0, 7, 4, 5, 7);
    GXSetTevAlphaOp(0, 0, 0, 2, 1, 0);
    GXSetChanCtrl(4, 0, 0, 1, 0, 0, 2);
    g_RenderTypeCache = -1;
    g_ParticleCounter = 0;
    for (CRenderBin* bin = m_pChildren; bin != NULL; bin = bin->m_pNext) {
        bin->Link(NULL, packet);
    }
    static int counter = 10;
    if (counter-- <= 0) {
        counter = 10;
        DebugMsg("# Particles = %d (MAX %d)\n", g_ParticleCounter, g_ParticleCounterMax);
    }
    if (g_ParticleCounter > g_ParticleCounterMax) {
        g_ParticleCounterMax = g_ParticleCounter;
    }
    m_pChildren = NULL;
}
