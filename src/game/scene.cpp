// CScene: the scene graph, its update with collision sub-stepping, and
// drawing.
#include "game/scene.h"

#ifdef FRONTLINE_NATIVE
#include <stddef.h>
#endif

#include "game/ISceneNode.h"
#include "game/IMovingSceneNode.h"
#include "game/IVolume.h"
#include "game/collision.h"
#include "game/cdbobject.h"
#include "game/player.h"
#include "game/static_obj.h"
#include "game/anim_obj.h"
#include "game/hierobject.h"
#include "game/world_object.h"
#include "game/bullet.h"
#include "game/light.h"
#include "game/camera.h"
#include "game/viewport.h"
#include "game/Screen.h"
#include "game/CFog.h"
#include "game/plane.h"
#include "game/draw_context.h"
#include "game/gcscratchpad.h"
#include "game/bstypes.h"
#include "game/bsevents.h"
#include "game/bsobject.h"
#include "game/Moh2.h"
#include "game/ShellMenu.h"

extern const float TIME_STEP_EPSILON = 0.015625f;
static const float MAX_TIME_STEP = 1.0f;

extern const u8 g_theCollisionTable[435];  // clsn_table.cpp

#define BIG_FLOAT 3.4028235e38f

// The scene's words at the start of every node (see CSceneNodeData).
struct SSceneNodeView {
    /* 0x00 */ void* m_vtable;
    /* 0x04 */ PTR32(ISceneNode) m_prev;  // the nodes keep these in u32 words
    /* 0x08 */ PTR32(ISceneNode) m_next;
    /* 0x0C */ ISceneNode::SExtent* m_minX;
    /* 0x10 */ ISceneNode::SExtent* m_maxX;
    /* 0x14 */ ISceneNode::SExtent* m_minY;
    /* 0x18 */ ISceneNode::SExtent* m_maxY;
};

static inline SSceneNodeView* View(const ISceneNode* node) {
    return (SSceneNodeView*)node;
}

// The static, animated-object and light lists link through these words.
#ifdef FRONTLINE_NATIVE
// Natively at the members' own offsets (lights keep theirs where animated
// objects do).
struct SStaticLinks {
    u8 unk_0x0[offsetof(CStaticObject, m_pPrevStatic)];
    CStaticObject* m_prev;
    CStaticObject* m_next;
};
struct SAnimLinks {
    u8 unk_0x0[offsetof(CAnimObject, m_pPrevAnim)];
    void* m_prev;
    void* m_next;
};
static_assert(offsetof(SStaticLinks, m_next) == offsetof(CStaticObject, m_pNextStatic), "static links");
static_assert(offsetof(SAnimLinks, m_next) == offsetof(CAnimObject, m_pNextAnim), "anim links");
static_assert(offsetof(SAnimLinks, m_prev) == offsetof(CLight, field_0x24), "light links");
#else
struct SStaticLinks {
    /* 0x00 */ u8 unk_0x0[0x28];
    /* 0x28 */ CStaticObject* m_prev;
    /* 0x2C */ CStaticObject* m_next;
};
struct SAnimLinks {
    /* 0x00 */ u8 unk_0x0[0x24];
    /* 0x24 */ void* m_prev;
    /* 0x28 */ void* m_next;
};
#endif

// Exclusions end: players touching the pair fall again.
static inline void WakePlayers(const CScene::SExclusionEntry& e) {
    CPlayerObject* a = (CPlayerObject*)e.m_a->AsPlayerObject();
    CPlayerObject* b = (CPlayerObject*)e.m_b->AsPlayerObject();
    if (a != NULL) {
        a->m_gravity = true;
    }
    if (b != NULL) {
        b->m_gravity = true;
    }
}

static inline void EraseExclusion(dwi::fast_vec<CScene::SExclusionEntry>& v, CScene::SExclusionEntry* e) {
    CScene::SExclusionEntry* end = v.end();
    for (CScene::SExclusionEntry* from = e + 1; from != end; from++, e++) {
        e->m_a = from->m_a;
        e->m_b = from->m_b;
        e->m_time = from->m_time;
    }
    v.m_size--;
}

static inline void DestroyCollisions(dwi::vector<CCollision>& v) {
    for (CCollision* c = v.begin(); c != v.end(); c++) {
        c->~CCollision();
    }
    v.m_size = 0;
}

static inline void PushCollision(dwi::vector<CCollision>& v, const CCollision& c) {
    CCollision* slot = v.end();
    if (slot != NULL) {
        new (slot) CCollision(c);
    }
    v.m_size++;
}

static inline bool PairLess(const CScene::UNodePair& a, const CScene::UNodePair& b) {
    if ((uptr)(a.m_a) != (uptr)(b.m_a)) {
        return (uptr)(a.m_a) < (uptr)(b.m_a);
    }
    return (uptr)(a.m_b) < (uptr)(b.m_b);
}

static inline bool PairEqual(const CScene::UNodePair& a, const CScene::UNodePair& b) {
    return a.m_a == b.m_a && a.m_b == b.m_b;
}

// The collision's volume type bits from a pair's table entry.
static inline void SetVolumeType(CCollision& c, s8 type) {
    int volume = type > 1 ? type - 1 : 0;
    c.m_flags = (u8)((c.m_flags & ~CLSN_FLAG_VOLUME_TYPE) | ((volume & 3) << 4));
}

int CScene::IsNodeInScene(const ISceneNode* node) const {
    if (node == NULL) {
        return false;
    }
    if (m_pFirst == node) {
        return true;
    }
    if (m_pLast == node) {
        return true;
    }
    if (View(node)->m_next != NULL) {
        return true;
    }
    return View(node)->m_prev != NULL;
}

CPlayerObject* CScene::GetPlayer(int index) const {
    return m_players[index];
}

// Every overlapping pair (on both axes) without a bullet is tested over
// `dt`. Touching pairs are kept for OnCollision; a penetrating one stops
// the search.
int CScene::DetectSlowCollisions(float dt, float& fraction) {
    int penetrating = false;
    DestroyCollisions(m_collisions);
    SweepAllExtents();
    UNodePair* x = m_pairsX.begin();
    UNodePair* xEnd = m_pairsX.end();
    UNodePair* y = m_pairsY.begin();
    UNodePair* yEnd = m_pairsY.end();
    while (x != xEnd && y != yEnd) {
        if (PairEqual(*x, *y)) {
            bool bullet = false;
            if (x->m_a->AsBullet() != NULL || x->m_b->AsBullet() != NULL) {
                bullet = true;
            }
            if (!bullet) {
                CCollision clsn;
                clsn.m_pNode0 = x->m_a;
                clsn.m_pNode1 = x->m_b;
                clsn.m_time = dt;
                SetVolumeType(clsn, x->m_type);
                bool noPassThru = false;
                if (clsn.m_pNode0->AsPlayerObject() != NULL || clsn.m_pNode1->AsPlayerObject() != NULL) {
                    CCDBObject::DisablePassThru(true);
                    noPassThru = true;
                }
                int result = clsn.Test(fraction);
                if (noPassThru) {
                    CCDBObject::DisablePassThru(false);
                }
                if (result == CLSN_TOUCHING) {
                    PushCollision(m_collisions, clsn);
                } else if (result == CLSN_PENETRATING) {
                    penetrating = true;
                    return penetrating;
                }
            }
            x++;
            y++;
        } else {
            if (PairLess(*x, *y)) {
                x++;
            }
            if (PairLess(*y, *x)) {
                y++;
            }
        }
    }
    return penetrating;
}

// Pairs with a bullet: each bullet keeps only its earliest hit.
int CScene::DetectFastCollisions(float dt) {
    DestroyCollisions(m_collisions);
    SweepAllExtents();
    UNodePair* x = m_pairsX.begin();
    UNodePair* xEnd = m_pairsX.end();
    UNodePair* y = m_pairsY.begin();
    UNodePair* yEnd = m_pairsY.end();
    while (x != xEnd && y != yEnd) {
        if (PairEqual(*x, *y)) {
            bool bullet = false;
            if (x->m_a->AsBullet() != NULL || x->m_b->AsBullet() != NULL) {
                bullet = true;
            }
            if (bullet) {
                CCollision clsn;
                clsn.m_pNode0 = x->m_a;
                clsn.m_pNode1 = x->m_b;
                clsn.m_time = dt;
                SetVolumeType(clsn, x->m_type);
                float fraction = 1.0f;
                if (clsn.Test(fraction) == CLSN_TOUCHING) {
                    CBullet* b = clsn.m_pNode0->AsBullet();
                    if (b == NULL) {
                        b = clsn.m_pNode1->AsBullet();
                    }
                    if (clsn.m_fraction < b->field_0x94) {
                        b->field_0x94 = clsn.m_fraction;
                        CCollision* old = m_collisions.begin();
                        CCollision* end = m_collisions.end();
                        while (old != end && (ISceneNode*)b != old->m_pNode0 && (ISceneNode*)b != old->m_pNode1) {
                            old++;
                        }
                        if (old != m_collisions.end()) {
                            CCollision* last = m_collisions.end();
                            CCollision* to = old;
                            for (CCollision* from = old + 1; from != last; from++, to++) {
                                *to = *from;
                            }
                            to->~CCollision();
                            m_collisions.m_size--;
                        }
                        PushCollision(m_collisions, clsn);
                    }
                }
            }
            x++;
            y++;
        } else {
            if (PairLess(*x, *y)) {
                x++;
            }
            if (PairLess(*y, *x)) {
                y++;
            }
        }
    }
    return m_collisions.size() != 0;
}

// Extents order: by value, starts before ends at the same value.
static inline bool ExtentLess(const ISceneNode::SExtent& a, const ISceneNode::SExtent& b) {
    if (a.m_value < b.m_value) {
        return true;
    }
    if (a.m_value == b.m_value && a.m_isStart && !b.m_isStart) {
        return true;
    }
    return false;
}

typedef dwi::list<ISceneNode::SExtent> ExtentList;

// Insertion sort (the lists stay nearly sorted between frames).
static inline void SortExtents(ExtentList& list) {
    ExtentList::node* head = list.end();
    ExtentList::node* min = list.begin();
    for (ExtentList::node* n = min; n != head; n = n->next) {
        if (ExtentLess(n->value, min->value)) {
            min = n;
        }
    }
    min->prev->next = min->next;
    min->next->prev = min->prev;
    min->next = head->next;
    head->next->prev = min;
    head->next = min;
    min->prev = head;
    for (ExtentList::node* n = min->next; n != head; n = n->next) {
        ExtentList::node* at = n;
        do {
            at = at->prev;
        } while (ExtentLess(n->value, at->value));
        if (at != n->prev) {
            n->prev->next = n->next;
            n->next->prev = n->prev;
            n->next = at->next;
            at->next->prev = n;
            at->next = n;
            n->prev = at;
        }
    }
}

// Walks the sorted extents keeping the nodes open on this axis; each node
// that opens pairs with every open node the collision table lets it hit
// (unless excluded).
static void SweepExtents(ExtentList& list, dwi::fast_vec<CScene::UNodePair>& pairs,
                         dwi::fast_vec<CScene::SExclusionEntry>& exclusions) {
    dwi::fast_vec<ISceneNode*> open(-1);
    open.reserve((ISceneNode**)SPR_BEGIN, 0xFA);
    for (ExtentList::node* n = list.begin(); n != list.end(); n = n->next) {
        ISceneNode* node = n->value.m_node;
        if (node->GetCollisionId() == -1) {
            continue;
        }
        if (n->value.m_isStart) {
            ISceneNode** begin = open.begin();
            ISceneNode** end = open.end();
            for (ISceneNode** o = begin; o != end; o++) {
                int id = node->GetCollisionId();
                int other = (*o)->GetCollisionId();
                s8 type;
                if (other == -1 || id == -1) {
                    type = 0;
                } else {
                    int index;
                    if (other > id) {
                        index = id + (other * (other + 1)) / 2;
                    } else {
                        index = other + (id * (id + 1)) / 2;
                    }
                    type = (s8)g_theCollisionTable[index];
                }
                if (type != 0) {
                    ISceneNode* a;
                    ISceneNode* b;
                    if ((uptr)(*o) < (uptr)(node)) {
                        a = *o;
                        b = node;
                    } else {
                        a = node;
                        b = *o;
                    }
                    CScene::SExclusionEntry* e = exclusions.begin();
                    CScene::SExclusionEntry* eEnd = exclusions.end();
                    while (e != eEnd && !(e->m_a == a && e->m_b == b)) {
                        e++;
                    }
                    if (e == eEnd) {
                        CScene::UNodePair pair;
                        if ((uptr)(*o) < (uptr)(node)) {
                            pair.m_a = *o;
                            pair.m_b = node;
                        } else {
                            pair.m_a = node;
                            pair.m_b = *o;
                        }
                        pair.m_type = type;
                        pairs.push_back(pair);
                    }
                }
            }
            open.push_back(node);
        } else {
            ISceneNode** o = open.begin();
            ISceneNode** end = open.end();
            while (o != end && *o != node) {
                o++;
            }
            for (ISceneNode** from = o + 1; from != end; from++, o++) {
                *o = *from;
            }
            open.m_size--;
        }
    }
}

bool CmpNodePairs(const CScene::UNodePair& a, const CScene::UNodePair& b) {
    return PairLess(a, b);
}

// Pairs are unique, so any correct sort gives the original's order.
static void SortPairs(CScene::UNodePair* begin, CScene::UNodePair* end) {
    int n = end - begin;
    // Heap sort (no recursion; up to 0x800 pairs).
    for (int start = n / 2 - 1; start >= 0; start--) {
        int root = start;
        for (;;) {
            int child = root * 2 + 1;
            if (child >= n) {
                break;
            }
            if (child + 1 < n && CmpNodePairs(begin[child], begin[child + 1])) {
                child++;
            }
            if (!CmpNodePairs(begin[root], begin[child])) {
                break;
            }
            CScene::UNodePair t = begin[root];
            begin[root] = begin[child];
            begin[child] = t;
            root = child;
        }
    }
    for (int last = n - 1; last > 0; last--) {
        CScene::UNodePair t = begin[0];
        begin[0] = begin[last];
        begin[last] = t;
        int root = 0;
        for (;;) {
            int child = root * 2 + 1;
            if (child >= last) {
                break;
            }
            if (child + 1 < last && CmpNodePairs(begin[child], begin[child + 1])) {
                child++;
            }
            if (!CmpNodePairs(begin[root], begin[child])) {
                break;
            }
            CScene::UNodePair u = begin[root];
            begin[root] = begin[child];
            begin[child] = u;
            root = child;
        }
    }
}

void CScene::SweepAllExtents() {
    m_pairsX.m_size = 0;
    if (m_extentsX.size() > 1) {
        SortExtents(m_extentsX);
    }
    SweepExtents(m_extentsX, m_pairsX, m_exclusions);
    SortPairs(m_pairsX.begin(), m_pairsX.end());
    m_pairsY.m_size = 0;
    if (m_extentsY.size() > 1) {
        SortExtents(m_extentsY);
    }
    SweepExtents(m_extentsY, m_pairsY, m_exclusions);
    SortPairs(m_pairsY.begin(), m_pairsY.end());
}

static inline void RemoveExtent(ExtentList& list, ISceneNode& node, u8 isStart) {
    ExtentList::node* n = list.begin();
    while (n != list.end() && !(n->value.m_node == &node && n->value.m_isStart == isStart)) {
        n = n->next;
    }
    if (n != list.end()) {
        list.erase(n);
    }
}

void CScene::RemoveNodeExtents(ISceneNode& node) {
    RemoveExtent(m_extentsX, node, 1);
    View(&node)->m_minX = NULL;
    RemoveExtent(m_extentsX, node, 0);
    View(&node)->m_maxX = NULL;
    RemoveExtent(m_extentsY, node, 1);
    View(&node)->m_minY = NULL;
    RemoveExtent(m_extentsY, node, 0);
    View(&node)->m_maxY = NULL;
}

static inline ISceneNode::SExtent* PushExtent(ExtentList& list, ISceneNode& node, u8 isStart) {
    ISceneNode::SExtent e;
    e.m_node = &node;
    e.m_isStart = isStart;
    e.m_value = 0.0f;
    e.unk_0xC = 0;
    list.push_back(e);
    return &list.end()->prev->value;
}

void CScene::AddNodeExtents(ISceneNode& node) {
    View(&node)->m_minX = PushExtent(m_extentsX, node, 1);
    View(&node)->m_maxX = PushExtent(m_extentsX, node, 0);
    View(&node)->m_minY = PushExtent(m_extentsY, node, 1);
    View(&node)->m_maxY = PushExtent(m_extentsY, node, 0);
}

CPlayerObject* CScene::FindClosestPlayer(CVector3 pos) {
    float best = BIG_FLOAT;
    CPlayerObject* closest = NULL;
    for (int i = 0; i < m_numPlayers; i++) {
        CVector3 d;
        m_players[i]->GetPosition(d);
        d.x -= pos.x;
        d.y -= pos.y;
        d.z -= pos.z;
        float dist = d.Length();
        if (dist < best) {
            best = dist;
            closest = m_players[i];
        }
    }
    return closest;
}

// Runs the queued changes.
static inline void RunModCommands(CScene* scene) {
    for (CScene::SModCommand* c = scene->m_modCommands.begin(); c != scene->m_modCommands.end(); c++) {
        (scene->*c->m_fn)(c->m_node);
    }
    scene->m_modCommands.m_size = 0;
}

void CScene::Draw(CViewport& viewport, CScreen& screen, CRenderList&, CFog& fog) {
    CCamera* camera = m_camera;
    CDrawContext ctx(camera->m_worldToCamera, camera->m_projection, viewport.GetTMClipToView(),
                     screen.GetTMViewToScreen(), viewport.m_index);
    camera = m_camera;
    ctx.m_pCamera = camera;
    ctx.field_0xD8 = (uptr)&fog;
    ctx.m_pLights = (CLightDrawList*)&m_lights;
    ctx.m_pShadows = (CShadowDrawList*)&m_shadows;
    camera->m_frustum.Set(*camera, screen, viewport);
    camera->m_frustum.Transform(camera->m_localToWorld);

    // The fog's near and far planes, in world space.
    CMatrix tm;
    m_camera->GetTMLocalToWorld(tm);
    CPlane* planes = (CPlane*)&fog;
    planes[0].m_normal[0] = 0.0f;
    planes[0].m_normal[1] = 1.0f;
    planes[0].m_normal[2] = 0.0f;
    planes[0].m_d = fog.m_start;
    planes[0].Transform(tm);
    planes[1].m_normal[0] = 0.0f;
    planes[1].m_normal[1] = -1.0f;
    planes[1].m_normal[2] = 0.0f;
    planes[1].m_d = fog.m_range;
    planes[1].Transform(tm);

    m_lights.m_size = 0;
    m_shadows.m_size = 0;
    for (CLight* light = m_pFirstLight; light != NULL; light = (CLight*)((SAnimLinks*)light)->m_next) {
        if (((ISceneNode*)light)->IsVisible(ctx)) {
            m_lights.push_back(light);
        }
    }
    m_deferMods = true;
    for (ISceneNode* node = m_pFirst; node != NULL; node = View(node)->m_next) {
        if (node->IsDrawEnabled() && node->IsVisible(ctx)) {
            node->Draw(ctx);
        }
    }
    m_world->DrawShadows(ctx);
    m_deferMods = false;
    RunModCommands(this);
}

// One frame: exclusion timers, BeginUpdate/ConstrainVelocity/UpdateAI/
// CommitAI on every node, then the motion in sub-steps of at most
// MAX_TIME_STEP. A step that would make something penetrate is shortened
// to the first contact; touching pairs get OnCollision before the commit.
// If a step gets shorter than 1/64 everything halts for the frame. Bullets
// move last, in one step, each hitting only its first contact.
void CScene::Update(float dt) {
    static bool isCSFWarningEnabled = true;
    if (dt == 0.0f) {
        return;
    }
    if (dt > 6.0f) {
        dt = 6.0f;
    }
    for (SExclusionEntry* e = m_exclusions.begin(); e != m_exclusions.end();) {
        if (e->m_time == 0.0f) {
            WakePlayers(*e);
            EraseExclusion(m_exclusions, e);
        } else {
            if (e->m_time > 0.0f) {
                e->m_time -= (float)(int)dt;
                if (e->m_time < 0.0f) {
                    e->m_time = 0.0f;
                }
            }
            e++;
        }
    }

    ISceneNode* node;
    m_deferMods = true;
    for (node = m_pFirst; node != NULL; node = View(node)->m_next) {
        node->BeginUpdate(dt);
    }
    for (node = m_pFirst; node != NULL; node = View(node)->m_next) {
        node->ConstrainVelocity();
    }
    for (node = m_pFirst; node != NULL; node = View(node)->m_next) {
        node->UpdateAI(dt);
    }
    for (node = m_pFirst; node != NULL; node = View(node)->m_next) {
        node->CommitAI();
    }
    m_deferMods = false;
    RunModCommands(this);

    float maxStep = dt < MAX_TIME_STEP ? dt : MAX_TIME_STEP;
    float next = dt;  // where to resume after a shortened step
    float done = 0.0f;
    float target = maxStep;
    int maxIterations = (int)(dt / 0.015625f) + 1;
    bool finished = false;
    for (int iteration = 1; iteration <= maxIterations; iteration++) {
        float step = target - done;
        if (step > maxStep) {
            step = maxStep;
            target = done + maxStep;
        }
        if (step < 0.015625f) {
            // Stuck: halt everything for this frame.
            if (isCSFWarningEnabled) {
                isCSFWarningEnabled = false;
            }
            m_deferMods = true;
            for (node = m_pFirst; node != NULL; node = View(node)->m_next) {
                IMovingSceneNode* moving = node->AsMovingNode();
                if (moving != NULL) {
                    moving->Halt();
                }
                node->AttemptUpdate(0.0f);
                node->CommitUpdate();
            }
            m_deferMods = false;
            RunModCommands(this);
            return;
        }
        m_deferMods = true;
        for (node = m_pFirst; node != NULL; node = View(node)->m_next) {
            if (node->AsBullet() == NULL) {
                node->AttemptUpdate(step);
            }
        }
        float fraction = 1.0f;
        if (DetectSlowCollisions(step, fraction)) {
            next = target;
            target = step * fraction + done;
        } else {
            if (m_collisions.size() != 0) {
                for (CCollision* c = m_collisions.begin(); c != m_collisions.end(); c++) {
                    c->m_pNode0->OnCollision(*c);
                    c->m_pNode1->OnCollision(*c);
                }
            }
            for (node = m_pFirst; node != NULL; node = View(node)->m_next) {
                if (node->AsBullet() == NULL) {
                    node->CommitUpdate();
                }
            }
            if (dt - target < 0.015625f) {
                m_deferMods = false;
                RunModCommands(this);
                finished = true;
                break;
            }
            done = target;
            target = next;
            next = dt;
        }
        m_deferMods = false;
        RunModCommands(this);
    }

    isCSFWarningEnabled = true;
    m_deferMods = true;
    for (node = m_pFirst; node != NULL; node = View(node)->m_next) {
        if (node->AsBullet() != NULL) {
            node->AttemptUpdate(dt);
        }
    }
    if (DetectFastCollisions(dt)) {
        for (CCollision* c = m_collisions.begin(); c != m_collisions.end(); c++) {
            ISceneNode* other = c->m_pNode1;
            CBullet* bullet = c->m_pNode0->AsBullet();
            if (bullet == NULL) {
                bullet = other->AsBullet();
            } else if (other->AsBullet() != NULL) {
                continue;  // two bullets
            }
            bullet->AttemptUpdate(c->m_time);
            bullet->OnCollision(*c);
        }
    }
    for (node = m_pFirst; node != NULL; node = View(node)->m_next) {
        if (node->AsBullet() != NULL) {
            node->CommitUpdate();
        }
    }
    m_deferMods = false;
    RunModCommands(this);
}

bool CScene::VisitAllNodesInVolume(dwi::IVisitor<ISceneNode>& visitor, const IVolume& volume,
                                   ISceneNode::EVolumeType type, bool skipWorld) {
    for (ISceneNode* node = m_pFirst; node != NULL; node = View(node)->m_next) {
        if (skipWorld && node->AsWorldObject() != NULL) {
            continue;
        }
        CCollision clsn;
        const IVolume* nodeVolume = node->GetWorldBoundingVolume(type);
        bool touching;
        if (nodeVolume != NULL) {
            touching = volume.TestCollision(*nodeVolume, clsn, false) != CLSN_NONE;
        } else {
            CVector3 pos;
            node->GetPosition(pos);
            touching = volume.TestCollision(pos, clsn, false) != CLSN_NONE;
        }
        if (touching && visitor.Visit(*node)) {
            return true;
        }
    }
    return false;
}

bool CScene::VisitAllStaticObjects(dwi::IVisitor<ISceneNode>& visitor) {
    for (CStaticObject* s = m_pFirstStatic; s != NULL; s = s->m_pNextStatic) {
        if (visitor.Visit(*s)) {
            return true;
        }
    }
    return false;
}

void CScene::IncludeCollision(const ISceneNode& n0, const ISceneNode& n1) {
    const ISceneNode* a;
    const ISceneNode* b;
    if ((uptr)(&n0) < (uptr)(&n1)) {
        a = &n0;
        b = &n1;
    } else {
        a = &n1;
        b = &n0;
    }
    SExclusionEntry* e = m_exclusions.begin();
    while (e != m_exclusions.end() && !(e->m_a == a && e->m_b == b)) {
        e++;
    }
    if (e != m_exclusions.end()) {
        WakePlayers(*e);
        EraseExclusion(m_exclusions, e);
    }
}

// A player excluded from a moving tank's hull (the tank drove into him) is
// crushed: hit event 0x2B with maximum damage.
void CScene::ExcludeCollision(const ISceneNode& n0, const ISceneNode& n1, int frames) {
    const ISceneNode* a;
    const ISceneNode* b;
    if ((uptr)(&n0) < (uptr)(&n1)) {
        a = &n0;
        b = &n1;
    } else {
        a = &n1;
        b = &n0;
    }
    float time = (float)frames;
    const CPlayerObject* player;
    if (n0.AsPlayerObject() != NULL) {
        player = n0.AsPlayerObject();
    } else {
        player = n1.AsPlayerObject();
    }
    if (player != NULL) {
        const CHierObject* hier;
        if (n1.AsHierObject() != NULL) {
            hier = n1.AsHierObject();
        } else {
            hier = n0.AsHierObject();
        }
        if (hier != NULL && ((ISceneNode*)hier)->GetCollisionId() == 0x17) {
            const CStaticObject* s = (const CStaticObject*)hier;
            bool moving = false;
            if (s->m_translating || s->m_rotating || s->m_motionPlaying) {
                moving = true;
            }
            if (moving || (s->m_pathState != CStaticObject::PATH_NONE && s->m_pathState != CStaticObject::PATH_PAUSED)) {
                BS_STRUCT_HitByBulletInfo_struct* event =
                    (BS_STRUCT_HitByBulletInfo_struct*)BSEventGetEventMemoryBlock(sizeof(BS_STRUCT_HitByBulletInfo_struct));
                event->m_bulletType = 0;
                event->m_damage = 0x7FFFFFFF;
                event->m_firedBy = NULL;
                event->m_side = 0;
                if (g_cheats.m_invincible) {
                    event->m_damage = 0;
                }
                BSObjectTriggerEvent(((ISceneNode*)player)->GetScriptObject(), 0x2B, event, NULL, false);
            }
        }
    }
    SExclusionEntry* e = m_exclusions.begin();
    while (e != m_exclusions.end() && !(e->m_a == a && e->m_b == b)) {
        e++;
    }
    if (e == m_exclusions.end()) {
        SExclusionEntry entry;
        entry.m_a = a;
        entry.m_b = b;
        entry.m_time = time;
        m_exclusions.push_back(entry);
    }
}

static inline void UnlinkNode(CScene* scene, ISceneNode* node) {
    ISceneNode* prev = View(node)->m_prev;
    ISceneNode* next = View(node)->m_next;
    if (scene->m_pFirst == node) {
        scene->m_pFirst = next;
    }
    if (scene->m_pLast == node) {
        scene->m_pLast = prev;
    }
    if (prev != NULL) {
        View(prev)->m_next = next;
    }
    if (next != NULL) {
        View(next)->m_prev = prev;
    }
    View(node)->m_prev = NULL;
    View(node)->m_next = NULL;
    scene->m_numNodes--;
}

static inline void LinkNode(CScene* scene, ISceneNode* node) {
    if (scene->m_pLast != NULL) {
        View(scene->m_pLast)->m_next = node;
    }
    View(node)->m_prev = scene->m_pLast;
    if (scene->m_pFirst == NULL) {
        scene->m_pFirst = node;
    }
    scene->m_pLast = node;
    scene->m_numNodes++;
}

void CScene::RemoveNode_now(ISceneNode* node) {
    if (!IsNodeInScene(node)) {
        return;
    }
    UnlinkNode(this, node);
    CStaticObject* s = node->AsStaticObject();
    CAnimObject* anim = node->AsAnimObject();
    CLight* light = node->AsLight();
    if (anim != NULL) {
        SAnimLinks* links = (SAnimLinks*)anim;
        CAnimObject* prev = (CAnimObject*)links->m_prev;
        CAnimObject* next = (CAnimObject*)links->m_next;
        if (m_pFirstAnim == anim) {
            m_pFirstAnim = next;
        }
        if (m_pLastAnim == anim) {
            m_pLastAnim = prev;
        }
        if (prev != NULL) {
            ((SAnimLinks*)prev)->m_next = next;
        }
        if (next != NULL) {
            ((SAnimLinks*)next)->m_prev = prev;
        }
        links->m_prev = NULL;
        links->m_next = NULL;
        m_numAnim--;
    } else if (s != NULL) {
        CStaticObject* prev = s->m_pPrevStatic;
        CStaticObject* next = s->m_pNextStatic;
        if (m_pFirstStatic == s) {
            m_pFirstStatic = next;
        }
        if (m_pLastStatic == s) {
            m_pLastStatic = prev;
        }
        if (prev != NULL) {
            prev->m_pNextStatic = next;
        }
        if (next != NULL) {
            next->m_pPrevStatic = prev;
        }
        s->m_pPrevStatic = NULL;
        s->m_pNextStatic = NULL;
        m_numStatic--;
    } else if (light != NULL) {
        SAnimLinks* links = (SAnimLinks*)light;
        CLight* prev = (CLight*)links->m_prev;
        CLight* next = (CLight*)links->m_next;
        if (m_pFirstLight == light) {
            m_pFirstLight = next;
        }
        if (m_pLastLight == light) {
            m_pLastLight = prev;
        }
        if (prev != NULL) {
            ((SAnimLinks*)prev)->m_next = next;
        }
        if (next != NULL) {
            ((SAnimLinks*)next)->m_prev = prev;
        }
        links->m_prev = NULL;
        links->m_next = NULL;
        m_numLights--;
    }
    if (light == NULL) {
        RemoveNodeExtents(*node);
    }
    // Its exclusions go too.
    for (SExclusionEntry* e = m_exclusions.begin(); e != m_exclusions.end();) {
        if (e->m_a == node || e->m_b == node) {
            WakePlayers(*e);
            EraseExclusion(m_exclusions, e);
        } else {
            e++;
        }
    }
}

void CScene::AddNode_now(ISceneNode* node) {
    LinkNode(this, node);
    CAnimObject* anim = node->AsAnimObject();
    CStaticObject* s = node->AsStaticObject();
    CLight* light = node->AsLight();
    if (anim != NULL) {
        SAnimLinks* links = (SAnimLinks*)anim;
        if (m_pLastAnim != NULL) {
            ((SAnimLinks*)m_pLastAnim)->m_next = anim;
        }
        links->m_prev = m_pLastAnim;
        if (m_pFirstAnim == NULL) {
            m_pFirstAnim = anim;
        }
        m_pLastAnim = anim;
        m_numAnim++;
    } else if (s != NULL) {
        if (m_pLastStatic != NULL) {
            m_pLastStatic->m_pNextStatic = s;
        }
        s->m_pPrevStatic = m_pLastStatic;
        if (m_pFirstStatic == NULL) {
            m_pFirstStatic = s;
        }
        m_pLastStatic = s;
        m_numStatic++;
    } else if (light != NULL) {
        SAnimLinks* links = (SAnimLinks*)light;
        if (m_pLastLight != NULL) {
            ((SAnimLinks*)m_pLastLight)->m_next = light;
        }
        links->m_prev = m_pLastLight;
        if (m_pFirstLight == NULL) {
            m_pFirstLight = light;
        }
        m_pLastLight = light;
        m_numLights++;
    }
    if (light == NULL) {
        AddNodeExtents(*node);
    }
}

// In multiplayer each player's camera stays out of the scene.
void CScene::SetCamera_now(ISceneNode* camera) {
    ISceneNode* old = (ISceneNode*)m_camera;
    if (camera == old) {
        return;
    }
    if (old != NULL && !g_bInMultiplayerMode) {
        UnlinkNode(this, old);
        RemoveNodeExtents(*(ISceneNode*)m_camera);
    }
    if (camera != NULL && !g_bInMultiplayerMode) {
        LinkNode(this, camera);
        AddNodeExtents(*camera);
    }
    m_camera = (CCamera*)camera;
}

void CScene::SetWorld_now(ISceneNode* world) {
    ISceneNode* old = (ISceneNode*)m_world;
    if (world == old) {
        return;
    }
    if (old != NULL) {
        UnlinkNode(this, old);
        RemoveNodeExtents(*(ISceneNode*)m_world);
    }
    if (world != NULL) {
        LinkNode(this, world);
        AddNodeExtents(*world);
    }
    m_world = (CWorldObject*)world;
}

void CScene::AddPlayer_now(ISceneNode* player) {
    for (int i = 0; i < m_numPlayers; i++) {
        if (player == (ISceneNode*)m_players[i]) {
            return;
        }
    }
    m_players[m_numPlayers++] = (CPlayerObject*)player;
    LinkNode(this, player);
    AddNodeExtents(*player);
}

static inline void Queue(CScene* scene, void (CScene::*fn)(ISceneNode*), ISceneNode* node) {
    CScene::SModCommand cmd;
    cmd.m_fn = fn;
    cmd.m_node = node;
    scene->m_modCommands.push_back(cmd);
}

void CScene::Remove(ISceneNode& node) {
    if (m_deferMods) {
        Queue(this, &CScene::RemoveNode_now, &node);
    } else {
        RemoveNode_now(&node);
    }
}

void CScene::Add(ISceneNode& node) {
    if (m_deferMods) {
        Queue(this, &CScene::AddNode_now, &node);
    } else {
        AddNode_now(&node);
    }
}

void CScene::SetCamera(CCamera* camera) {
    if (m_deferMods) {
        Queue(this, &CScene::SetCamera_now, (ISceneNode*)camera);
    } else {
        SetCamera_now((ISceneNode*)camera);
    }
}

void CScene::SetWorld(CWorldObject* world) {
    if (m_deferMods) {
        Queue(this, &CScene::SetWorld_now, (ISceneNode*)world);
    } else {
        SetWorld_now((ISceneNode*)world);
    }
}

void CScene::AddPlayer(CPlayerObject* player) {
    if (m_deferMods) {
        Queue(this, &CScene::AddPlayer_now, (ISceneNode*)player);
    } else {
        AddPlayer_now((ISceneNode*)player);
    }
}

void CScene::Reset() {
    m_numPlayers = 0;
    m_players[0] = NULL;
    m_players[1] = NULL;
    m_players[2] = NULL;
    m_players[3] = NULL;
    m_pFirst = NULL;
    m_pLast = NULL;
    m_world = NULL;
    m_camera = NULL;
    m_pFirstStatic = NULL;
    m_pLastStatic = NULL;
    m_pFirstAnim = NULL;
    m_pLastAnim = NULL;
    m_pFirstLight = NULL;
    m_pLastLight = NULL;
    m_numNodes = 0;
    m_numStatic = 0;
    m_numAnim = 0;
    m_numLights = 0;
    m_lights.m_size = 0;
    m_shadows.m_size = 0;
    m_extentsX.clear();
    m_extentsY.clear();
    m_pairsX.m_size = 0;
    m_pairsY.m_size = 0;
    DestroyCollisions(m_collisions);
    m_exclusions.m_size = 0;
    m_modCommands.m_size = 0;
}

void CScene::Shutdown() {
    while (m_pFirst != NULL) {
        Remove(*m_pFirst);
    }
}

CScene::~CScene() {
    if (m_numNodes > 0) {
        Shutdown();
    }
    DestroyCollisions(m_collisions);
}

CScene::CScene()
    : m_lights(2), m_shadows((int)0xDEADBEEF), m_pairsX(5), m_pairsY(6), m_exclusions(9), m_modCommands(10) {
    m_pFirst = NULL;
    m_world = NULL;
    m_camera = NULL;
    m_pFirstStatic = NULL;
    m_pLastStatic = NULL;
    m_pFirstAnim = NULL;
    m_pLastAnim = NULL;
    m_pFirstLight = NULL;
    m_pLastLight = NULL;
    m_numNodes = 0;
    m_numStatic = 0;
    m_numAnim = 0;
    m_numLights = 0;
    m_deferMods = false;
    m_numPlayers = 0;
    m_lights.allocate(0x14, "Scene lights");
    m_shadows.allocate(0x14, "Scene shadows");
    m_extentsX.reserve(500, "Scene extents X");
    m_extentsY.reserve(500, "Scene extents Y");
    m_pairsX.allocate(0x800, "Scene node pairs X");
    m_pairsY.allocate(0x800, "Scene node pairs Y");
    m_collisions.reserve(0x800, "Scene collisions");
    m_exclusions.allocate(0x800, "Scene exlusions");
    m_modCommands.allocate(0x28, "Scene mod commands");
}
