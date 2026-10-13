// The AI filter layer: each AI-driven game object (soldier, tank, player)
// has a filter linking its doodad, its CAIObject (built in place after the
// filter) and its script, copying state between game and AI each frame.
// Also the global AI services (CAIFilterGlobal): time, environment line
// checks, cover points, waypoints, path nodes, spline paths and sounds.
// Hand-maintained.
#ifndef _GAME_AIFILTER_H_
#define _GAME_AIFILTER_H_

#include "types.h"
#include "game_types.h"
#include "game/fiber.h"

struct BPDPathFindingNode;
struct BPDPolyPath;
struct BSObject;
class BSGO_Basic;
struct CPathNode;
class CAIFilterGlobal;
class CAIFilterObject;
class CAIFilterPlayerObject;
class CAIFilterRealEulerDirection;
class CAIFilterRealPosition;
class CAIFilterRealVector3;
class CAIFilterSoldierObject;
struct CAIFilterSoundInfo;
class CAIFilterTankObject;
class CAIObject;
class CCoverPoint;
class ISceneNode;
class IVolume;
class MechanicCollisionChecker;
struct _PropBSPLeaf;
struct aistatus_match;
struct TriggerObject_struct;
class CAIDoodad;
class CAISplinePath;
class CAISplinePathManager;
class CPathNodeList;
struct _aifilter_simple_v3;

// A level poly path (BPDHeader::m_polyPaths), points big-endian until
// CreateSplinePathManager swaps them.
struct BPDPolyPath {
    /* 0x0 */ u32 m_id;
    /* 0x4 */ s32 m_numPoints;
    /* 0x8 */ _aifilter_simple_v3* m_points;  // offset from the file until patched
};

// size >= 0xC (from field accesses); vtable 0x80183800
class CAIFilterObject {
public:
    // vtable pointer at 0x0
    virtual ~CAIFilterObject();  // 0x8005BE5C
    // The chosen target's script, or NULL.
    virtual BSObject* ChooseTarget(float);  // 0x8005BE54
    virtual void SetTargetMode(aifilter_target_mode);  // 0x8005BE50
    virtual void ResetTargetMatchList(aifilter_target_mode);  // 0x8005BE4C
    virtual void SetupTargetMatchList(aifilter_target_mode, aistatus_match*, int);  // 0x8005BE48
    virtual void SetMoveTarget(const CAIFilterRealPosition&);  // 0x8004A0D8
    virtual void GetLookDirection(CAIFilterRealVector3&, CAIFilterRealVector3&);  // 0x80048560
    /* 0x4 */ CAIDoodad* m_pDoodad;  // the game side (LinkGameObject)
    /* 0x8 */ CAIObject* m_pAIObject;
    bool ShouldILeaveMMG(BSObject*);  // 0x8005A4D0
    BSObject* ChooseMMGPoint();  // the mounted gun point's script  // 0x8005A4F8
    void GetTargetGuessPosition(CAIFilterRealPosition&);  // 0x8005A530
    BSObject* GetTargetScript() const;  // 0x8005BE1C
    void LinkGameObject(void*);  // 0x8005BE40
    bool FindRunAwayPoint(const ISceneNode*, float, CAIFilterRealPosition*);  // 0x8005C9C8
};

// size >= 0xC (from field accesses)
class CAIFilterRealPosition {
public:
    /* 0x0 */ f32 field_0x0;
    /* 0x4 */ f32 field_0x4;
    /* 0x8 */ f32 field_0x8;
    /* 0xC */ u8 unk_0xC[0x4];
    CAIFilterRealPosition() {}  // 0x800523C8
    float GetDistanceSquaredXYReal(const CAIFilterRealPosition&) const;  // 0x8005A6D0
    float GetDistanceSquaredXYZReal(const CAIFilterRealPosition&) const;  // 0x8005A6F4
    float GetDistanceXYReal(const CAIFilterRealPosition&) const;  // 0x8005A728
    float GetDistanceXYZReal(const CAIFilterRealPosition&) const;  // 0x8005A7B0
};

// size 0x18 (operator new); vtable 0x801836BC
class CAIFilterPlayerObject : public CAIFilterObject {
public:
    /* 0xC */ BSObject* m_pBSObject;
    /* 0x10 */ BSGO_Basic* m_pGameObject;
    /* 0x14 */ f32 m_nextSeenCheck;  // time of the next "AI sees the player" check
    virtual void GetLookDirection(CAIFilterRealVector3&, CAIFilterRealVector3&);  // 0x8005A410
    void UpdateFromGameObject();  // 0x8005A844
    void Update(float);  // 0x8005AA2C
    void PreUpdate();  // 0x8005ABBC
    void UpdateManualUpdateVars();  // 0x8005ABFC
    void SetOneTimeVars();  // 0x8005AC00
    virtual ~CAIFilterPlayerObject();  // 0x8005ACE0
    CAIFilterPlayerObject();  // 0x8005AD6C
};

// size >= 0xC (from field accesses)
class CAIFilterRealEulerDirection {
public:
    /* 0x0 */ f32 field_0x0;
    /* 0x4 */ u8 unk_0x4[0x4];
    /* 0x8 */ f32 field_0x8;
    void CalculateAngleDiff(const CAIFilterRealPosition&, const CAIFilterRealPosition&, float*, float*) const;  // 0x8005A568
};

// size 0x10 (copied with two lfd/stfd in ObstacleAvoidance)
class CAIFilterRealVector3 {
public:
    /* 0x0 */ f32 field_0x0;
    /* 0x4 */ f32 field_0x4;
    /* 0x8 */ f32 field_0x8;
    /* 0xC */ u8 unk_0xC[0x4];
    void RotateAboutZ(float);  // 0x8005A650
};

// size 0x14 (operator new); vtable 0x801837AC
class CAIFilterTankObject : public CAIFilterObject {
public:
    /* 0xC */ BSObject* m_pBSObject;
    /* 0x10 */ BSGO_Basic* m_pGameObject;
    void UpdateFromGameObject();  // 0x8005ADD4
    void Update(float);  // 0x8005AF98
    void PreUpdate();  // 0x8005B068
    virtual void SetupTargetMatchList(aifilter_target_mode, aistatus_match*, int);  // 0x8005B0A8
    virtual void ResetTargetMatchList(aifilter_target_mode);  // 0x8005B0CC
    virtual void SetTargetMode(aifilter_target_mode);  // 0x8005B0F0
    virtual BSObject* ChooseTarget(float);  // 0x8005B0FC
    void UpdateManualUpdateVars();  // 0x8005B15C
    void SetOneTimeVars();  // 0x8005B160
    void* operator new(unsigned long, void*);  // 0x8005B390
    void operator delete(void*) {}  // lives inside the tank object
    virtual ~CAIFilterTankObject();  // 0x8005B398
    CAIFilterTankObject();  // 0x8005B408
    virtual void GetLookDirection(CAIFilterRealVector3&, CAIFilterRealVector3&);  // 0x8005DBA8
};

// size 0x14 (operator new); vtable 0x801837DC
class CAIFilterSoldierObject : public CAIFilterObject {
public:
    /* 0xC */ BSObject* m_pBSObject;
    /* 0x10 */ BSGO_Basic* m_pGameObject;
    void UpdateToGameObject(float);  // 0x8005B494
    void UpdateFromGameObject();  // 0x8005B62C
    void Update(float);  // 0x8005B894
    void PreUpdate();  // 0x8005B90C
    virtual void SetupTargetMatchList(aifilter_target_mode, aistatus_match*, int);  // 0x8005B94C
    virtual void ResetTargetMatchList(aifilter_target_mode);  // 0x8005B970
    virtual void SetTargetMode(aifilter_target_mode);  // 0x8005B994
    virtual BSObject* ChooseTarget(float);  // 0x8005B9BC
    void UpdateManualUpdateVars();  // 0x8005BA1C
    void SetOneTimeVars();  // 0x8005BB00
    void* operator new(unsigned long, void*);  // 0x8005BD2C
    void operator delete(void*) {}  // lives inside the soldier object
    virtual ~CAIFilterSoldierObject();  // 0x8005BD34
    CAIFilterSoldierObject();  // 0x8005BDA4
    virtual void SetMoveTarget(const CAIFilterRealPosition&);  // 0x8005DC64
    virtual void GetLookDirection(CAIFilterRealVector3&, CAIFilterRealVector3&);  // 0x8005DC8C
};

// A noise heard by the AIs (CAIFilterGlobal::PropagateSoundToAllObjects).
struct CAIFilterSoundInfo {
    /* 0x00 */ void* m_source;  // the scene node (or trigger owner) making it
    /* 0x04 */ CAIFilterObject* m_sourceFilter;
    /* 0x08 */ CAIFilterRealPosition m_position;
    /* 0x18 */ s16 m_type;  // 0x33 footstep-like (silent while disguised), 0x34 noise, ...
    /* 0x1C */ BSObject* m_sourceScript;
    /* 0x20 */ f32 m_radius;
    /* 0x24 */ f32 m_volume;  // 0..6
    /* 0x28 */ bool field_0x28;
    /* 0x2C */ s32 field_0x2C;
};

// size >= 0x14 (from field accesses)
class CAIFilterGlobal {
public:
    /* 0x0 */ u32 m_frame;
    /* 0x4 */ f32 m_time;  // game time (seconds), from UpdateFromGlobals
    /* 0x8 */ u8 unk_0x8[0x4];
    /* 0xC */ CAISplinePathManager* m_splinePaths;
    /* 0x10 */ CPathNodeList* m_pathNodes;
    /* 0x14 */ s32 m_largestAIObjectSize;  // the anim object pool's element size

    CAIFilterGlobal()
        : m_frame(0), m_time(0.0f), m_splinePaths(NULL), m_pathNodes(NULL), m_largestAIObjectSize(0x4CA0) {}
    static float GetCollisionDistLineToEnvironment(const CAIFilterRealPosition&, const CAIFilterRealPosition&, CAIFilterObject*, float);  // 0x8005BEA4
    static bool CheckCollisionLineToEnvironment(const CAIFilterRealPosition&, const CAIFilterRealPosition&, CAIFilterObject*);  // 0x8005C17C
    static int CheckVisionLineToEnvironment(const CAIFilterRealPosition&, const CAIFilterRealPosition&, CAIFilterObject*);  // 0x8005C370
    TriggerObject_struct* GetClosestWaypointByType(const _PropBSPLeaf*, const CAIFilterRealPosition&, DWI_OBJECT_CRC_ENUM, bool, float) const;  // 0x8005C6B8
    CCoverPoint* ChooseCoverPointByType(const _PropBSPLeaf*, const CAIFilterRealPosition&, const CAIFilterRealPosition*, ECoverPointSelectionType, float, float, CAIObject*) const;  // 0x8005CA88
    CCoverPoint* GetClosestUsableCoverPoint(const _PropBSPLeaf*, const CAIFilterRealPosition&, const CAIFilterRealPosition*, COVERPOINT_WALK_MODE*, COVERPOINT_TYPE*, CAIFilterRealPosition*, float, CAIObject*) const;  // 0x8005D0E8
    int GetCoverPointIndex(CCoverPoint*) const;  // 0x8005D470
    CCoverPoint* GetCoverPointByIndex(int) const;  // 0x8005D498
    void InvalidateCoverPointByID(int);  // 0x8005D4AC
    void DisableCollidingAStarPathNodes(const IVolume*);  // 0x8005D5D4
    void EnableAllAStarPathNodes();  // 0x8005D5F8
    CPathNode* GetClosestAStarPathNode(const CAIFilterRealPosition&, const CAIFilterRealVector3&) const;  // 0x8005D61C
    CPathNode* GetAStarPathNodeByIndex(int) const;  // 0x8005D640
    void CreateAStarPathNodeList(void*, BPDPathFindingNode*, int);  // 0x8005D678
    CAISplinePath* GetSplinePath(unsigned long, bool) const;  // 0x8005D6D4
    void ResetSplinePathManager(void*, BPDPolyPath*, int);  // 0x8005D72C
    void CreateSplinePathManager(void*, BPDPolyPath*, int);  // 0x8005D760
    static void PropagateSoundToAllObjects(const CAIFilterSoundInfo&);  // 0x8005D8B4
    void UpdateToGlobals();  // 0x8005DB14
    void UpdateFromGlobals(float);  // 0x8005DB18
    ~CAIFilterGlobal();  // 0x8005DB6C
    static float s_rNextSoundPropagateTime;  // 0x8034E928
};

// size >= 0x70 (from field accesses); vtable 0x80183830
// Tests a line against the static scene objects (mechanics).
class MechanicCollisionChecker : public dwi::IVisitor<ISceneNode> {
public:
    MechanicCollisionChecker(const CVolFiber& fiber, bool stopOnHit, bool skipSome, float length,
                             ISceneNode* ignore)
        : m_fiber(fiber), m_stopOnHit(stopOnHit), m_skipSome(skipSome), m_length(length), m_closest(length),
          m_ignore(ignore) {}
    /* 0x4 */ u8 unk_0x4[0xC];
    /* 0x10 */ CVolFiber m_fiber;  // size 0x50
    /* 0x60 */ bool m_stopOnHit;  // what Visit returns on a hit
    /* 0x61 */ bool m_skipSome;   // ignore collision ids 15 and 17 too
    /* 0x62 */ u8 unk_0x62[0x2];
    /* 0x64 */ f32 m_length;      // > 0: track the closest hit distance
    /* 0x68 */ f32 m_closest;
    /* 0x6C */ ISceneNode* m_ignore;
    virtual ~MechanicCollisionChecker();  // 0x8005C104
    virtual bool Visit(ISceneNode&);  // 0x8005C570
};

extern CAIFilterGlobal g_aigAIFilterGlobalObject;  // 0x801C6CD4
#endif
