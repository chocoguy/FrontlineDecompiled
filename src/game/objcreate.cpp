// Object creation: builds the game objects behind placed level objects
// (the player, boxes/mechanics, tanks, collision volumes, German soldiers),
// their bounding volumes and scripts; queues Germans while too many are
// alive; and manages "dump objects" (dropped weapons and health).
#include "game/objcreate.h"
#include "game/AIFilter.h"
#include "game/AIDoodad.h"
#include "game/AnimObjFactory.h"
#include "game/BSGO_Dummy.h"
#include "game/ISceneNode.h"
#include "game/IMovingSceneNode.h"
#include "game/IVolume.h"
#include "game/Moh2.h"
#include "game/ShellMenu.h"
#include "game/Soldier_object.h"
#include "game/Tank_object.h"
#include "game/animated_volume.h"
#include "game/anim_obj.h"
#include "game/box.h"
#include "game/bsmessage.h"
#include "game/bsobject.h"
#include "game/bstypes.h"
#include "game/bsutil.h"
#include "game/collisionvolume.h"
#include "game/hierobject.h"
#include "game/mainloop.h"
#include "game/matrix.h"
#include "game/particlesystem.h"
#include "game/light.h"
#include "game/memory.h"
#include "game/player.h"
#include "game/player_weapon_object.h"
#include "game/proxtrig.h"
#include "game/quaternion.h"
#include "game/scene.h"
#include "game/sound.h"
#include "game/soundtable.h"
#include "game/sphere.h"
#include "game/static_obj.h"
#include "game/staticobjfactory.h"
#include "game/thrown_obj.h"
#include "game/toilet.h"
#include "game/vector.h"
#include "game/weapon.h"
#include "math.h"
#include "string.h"

// Pools.
#define POOL_DESTRUCTABLES 0  // BSGO_Destructable, 200
#define POOL_ACTORS 1         // BSGO_Soldier / BSGO_Tank, 25

static BSUtilObjectInstanceMemoryAllocator g_pObjectAllocators[2];
static BSUtilObjectInstanceMemoryAllocator g_pVolumeAllocator;          // boxes and spheres
static BSUtilObjectInstanceMemoryAllocator g_pAnimatedVolumeAllocator;
static BSUtilObjectInstanceMemoryAllocator g_pEnvModMechAllocator;      // collision volumes
static CDumpObjectContainer g_DumpObjectContainer;

// Germans waiting for a free soldier slot (a ring of 15).
struct SGermanCreation {
    /* 0x0 */ TriggerObject_struct* m_trigger;
    /* 0x4 */ int m_arg;
    /* 0x8 */ int m_framesWaited;
};

#define GERMAN_QUEUE_SIZE 15
static SGermanCreation g_pGermanCreationQueue[GERMAN_QUEUE_SIZE];

// Objects to destroy a couple of frames later (nothing queues them).
struct SDelayDestroy {
    /* 0x0 */ BSGO_Basic* m_object;
    /* 0x4 */ int m_frame;
};

#define DELAY_DESTROY_SIZE 10
static SDelayDestroy g_pDelayDestroyList[DELAY_DESTROY_SIZE];

int g_iGermanCreationQueueHeadIndex;
int g_iGermanCreationQueueTailIndex;
static int g_iNumGermansQueuedThisFrame;
static int g_delayDestroyHead;
static int g_delayDestroyTail;
static int g_frameCounter;
BSGO_Basic* g_pBoxList;
BSGO_Basic* g_pGermanList;
BSGO_Basic* g_pTankList;
int g_numActiveSoldiers;
int g_maxActiveSoldiers;
static float g_fDumpObjectCollisionRadius;
int g_iNumberOfEnemiesSpawned;

// Level settings fields.
#define LS_MAX_SOLDIERS 0x38
#define LS_DUMP_RADIUS 0x3E  // in resource 0xB

#define WEAPON_DEFAULT_RIFLE 0x5760E46D
#define HAND_LEFT_CRC 0x4C83C769
#define HAND_RIGHT_CRC 0xD58A96D3

static inline int NextGermanIndex(int i) {
    i++;
    return i >= GERMAN_QUEUE_SIZE ? 0 : i;
}

static inline float Length(const CVector3& v) {
    float len = v.z * v.z + (v.x * v.x + v.y * v.y);
    if (len > 0.0f) {
        len = sqrtf(len);
    }
    return len;
}

static inline void Normalize(CVector3& v) {
    float len = Length(v);
    if (len != 0.0f) {
        float inv = 1.0f / len;
        v.x *= inv;
        v.y *= inv;
        v.z *= inv;
    }
}

// The node's transform from its trigger's placement.
static inline void PlaceAtTrigger(IMovingSceneNode* node, TriggerObjectData_struct* data, CVector3& pos) {
    CMatrix m;
    pos.x = data->m_position[0];
    pos.y = data->m_position[1];
    pos.z = data->m_position[2];
    ((CQuaternion*)data->m_orientation)->GetMatrix(m);
    m.SetPos(pos);
    node->SetTMLocalToWorld(m);
}

// A placed object's script (patched by PatchUpCore) and maximum health.
static inline const char* ChildScript(TriggerObjectData_struct* data) {
    return *(const char**)((u8*)data + 0x4C);
}

// The bounding box of a static object's mesh.
static inline CVolBox* MeshBounds(CStaticObject* obj) {
    return *(CVolBox**)(*(u8**)((u8*)obj->m_pMesh + 4) + 0x20);
}

void* BSGO_Player::GetScriptData() {
    return m_scriptData;
}

ISceneNode* BSGO_Player::GetSceneNode() {
    return (ISceneNode*)m_player;
}

void* BSGO_Player::GetProximityData() {
    return m_proximity;
}

// Asks the farthest German soldiers (not marked to stay) to remove
// themselves (event 0x2D) so `needed` more fit under the soldier limit.
void NonVisDestructFarthestSoldiers(int needed) {
    CSoldierObject* soldiers[0x80];
    float distances[0x80];
    int excess = needed - (g_maxActiveSoldiers - g_numActiveSoldiers);
    if (excess <= 0) {
        return;
    }
    DebugMsg("-----The maximum number of soldiers in existance has been reached.\n");
    DebugMsg("-----This number is currently %d.  It is controled by commondata.crp\n", g_maxActiveSoldiers);
    DebugMsg("-----Attempting to remove %d soldiers\n", excess);
    CVector3 playerPos;
    g_scene.GetPlayer(0)->GetPosition(playerPos);
    int count = 0;
    for (BSGO_Basic* go = g_pGermanList; go != NULL; go = go->m_pNext, count++) {
        CSoldierObject* soldier = (CSoldierObject*)go->GetSceneNode();
        if (soldier->m_noNonVisDestruct) {
            count--;
        } else {
            CVector3 pos;
            soldier->GetPosition(pos);
            pos.x -= playerPos.x;
            pos.y -= playerPos.y;
            pos.z -= playerPos.z;
            soldiers[count] = soldier;
            distances[count] = pos.z * pos.z + (pos.x * pos.x + pos.y * pos.y);
        }
    }
    float limit = 1e35f;
    int removed = 0;
    while (removed < excess) {
        float farthest = 0.0f;
        CSoldierObject* candidate = NULL;
        for (int i = 0; i < count; i++) {
            float d = distances[i];
            if (d > farthest && d < limit) {
                candidate = soldiers[i];
                farthest = d;
            }
        }
        limit = farthest;
        if (candidate == NULL) {
            DebugMsg("WARNING:WARNING: I tried to NonVisDestruct %d soldiers because there\n", excess);
            DebugMsg("WARNING:WARNING: are too many in existance, but I was only able to\n");
            DebugMsg("WARNING:WARNING: NonVisDestruct %d soldiers.  I'll try again later\n", removed);
            return;
        }
        if (BSObjectTriggerEvent(candidate->GetScriptObject(), 0x2D, NULL, NULL, true)) {
            removed++;
            DebugMsg("-----I found a suitable candidate.\n");
        }
    }
}

void FreeSphereVolume(void* volume) {
    g_pVolumeAllocator.FreeElement(volume);
}

CVolSphere* AllocateSphereVolume() {
    void* memory = g_pVolumeAllocator.GetFreeElement(true);
    if (memory != NULL) {
        return new (memory) CVolSphere;
    }
    return NULL;
}

void FreeBoxVolume(void* volume) {
    g_pVolumeAllocator.FreeElement(volume);
}

CVolBox* AllocateBoxVolume() {
    void* memory = g_pVolumeAllocator.GetFreeElement(true);
    if (memory != NULL) {
        return new (memory) CVolBox;
    }
    return NULL;
}

void FreeAnimatedVolume(void* volume) {
    g_pAnimatedVolumeAllocator.FreeElement(volume);
}

CAnimatedVolume* AllocateAnimatedVolume() {
    void* memory = g_pAnimatedVolumeAllocator.GetFreeElement(true);
    if (memory != NULL) {
        return new (memory) CAnimatedVolume;
    }
    return NULL;
}

void BSGO_Basic::Destroy() {}

// Marks the object's node for destruction and invalidates its script.
void DelayDestroyObject(BSGO_Basic* go) {
    if (go->GetSceneNode() != NULL) {
        go->GetSceneNode()->MarkForDestruction(2);
    }
    if (go->m_object != NULL) {
        go->m_object->m_uniqueID = -1;
    }
}

void BSGO_Player::Destroy() {
    m_player->CleanUp();
    DestroyBSObject(m_object);
    m_object = NULL;
    m_player = NULL;
}

static inline void Unlink(BSGO_Basic* go, BSGO_Basic*& list) {
    if (go->m_pPrev != NULL) {
        go->m_pPrev->m_pNext = go->m_pNext;
    }
    if (go->m_pNext != NULL) {
        go->m_pNext->m_pPrev = go->m_pPrev;
    }
    if (list == go) {
        list = go->m_pNext;
    }
}

static inline void Link(BSGO_Basic* go, BSGO_Basic*& list) {
    if (list != NULL) {
        list->m_pPrev = go;
    }
    go->m_pNext = list;
    go->m_pPrev = NULL;
    list = go;
}

void BSGO_Destructable::Destroy() {
    Unlink(this, g_pBoxList);
    g_scene.Remove(*m_node);
    CStaticObject* obj = (CStaticObject*)m_node;
    const IVolume* local = obj->GetLocalBoundingVolume((ISceneNode::EVolumeType)3);
    if (local != NULL) {
        g_pVolumeAllocator.FreeElement((void*)local);
    }
    const IVolume* world = obj->GetWorldBoundingVolume((ISceneNode::EVolumeType)3);
    if (world != NULL && world != local) {
        g_pVolumeAllocator.FreeElement((void*)world);
    }
    switch (m_object->m_trigger->m_data->m_type) {
    case 8:
        g_pEnvModMechAllocator.FreeElement(m_node);
        break;
    case 4:
    case 2:
        CStaticObjectFactory::DeallocateStaticObject(obj);
        break;
    }
    DestroyBSObject(m_object);
    m_object = NULL;
    m_node = NULL;
    g_pObjectAllocators[POOL_DESTRUCTABLES].FreeElement(this);
}

void BSGO_Tank::Destroy() {
    Unlink(this, g_pTankList);
    g_scene.Remove(*(ISceneNode*)m_tank);
    m_tank->CleanUp();
    DestroyBSObject(m_object);
    m_object = NULL;
    m_tank = NULL;
    g_pObjectAllocators[POOL_ACTORS].FreeElement(this);
}

void BSGO_Soldier::Destroy() {
    g_numActiveSoldiers--;
    Unlink(this, g_pGermanList);
    m_soldier->CleanUp();
    g_AnimObjFactory.ReleaseObject(m_soldier);
    DestroyBSObject(m_object);
    m_object = NULL;
    m_soldier = NULL;
    g_pObjectAllocators[POOL_ACTORS].FreeElement(this);
}

static inline void ClearProximity(u32* proximity) {
    proximity[1] = 0;
    proximity[0] = 0;
    proximity[3] = 0;
    proximity[2] = 0;
}

// Places player `index` at its start trigger and starts its script, with
// the health it finished the previous level with (single player, by
// difficulty) or its multiplayer health.
BSGO_Player* CreatePlayer(TriggerObject_struct* trigger, int index) {
    TriggerObjectData_struct* data = trigger->m_data;
    CPlayerObject* player = g_pPlayers[index];
    data->m_team = 1;
    BSGO_Player* go = &player->m_bsgo;
    CVector3 pos;
    PlaceAtTrigger(player, data, pos);
    go->m_player = player;
    ClearProximity(go->m_proximity);
    int mission;
    int stage;
    if (g_bInMultiplayerMode) {
        mission = g_Shell.m_mpMission;
        stage = g_Shell.m_mpStage;
    } else {
        mission = g_Shell.Get_currentMission();
        stage = g_Shell.Get_currentStage();
    }
    int maxHealth = data->field_0x70;
    int health;
    if (!g_bInMultiplayerMode && g_Shell.ShouldIUsePlayersFinishedLevelData(mission, stage)) {
        health = (int)g_Shell.GetPlayersFinishedLevelHealth(mission, stage - 1);
        switch (g_Shell.Get_currentDifficulty()) {
        case 1:
            health = maxHealth;
            break;
        case 2: {
            float half = maxHealth * 0.5f;
            health = health < half ? (int)half : health;
            break;
        }
        case 3:  // hard: keep the health the previous level ended with
            break;
        default:
            DebugMsg("Unknown difficulty level.  Assuming Level TE\n");
            health = maxHealth;
            break;
        }
    } else if (g_bInMultiplayerMode) {
        health = g_Shell.field_0x143C[player->m_controllerPort];
    } else {
        health = maxHealth;
    }
    go->m_scriptData[0] = health;
    CreateBSObject(ChildScript(data), &go->m_object, go, trigger, (void**)&go->m_scriptData[3]);
    player->SetScript(go->m_object);
    BSObjectTriggerEvent(go->m_object, BSEVENT_START, NULL, NULL, g_bInMultiplayerMode != 0);
    player->InitAI();
    return go;
}

// Sizes `box` from a mesh's bounding box (axes scaled by the half extents):
// doubled, or for tanks a slightly tighter, raised box (one level uses a
// larger one).
static inline void FitBox(CVolBox* box, const CVolBox* bounds, bool tank) {
    CVector3 x = bounds->m_axisX;
    CVector3 y = bounds->m_axisY;
    CVector3 z = bounds->m_axisZ;
    CVector3 center = bounds->m_center;
    if (tank) {
        if (g_Shell.Get_currentMission() == 4 && g_Shell.Get_currentStage() == 2 && !g_bInMultiplayerMode) {
            float lx = Length(x);
            float lz = Length(z);
            center.x += 0.0f;
            float ly = Length(y);
            center.y += 0.0f;
            center.z += 0.0f;
            box->SetWidth(2.4f * lx);
            box->SetHeight(2.4f * lz);
            box->SetDepth(2.4f * ly);
        } else {
            float lx = Length(x);
            float lz = Length(z);
            center.x += 0.0f;
            float ly = Length(y);
            center.y += 0.1f * lz;
            center.z += -0.1f * ly;
            box->SetWidth(1.8f * lx);
            box->SetHeight(1.9f * lz);
            box->SetDepth(1.6f * ly);
        }
    } else {
        box->SetWidth(2.0f * Length(x));
        box->SetHeight(2.0f * Length(z));
        box->SetDepth(2.0f * Length(y));
    }
    Normalize(x);
    Normalize(y);
    Normalize(z);
    box->SetBasis(x, y, z);
    box->SetCenter(center);
}

// Gives `obj` local and world bounding boxes fitted to its mesh.
static inline void SetMeshBoxVolumes(CStaticObject* obj, bool tank) {
    CVolBox* local = AllocateBoxVolume();
    FitBox(local, MeshBounds(obj), tank);
    obj->SetLocalBoundingVolume((ISceneNode::EVolumeType)3, local);
    obj->SetLocalBoundingVolume((ISceneNode::EVolumeType)0, local);
    CVolBox* world = AllocateBoxVolume();
    *world = *local;
    obj->SetWorldBoundingVolume((ISceneNode::EVolumeType)3, world);
    obj->SetWorldBoundingVolume((ISceneNode::EVolumeType)0, world);
}

static inline void ReportMissingScript(const char* kind, const CVector3& pos) {
    int x = (int)pos.x;
    int y = (int)pos.y;
    int z = (int)pos.z;
    if (kind[0] == 'M') {
        DebugMsg("SOON TO BE ERROR! Mechanic at x: %d  y: %d  z: %d  has no child script.  Thats bad\n", x, y, z);
        for (int i = 0; i < 9; i++) {
            DebugMsg("(repeated for importance)! Mechanic at x: %d  y: %d  z: %d  has no child script.  Thats bad\n", x,
                     y, z);
        }
    } else {
        DebugMsg("SOON TO BE ERROR! Soldier at x: %d  y: %d  z: %d  has no child script.  Thats bad\n", x, y, z);
        for (int i = 0; i < 9; i++) {
            DebugMsg("(repeated for importance)! Soldier at x: %d  y: %d  z: %d  has no child script.  Thats bad\n", x, y,
                     z);
        }
    }
}

static inline BSGO_Destructable* NewDestructable() {
    void* memory = g_pObjectAllocators[POOL_DESTRUCTABLES].GetFreeElement(true);
    if (memory != NULL) {
        return new (memory) BSGO_Destructable;
    }
    return NULL;
}

// A box or mechanic: its static object, box volumes and script.
BSGO_Destructable* CreateDestructable(TriggerObject_struct* trigger, int arg) {
    TriggerObjectData_struct* data = trigger->m_data;
    BSGO_Destructable* go = NewDestructable();
    Link(go, g_pBoxList);
    CStaticObject* obj = CStaticObjectFactory::AllocateStaticObject(data->field_0x48, -1);
    CVector3 pos;
    PlaceAtTrigger(obj, data, pos);
    SetMeshBoxVolumes(obj, false);
    obj->Init();
    g_scene.Add(*obj);
    go->m_node = obj;
    ClearProximity(go->m_proximity);
    go->m_scriptData[0] = data->field_0x70;
    const char* script = ChildScript(data);
    if (script != NULL) {
        CreateBSObject(script, &go->m_object, go, trigger, (void**)&go->m_scriptData[2]);
        obj->SetScript(go->m_object);
        BSObjectTriggerEvent(go->m_object, BSEVENT_START, (void*)arg, NULL, false);
    } else {
        ReportMissingScript("Mechanic", pos);
    }
    ForceUpdateProximityTriggerStatus(go, true);
    return go;
}

void CStaticObject::SetScript(BSObject* obj) {
    m_pScriptObject = obj;
}

void DestroySubHierObject(CHierObject* hier, int index) {
    ((CHierObject*)hier->GetSubObject(index))->CleanUp();
}

// Replaces sub-object `index`'s mesh and motion with variant `variant`.
void SwapHierObject(CHierObject* hier, int index, int variant) {
    int crc = hier->GetScriptObject()->m_trigger->m_data->field_0x48;
    CStaticMesh* mesh = CStaticObjectFactory::GetStaticMesh(crc, variant);
    MotionData* motion = CStaticObjectFactory::GetMotionData(crc, variant);
    ((CHierObject*)hier->GetSubObject(index))->SwapOut(mesh, motion);
}

// Adds sub-object `index` of the parent's model under it.
void CreateSubHierObject(CHierObject* parent, int index, int arg, DWI_ATTACHMENT_CRC_ENUM attachment) {
    CHierObject* hier = CStaticObjectFactory::AllocateStaticObject(
                            parent->GetScriptObject()->m_trigger->m_data->field_0x48, index)
                            ->AsHierObject();
    CTankObject* tank = parent->AsHierTankObject();
    SetMeshBoxVolumes(hier, tank != NULL);
    hier->SetScript(parent->GetScriptObject());
    CVector3 offset(0.0f, 0.0f, 0.0f);
    hier->Init(parent, index, arg, attachment, offset);
}

// A tank (or other hierarchical object): its root object, box volumes,
// script, and for tanks the weapons and AI.
BSGO_Tank* CreateRootHierObject(TriggerObject_struct* trigger, int arg) {
    TriggerObjectData_struct* data = trigger->m_data;
    void* memory = g_pObjectAllocators[POOL_ACTORS].GetFreeElement(true);
    BSGO_Tank* go = memory != NULL ? new (memory) BSGO_Tank : NULL;
    Link(go, g_pTankList);
    CHierObject* hier = CStaticObjectFactory::AllocateStaticObject(data->field_0x48, 0)->AsHierObject();
    CTankObject* tank = hier->AsHierTankObject();
    CVector3 pos;
    PlaceAtTrigger(hier, data, pos);
    SetMeshBoxVolumes(hier, tank != NULL);
    CMatrix ident;
    ident.Ident();
    hier->Init(hier, 0, 0, 0, pos);
    g_scene.Add(*hier);
    go->m_tank = (CTankObject*)hier;
    ClearProximity(go->m_proximity);
    go->m_scriptData[0] = data->field_0x70;
    CreateBSObject(ChildScript(data), &go->m_object, go, trigger, (void**)&go->m_scriptData[1]);
    hier->SetScript(go->m_object);
    BSObjectTriggerEvent(go->m_object, BSEVENT_START, (void*)arg, NULL, false);
    if (tank != NULL) {
        tank->SetCollisionId((EClsnId)0x17);
        tank->AddWeapon(0, *(int*)((u8*)data + 0x98));
        tank->AddWeapon(1, *(int*)((u8*)data + 0x9C));
        tank->AddWeapon(2, *(int*)((u8*)data + 0xA0));
        tank->InitAI();
    }
    ForceUpdateProximityTriggerStatus(go, true);
    return go;
}

void DeleteSpecialThrownObject(CThrownObject* thrown) {
    thrown->CleanUp();
    CStaticObjectFactory::DeallocateStaticObject(thrown);
    BSObject* obj = thrown->GetScriptObject();
    if (obj != NULL) {
        g_pObjectAllocators[POOL_DESTRUCTABLES].FreeElement(obj->m_gameObject);
        DestroyBSObject(obj);
    }
}

// A thrown object (grenade, bottle) of kind `crc`, with its trigger's
// script if it has one.
CThrownObject* CreateSpecialThrownObject(TriggerObject_struct* trigger, int crc, int arg) {
    CStaticObject* obj = CStaticObjectFactory::AllocateStaticObject(crc, -1);
    if (obj == NULL) {
        return NULL;
    }
    CThrownObject* thrown = obj->AsThrownObject();
    thrown->Init();
    thrown->SetCollisionId((EClsnId)0x12);
    g_scene.Add(*thrown);
    if (trigger != NULL) {
        TriggerObjectData_struct* data = trigger->m_data;
        if (ChildScript(data) != NULL) {
            BSGO_Destructable* go = NewDestructable();
            go->m_node = thrown;
            ClearProximity(go->m_proximity);
            go->m_scriptData[0] = data->field_0x70;
            CreateBSObject(ChildScript(data), &go->m_object, go, trigger, (void**)&go->m_scriptData[2]);
            thrown->SetScript(go->m_object);
            BSObjectTriggerEvent(go->m_object, BSEVENT_START, (void*)arg, NULL, false);
        }
    }
    return thrown;
}

// A scripted collision volume (sphere or box shaped trigger).
BSGO_Destructable* CreateCollisionVolume(TriggerObject_struct* trigger, int arg) {
    TriggerObjectData_struct* data = trigger->m_data;
    BSGO_Destructable* go = NewDestructable();
    Link(go, g_pBoxList);
    void* memory = g_pEnvModMechAllocator.GetFreeElement(true);
    CCollisionVolume* volume = memory != NULL ? new (memory) CCollisionVolume : NULL;
    CMatrix m;
    CVector3 pos;
    pos.x = data->m_position[0];
    pos.y = data->m_position[1];
    pos.z = data->m_position[2];
    ((CQuaternion*)data->m_orientation)->GetMatrix(m);
    m.SetPos(pos);
    s16 shape = *(s16*)((u8*)data + 0x2C);
    float size0 = *(s16*)((u8*)data + 0x2E) * 0.0625f;
    IVolume* shapeVolume = NULL;
    if (shape == 0) {
        CVolSphere* sphere = AllocateSphereVolume();
        sphere->SetRadius(size0);
        sphere->SetCenter(CVector3(0.0f, 0.0f, 0.0f));
        sphere->TransformedCopy(*sphere, m);
        shapeVolume = sphere;
    } else if (shape == 2) {
        CVolBox* box = AllocateBoxVolume();
        box->SetWidth(size0);
        box->SetDepth(-(*(s16*)((u8*)data + 0x32) * 0.0625f));
        box->SetHeight(*(s16*)((u8*)data + 0x30) * 0.0625f);
        box->SetCenter(CVector3(0.0f, 0.0f, 0.0f));
        box->SetBasis(CVector3(1.0f, 0.0f, 0.0f), CVector3(0.0f, 1.0f, 0.0f), CVector3(0.0f, 0.0f, 1.0f));
        box->TransformedCopy(*box, m);
        shapeVolume = box;
    }
    volume->Init(shapeVolume, &m, (EClsnId)0xC);
    g_scene.Add(*volume);
    go->m_node = volume;
    go->m_scriptData[0] = data->field_0x70;
    const char* script = ChildScript(data);
    if (script != NULL) {
        CreateBSObject(script, &go->m_object, go, trigger, (void**)&go->m_scriptData[2]);
        volume->SetScript(go->m_object);
        BSObjectTriggerEvent(go->m_object, BSEVENT_START, (void*)arg, NULL, false);
    } else {
        ReportMissingScript("Mechanic", pos);
    }
    return go;
}

// Arms a soldier with weapon slot 0 in the hand its weapon wants.
static inline void ArmSoldier(CSoldierObject* soldier) {
    CWeapon* weapon = soldier->m_pWeapons[soldier->m_curWeapon];
    if (weapon != NULL) {
        weapon->Update(1.0f, false);
        soldier->m_curWeapon = 0;
        char* hand;
        int handCrc;
        if (!(soldier->m_pWeapons[soldier->m_curWeapon]->m_pProperties->m_hand & 1)) {
            hand = "lt_hand";
            handCrc = HAND_LEFT_CRC;
        } else {
            hand = "rt_hand";
            handCrc = HAND_RIGHT_CRC;
        }
        if (g_bInMultiplayerMode) {
            soldier->DetachObject("lt_hand");
            soldier->DetachObject("rt_hand");
        } else {
            soldier->DetachObject(hand);
        }
        soldier->AttachObject(soldier->m_pWeapons[soldier->m_curWeapon], hand, handCrc);
        if (!g_bInMultiplayerMode) {
            ((CAIFilterSoldierObject*)soldier->GetAIDoodad()->m_pFilter)->UpdateManualUpdateVars();
        }
        soldier->SetBulletEmitter(soldier->m_pWeapons[soldier->m_curWeapon], hand);
    } else {
        soldier->m_curWeapon = 0;
    }
}

// A German (or allied) soldier. With too many alive it is queued instead
// (when `queue`), and created by UpdateObjectCreation once there is room.
BSGO_Soldier* CreateGerman(TriggerObject_struct* trigger, int arg, bool queue) {
    static int guysLostCount = 0;
    if (g_numActiveSoldiers >= g_maxActiveSoldiers) {
        if (queue) {
            int tail = g_iGermanCreationQueueTailIndex;
            g_pGermanCreationQueue[tail].m_trigger = trigger;
            g_pGermanCreationQueue[tail].m_arg = arg;
            g_iNumGermansQueuedThisFrame++;
            g_pGermanCreationQueue[tail].m_framesWaited = 0;
            g_iGermanCreationQueueTailIndex = NextGermanIndex(tail);
            if (g_iGermanCreationQueueTailIndex == g_iGermanCreationQueueHeadIndex) {
                // Full: drop the newest.
                guysLostCount++;
                int last = g_iGermanCreationQueueTailIndex - 1;
                g_iNumberOfEnemiesSpawned--;
                if (last < 0) {
                    last = GERMAN_QUEUE_SIZE - 1;
                }
                g_iGermanCreationQueueTailIndex = last;
                g_pGermanCreationQueue[last].m_trigger = NULL;
                g_pGermanCreationQueue[last].m_arg = -1;
                g_pGermanCreationQueue[last].m_framesWaited = 0;
            } else {
                guysLostCount = 0;
            }
        }
        return NULL;
    }
    g_numActiveSoldiers++;
    TriggerObjectData_struct* data = trigger->m_data;
    void* memory = g_pObjectAllocators[POOL_ACTORS].GetFreeElement(true);
    BSGO_Soldier* go = memory != NULL ? new (memory) BSGO_Soldier : NULL;
    Link(go, g_pGermanList);
    CSoldierObject* soldier = (CSoldierObject*)g_AnimObjFactory.AllocateObject(trigger->m_data->field_0x48, (EObjectClass)1);
    int collision;
    switch (data->m_team) {
    case 1:
        collision = 5;
        break;
    case 2:
        collision = 2;
        g_iNumberOfEnemiesSpawned++;
        break;
    case 0:
        collision = 7;
        break;
    default:
        collision = -1;
        DebugMsg("Unknown team affiliation\n");
        break;
    }
    soldier->SetCollisionId((EClsnId)collision);
    int primary = *(int*)((u8*)data + 0x98);
    if (primary == 0) {
        soldier->AddWeapon(0, WEAPON_DEFAULT_RIFLE);
    } else {
        soldier->AddWeapon(0, primary);
    }
    soldier->AddWeapon(1, *(int*)((u8*)data + 0x9C));
    soldier->AddWeapon(2, *(int*)((u8*)data + 0xA0));
    CVector3 pos;
    pos.x = data->m_position[0];
    pos.y = data->m_position[1];
    pos.z = data->m_position[2];
    soldier->Reset();
    soldier->SetPosition(pos);
    CMatrix m;
    ((CQuaternion*)data->m_orientation)->GetMatrix(m);
    float rx;
    float ry;
    float rz;
    m.ToEulerXYZ(rx, ry, rz);
    soldier->Yaw(rz);
    g_scene.Add(*soldier);
    go->m_soldier = soldier;
    ClearProximity(go->m_proximity);
    go->m_scriptData[0] = data->field_0x70;
    const char* script = ChildScript(data);
    if (script != NULL) {
        CreateBSObject(script, &go->m_object, go, trigger, (void**)&go->m_scriptData[1]);
        soldier->SetScript(go->m_object);
        BSObjectTriggerEvent(go->m_object, BSEVENT_START, (void*)arg, NULL, false);
    } else {
        ReportMissingScript("Soldier", pos);
    }
    soldier->InitAI();
    ArmSoldier(soldier);
    // Head tracking limits (look direction, pitch and yaw ranges).
    CSoldierObject* look = soldier->m_aiDoodad.m_pSoldier;
    CVector3 up(0.0f, 0.0f, 1.0f);
    *(CVector3*)look->m_headPosition = up;
    look->m_lookLimits[0] = -1.2217305f;
    look->m_lookLimits[1] = 1.5707964f;
    look->m_lookLimits[2] = -0.34906584f;
    look->m_lookLimits[3] = 0.34906584f;
    ForceUpdateProximityTriggerStatus(go, false);
    return go;
}

void CAnimObject::SetScript(BSObject* obj) {
    m_pScriptObject = obj;
}

// Lets dump objects of `type` (and `weaponType`, -1 any) be picked up.
void EnableDumpCollisionsByType(EDumpType type, int weaponType, bool enable) {
    int collision = enable ? 0xE : -1;
    for (SDumpObject* d = g_DumpObjectContainer.m_head; d != NULL; d = d->m_next) {
        if (d->m_type == type && (weaponType == -1 || d->m_weaponType == weaponType)) {
            d->m_object->SetCollisionId((EClsnId)collision);
        }
    }
}

// Destroys a dump object's node and takes it out of the container.
static inline void RemoveDumpObject(SDumpObject* d) {
    d->m_object->MarkForDestruction(0);
    d->m_object = NULL;
    CDumpObjectContainer& c = g_DumpObjectContainer;
    if (c.m_head == d) {
        c.m_head = d->m_next;
        if (c.m_tail == d) {
            c.m_tail = NULL;
        } else {
            c.m_head->m_prev = NULL;
        }
    } else if (c.m_tail == d) {
        c.m_tail = d->m_prev;
        c.m_tail->m_next = NULL;
    } else {
        d->m_next->m_prev = d->m_prev;
        d->m_prev->m_next = d->m_next;
    }
    d->m_crc = 0;
    c.m_count--;
}

void DestroyOldestDumpObject() {
    if (g_DumpObjectContainer.m_count > 0) {
        RemoveDumpObject(g_DumpObjectContainer.m_head);
    }
}

void DestroyDumpObject(CStaticObject* obj) {
    CStaticObjectFactory::DeallocateStaticObject(obj);
    if (obj->GetLocalBoundingVolume((ISceneNode::EVolumeType)3) != NULL) {
        g_pVolumeAllocator.FreeElement((void*)obj->GetLocalBoundingVolume((ISceneNode::EVolumeType)3));
    }
    if (obj->GetWorldBoundingVolume((ISceneNode::EVolumeType)3) != NULL) {
        g_pVolumeAllocator.FreeElement((void*)obj->GetWorldBoundingVolume((ISceneNode::EVolumeType)3));
    }
}

void MarkDumpObjectForDestruction(void* dump) {
    RemoveDumpObject((SDumpObject*)dump);
}

// The dump object of `obj` and what it holds.
void* HitDumpObject(CStaticObject* obj, int* value, int* crc, EDumpType* type, int* weaponType) {
    SDumpObject* d = g_DumpObjectContainer.m_head;
    while (d->m_object != obj) {
        d = d->m_next;
    }
    *value = d->m_value;
    *crc = d->m_crc;
    *type = d->m_type;
    if (weaponType != NULL) {
        *weaponType = d->m_weaponType;
    }
    return d;
}

#define DUMP_HEALTH0 0x598E4105
#define DUMP_HEALTH1 0x72441AA1
#define DUMP_HEALTH2 0x10EFEBA2
#define DUMP_HEALTH3 0xF7BADDB9

// Drops a pickup of kind `crc` (a health pack or a weapon) holding `value`.
// It can be picked up if the player needs it; the oldest pickup makes room
// when 25 exist.
CStaticObject* CreateDumpObject(const CVector3& pos, const CQuaternion& orientation, int crc, int value) {
    CStaticObject* obj = CStaticObjectFactory::AllocateStaticObject(crc, -1);
    if (obj == NULL) {
        DebugMsg("\nERROR: Unable to create DumpObject type 0x%x.\n  CRP might not be in level file.\n", crc);
        DebugMsg("Or it's possible the static object pool is all used up\n\n");
        RemoveDumpObject(g_DumpObjectContainer.m_head);
        return NULL;
    }
    CMatrix m;
    CQuaternion q = orientation;
    q.GetMatrix(m);
    m.SetPos(pos);
    obj->SetTMLocalToWorld(m);
    u8* mesh = (u8*)obj->m_pMesh;
    obj->Init();
    CVolSphere* local = AllocateSphereVolume();
    CVolBox* bounds = *(CVolBox**)(*(u8**)(mesh + 4) + 0x20);
    float half = g_fDumpObjectCollisionRadius * 0.5f;
    CVector3 center(0.0f, 0.0f, half);
    center.z = half + bounds->m_center.z;
    center.x = 0.0f + bounds->m_center.x;
    center.y = 0.0f + bounds->m_center.y;
    local->SetCenter(center);
    local->SetRadius(g_fDumpObjectCollisionRadius);
    CVolSphere* world = AllocateSphereVolume();
    *world = *local;
    obj->SetLocalBoundingVolume((ISceneNode::EVolumeType)3, local);
    obj->SetLocalBoundingVolume((ISceneNode::EVolumeType)0, local);
    obj->SetWorldBoundingVolume((ISceneNode::EVolumeType)3, world);
    obj->SetWorldBoundingVolume((ISceneNode::EVolumeType)0, world);
    g_scene.Add(*obj);

    EDumpType type;
    int weaponType;
    switch ((u32)crc) {
    case DUMP_HEALTH0:
    case DUMP_HEALTH1:
    case DUMP_HEALTH2:
    case DUMP_HEALTH3: {
        // Health: only collectable when the player is hurt.
        BSObject* script = g_scene.GetPlayer(0)->GetScriptObject();
        TriggerObjectData_struct* data = script->m_trigger->m_data;
        if (*(int*)script->m_gameObject->GetScriptData() == data->field_0x70) {
            obj->SetCollisionId((EClsnId)-1);
        } else {
            obj->SetCollisionId((EClsnId)0xE);
        }
        type = (EDumpType)0;
        weaponType = 0;
        break;
    }
    default: {
        // A weapon: collectable when the player can carry more ammo.
        CPlayerObject* player = g_scene.GetPlayer(0);
        obj->SetCollisionId((EClsnId)-1);
        CWeapon* weapon = player->GetWeaponByCRC(crc)->m_pWeapon;
        if (g_bInMultiplayerMode ||
            (weapon != NULL && (u32)weapon->m_reserveAmmo < (u32)weapon->m_pProperties->m_maxAmmo)) {
            obj->SetCollisionId((EClsnId)0xE);
        }
        CSoundTable::SSoundParms parms;
        if (g_soundMapSingleton.GetSoundEvent(0x1C9, -1, 0x57, &parms)) {
            PlayScriptedSound(obj, pos, parms, -1, NULL);
        }
        weaponType = weapon->m_weaponType;
        type = (EDumpType)1;
        break;
    }
    }

    SDumpObject* d = g_DumpObjectContainer.m_objects;
    int i;
    for (i = 0; i < MAX_DUMP_OBJECTS && d->m_crc != 0; i++) {
        d++;
    }
    if (i == MAX_DUMP_OBJECTS) {
        d = NULL;
    }
    if (d == NULL) {
        d = g_DumpObjectContainer.m_head;
        RemoveDumpObject(d);
    }
    d->m_object = obj;
    d->m_crc = crc;
    d->m_value = value;
    d->m_type = type;
    d->m_weaponType = weaponType;
    d->m_next = NULL;
    d->m_prev = g_DumpObjectContainer.m_tail;
    if (g_DumpObjectContainer.m_tail != NULL) {
        g_DumpObjectContainer.m_tail->m_next = d;
    }
    g_DumpObjectContainer.m_tail = d;
    if (g_DumpObjectContainer.m_head == NULL) {
        g_DumpObjectContainer.m_head = d;
    }
    g_DumpObjectContainer.m_count++;
    obj->m_isDumpObject = 1;
    // Check its triggers now through a temporary game object.
    BSGO_Dummy dummy;
    dummy.m_object = NULL;
    dummy.m_pNext = NULL;
    dummy.m_pPrev = NULL;
    dummy.m_proximity[1] = 0;
    dummy.m_proximity[0] = 0;
    dummy.m_proximity[3] = 0;
    dummy.m_proximity[2] = 0;
    dummy.m_object = NULL;
    dummy.m_node = obj;
    ForceUpdateProximityTriggerStatus(&dummy, true);
    return obj;
}

BSObject* CPlayerObject::GetScriptObject() const {
    return *(BSObject**)((u8*)this + 0x250);
}

// Creates the object a trigger places (with `arg` passed to its script's
// start event, or for players the player index).
void* CreateObject(TriggerObject_struct* trigger, int arg, void* extra) {
    void* result = NULL;
    TriggerObjectData_struct* data = trigger->m_data;
    switch (data->m_type) {
    case 2:
        switch ((u32)data->field_0x48) {
        case 0x271BD574:  // player start
        case 0xFD092133:  // player restart
            result = CreatePlayer(trigger, (int)extra);
            break;
        default:
            result = CreateDestructable(trigger, arg);
            break;
        }
        break;
    case 8:
        result = CreateCollisionVolume(trigger, arg);
        break;
    case 3:
        result = CreateGerman(trigger, arg, true);
        break;
    case 4:
        result = CreateDestructable(trigger, arg);
        break;
    case 9:
        result = CParticleSystem::Create(data);
        if (*(f32*)((u8*)trigger->m_data + 0x88) != 0.0f) {
            result = NULL;
        }
        break;
    case 14:
        result = CLight::Create(data);
        if (trigger->m_data->field_0x70 == 0) {
            result = NULL;
        }
        break;
    case 0:
        DebugMsg("How is a UNIQUE_PROP trying to create something?\n");
        break;
    }
    return result;
}

static inline void RunDelayedDestroys(bool waitForFrames) {
    while (g_delayDestroyHead != g_delayDestroyTail) {
        SDelayDestroy* e = &g_pDelayDestroyList[g_delayDestroyHead];
        if (waitForFrames && (u32)(g_frameCounter - e->m_frame) < 2) {
            break;
        }
        BSGO_Basic* go = e->m_object;
        BSObject* obj = go->m_object;
        BSSendMessage(0x1B9, obj, obj->m_trigger, obj, obj);
        go->Destroy();
        e = &g_pDelayDestroyList[g_delayDestroyHead++];
        e->m_frame = 0;
        e->m_object = NULL;
        if (g_delayDestroyHead >= DELAY_DESTROY_SIZE) {
            g_delayDestroyHead = 0;
        }
    }
}

// Per frame: delayed destroys, then queued Germans (making room by removing
// far soldiers for those waiting more than 6 frames).
void UpdateObjectCreation() {
    RunDelayedDestroys(true);
    int waiting = 0;
    for (int i = g_iGermanCreationQueueHeadIndex; i != g_iGermanCreationQueueTailIndex; i = NextGermanIndex(i)) {
        SGermanCreation* e = &g_pGermanCreationQueue[i];
        e->m_framesWaited++;
        if (e->m_framesWaited > 6) {
            waiting++;
        }
    }
    NonVisDestructFarthestSoldiers(g_iNumGermansQueuedThisFrame + waiting);
    g_iNumGermansQueuedThisFrame = 0;
    while (g_iGermanCreationQueueHeadIndex != g_iGermanCreationQueueTailIndex) {
        SGermanCreation* e = &g_pGermanCreationQueue[g_iGermanCreationQueueHeadIndex];
        BSGO_Soldier* soldier = CreateGerman(e->m_trigger, e->m_arg, false);
        if (soldier == NULL) {
            break;
        }
        BSObjectTriggerEvent(g_pGermanCreationQueue[g_iGermanCreationQueueHeadIndex].m_trigger->m_object, 0xF3, soldier,
                             NULL, false);
        e = &g_pGermanCreationQueue[g_iGermanCreationQueueHeadIndex];
        g_iGermanCreationQueueHeadIndex++;
        e->m_trigger = NULL;
        e->m_arg = -1;
        e->m_framesWaited = 0;
        g_iGermanCreationQueueHeadIndex =
            g_iGermanCreationQueueHeadIndex >= GERMAN_QUEUE_SIZE ? 0 : g_iGermanCreationQueueHeadIndex;
    }
    g_frameCounter++;
}

void ShutdownObjectCreators() {
    RunDelayedDestroys(false);
    g_pBoxList = NULL;
    g_pGermanList = NULL;
    g_pTankList = NULL;
}

static inline int Align16(int size) {
    return size + ((0x10 - (size & 0xF)) & 0xF);
}

static inline int PoolSizes() {
    int size = g_pObjectAllocators[POOL_DESTRUCTABLES].ComputeNeededSize(sizeof(BSGO_Destructable), 200, 0);
    size = Align16(size + g_pObjectAllocators[POOL_ACTORS].ComputeNeededSize(sizeof(BSGO_Soldier), 25, 0));
    size = Align16(size + g_pVolumeAllocator.ComputeNeededSize(0x50, 450, 0x10));
    size = Align16(size + g_pEnvModMechAllocator.ComputeNeededSize(0x80, 50, 0x10));
    return size + g_pAnimatedVolumeAllocator.ComputeNeededSize(0x7C0, 50, 0x10);
}

static inline u8* AlignPtr16(u8* p) {
    return p + ((0x10 - ((s32)p & 0xF)) & 0xF);
}

void InitObjectCreators() {
    u8* memory = (u8*)BSUtilGetMemory(PoolSizes(), 0x10);
    g_pObjectAllocators[POOL_DESTRUCTABLES].Init(memory, sizeof(BSGO_Destructable), 200, 0);
    memory += g_pObjectAllocators[POOL_DESTRUCTABLES].ComputeNeededSize(sizeof(BSGO_Destructable), 200, 0);
    g_pObjectAllocators[POOL_ACTORS].Init(memory, sizeof(BSGO_Soldier), 25, 0);
    memory = AlignPtr16(memory + g_pObjectAllocators[POOL_ACTORS].ComputeNeededSize(sizeof(BSGO_Soldier), 25, 0));
    g_pVolumeAllocator.Init(memory, 0x50, 450, 0x10);
    memory = AlignPtr16(memory + g_pVolumeAllocator.ComputeNeededSize(0x50, 450, 0x10));
    g_pEnvModMechAllocator.Init(memory, 0x80, 50, 0x10);
    memory = AlignPtr16(memory + g_pEnvModMechAllocator.ComputeNeededSize(0x80, 50, 0x10));
    g_pAnimatedVolumeAllocator.Init(memory, 0x7C0, 50, 0x10);
    g_pAnimatedVolumeAllocator.ComputeNeededSize(0x7C0, 50, 0x10);
    int i;
    for (i = 0; i < DELAY_DESTROY_SIZE; i++) {
        g_pDelayDestroyList[i].m_frame = 0;
        g_pDelayDestroyList[i].m_object = NULL;
    }
    g_pBoxList = NULL;
    g_pGermanList = NULL;
    g_pTankList = NULL;
    g_frameCounter = 3;
    g_delayDestroyHead = 0;
    g_delayDestroyTail = 0;
    g_numActiveSoldiers = 0;
    g_maxActiveSoldiers = TLT_FindNextResourceByType((TLTResourceID)0xD, NULL)->fields[LS_MAX_SOLDIERS].value;
    g_iNumGermansQueuedThisFrame = 0;
    g_iGermanCreationQueueHeadIndex = 0;
    g_iGermanCreationQueueTailIndex = 0;
    for (i = 0; i < GERMAN_QUEUE_SIZE; i++) {
        g_pGermanCreationQueue[i].m_trigger = NULL;
        g_pGermanCreationQueue[i].m_arg = -1;
        g_pGermanCreationQueue[i].m_framesWaited = 0;
    }
    g_fDumpObjectCollisionRadius =
        *(f32*)&TLT_FindNextResourceByType((TLTResourceID)0xB, NULL)->fields[LS_DUMP_RADIUS].value;
    memset(g_DumpObjectContainer.m_objects, 0, MAX_DUMP_OBJECTS * sizeof(SDumpObject));
    g_DumpObjectContainer.m_tail = NULL;
    g_DumpObjectContainer.m_head = NULL;
    g_DumpObjectContainer.m_count = 0;
    g_iNumberOfEnemiesSpawned = 0;
}

int CreateObjectGetMemoryRequirements() {
    return PoolSizes();
}

CDumpObjectContainer::~CDumpObjectContainer() {
    delete[] m_objects;
    m_objects = NULL;
}

// Weak methods emitted by this unit.
ISceneNode* BSGO_Dummy::GetSceneNode() {
    return m_node;
}

void* BSGO_Dummy::GetProximityData() {
    return m_proximity;
}

void* BSGO_Destructable::GetScriptData() {
    return m_scriptData;
}

ISceneNode* BSGO_Destructable::GetSceneNode() {
    return m_node;
}

void* BSGO_Destructable::GetProximityData() {
    return m_proximity;
}

void* BSGO_Tank::GetScriptData() {
    return m_scriptData;
}

ISceneNode* BSGO_Tank::GetSceneNode() {
    return (ISceneNode*)m_tank;
}

void* BSGO_Tank::GetProximityData() {
    return m_proximity;
}

void* BSGO_Soldier::GetScriptData() {
    return m_scriptData;
}

ISceneNode* BSGO_Soldier::GetSceneNode() {
    return (ISceneNode*)m_soldier;
}

void* BSGO_Soldier::GetProximityData() {
    return m_proximity;
}

void* BSGO_Soldier::GetEmbeddedHitReactionInfo() {
    return (u8*)this + 0x98;
}
