// CScene: the scene graph. Keeps every node (with separate lists of static
// objects, animated objects and lights), the players, camera and world,
// runs the per-frame update with collision sub-stepping (sweep and prune
// over sorted X and Y extents), and draws the visible nodes. Changes made
// while it iterates are queued and applied afterwards. Hand-maintained.
#ifndef _GAME_SCENE_H_
#define _GAME_SCENE_H_

#include "types.h"
#include "game_types.h"
#include "game/ISceneNode.h"
#include "game/collision.h"
#include "dwi/list.h"
#include "dwi/fast_vec.h"
#include "dwi/vector.h"

class CAnimObject;
class CCamera;
class CCharacterShadow;
class CFog;
class CLight;
class CPlayerObject;
class CRenderList;
class CScene;
class CScreen;
class CStaticObject;
class CVector3;
class CViewport;
class CWorldObject;
class ISceneNode;
class IVolume;

// size 0x148
class CScene {
public:
    // Two nodes whose extents overlap on an axis (a < b). size 0x10
    struct UNodePair {
        /* 0x0 */ ISceneNode* m_a;
        /* 0x4 */ ISceneNode* m_b;
        /* 0x8 */ s8 m_type;  // g_theCollisionTable entry (> 1: volume type + 1)
        /* 0x9 */ u8 unk_0x9[0x7];
    };
    // Two nodes that don't collide for a while (a < b). size 0xC
    struct SExclusionEntry {
        /* 0x0 */ const ISceneNode* m_a;
        /* 0x4 */ const ISceneNode* m_b;
        /* 0x8 */ f32 m_time;  // frames left; < 0 forever, 0 ends it
    };
    // A queued change. size 0x10
    struct SModCommand {
        /* 0x0 */ void (CScene::*m_fn)(ISceneNode*);
        /* 0xC */ ISceneNode* m_node;
    };

    /* 0x0 */ ISceneNode* m_pFirst;  // all nodes, linked through their 0x4/0x8 words
    /* 0x4 */ ISceneNode* m_pLast;
    /* 0x8 */ CPlayerObject* m_players[4];
    /* 0x18 */ CWorldObject* m_world;
    /* 0x1C */ CCamera* m_camera;
    /* 0x20 */ CStaticObject* m_pFirstStatic;
    /* 0x24 */ CStaticObject* m_pLastStatic;
    /* 0x28 */ CAnimObject* m_pFirstAnim;
    /* 0x2C */ CAnimObject* m_pLastAnim;
    /* 0x30 */ CLight* m_pFirstLight;
    /* 0x34 */ CLight* m_pLastLight;
    /* 0x38 */ int m_numNodes;
    /* 0x3C */ int m_numStatic;
    /* 0x40 */ int m_numAnim;
    /* 0x44 */ int m_numLights;
    /* 0x48 */ bool m_deferMods;  // queue Add/Remove/... while iterating
    /* 0x49 */ u8 unk_0x49[0x3];
    /* 0x4C */ int m_numPlayers;
    /* 0x50 */ dwi::fast_vec<CLight*> m_lights;  // visible this frame
    /* 0x68 */ dwi::fast_vec<CCharacterShadow*> m_shadows;
    /* 0x80 */ dwi::list<ISceneNode::SExtent> m_extentsX;
    /* 0xAC */ dwi::list<ISceneNode::SExtent> m_extentsY;
    /* 0xD8 */ dwi::fast_vec<UNodePair> m_pairsX;
    /* 0xF0 */ dwi::fast_vec<UNodePair> m_pairsY;
    /* 0x108 */ dwi::vector<CCollision> m_collisions;
    /* 0x118 */ dwi::fast_vec<SExclusionEntry> m_exclusions;
    /* 0x130 */ dwi::fast_vec<SModCommand> m_modCommands;

    int IsNodeInScene(const ISceneNode*) const;  // 0x800D8214
    CPlayerObject* GetPlayer(int index) const;  // 0x800D8274
    // Collisions of everything but bullets over `dt`; true when something
    // would penetrate (`fraction` gets how far the step got).
    int DetectSlowCollisions(float dt, float& fraction);  // 0x800D8284
    // Bullets' collisions (the first hit per bullet).
    int DetectFastCollisions(float);  // 0x800D85F0
    void SweepAllExtents();  // 0x800D8BAC
    void RemoveNodeExtents(ISceneNode&);  // 0x800DA178
    void AddNodeExtents(ISceneNode&);  // 0x800DA374
    CPlayerObject* FindClosestPlayer(CVector3);  // 0x800DA540
    void Draw(CViewport&, CScreen&, CRenderList&, CFog&);  // 0x800DA67C
    void Update(float);  // 0x800DA8F4
    // Visits the nodes touching `volume` until a visit returns true.
    bool VisitAllNodesInVolume(dwi::IVisitor<ISceneNode>&, const IVolume&, ISceneNode::EVolumeType, bool skipWorld);  // 0x800DB458
    bool VisitAllStaticObjects(dwi::IVisitor<ISceneNode>&);  // true if a visit stopped it  // 0x800DB5C0
    void IncludeCollision(const ISceneNode&, const ISceneNode&);  // 0x800DB630
    // `frames` < 0: until IncludeCollision.
    void ExcludeCollision(const ISceneNode&, const ISceneNode&, int frames);  // 0x800DB778
    void RemoveNode_now(ISceneNode*);  // 0x800DB9E0
    void AddNode_now(ISceneNode*);  // 0x800DBD4C
    void SetCamera_now(ISceneNode*);  // 0x800DBEE8
    void SetWorld_now(ISceneNode*);  // 0x800DBFFC
    void AddPlayer_now(ISceneNode*);  // 0x800DC0F8
    void Remove(ISceneNode&);  // 0x800DC190
    void Add(ISceneNode&);  // 0x800DC21C
    void SetCamera(CCamera*);  // 0x800DC2A8
    void SetWorld(CWorldObject*);  // 0x800DC334
    void AddPlayer(CPlayerObject*);  // 0x800DC3C0
    void Reset();  // 0x800DC44C
    void Shutdown();  // 0x800DC5E0
    ~CScene();  // 0x800DC620
    CScene();  // 0x800DC8C4
};
OFFSET_ASSERT(CScene, m_world, 0x18);
OFFSET_ASSERT(CScene, m_numPlayers, 0x4C);
OFFSET_ASSERT(CScene, m_extentsX, 0x80);
OFFSET_ASSERT(CScene, m_extentsY, 0xAC);
OFFSET_ASSERT(CScene, m_pairsX, 0xD8);
OFFSET_ASSERT(CScene, m_collisions, 0x108);
OFFSET_ASSERT(CScene, m_exclusions, 0x118);
OFFSET_ASSERT(CScene, m_modCommands, 0x130);
SIZE_ASSERT(CScene, 0x148);

// Pairs sort by (a, b).
bool CmpNodePairs(const CScene::UNodePair&, const CScene::UNodePair&);  // 0x800D9E24

// Sub-steps shorter than this end the update (everything halts).
extern const float TIME_STEP_EPSILON;  // 0x80350E58

extern CScene g_scene;  // 0x8019484C

#endif
