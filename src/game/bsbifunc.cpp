// Script built-in functions (BIFunc_*): the engine side of the level scripts.
// BSInitBIFuncs binds each function the script code imports (by CRC) to its
// implementation in g_BIFCRCArray. Also the periodic AI checks scripts can
// register (distance to target, target search/check, corpse search, buddy
// check, mounted-gun points), the hint and prompt text, and fog/vibration/
// particle/sound helpers.
#include "game/bsbifunc.h"
#ifdef FRONTLINE_NATIVE
#include "native/ai.h"
#endif
#include "game/AIDoodad.h"
#include "game/AIFilter.h"
#include "game/AIObject.h"
#include "game/AISplinePath.h"
#include "game/BSGO_Basic.h"
#include "game/CoverPoint.h"
#include "game/ISceneNode.h"
#include "game/PathNode.h"
#include "game/PathNodeList.h"
#include "game/anim_obj.h"
#include "game/bsbif.h"
#include "game/bsobject.h"
#include "game/bstypes.h"
#include "game/camera.h"
#include "game/matrix.h"
#include "game/quaternion.h"
#include "game/vector.h"
#include "game/Moh2.h"
#include "game/objcreate.h"
#include "game/CScreenFlash.h"
#include "game/Soldier_object.h"
#include "game/UserInterface.h"
#include "game/bsmachin.h"
#include "game/bsmessage.h"
#include "game/bstimer.h"
#include "game/color.h"
#include "game/fader.h"
#include "game/player.h"
#include "game/scene.h"
#include "game/ShellMenu.h"
#include "game/static_obj.h"
#include "game/weapon.h"
#include "game/MathFun.h"
#include "game/bsschedule.h"
#include "game/hierobject.h"
#include "game/light.h"
#include "game/particlesystem.h"
#include "game/thrown_obj.h"
#include "game/propdat.h"
#include "game/sound.h"
#include "game/soundschedule.h"
#include "game/Screen.h"
#include "game/popup.h"
#include "game/pumhandler.h"
#include "game/Pause.h"
#include "game/buddy.h"
#include "game/input.h"
#include "game/proxtrig.h"
#include "game/bullet.h"
#include "game/explosion.h"
#include "game/impact.h"
#include "game/jaw_controller.h"
#include "stdlib.h"
#include "game/attachobject.h"
#include "game/bulletfactory.h"
#include "game/mainloop.h"
#include "game/weaponfactory.h"
#include "game/player_weapon_object.h"
#include "game/thrownbullet.h"
#include "game/propdat.h"
#include "game/bsevents.h"
#include "game/soundtable.h"
#include "game/toilet.h"
#include "game/Tank_object.h"
#include "game/animplayer.h"
#include "game/alarm.h"
#include "game/endian.h"
#include "game/staticmesh.h"
#include "math.h"
#include "string.h"

// The periodic AI checks scripts register.
static BSSchedule g_DistanceToTargetSchedule;
static BSSchedule g_TargetSearchSchedule;
static BSSchedule g_TargetCheckSchedule;
static BSSchedule g_CorpseSearchSchedule;
static BSSchedule g_BuddyPlayerCheckSchedule;
static BSSchedule g_MMGPointCheckSchedule;
#include "stdio.h"

// What scripts pass for "no object" besides NULL.
#define BS_INVALID_OBJECT ((BSObject*)-1)

// The last-pushed argument `k` words below the top (scripts that pass a
// fixed number of arguments are read this way).
static inline int& BIFTop(int** sp, int k) {
    return (*sp)[-k];
}

static inline float BIFArgF(int** sp, int i) {
    return *(float*)(*sp - (BIFCurrent().m_numArgs - 1 - i));
}

static inline void BIFReturnF(int** sp, float value) {
    *(float*)*sp = value;
}

// The AI of the object running the script.
static inline CAIFilterObject* SelfFilter(void* self) {
    return ((ISceneNode*)self)->GetAIDoodad()->m_pFilter;
}

static inline CAIObject* SelfAI(void* self) {
    return SelfFilter(self)->m_pAIObject;
}

// The AI filter behind a script object, if it has one.
static inline CAIFilterObject* GetAIFilter(BSObject* obj) {
    ISceneNode* node;
    CAIDoodad* doodad;
    if (obj != NULL && obj->m_gameObject != NULL && (node = obj->m_gameObject->GetSceneNode()) != NULL &&
        (doodad = node->GetAIDoodad()) != NULL) {
        return doodad->m_pFilter;
    }
    return NULL;
}

// The AI object behind a script object, if it has one.
static inline CAIObject* GetAIObject(BSObject* obj) {
    CAIFilterObject* filter = GetAIFilter(obj);
    if (filter != NULL) {
        return filter->m_pAIObject;
    }
    return NULL;
}

// The script object of an AI soldier (its owner filter's).
static inline BSObject* AIScript(CAIObject* ai) {
    return ((CAIFilterSoldierObject*)ai->m_pFilter)->m_pBSObject;
}

// The running object's AI script data.
static inline BSAIScriptData* SelfScriptData(void* self) {
    return (BSAIScriptData*)((ISceneNode*)self)->GetScriptObject()->m_gameObject->GetScriptData();
}

BSBuiltInFunction_struct* g_pBuiltInFunctions;
int g_iCurrentBIFIndex;  // the function being called
int g_pNumBuiltInFunctions;
SoundSchedule g_SoundSchedule;
PopUpMessageHandler* g_pPopUpMessageHandler[4];  // one per player
CScreenFlash g_screenflash(CColor(0x80, 0x80, 0x80, 0xFF), CColor(0x80, 0x80, 0x80, 0), 10.0f);

// The one-shot sound bank loaded (see BIFunc_LoadOneShotSoundBank).
int g_CurrentMission = -1;
int g_CurrentLevel = -1;
int g_CurrentSoundBank = -1;
int g_levelWeaponSetID = 1;

void BSBifuncInvalid(int** sp, void*) {
    DebugMsg("WARNING: Your engine has no implementation for BIF CRC 0x%x\n...ignoring\n", BIFCurrent().m_crc);
    DebugMsg("  look in source\\ai_script\\bsbifunc.h for string name of BIF CRC\n");
    BIFPop(sp);
}

// Debug: walk towards the camera.
void BIFunc_DoSomethingInteresting(int** sp, void* self) {
    CVector3 pos;
    g_camera.GetPosition(pos);
    ((CAnimObject*)self)->SetMoveTarget(pos);
    BIFPop(sp);
}

void BIFunc_AISetTargetMode(int** sp, void* self) {
    SelfFilter(self)->SetTargetMode((aifilter_target_mode)BIFTop(sp, 0));
    BIFPop(sp);
}

void BIFunc_AIResetTargetMatchList(int** sp, void* self) {
    SelfFilter(self)->ResetTargetMatchList((aifilter_target_mode)BIFTop(sp, 0));
    BIFPop(sp);
}

// A match list argument is the address of a 32-bit slot holding the list's
// address: a pointer in the match list struct (CAIDoodad) or a script variable.
#define BIF_MATCH_LIST(slot) ADDR_TO_PTR(aistatus_match, *(slot))

void BIFunc_AISetupTargetMatchList(int** sp, void* self) {
    CAIFilterObject* filter = SelfFilter(self);
    filter->SetupTargetMatchList((aifilter_target_mode)BIFArg(sp, 0),
                                 BIF_MATCH_LIST(ADDR_TO_PTR(u32, BIFArg(sp, 1))), BIFArg(sp, 2));
    BIFPop(sp);
}

void BIFunc_AIChooseTarget(int** sp, void* self) {
    float dt = BIFArgF(sp, 0);
    int target = (int)PTR_TO_ADDR(SelfFilter(self)->ChooseTarget(dt));
    BIFPop(sp);
    BIFReturn(sp, target);
}

// Targets the object (AI or not); returns false (and clears the target) for
// no object.
void BIFunc_AIChooseTargetByID(int** sp, void* self) {
    CAIObject* ai = SelfAI(self);
    BSObject* obj = ADDR_TO_PTR(BSObject, BIFArg(sp, 0));
    bool result;
    if (obj == NULL || obj == BS_INVALID_OBJECT) {
        ai->SetTarget(NULL);
        result = false;
    } else {
        CAIObject* target = GetAIObject(obj);
        if (target != NULL) {
            ai->SetTarget(target);
        } else {
            ai->SetNonAITarget(obj);
        }
        result = true;
    }
    BIFPop(sp);
    BIFReturn(sp, result);
}

void BIFunc_AISetHearingRadius(int** sp, void* self) {
    CAIObject* ai = SelfAI(self);
    float radius = BIFArgF(sp, 0);
    if (radius <= 0.001f) {
        printf("Warning: Hearing Radius is zero, changing to default\n");
        radius = 20.0f;
    }
    // Rounded to 1/16 unit.
    ai->m_hearingRadius = 0.0625f * (s16)(16.0f * radius);
    BIFPop(sp);
}

void BIFunc_AIGetSoldierMatchListStruct(int** sp, void* self) {
#ifdef FRONTLINE_NATIVE
    int list = (int)PTR_TO_ADDR(&((ISceneNode*)self)->GetAIDoodad()->m_pBuffer0);
#else
    int list = (int)PTR_TO_ADDR(((u8*)((ISceneNode*)self)->GetAIDoodad() + 0x10));
#endif
    BIFPop(sp);
    BIFReturn(sp, list);
}

void BIFunc_AIGetSoldierPointer(int** sp, void* self) {
    int obj = (int)PTR_TO_ADDR(((ISceneNode*)self)->GetScriptObject());
    BIFPop(sp);
    BIFReturn(sp, obj);
}

void BIFunc_AIStartSplinePathWalk(int** sp, void* self) {
    int reverse = BIFArg(sp, 1);
    u32 id = BIFArg(sp, 0);
    CAIFilterObject* filter = SelfFilter(self);
    filter->m_pAIObject->StartSplinePathWalkForward(
        (CAISplinePath*)g_aigAIFilterGlobalObject.GetSplinePath(id, reverse != 0));
    BIFPop(sp);
}

void BIFunc_AIStopSplinePathWalk(int** sp, void* self) {
    SelfAI(self)->m_moveMode = 0;
    BIFPop(sp);
}

void BIFunc_AIContinueSplinePathWalk(int** sp, void* self) {
    CAIObject* ai = SelfAI(self);
    switch (ai->m_splinePath.m_state) {
    case 1:
    case 3:
    case 5:
        ai->m_moveMode = 1;
        break;
    case 2:
    case 4:
    case 6:
        ai->m_splinePath.ContinueTraversalForwardAStar(ai->m_pFilter);
        ai->m_moveMode = 1;
        break;
    }
    BIFPop(sp);
}

void BIFunc_AIGetDistanceToTargetXY(int** sp, void* self) {
    CAIObject* ai = SelfAI(self);
    float dist = ai->GetDistanceSquaredXYReal(ai->m_pTarget);
    BIFPop(sp);
    BIFReturnF(sp, dist);
}

void BIFunc_AIGetDistanceToTargetXYZ(int** sp, void* self) {
    CAIObject* ai = SelfAI(self);
    float dist;
    if (ai->m_nonAITarget) {
        dist = ai->GetDistanceSquaredXYZReal();
    } else {
        dist = ai->GetDistanceSquaredXYZReal(ai->m_pTarget);
    }
    BIFPop(sp);
    BIFReturnF(sp, dist);
}

void BIFunc_AIEnableTargetAim(int** sp, void*) {
    BIFPop(sp);
}

void BIFunc_AIIsItAGoodTimeToFire(int** sp, void* self) {
    ((ISceneNode*)self)->GetAIDoodad();
    BIFPop(sp);
    BIFReturn(sp, 1);
}

void BIFunc_AIStartAStarPathWalk(int** sp, void* self) {
    BSObject* obj = ADDR_TO_PTR(BSObject, BIFArg(sp, 0));
    CAIObject* target = NULL;
    CAIObject* ai = SelfAI(self);
    target = GetAIObject(obj);
    bool started;
    if (target != NULL) {
        started = ai->StartAStarPathWalk(target);
    } else {
        started = false;
    }
    BIFPop(sp);
    BIFReturn(sp, started);
}

void BIFunc_AIStartAStarPathWalkWithCurrentTarget(int** sp, void* self) {
    CAIObject* ai = SelfAI(self);
    bool started = ai->StartAStarPathWalk(ai->m_pTarget);
    BIFPop(sp);
    BIFReturn(sp, started);
}

void BIFunc_AIStartAStarPathWalkToMovePoint(int** sp, void* self) {
    bool started = SelfAI(self)->StartAStarPathWalk();
    BIFPop(sp);
    BIFReturn(sp, started);
}

void BIFunc_AIStopAStarPathWalk(int** sp, void* self) {
    CAIObject* ai = SelfAI(self);
    ai->m_pCurNode = 0;
    ai->m_pGoalNode = 0;
    ai->m_pNextNode = 0;
    ai->m_destination.field_0x0 = ai->m_movePoint.field_0x0;
    ai->m_destination.field_0x4 = ai->m_movePoint.field_0x4;
    ai->m_destination.field_0x8 = ai->m_movePoint.field_0x8;
    ai->m_lastDistSq = ai->m_position.GetDistanceSquaredXYZReal(ai->m_destination);
    ai->m_approaching = 0;
    ai->m_skippedNode = 0;
    ai->m_moveMode = 0;
    ai->CleanupAStarPathWalk();
    BIFPop(sp);
}

void BIFunc_AIContinueAStarPathWalk(int** sp, void* self) {
    SelfAI(self)->m_moveMode = 2;
    BIFPop(sp);
}

void BIFunc_AIStartCoverPointWalk(int** sp, void* self) {
    bool started = SelfAI(self)->StartCoverPointWalkToStored(true);
    BIFPop(sp);
    BIFReturn(sp, started);
}

void BIFunc_AIStartCoverPointWalkToStored(int** sp, void* self) {
    int direct = BIFTop(sp, 0);
    bool started = SelfAI(self)->StartCoverPointWalkToStored(direct != 0);
    BIFPop(sp);
    BIFReturn(sp, started);
}

void BIFunc_AICommitCoverPointWalkToStored(int** sp, void* self) {
    SelfAI(self)->CommitCoverPointWalkToStored();
    BIFPop(sp);
}

void BIFunc_AIStopCoverPointWalk(int** sp, void* self) {
    CAIObject* ai = SelfAI(self);
    if (ai->m_atCoverPoint) {
        ai->m_moveMode = 5;
    } else {
        ai->m_moveMode = 0;
    }
    BIFPop(sp);
}

// Snaps to the cover position (with `angle` if `useAngle`); too far away
// (over 0.6) sends event 0x42 instead.
void BIFunc_AISnapToCoverPointPosition(int** sp, void* self) {
    bool useAngle = BIFTop(sp, 1) != 0;
    float angle = *(float*)&BIFTop(sp, 0);
    CAIObject* ai = SelfAI(self);
    if (ai->m_atCoverPoint) {
        ai->m_pCoverPoint->GetModifiedPosition(*(CAIFilterRealVector3*)&ai->m_snapPosition,
                                               (COVERPOINT_TYPE)ai->m_coverType);
        ai->m_snapToAngle = useAngle;
        ai->m_snapAngle = angle;
        if (ai->m_position.GetDistanceXYReal(ai->m_snapPosition) > 0.6f) {
            BSObjectTriggerEvent(AIScript(ai), 0x42, NULL, NULL, false);
        } else {
            ai->field_0x1A8 |= 2;
        }
    }
    BIFPop(sp);
}

void BIFunc_AIInvalidateCoverPointByID(int** sp, void*) {
    int id = BIFArg(sp, 0);
    if (id != 0) {
        g_aigAIFilterGlobalObject.InvalidateCoverPointByID(id);
    }
    BIFPop(sp);
}

void BIFunc_AIContinueCoverPointWalk(int** sp, void* self) {
    SelfAI(self)->m_moveMode = 9;
    BIFPop(sp);
}

// Whether the AI is at (not just heading for) cover point `index`.
void BIFunc_AIArrivedAtCoverpoint(int** sp, void* self) {
    int index = BIFArg(sp, 0);
    CAIFilterObject* filter = SelfFilter(self);
    CCoverPoint* cover = (CCoverPoint*)g_aigAIFilterGlobalObject.GetCoverPointByIndex(index);
    CAIObject* ai = filter->m_pAIObject;
    bool arrived = false;
    if (cover == ai->m_pCoverPoint && ai->m_atCoverPoint) {
        arrived = true;
    }
    BIFPop(sp);
    BIFReturn(sp, arrived);
}

void BIFunc_AIStartOrientTowardsCoverPoint(int** sp, void*) {
    BIFPop(sp);
}

void BIFunc_AIContinueOrientTowardsCoverPoint(int** sp, void*) {
    BIFPop(sp);
}

void BIFunc_AIStopOrientTowardsCoverPoint(int** sp, void*) {
    BIFPop(sp);
}

static inline int CoverPointIndex(CCoverPoint* cover) {
    return cover != NULL ? g_aigAIFilterGlobalObject.GetCoverPointIndex(cover) : -1;
}

void BIFunc_AIStoreBestCoverPoint(int** sp, void* self) {
    int index = CoverPointIndex(SelfAI(self)->StoreClosestCoverPoint());
    BIFPop(sp);
    BIFReturn(sp, index);
}

void BIFunc_AILeaveCoverPoint(int** sp, void* self) {
    CAIObject* ai = SelfAI(self);
    ai->field_0x1A8 &= ~2;
    CCoverPoint* cover = ai->m_pCoverPoint;
    if (cover != NULL) {
        cover->FlagAsUnoccupied((COVERPOINT_TYPE)ai->m_coverType);
        ai->m_pCoverPoint = NULL;
        ai->m_moveMode = 0;
        ai->m_atCoverPoint = false;
    }
    BIFPop(sp);
}

// Picks a cover point by `type`, against threat `threat` if `useThreat`,
// within the distance range; returns its index or -1.
void BIFunc_AIChooseCoverPoint(int** sp, void* self) {
    int useThreat = BIFArg(sp, 1);
    int type = BIFArg(sp, 0);
    float minDist = BIFArgF(sp, 3);
    float maxDist = BIFArgF(sp, 4);
    CAIFilterObject* filter = SelfFilter(self);
    int index;
    if (useThreat != 0) {
        index = CoverPointIndex(filter->m_pAIObject->ChooseCoverPoint(
            (ECoverPointSelectionType)type, ADDR_TO_PTR(ISceneNode, BIFArg(sp, 2)), minDist, maxDist));
    } else {
        index = CoverPointIndex(
            filter->m_pAIObject->ChooseCoverPoint((ECoverPointSelectionType)type, NULL, minDist, maxDist));
    }
    BIFPop(sp);
    BIFReturn(sp, index);
}

void BIFunc_AISetCoverPoint(int** sp, void* self) {
    int direct = BIFArg(sp, 1);
    int index = BIFArg(sp, 0);
    SelfAI(self)->SetCoverPoint(index, direct != 0);
    BIFPop(sp);
}

// Sets the move point to a spot `dist` away from `threat`; returns whether
// one was found.
void BIFunc_AISetRunAwayMovepoint(int** sp, void* self) {
    ISceneNode* threat = ADDR_TO_PTR(ISceneNode, BIFArg(sp, 0));
    float dist = BIFArgF(sp, 1);
    CAIDoodad* doodad = ((ISceneNode*)self)->GetAIDoodad();
    bool found = false;
    if (threat != NULL) {
        CAIFilterRealPosition pos;
        CAIFilterObject* filter = doodad->m_pFilter;
        found = filter->FindRunAwayPoint(threat, dist, &pos);
        if (found) {
            BSAIScriptData* data = SelfScriptData(self);
            data->m_movePoint.x = pos.field_0x0;
            data->m_movePoint.y = pos.field_0x4;
            data->m_movePoint.z = pos.field_0x8;
            filter->m_pAIObject->SetupArbitraryPointUpdate((EMovePointType)3, &data->m_movePoint, 0.8f);
        }
    }
    BIFPop(sp);
    BIFReturn(sp, found);
}

void BIFunc_AIFireAtCurrentTarget(int** sp, void* self) {
    float dt = BIFArgF(sp, 0);
    ((ISceneNode*)self)->GetAIDoodad()->FireAtCurrentTarget(dt);
    BIFPop(sp);
}

// The mounted gun (a box or tank object) placed by the same trigger as
// script object `gun`.
static ISceneNode* FindMountedGun(TriggerObject_struct* trigger) {
    BSGO_Basic* go;
    for (go = g_pBoxList; go != NULL; go = go->m_pNext) {
        ISceneNode* node = go->GetSceneNode();
        BSObject* script;
        if (node != NULL && node->AsStaticObject() != NULL && (script = node->GetScriptObject()) != NULL &&
            script->m_trigger == trigger) {
            return node;
        }
    }
    for (go = g_pTankList; go != NULL; go = go->m_pNext) {
        ISceneNode* node = go->GetSceneNode();
        BSObject* script;
        if (node != NULL && node->AsHierObject() != NULL && (script = node->GetScriptObject()) != NULL &&
            script->m_trigger == trigger) {
            return node;
        }
    }
    return NULL;
}

void BIFunc_AIFireMMGAtCurrentTarget(int** sp, void* self) {
    BSObject* gun = ADDR_TO_PTR(BSObject, BIFArg(sp, 0));
    if (gun != NULL) {
        ISceneNode* node = FindMountedGun(gun->m_trigger);
        if (node != NULL) {
            ((ISceneNode*)self)->GetAIDoodad()->FireMMGAtCurrentTarget((CStaticObject*)node);
        }
    }
    BIFPop(sp);
}

void BIFunc_AICollisionTest(int** sp, void* self) {
    int direction = BIFArg(sp, 0);
    float dist = BIFArgF(sp, 1);
    dist = SelfAI(self)->PerformCollisionTest((ECollisionTestDirection)direction, dist);
    BIFPop(sp);
    BIFReturnF(sp, dist);
}

void BIFunc_AIEngageObstacleAvoidance(int** sp, void* self) {
    int engage = BIFArg(sp, 0);
    float a = BIFArgF(sp, 1);
    float b = BIFArgF(sp, 2);
    float c = BIFArgF(sp, 3);
    SelfAI(self)->EngageObstacleAvoidance(engage != 0, a, b, c);
    BIFPop(sp);
}

// Sets the AI's move point from `target` according to `type` (1/2 the
// target's guessed position, 3 a script object, 4/5 a trigger, 6 a trigger's
// facing, 7 a scene node, 8 the path node nearest a vector, 10 1.5 units
// behind a trigger) and starts updating it; returns whether one was found.
void BIFunc_AISetMovePoint(int** sp, void* self) {
    bool found = false;
    CAIFilterObject* filter = SelfFilter(self);
    u32 type = BIFArg(sp, 0);
    CVector3 pos;
    switch (type) {
    case 1:
    case 2:
        filter->GetTargetGuessPosition(*(CAIFilterRealPosition*)&pos);
        found = true;
        break;
    case 3: {
        BSObject* obj = ADDR_TO_PTR(BSObject, BIFArg(sp, 1));
        if (obj != NULL) {
            obj->m_gameObject->GetSceneNode()->GetPosition(pos);
            found = true;
        }
        break;
    }
    case 4:
    case 5: {
        BSObject* obj = ADDR_TO_PTR(BSObject, BIFArg(sp, 1));
        if (obj != NULL) {
            found = true;
            TriggerObjectData_struct* data = obj->m_trigger->m_data;
            pos.x = data->m_position[0];
            pos.y = data->m_position[1];
            pos.z = data->m_position[2];
        }
        break;
    }
    case 6: {
        TriggerObjectData_struct* data = (ADDR_TO_PTR(BSObject, BIFArg(sp, 1)))->m_trigger->m_data;
        CMatrix m;
        ((CQuaternion*)data->m_orientation)->GetMatrix(m);
        found = true;
        pos = *(CVector3*)m.m[1];
        pos.x -= data->m_position[0];
        pos.y -= data->m_position[1];
        pos.z -= data->m_position[2];
        break;
    }
    case 7:
        (ADDR_TO_PTR(ISceneNode, BIFArg(sp, 1)))->GetPosition(pos);
        found = true;
        break;
    case 8: {
        BS_STRUCT_Vector_struct* v = ADDR_TO_PTR(BS_STRUCT_Vector_struct, BIFArg(sp, 1));
        pos.x = v->x;
        pos.y = v->y;
        pos.z = v->z;
        CPathNode* node = g_aigAIFilterGlobalObject.m_pathNodes
                              ->GetClosestNode(*(CAIFilterRealPosition*)&pos, true);
        if (node != NULL) {
            found = true;
            pos.x = node->m_pos[0];
            pos.y = node->m_pos[1];
            pos.z = node->m_pos[2];
        } else {
            found = false;
        }
        break;
    }
    case 10: {
        BSObject* obj = ADDR_TO_PTR(BSObject, BIFArg(sp, 1));
        if (obj != NULL) {
            TriggerObjectData_struct* data = obj->m_trigger->m_data;
            pos.x = data->m_position[0];
            pos.y = data->m_position[1];
            pos.z = data->m_position[2];
            CQuaternion q = *(CQuaternion*)data->m_orientation;
            CVector3 back(0.0f, -1.0f, 0.0f);
            q.Rotate(back);
            found = true;
            back.x *= 1.5f;
            back.y *= 1.5f;
            back.z *= 1.5f;
            pos.x += back.x;
            pos.y += back.y;
            pos.z += back.z;
        }
        break;
    }
    }
    float speed = BIFArgF(sp, 2);
    BSAIScriptData* data = SelfScriptData(self);
    data->m_movePoint.x = pos.x;
    data->m_movePoint.y = pos.y;
    data->m_movePoint.z = pos.z;
    filter->m_pAIObject->SetupArbitraryPointUpdate((EMovePointType)type, &data->m_movePoint, speed);
    BIFPop(sp);
    BIFReturn(sp, found);
}

void BIFunc_AIStartMovePointWalk(int** sp, void* self) {
    CAIFilterObject* filter = SelfFilter(self);
    BSAIScriptData* data = SelfScriptData(self);
    CAIFilterRealPosition pos;
    pos.field_0x0 = data->m_movePoint.x;
    pos.field_0x4 = data->m_movePoint.y;
    pos.field_0x8 = data->m_movePoint.z;
    filter->m_pAIObject->StartWalkToArbitraryPoint(pos);
    BIFPop(sp);
}

void BIFunc_AIStopMovePointWalk(int** sp, void* self) {
    SelfAI(self)->m_moveMode = 0;
    BIFPop(sp);
}

void BIFunc_AIContinueMovePointWalk(int** sp, void* self) {
    SelfAI(self)->m_moveMode = 8;
    BIFPop(sp);
}

void BIFunc_AISnapToMovePoint(int** sp, void* self) {
    CAIObject* ai = SelfAI(self);
    ai->m_snapToAngle = false;
    ai->m_snapPosition.field_0x0 = ai->m_movePoint.field_0x0;
    ai->m_snapPosition.field_0x4 = ai->m_movePoint.field_0x4;
    ai->m_snapPosition.field_0x8 = ai->m_movePoint.field_0x8;
    ai->field_0x1A8 |= 2;
    BIFPop(sp);
}

void BIFunc_AISnapToMovePointAndAngle(int** sp, void* self) {
    float angle = BIFArgF(sp, 0);
    CAIObject* ai = SelfAI(self);
    ai->m_snapToAngle = true;
    ai->m_snapAngle = angle;
    ai->m_snapPosition.field_0x0 = ai->m_movePoint.field_0x0;
    ai->m_snapPosition.field_0x4 = ai->m_movePoint.field_0x4;
    ai->m_snapPosition.field_0x8 = ai->m_movePoint.field_0x8;
    ai->field_0x1A8 |= 2;
    BIFPop(sp);
}

void BIFunc_AIGetDistanceToMovePoint(int** sp, void* self) {
    CAIObject* ai = SelfAI(self);
    float dist = ai->m_position.GetDistanceXYReal(ai->m_movePoint);
    BIFPop(sp);
    BIFReturnF(sp, dist);
}

// Sets (or for 2 and 8, sets/clears by the second argument) AI flag bits.
void BIFunc_AISendFlag(int** sp, void* self) {
    CAIFilterObject* filter = SelfFilter(self);
    int flag = BIFArg(sp, 0);
    switch (flag) {
    default:
        filter->m_pAIObject->m_flags |= flag;
        break;
    case 1:
        filter->m_pAIObject->m_flags |= 1;
        break;
    case 2: {
        CAIObject* ai = filter->m_pAIObject;
        if (BIFArg(sp, 1) != 0) {
            ai->m_flags |= 2;
        } else {
            ai->m_flags &= ~2;
            ai->field_0x1A8 &= ~1;
        }
        break;
    }
    case 4:
        filter->m_pAIObject->m_flags |= 4;
        break;
    case 8:
        if (BIFArg(sp, 1) != 0) {
            filter->m_pAIObject->m_flags |= 8;
        } else {
            filter->m_pAIObject->m_flags &= ~8;
        }
        break;
    }
    BIFPop(sp);
}

// Sets AI status variable `index` (0x1B4 + 4 * index). Setting 13 to 15
// (dead) also clears variable 0 and turns an animated object with collision
// id 13 or 2 into id 25.
void BIFunc_AISetStatusVar(int** sp, void* self) {
    int index = BIFArg(sp, 0);
    int value = BIFArg(sp, 1);
    CAIObject* ai = SelfAI(self);
    if (index == 13 && value == 15) {
        ai->m_status.m_vars[AISTATUS_HEALTH] = 0;
    }
    ai->m_status.m_vars[index] = value;
    if (index == 13 && value == 15) {
        CAnimObject* anim = ((ISceneNode*)self)->AsAnimObject();
        if (anim != NULL && (anim->GetCollisionId() == 13 || anim->GetCollisionId() == 2)) {
            anim->SetCollisionId((EClsnId)25);
        }
    }
#ifdef FRONTLINE_NATIVE
    // Rewritten: an enemy drops its item as it dies, not after its animation.
    if (index == 13 && value == 15) {
        NativeEnemyDied((ISceneNode*)self);
    }
#endif
    BIFPop(sp);
}

// Tests grenade arcs at 20, 45 and 5 degrees (as `flags` allow) and returns
// the flag of the first that reaches the target; the launch speed goes to
// the script data.
void BIFunc_SetupGrenadeShot(int** sp, void* self) {
    int flags = BIFArg(sp, 0);
    CAIObject* ai = SelfAI(self);
    float speed = -1.0f;
    int result;
    if (ai->m_pTarget != NULL) {
        if ((flags & 1) && (speed = ai->TestGrenadeShot(0.34906584f)) != -1.0f) {
            result = 1;
        } else if ((flags & 2) && (speed = ai->TestGrenadeShot(0.7853982f)) != -1.0f) {
            result = 2;
        } else if ((flags & 4) && (speed = ai->TestGrenadeShot(0.08726646f)) != -1.0f) {
            result = 4;
        } else {
            result = 0;
        }
    } else {
        result = 0;
    }
    SelfScriptData(self)->m_grenadeSpeed = speed;
    BIFPop(sp);
    BIFReturn(sp, result);
}

void BIFunc_SetGravity(int** sp, void* self) {
    int on = BIFArg(sp, 0);
    ((ISceneNode*)self)->AsAnimObject()->field_0x24B2 = on != 0;
    BIFPop(sp);
}

void BIFunc_SetGroundCheck(int** sp, void* self) {
    int on = BIFArg(sp, 0);
    ((ISceneNode*)self)->AsAnimObject()->field_0x24B3 = on != 0;
    BIFPop(sp);
}

void BIFunc_InduceGroundCheck(int** sp, void* self) {
    CSoldierObject* soldier = ((ISceneNode*)self)->AsSoldierObject();
    if (((CAnimObject*)soldier)->field_0x2454) {
        BSObjectTriggerEvent(((CAnimObject*)soldier)->m_pScriptObject, 0x85, NULL, NULL, true);
    }
    BIFPop(sp);
}

// High-detail collision: switches an animated object between ids 13 and 2.
void BIFunc_SetHDCollision(int** sp, void* self) {
    int on = BIFArg(sp, 0);
    CAnimObject* anim = ((ISceneNode*)self)->AsAnimObject();
    if (anim->GetCollisionId() == 13 || anim->GetCollisionId() == 2) {
        if (on != 0) {
            anim->SetCollisionId((EClsnId)13);
        } else {
            anim->SetCollisionId((EClsnId)2);
        }
    }
    BIFPop(sp);
}

void BIFunc_SetCollisionToPlayer(int** sp, void* self) {
    int on = BIFArg(sp, 0);
    CAnimObject* anim = ((ISceneNode*)self)->AsAnimObject();
    if (on != 0) {
        anim->SetCollisionId((EClsnId)2);
    } else {
        anim->SetCollisionId((EClsnId)25);
    }
    BIFPop(sp);
}

void BIFunc_SetCanBePushed(int** sp, void* self) {
    bool on = BIFArg(sp, 0) != 0;
    if (self != NULL) {
        CAnimObject* anim = ((ISceneNode*)self)->AsAnimObject();
        if (anim != NULL) {
            anim->field_0x2451 = on;
        }
    }
    BIFPop(sp);
}

void BIFunc_SetInShadow(int** sp, void* self) {
    bool on = BIFArg(sp, 0) != 0;
    if (self != NULL) {
        CAIFilterObject* filter = SelfFilter(self);
        if (filter != NULL) {
            filter->m_pAIObject->m_field_0x730 = on;
        }
    }
    BIFPop(sp);
}

void BIFunc_DisableCollision(int** sp, void* self) {
    ((ISceneNode*)self)->AsAnimObject()->SetCollisionId((EClsnId)-1);
    BIFPop(sp);
}

// Script collision types 0..11 to collision ids (-1 = none).
void BIFunc_SetMechanicCollision(int** sp, void* self) {
    u32 type = BIFArg(sp, 0);
    ISceneNode* node = (ISceneNode*)self;
    if (g_pBSObject->m_trigger->m_data->m_type != 8) {
        node = (ISceneNode*)node->AsStaticObject();
    }
    int id = -1;
    switch (type) {
    case 0:
        id = 0;
        break;
    case 2:
        id = 15;
        break;
    case 3:
        id = 17;
        break;
    case 4:
        id = 16;
        break;
    case 5:
        id = 19;
        break;
    case 6:
        id = 18;
        break;
    case 7:
        id = 20;
        break;
    case 8:
        id = 24;
        break;
    case 9:
        id = 26;
        break;
    case 10:
        id = 27;
        break;
    case 11:
        id = 1;
        break;
    }
    node->SetCollisionId((EClsnId)id);
    BIFPop(sp);
}

void BIFunc_PathMechAllowCollisionDamage(int** sp, void* self) {
    int allow = BIFArg(sp, 0);
    CStaticObject* obj = ((ISceneNode*)self)->AsStaticObject();
    obj->m_collisionDamage = allow != 0;
    BIFPop(sp);
}

void BIFunc_SetHeadAsRoot(int** sp, void* self) {
    ((CSoldierObject*)self)->SetHeadTrans(BIFArg(sp, 0) != 0);
    BIFPop(sp);
}

// Player collision: 0 normal (1), 2 id 21, else none.
void BIFunc_SetPlayerCollision(int** sp, void*) {
    int type = BIFArg(sp, 0);
    CPlayerObject* player = g_scene.GetPlayer(0);
    if (player != NULL) {
        int id = -1;
        switch (type) {
        case 1:
            break;
        case 0:
            id = 1;
            break;
        case 2:
            id = 21;
            break;
        }
        player->SetCollisionId((EClsnId)id);
    }
    BIFPop(sp);
}

void BIFunc_SendEventSelf(int** sp, void*) {
    BSObjectTriggerEvent(g_pBSObject, BIFTop(sp, 1), ADDR_TO_PTR(void, BIFTop(sp, 0)), g_pBSObject, false);
    BIFPop(sp);
}

void BIFunc_SendEventParent(int** sp, void*) {
    BSObjectTriggerEvent(g_pBSObject->m_trigger->m_object, BIFTop(sp, 1), ADDR_TO_PTR(void, BIFTop(sp, 0)), g_pBSObject,
                         false);
    BIFPop(sp);
}

void BIFunc_SendEvent(int** sp, void*) {
    BSObject* obj = ADDR_TO_PTR(BSObject, BIFArg(sp, 0));
    if (obj != NULL && obj != BS_INVALID_OBJECT) {
        BSObjectTriggerEvent(obj, BIFArg(sp, 1), ADDR_TO_PTR(void, BIFArg(sp, 2)), g_pBSObject, false);
    }
    BIFPop(sp);
}

void BIFunc_SendEventImmediate(int** sp, void*) {
    BSObjectTriggerEvent(ADDR_TO_PTR(BSObject, BIFArg(sp, 0)), BIFArg(sp, 1), ADDR_TO_PTR(void, BIFArg(sp, 2)), g_pBSObject, true);
    BIFPop(sp);
}

void BIFunc_SendEventDelayed(int** sp, void*) {
    BSRegisterTimerEvent(BIFArg(sp, 0), BIFArg(sp, 2), ADDR_TO_PTR(BSObject, BIFArg(sp, 1)), ADDR_TO_PTR(void, BIFArg(sp, 3)),
                         (ETimerReplaceMethod)1);
    BIFPop(sp);
}

void BIFunc_SendMessage(int** sp, void*) {
    BSSendMessage(BIFArg(sp, 0), g_pBSObject, g_pBSObject->m_trigger, g_pBSObject, ADDR_TO_PTR(void, BIFArg(sp, 1)));
    BIFPop(sp);
}

void BIFunc_PlayerIsMyWeaponAProjectile(int** sp, void* self) {
    CPlayerObject* player = ((ISceneNode*)self)->AsPlayerObject();
    BIFPop(sp);
    BIFReturn(sp, player->GetCurrentWeapon()->m_pProperties->m_isProjectile != 0);
}

void BIFunc_SetPlayerWielding(int** sp, void* self) {
    CPlayerObject* player = ((ISceneNode*)self)->AsPlayerObject();
    int on = BIFArg(sp, 0);
    player->m_wielding = on != 0;
    BIFPop(sp);
}

void BIFunc_PlayerFireMyWeapon(int** sp, void* self) {
    ((ISceneNode*)self)->AsPlayerObject()->FireMyWeapon(BIFArgF(sp, 0));
    BIFPop(sp);
}

void BIFunc_PlayerStartReloading(int** sp, void* self) {
    ((ISceneNode*)self)->AsPlayerObject()->GetCurrentWeapon()->StartReloading();
    BIFPop(sp);
}

void BIFunc_PlayerReloadSingle(int** sp, void* self) {
    ((ISceneNode*)self)->AsPlayerObject()->GetCurrentWeapon()->ReloadSingle();
    BIFPop(sp);
}

void BIFunc_PlayerClearReloading(int** sp, void* self) {
    ((ISceneNode*)self)->AsPlayerObject()->GetCurrentWeapon()->m_bReloading = false;
    BIFPop(sp);
}

void BIFunc_PlayerFinishedReloading(int** sp, void* self) {
    ((ISceneNode*)self)->AsPlayerObject()->GetCurrentWeapon()->DoneReloading();
    BIFPop(sp);
}

// The player the running script belongs to in multiplayer, else player 0.
static inline CPlayerObject* ScriptPlayer() {
    CPlayerObject* player = g_scene.GetPlayer(0);
    if (g_bInMultiplayerMode && g_pBSObject != NULL && g_pBSObject->m_gameObject != NULL &&
        g_pBSObject->m_gameObject->GetSceneNode() != NULL &&
        g_pBSObject->m_gameObject->GetSceneNode()->AsPlayerObject() != NULL) {
        player = g_pBSObject->m_gameObject->GetSceneNode()->AsPlayerObject();
    }
    return player;
}

void BIFunc_PlayerStartCameraShake(int** sp, void*) {
    float strength = BIFArgF(sp, 0);
    float time = BIFArgF(sp, 1);
    CPlayerObject* player = ScriptPlayer();
    if (player != NULL) {
        player->StartCameraShake(strength, time);
    }
    BIFPop(sp);
}

void BIFunc_PlayerStopCameraShake(int** sp, void*) {
    float time = BIFArgF(sp, 0);
    CPlayerObject* player = ScriptPlayer();
    if (player != NULL) {
        player->StopCameraShake(time);
    }
    BIFPop(sp);
}

void BIFunc_PlayerCanCrouch(int** sp, void*) {
    int on = BIFArg(sp, 0) != 0;
    CPlayerObject* player = g_scene.GetPlayer(0);
    if (player != NULL) {
        player->m_canCrouch = on;
    }
    BIFPop(sp);
}

void BIFunc_PlayerMotionShake(int** sp, void*) {
    float a = BIFArgF(sp, 0);
    float b = BIFArgF(sp, 1);
    CPlayerObject* player = g_scene.GetPlayer(0);
    if (player != NULL) {
        player->DoMotionShake(a, b);
    }
    BIFPop(sp);
}

// A full-screen flash from colour (r, g, b, a) to the second colour.
void BIFunc_PlayerScreenFlash(int** sp, void*) {
    CColor from(BIFArg(sp, 0), BIFArg(sp, 1), BIFArg(sp, 2), BIFArg(sp, 3));
    CColor to(BIFArg(sp, 4), BIFArg(sp, 5), BIFArg(sp, 6), BIFArg(sp, 7));
    g_screenflash.m_time = 0.0f;
    g_screenflash.m_from = from;
    g_screenflash.m_to = to;
    g_screenflash.m_duration = BIFArgF(sp, 8);
    CFader::SetControl(&g_screenflash);
    BIFPop(sp);
}

// The spline path with this id (or one past the last if there is none).
static inline CAISplinePath* FindSplinePath(u32 id) {
    CAISplinePathManager* paths = g_aigAIFilterGlobalObject.m_splinePaths;
    u32 i;
    for (i = 0; i < paths->m_numPaths; i++) {
        if (id == paths->m_paths[i].m_id) {
            break;
        }
    }
    return &paths->m_paths[i];
}

void BIFunc_PlayerStartPath(int** sp, void*) {
    u32 pathId = BIFArg(sp, 0);
    u32 lookId = BIFArg(sp, 1);
    float speed = BIFArgF(sp, 2);
    CAISplinePath* path = pathId != 0 ? FindSplinePath(pathId) : NULL;
    CAISplinePath* look = lookId != 0 ? FindSplinePath(lookId) : NULL;
    g_scene.GetPlayer(0)->MoveOnPath(path, look, speed);
    BIFPop(sp);
}

void BIFunc_PlayerStopPath(int** sp, void*) {
    g_scene.GetPlayer(0)->StopPath();
    BIFPop(sp);
}

void BIFunc_PlayerDeath(int** sp, void* self) {
    CPlayerObject* player = ((ISceneNode*)self)->AsPlayerObject();
    if (g_bInMultiplayerMode) {
        player->StartDeathSequence();
        player->DropMPDumpObject();
    } else {
        g_scene.GetPlayer(0)->StartDeathSequence();
    }
    BIFPop(sp);
}

void BIFunc_PlayerGetWeaponRefireRate(int** sp, void* self) {
    CWeapon* weapon = ((ISceneNode*)self)->AsPlayerObject()->GetCurrentWeapon();
    s16 rate = 0;
    if (weapon != NULL) {
        rate = weapon->m_pProperties->m_refireRate;
    }
    BIFPop(sp);
    BIFReturn(sp, rate);
}

// Shows a hit marker on the HUD: 0 front, 1 right, 2 back, 3 left.
void BIFunc_PlayerAddHitDirection(int** sp, void* self) {
    int dir = BIFArg(sp, 0);
    CPlayerObject* player = ((ISceneNode*)self)->AsPlayerObject();
    if (dir == 0) {
        player->m_ui.AddHitDirection(1);
    } else if (dir == 1) {
        player->m_ui.AddHitDirection(3);
    } else if (dir == 2) {
        player->m_ui.AddHitDirection(4);
    } else if (dir == 3) {
        player->m_ui.AddHitDirection(2);
    }
    BIFPop(sp);
}

void BIFunc_IsPlayerAiming(int** sp, void* self) {
    BIFPop(sp);
    BIFReturn(sp, ((ISceneNode*)self)->AsPlayerObject()->m_zoomed);
}

// Whether the player wears a disguise (script data words 0x10 and 0x14) and
// holds no weapon other than type 0x25.
void BIFunc_IsPlayerDisguised(int** sp, void*) {
    // The original pops twice.
    BIFPop(sp);
    BIFPop(sp);
    u32* data = (u32*)g_scene.GetPlayer(0)->GetScriptObject()->m_gameObject->GetScriptData();
    bool disguised = false;
    if (data[4] != 0 && data[5] != 0) {
        CWeapon* weapon = g_scene.GetPlayer(0)->GetCurrentWeapon();
        int type = -1;
        if (weapon != NULL) {
            type = weapon->m_weaponType;
        }
        bool armed;
        if (type == 0x25 || type == -1) {
            armed = false;
        } else {
            armed = true;
        }
        if (!armed) {
            disguised = true;
        }
    }
    BIFReturn(sp, disguised);
}

void BIFunc_IsPlayerCrouching(int** sp, void*) {
    BIFPop(sp);
    CPlayerObject* player;
    if (g_bInMultiplayerMode) {
        player = g_pBSObject->m_gameObject->GetSceneNode()->AsPlayerObject();
    } else {
        player = g_scene.GetPlayer(0);
    }
    BIFReturn(sp, player->IsPlayerCrouching());
}

void BIFunc_SetPlayerFallingDamage(int** sp, void*) {
    int on = BIFArg(sp, 0) != 0;
    g_scene.GetPlayer(0)->SetFallingDamageState(on);
    BIFPop(sp);
}

void BIFunc_PlayerRespawn(int** sp, void* self) {
    ((ISceneNode*)self)->AsPlayerObject()->Respawn();
    BIFPop(sp);
}

// Plays animation `anim` (mode `mode`, with `data`); if it can't start yet
// and the script data allows (word 0 <= 0), queues it.
static inline void PlayAnim(int** sp, CAnimObject* anim, bool locomotion, bool translate) {
    int mode = BIFArg(sp, 1);
    int id = BIFArg(sp, 0);
    void* data = ADDR_TO_PTR(void, BIFArg(sp, 2));
    anim->field_0x2458 = locomotion;
    anim->field_0x2459 = translate;
    if (anim->SetAnimationState(id, 1.0f, mode, data) == 0xFFFF0000) {
        if (*(int*)anim->GetScriptObject()->m_gameObject->GetScriptData() <= 0) {
            anim->QueueAnimationState(id, 1.0f, mode, data);
        }
    }
    BIFPop(sp);
}

void BIFunc_PlayAnimation(int** sp, void* self) {
    PlayAnim(sp, (CAnimObject*)self, false, true);
}

void BIFunc_PlayLocomotion(int** sp, void* self) {
    PlayAnim(sp, (CAnimObject*)self, true, true);
}

void BIFunc_PlayAnimationNoTranslation(int** sp, void* self) {
    PlayAnim(sp, (CAnimObject*)self, false, false);
}

void BIFunc_PlayAnimationOnOther(int** sp, void*) {
    (ADDR_TO_PTR(CAnimObject, BIFArg(sp, 3)))->SetAnimationState(BIFArg(sp, 0), 1.0f, BIFArg(sp, 1), ADDR_TO_PTR(void, BIFArg(sp, 2)));
    BIFPop(sp);
}

void BIFunc_PlayFacialAnimation(int** sp, void* self) {
    ((CSoldierObject*)self)->SetFaceAnimationState(BIFArg(sp, 0), -1);
    BIFPop(sp);
}

void BIFunc_RegisterTimerEvent(int** sp, void*) {
#ifdef FRONTLINE_NATIVE
    // Rewritten's AI shortens some of the soldier scripts' timers.
    int delay = BIFArg(sp, 0);
    if (NativeAIOn()) {
        delay = NativeAIScriptTimer(delay, BIFArg(sp, 1));
    }
    BSRegisterTimerEvent(delay, BIFArg(sp, 1), g_pBSObject, ADDR_TO_PTR(void, BIFArg(sp, 2)), (ETimerReplaceMethod)0);
#else
    BSRegisterTimerEvent(BIFArg(sp, 0), BIFArg(sp, 1), g_pBSObject, ADDR_TO_PTR(void, BIFArg(sp, 2)), (ETimerReplaceMethod)0);
#endif
    BIFPop(sp);
}

void BIFunc_EnableNonVisDestruct(int** sp, void* self) {
    ((CSoldierObject*)self)->m_noNonVisDestruct = BIFArg(sp, 0) == 0;
    BIFPop(sp);
}

// Weapon types 0x21, 0x22 and 0x25 don't count as melee weapons.
void BIFunc_PlayerHasMeleeWeapon(int** sp, void* self) {
    bool melee = false;
    CPlayerObject* player = ((ISceneNode*)self)->AsPlayerObject();
    if (player->GetCurrentWeapon() != NULL) {
        int type = player->GetCurrentWeapon()->m_weaponType;
        if (type == 0x25 || (type < 0x23 && type >= 0x21)) {
            melee = false;
        } else {
            melee = true;
        }
    }
    BIFPop(sp);
    BIFReturn(sp, melee);
}

void BIFunc_PlayerCanCookGrenades(int** sp, void* self) {
    BIFPop(sp);
    bool can;
    if (g_bInMultiplayerMode) {
        can = g_Shell.m_players[((ISceneNode*)self)->AsPlayerObject()->m_controllerPort].m_cookGrenades != 0;
    } else {
        can = g_Shell.m_cookGrenades != 0;
    }
    BIFReturn(sp, can);
}

void BIFunc_PlayerCanSwitchWeapon(int** sp, void* self) {
    CPlayerObject* player = ((ISceneNode*)self)->AsPlayerObject();
    BIFPop(sp);
    BIFReturn(sp, player->CanPlayerSwitchWeapon());
}

void BIFunc_PlayerSwitchWeapons(int** sp, void* self) {
    int dir = BIFArg(sp, 0);
    ((ISceneNode*)self)->AsPlayerObject()->CycleWeapon(dir, -1);
    BIFPop(sp);
}

void BIFunc_GetRandomInteger(int** sp, void*) {
    int lo = BIFArg(sp, 0);
    int hi = BIFArg(sp, 1);
    int value = lo;
    if (lo != hi) {
        value = MathFunRandomI64(lo, hi);
    }
#ifdef FRONTLINE_NATIVE
    // Rewritten's AI biases some of the soldier scripts' decisions.
    if (NativeAIOn()) {
        value = NativeAIScriptRandom(lo, hi, value);
    }
#endif
    BIFPop(sp);
    BIFReturn(sp, value);
}

void BIFunc_GetRandomFloat(int** sp, void*) {
    float lo = BIFArgF(sp, 0);
    float hi = BIFArgF(sp, 1);
    float value = lo;
    if (lo != hi) {
        value = MathFunRandomReal(lo, hi);
    }
    BIFPop(sp);
    BIFReturnF(sp, value);
}

void BIFunc_GetMyInstanceData(int** sp, void*) {
    BIFPop(sp);
    BIFReturn(sp, (int)PTR_TO_ADDR(g_pBSObject->m_gameObject->GetScriptData()));
}

void BIFunc_CreateChildRootHierObject(int** sp, void*) {
    int obj = (int)PTR_TO_ADDR(CreateRootHierObject(g_pBSObject->m_trigger, BIFArg(sp, 0))->m_object);
    BIFPop(sp);
    BIFReturn(sp, obj);
}

void BIFunc_CreateChildSubHierObject(int** sp, void* self) {
    int a = BIFArg(sp, 0);
    int b = BIFArg(sp, 1);
    int attach = BIFArg(sp, 2);
    CreateSubHierObject(((ISceneNode*)self)->AsHierObject(), a, b, (DWI_ATTACHMENT_CRC_ENUM)attach);
    BIFPop(sp);
}

void BIFunc_SwapHierObject(int** sp, void* self) {
    int a = BIFArg(sp, 0);
    int b = BIFArg(sp, 1);
    SwapHierObject(((ISceneNode*)self)->AsHierObject(), a, b);
    BIFPop(sp);
}

// Destroys sub-object `index`, or with 0 the whole object.
void BIFunc_DestroySubHierObject(int** sp, void* self) {
    int index = BIFArg(sp, 0);
    CHierObject* hier = ((ISceneNode*)self)->AsHierObject();
    if (index == 0) {
        DelayDestroyObject(g_pBSObject->m_gameObject);
    } else {
        DestroySubHierObject(hier, index);
    }
    BIFPop(sp);
}

// A created game object's script object, as its first word. (MWCC puts
// BSGO_Basic's vtable pointer after its members; natively it comes first.)
#ifdef FRONTLINE_NATIVE
#define GO_SCRIPT_WORD(go) ((int)PTR_TO_ADDR(((BSGO_Basic*)(go))->m_object))
#else
#define GO_SCRIPT_WORD(go) (*(int*)(go))
#endif

// Creates object `index` of the running object's trigger. Particle (9) and
// light (14) creators return the object itself, others a script object.
void BIFunc_CreateChild(int** sp, void*) {
    TriggerObject_struct* trigger = g_pBSObject->m_trigger;
    int result = 0;
    int type = trigger->m_data->m_type;
    int index = BIFArg(sp, 0);
    switch (type) {
    case 9:
    case 14:
        result = (int)PTR_TO_ADDR(CreateObject(trigger, index, NULL));
        break;
    case 3: {
        int* obj = (int*)CreateObject(trigger, index, NULL);
        if (obj != NULL) {
            result = GO_SCRIPT_WORD(obj);
        }
        break;
    }
    default:
        result = GO_SCRIPT_WORD(CreateObject(trigger, index, NULL));
        break;
    }
    BIFPop(sp);
    BIFReturn(sp, result);
}

void BIFunc_DestroyObject(int** sp, void*) {
    int type = g_pBSObject->m_trigger->m_data->m_type;
    BSObject* obj = ADDR_TO_PTR(BSObject, BIFArg(sp, 0));
    switch (type) {
    case 14:
        ((CLight*)obj)->MarkForDestruction(0);
        break;
    case 9:
        ((CParticleSystem*)obj)->DeActivate();
        break;
    default:
        if (obj == NULL) {
            obj = g_pBSObject;
        }
        BSGO_Basic* go = obj->m_gameObject;
        if (go != NULL) {
            DelayDestroyObject(go);
        }
        break;
    }
    BIFPop(sp);
}

void BIFunc_CanICreateChild(int** sp, void*) {
    bool can = false;
    if (g_iGermanCreationQueueHeadIndex == g_iGermanCreationQueueTailIndex &&
        g_numActiveSoldiers < g_maxActiveSoldiers) {
        can = true;
    }
    BIFPop(sp);
    BIFReturn(sp, can);
}

// Script collision types to collision ids, as BIFunc_SetMechanicCollision.
static inline int ScriptCollisionId(u32 type) {
    int id = -1;
    switch (type) {
    case 0:
        id = 0;
        break;
    case 2:
        id = 15;
        break;
    case 3:
        id = 17;
        break;
    case 4:
        id = 16;
        break;
    case 5:
        id = 19;
        break;
    case 6:
        id = 18;
        break;
    case 7:
        id = 20;
        break;
    case 8:
        id = 24;
        break;
    case 9:
        id = 26;
        break;
    case 10:
        id = 27;
        break;
    case 11:
        id = 1;
        break;
    }
    return id;
}

// Places a new thrown object at `self` (or the running object's trigger).
static inline void PlaceThrownObject(CThrownObject* thrown, void* self) {
    CMatrix tm;
    if (self != NULL) {
        ((ISceneNode*)self)->GetTMLocalToWorld(tm);
        thrown->SetTMLocalToWorld(tm);
    } else {
        TriggerObjectData_struct* data = g_pBSObject->m_trigger->m_data;
        CVector3 pos(data->m_position[0], data->m_position[1], data->m_position[2]);
        ((CQuaternion*)data->m_orientation)->GetMatrix(tm);
        tm.SetPos(pos);
        thrown->SetTMLocalToWorld(tm);
    }
}

// Throws child object `index` along `dir` at `speed`.
void BIFunc_CreateChildThrownObject(int** sp, void* self) {
    TriggerObject_struct* trigger = g_pBSObject->m_trigger;
    float speed = BIFArgF(sp, 1);
    float dx = BIFArgF(sp, 2);
    float dy = BIFArgF(sp, 3);
    float dz = BIFArgF(sp, 4);
    u32 collision = BIFArg(sp, 5);
    CThrownObject* thrown = CreateSpecialThrownObject(trigger, trigger->m_data->field_0x48, BIFArg(sp, 0));
    PlaceThrownObject(thrown, self);
    CVector3 vel;
    vel.x = dx;
    vel.y = dy;
    vel.z = dz;
    float len = dz * dz + (dx * dx + dy * dy);
    if (len > 0.0f) {
        len = sqrtf(len);
    }
    if (len != 0.0f) {
        float inv = 1.0f / len;
        vel.x *= inv;
        vel.y *= inv;
        vel.z *= inv;
    }
    vel.x *= speed;
    vel.y *= speed;
    vel.z *= speed;
    thrown->SetVelocity(vel);
    thrown->BeginUpdate(1.0f);
    thrown->SetCollisionId((EClsnId)ScriptCollisionId(collision));
    BIFPop(sp);
}

// A child object that stays put until BIFunc_DropFallingObject.
void BIFunc_CreateChildFallingObject(int** sp, void* self) {
    TriggerObject_struct* trigger = g_pBSObject->m_trigger;
    u32 collision = BIFArg(sp, 1);
    CThrownObject* thrown = CreateSpecialThrownObject(trigger, trigger->m_data->field_0x48, BIFArg(sp, 0));
    PlaceThrownObject(thrown, self);
    thrown->m_state = 0;
    CVector3 vel(0.0f, 0.0f, 0.0f);
    thrown->SetVelocity(vel);
    thrown->BeginUpdate(1.0f);
    thrown->SetCollisionId((EClsnId)ScriptCollisionId(collision));
    BIFPop(sp);
}

// Doesn't pop its arguments (as in the original).
void BIFunc_DropFallingObject(int**, void* self) {
    ((ISceneNode*)self)->AsStaticObject()->AsThrownObject()->m_state = 1;
}

void BIFunc_DestroyThrownObject(int** sp, void* self) {
    ((ISceneNode*)self)->AsStaticObject()->AsThrownObject()->MarkForDestruction(2);
    BIFPop(sp);
}

// Polls the AI's distance to its target between `min` and `max`, raising
// `event`; without a target, raises event 0x45 at once.
void BIFunc_RegisterDistanceToTarget(int** sp, void* self) {
    BSScheduleRegistrationRecord_struct* rec = NULL;
    int event = BIFArg(sp, 0);
    int minDist = BIFArg(sp, 1);
    int maxDist = BIFArg(sp, 2);
    int oneShot = BIFArg(sp, 3);
    bool hasTarget = false;
    CAIObject* ai = SelfAI(self);
    if (ai != NULL && (ai->m_nonAITarget || ai->m_pTarget != NULL)) {
        hasTarget = true;
    }
    if (hasTarget) {
        rec = g_DistanceToTargetSchedule.Register(g_pBSObject, event, oneShot != 0, minDist * minDist,
                                                  maxDist * maxDist, 0);
    } else {
        BSObjectTriggerEvent(g_pBSObject, 0x45, NULL, g_pBSObject, true);
    }
    BIFPop(sp);
    BIFReturn(sp, (int)PTR_TO_ADDR(rec));
}

// Polls for targets in field of view (mode 2) or line of sight (3).
void BIFunc_RegisterTargetSearch(int** sp, void* self) {
    CAIFilterObject* filter = SelfFilter(self);
    int oneShot = BIFArg(sp, 6);
    int event = BIFArg(sp, 5);
    int mode = BIFArg(sp, 0);
    float range = BIFArgF(sp, 1);
    float fov = BIFArgF(sp, 2);
    u32* list = ADDR_TO_PTR(u32, BIFArg(sp, 3));
    int count = BIFArg(sp, 4);
    filter->SetTargetMode((aifilter_target_mode)0);
    switch (mode) {
    case 2:
        filter->m_pAIObject->SetupFieldOfViewModeList(BIF_MATCH_LIST(list), count, range, fov);
        break;
    case 3:
        filter->m_pAIObject->SetupLineOfSightModeList(BIF_MATCH_LIST(list), count, range);
        break;
    }
    BSScheduleRegistrationRecord_struct* rec =
        g_TargetSearchSchedule.Register(g_pBSObject, event, oneShot != 0, mode, 0, 0);
    BIFPop(sp);
    BIFReturn(sp, (int)PTR_TO_ADDR(rec));
}

void BIFunc_RegisterCorpseSearch(int** sp, void* self) {
    u16 event = BIFArg(sp, 2);
    SelfAI(self)->SetupFieldOfViewCorpse(BIFArgF(sp, 0), BIFArgF(sp, 1));
    BSScheduleRegistrationRecord_struct* rec = g_CorpseSearchSchedule.Register(g_pBSObject, event, true, 0, 0, 0);
    BIFPop(sp);
    BIFReturn(sp, (int)PTR_TO_ADDR(rec));
}

// Polls whether the AI can see/sense script object `target` (mode 2 field of
// view, 3 and 5 line of sight, 4 seeking towards its last known position).
void BIFunc_RegisterTargetCheck(int** sp, void* self) {
    BSScheduleRegistrationRecord_struct* rec = NULL;
    CAIFilterObject* filter = SelfFilter(self);
    BSObject* target = ADDR_TO_PTR(BSObject, BIFArg(sp, 3));
    int mode = BIFArg(sp, 0);
    float range = BIFArgF(sp, 1);
    u16 event = BIFArg(sp, 4);
    float fov = BIFArgF(sp, 2);
    int oneShot = BIFArg(sp, 5);
    if (target != NULL && GetAIFilter(target) != NULL) {
        filter->SetTargetMode((aifilter_target_mode)0);
        switch (mode) {
        case 1:
            break;
        case 2:
            filter->m_pAIObject->SetupFieldOfViewModeObject(GetAIObject(target), range, fov);
            break;
        case 3:
            filter->m_pAIObject->SetupLineOfSightModeObject(GetAIObject(target), range);
            break;
        case 4: {
            CAIObject* other = GetAIObject(target);
            CAIObject* ai = filter->m_pAIObject;
            if (other == ai->m_pTarget) {
                ai->SetupVisionSeekingMode(other, ai->m_lastSeenPosition, range);
            } else {
                CAIFilterRealPosition pos;
                pos.field_0x0 = other->m_position.field_0x0;
                pos.field_0x4 = other->m_position.field_0x4;
                pos.field_0x8 = other->m_position.field_0x8 + 0.8f;
                ai->SetupVisionSeekingMode(other, pos, range);
            }
            break;
        }
        case 5:
            filter->m_pAIObject->SetupLineOfSightModeObject(GetAIObject(target), range);
            break;
        }
        rec = g_TargetCheckSchedule.Register(g_pBSObject, event, oneShot != 0, mode, (int)range, (int)PTR_TO_ADDR(target));
    } else {
        BSObjectTriggerEvent(g_pBSObject, 0x45, NULL, g_pBSObject, true);
    }
    BIFPop(sp);
    BIFReturn(sp, (int)PTR_TO_ADDR(rec));
}

void BIFunc_RegisterBuddyPlayerCheck(int** sp, void* self) {
    ((ISceneNode*)self)->GetAIDoodad();
    BSScheduleRegistrationRecord_struct* rec = g_BuddyPlayerCheckSchedule.Register(
        g_pBSObject, BIFArg(sp, 3), BIFArg(sp, 4) != 0, BIFArg(sp, 0) != 0, (int)BIFArgF(sp, 1), BIFArg(sp, 2));
    BIFPop(sp);
    BIFReturn(sp, (int)PTR_TO_ADDR(rec));
}

// Stops record `rec`, or with NULL every check of the running object.
static inline void StopCheck(int** sp, void* self, BSSchedule& schedule) {
    BSScheduleRegistrationRecord_struct* rec = ADDR_TO_PTR(BSScheduleRegistrationRecord_struct, BIFArg(sp, 0));
    if (rec != NULL) {
        schedule.Unregister(rec);
    } else {
        schedule.Unregister(((ISceneNode*)self)->GetScriptObject());
    }
    BIFPop(sp);
}

void BIFunc_StopTargetSearch(int** sp, void* self) {
    StopCheck(sp, self, g_TargetSearchSchedule);
}

void BIFunc_StopTargetCheck(int** sp, void* self) {
    StopCheck(sp, self, g_TargetCheckSchedule);
}

void BIFunc_StopDistanceToTarget(int** sp, void* self) {
    StopCheck(sp, self, g_DistanceToTargetSchedule);
}

void BIFunc_StopBuddyPlayerCheck(int** sp, void*) {
    BSScheduleRegistrationRecord_struct* rec = ADDR_TO_PTR(BSScheduleRegistrationRecord_struct, BIFArg(sp, 0));
    if (rec != NULL) {
        g_BuddyPlayerCheckSchedule.Unregister(rec);
    } else {
        g_BuddyPlayerCheckSchedule.Unregister(g_pBSObject);
    }
    BIFPop(sp);
}

void BIFunc_GetGrenadeHoldTime(int** sp, void* self) {
    BIFPop(sp);
    BIFReturnF(sp, ((ISceneNode*)self)->AsPlayerObject()->m_grenadeHoldTime);
}

void BIFunc_StartGrenadeHold(int** sp, void* self) {
    CPlayerObject* player = ((ISceneNode*)self)->AsPlayerObject();
    if (player->m_grenadeHoldTime < 0.0f) {
        player->m_grenadeHoldTime = 0.0f;
    }
    BIFPop(sp);
}

void BIFunc_StopGrenadeHold(int** sp, void* self) {
    ((ISceneNode*)self)->AsPlayerObject()->m_grenadeHoldTime = -1.0f;
    BIFPop(sp);
}

void BIFunc_PlayerStartGrenadeCook(int** sp, void* self) {
    ((ISceneNode*)self)->AsPlayerObject()->StartGrenadeCook();
    BIFPop(sp);
}

void BIFunc_SetWeaponEnable(int** sp, void* self) {
    int enable = BIFArg(sp, 1);
    int type = BIFArg(sp, 0);
    bool result = ((ISceneNode*)self)->AsPlayerObject()->SetWeaponEnable((EWeaponTypes)type, enable != 0);
    BIFPop(sp);
    BIFReturn(sp, result);
}

void BIFunc_BreakPoint(int** sp, void*) {
    printf("\nbreak point!");
    BIFPop(sp);
}

void BIFunc_DebugTTYPrint(int** sp, void*) {
    BIFPop(sp);
}

void BIFunc_ScriptError(int** sp, void*) {
    BIFPop(sp);
}

// The scene node of a script object, if it has one.
static inline ISceneNode* ScriptNode(BSObject* obj) {
    BSGO_Basic* go = NULL;
    ISceneNode* node = NULL;
    if (obj != NULL) {
        go = obj->m_gameObject;
    }
    if (go != NULL) {
        node = go->GetSceneNode();
    }
    return node;
}

// The running object's position: its node's, or its trigger's.
static inline void SelfPosition(void* self, CVector3& pos) {
    if (self != NULL) {
        ((ISceneNode*)self)->GetPosition(pos);
    } else if (g_pBSObject != NULL) {
        TriggerObject_struct* trigger = g_pBSObject->m_trigger;
        if (trigger != NULL) {
            TriggerObjectData_struct* data = trigger->m_data;
            if (data != NULL) {
                pos.x = data->m_position[0];
                pos.y = data->m_position[1];
                pos.z = data->m_position[2];
            }
        }
    }
}

static inline float Length2(float x, float y) {
    float len = x * x + y * y;
    if (len > 0.0f) {
        len = sqrtf(len);
    }
    return len;
}

static inline float Length3(float x, float y, float z) {
    float len = z * z + (x * x + y * y);
    if (len > 0.0f) {
        len = sqrtf(len);
    }
    return len;
}

// The angle (about z) from the facing of `self` to the offset `dx`, `dy`.
static inline float FacingAngleTo(void* self, float dx, float dy) {
    CVector3 right;
    ((ISceneNode*)self)->GetRightward(right);
    float facing = MathFunAtan2F(-right.y, right.x);
    return MathFunNormalizeAngleNegativePiToPi(MathFunAtan2F(dx, dy) - facing);
}

// Script queries about objects: `info` selects what (objects, distances,
// angles, visibility, trigger data); `obj` is the object asked about and
// `arg` an extra value. Returns an object/int, or a float for the distance
// and angle queries.
void BIFunc_GetObjInfo(int** sp, void* self) {
    CAIFilterObject* filter = NULL;
    BSObject* obj = ADDR_TO_PTR(BSObject, BIFArg(sp, 1));
    int result = 0;
    BSGO_Basic* go = NULL;
    u32 info = BIFArg(sp, 0);
    float value = 0.0f;
    ISceneNode* node = NULL;
    float arg = BIFArgF(sp, 2);
    bool isFloat = false;
    CVector3 selfPos;
    CVector3 objPos;
    switch (info) {
    case 0x1:
        result = (int)PTR_TO_ADDR(g_scene.GetPlayer(0)->GetScriptObject());
        break;
    case 0x2:
        result = (int)PTR_TO_ADDR(g_pBSObject);
        break;
    case 0x3:
        if (obj != NULL) {
            if (self != NULL) {
                filter = SelfFilter(self);
            }
            result = (int)PTR_TO_ADDR(filter->GetTargetScript());
        }
        break;
    case 0xA:  // XY distance
        if (obj != NULL) {
            go = obj->m_gameObject;
        }
        if (go != NULL) {
            node = go->GetSceneNode();
        }
        if (node != NULL) {
            SelfPosition(self, selfPos);
            node->GetPosition(objPos);
            isFloat = true;
            value = Length2(selfPos.x - objPos.x, selfPos.y - objPos.y);
        }
        break;
    case 0xB:  // distance
        if (obj != NULL) {
            go = obj->m_gameObject;
        }
        if (go != NULL) {
            node = go->GetSceneNode();
        }
        if (node != NULL) {
            SelfPosition(self, selfPos);
            node->GetPosition(objPos);
            isFloat = true;
            selfPos.x -= objPos.x;
            selfPos.y -= objPos.y;
            selfPos.z -= objPos.z;
            value = Length3(selfPos.x, selfPos.y, selfPos.z);
        }
        break;
    case 0xC: {  // XY distance squared
        if (obj != NULL) {
            go = obj->m_gameObject;
        }
        if (go != NULL) {
            node = go->GetSceneNode();
        }
        if (node != NULL) {
            SelfPosition(self, selfPos);
            node->GetPosition(objPos);
            isFloat = true;
            float dy = selfPos.y - objPos.y;
            float dx = selfPos.x - objPos.x;
            value = dx * dx + dy * dy;
        }
        break;
    }
    case 0xD: {  // distance squared
        if (obj != NULL) {
            go = obj->m_gameObject;
        }
        if (go != NULL) {
            node = go->GetSceneNode();
        }
        if (node != NULL) {
            SelfPosition(self, selfPos);
            node->GetPosition(objPos);
            isFloat = true;
            float dy = selfPos.y - objPos.y;
            float dx = selfPos.x - objPos.x;
            float dz = selfPos.z - objPos.z;
            value = dz * dz + (dx * dx + dy * dy);
        }
        break;
    }
    case 0xE:  // pan angle to the object
        if (obj != NULL) {
            go = obj->m_gameObject;
        }
        if (go != NULL) {
            node = go->GetSceneNode();
        }
        if (node != NULL) {
            CVector3 forward;
            ((ISceneNode*)self)->GetPosition(selfPos);
            node->GetPosition(objPos);
            ((ISceneNode*)self)->GetRightward(forward);
            value = MathFunGetPanAngleDiffNoRoll(&selfPos, &objPos, &forward);
            isFloat = true;
        }
        break;
    case 0xF:  // tilt angle to the object
        if (obj != NULL) {
            go = obj->m_gameObject;
        }
        if (go != NULL) {
            node = go->GetSceneNode();
        }
        if (node != NULL) {
            CVector3 right;
            CVector3 forward;
            ((ISceneNode*)self)->GetPosition(selfPos);
            node->GetPosition(objPos);
            ((ISceneNode*)self)->GetRightward(right);
            ((ISceneNode*)self)->GetForward(forward);
            value = MathFunGetTiltAngleDiffNoRoll(&selfPos, &objPos, &right, &forward);
            isFloat = true;
        }
        break;
    case 0x10:  // height difference
        if (obj != NULL) {
            go = obj->m_gameObject;
        }
        if (go != NULL) {
            node = go->GetSceneNode();
        }
        if (node != NULL) {
            ((ISceneNode*)self)->GetPosition(selfPos);
            node->GetPosition(objPos);
            isFloat = true;
            value = objPos.z - selfPos.z;
        }
        break;
    case 0x11: {  // pan angle to the target's guessed position
        CAIFilterRealPosition guess;
        SelfFilter(self)->GetTargetGuessPosition(guess);
        ((ISceneNode*)self)->GetPosition(selfPos);
        guess.field_0x0 -= selfPos.x;
        guess.field_0x4 -= selfPos.y;
        guess.field_0x8 -= selfPos.z;
        value = FacingAngleTo(self, guess.field_0x0, guess.field_0x4);
        isFloat = true;
        break;
    }
    case 0x12: {  // tilt angle to the target's guessed position
        CAIFilterRealPosition guess;
        CVector3 right;
        CVector3 forward;
        SelfFilter(self)->GetTargetGuessPosition(guess);
        ((ISceneNode*)self)->GetPosition(selfPos);
        ((ISceneNode*)self)->GetRightward(right);
        float facing = MathFunAtan2F(-right.y, right.x);
        ((ISceneNode*)self)->GetForward(forward);
        MathFunRotateAboutZ(&forward.x, &forward.y, -facing);
        float dx = guess.field_0x0 - selfPos.x;
        float dy = guess.field_0x4 - selfPos.y;
        guess.field_0x0 = dx;
        guess.field_0x4 = dy;
        guess.field_0x8 -= selfPos.z;
        MathFunRotateAboutZ(&guess.field_0x0, &guess.field_0x4, -MathFunAtan2F(dx, dy));
        float pitch = MathFunAtan2F(-forward.z, forward.y);
        value = MathFunNormalizeAngleNegativePiToPi(MathFunAtan2F(-guess.field_0x8, guess.field_0x4) - pitch);
        isFloat = true;
        break;
    }
    case 0x14: {  // pan angle to the move point
        BSAIScriptData* data = SelfScriptData(self);
        ((ISceneNode*)self)->GetPosition(selfPos);
        objPos.x = data->m_movePoint.x - selfPos.x;
        objPos.y = data->m_movePoint.y - selfPos.y;
        objPos.z = data->m_movePoint.z - selfPos.z;
        value = FacingAngleTo(self, objPos.x, objPos.y);
        isFloat = true;
        break;
    }
    case 0x15: {  // pan angle to where the AI is walking
        if (self != NULL) {
            filter = SelfFilter(self);
        }
        CAIObject* ai = filter->m_pAIObject;
        CVector3 dest(ai->m_destination.field_0x0, ai->m_destination.field_0x4, ai->m_destination.field_0x8);
        ((ISceneNode*)self)->GetPosition(selfPos);
        dest.x -= selfPos.x;
        dest.y -= selfPos.y;
        dest.z -= selfPos.z;
        value = FacingAngleTo(self, dest.x, dest.y);
        isFloat = true;
        break;
    }
    case 0x17:  // clear line of sight from `arg` above self to the object
        result = 0;
        if (obj != NULL) {
            go = obj->m_gameObject;
        }
        if (go != NULL) {
            node = go->GetSceneNode();
        }
        if (node != NULL) {
            CAIFilterRealPosition from;
            CAIFilterRealPosition to;
            ((ISceneNode*)self)->GetPosition(objPos);
            from.field_0x0 = objPos.x;
            from.field_0x4 = objPos.y;
            from.field_0x8 = objPos.z;
            node->GetPosition(objPos);
            to.field_0x0 = objPos.x;
            to.field_0x4 = objPos.y;
            to.field_0x8 = objPos.z;
            from.field_0x8 += arg;
            to.field_0x8 = objPos.z + 0.8f;
            result = CAIFilterGlobal::CheckVisionLineToEnvironment(from, to, NULL) == 0;
        }
        break;
    case 0x18:  // distance from the target's last known position to the object
        value = 0.0f;
        isFloat = true;
        if (self != NULL && ((ISceneNode*)self)->AsSoldierObject() != NULL) {
            CAIFilterObject* own = NULL;
            if (self != NULL) {
                own = SelfFilter(self);
            }
            if (own != NULL && own->m_pAIObject->m_targetSeen) {
                if (obj != NULL) {
                    go = obj->m_gameObject;
                }
                if (go != NULL) {
                    node = go->GetSceneNode();
                }
                if (node != NULL) {
                    CAIObject* ai = own->m_pAIObject;
                    selfPos.x = ai->m_lastSeenPosition.field_0x0;
                    selfPos.y = ai->m_lastSeenPosition.field_0x4;
                    selfPos.z = ai->m_lastSeenPosition.field_0x8;
                    node->GetPosition(objPos);
                    selfPos.x -= objPos.x;
                    selfPos.y -= objPos.y;
                    selfPos.z -= objPos.z;
                    value = Length3(selfPos.x, selfPos.y, selfPos.z);
                }
            }
        }
        break;
    case 0x19:  // whether the target's last known position is in sight
        result = 0;
        if (((ISceneNode*)self)->AsSoldierObject() != NULL) {
            CAIFilterObject* own = NULL;
            if (self != NULL) {
                own = SelfFilter(self);
            }
            CAIObject* ai = own->m_pAIObject;
            if (ai->m_targetSeen) {
                CAIFilterRealPosition pos;
                pos.field_0x0 = ai->m_lastSeenPosition.field_0x0;
                pos.field_0x4 = ai->m_lastSeenPosition.field_0x4;
                pos.field_0x8 = ai->m_lastSeenPosition.field_0x8;
                result = ai->IsObjectInLineOfSight(pos, arg, false);
            }
        }
        break;
    case 0x1A:  // object in line of sight
        if (obj != NULL) {
            go = obj->m_gameObject;
        }
        if (go != NULL) {
            node = go->GetSceneNode();
        }
        if (node != NULL) {
            CAIFilterObject* own = NULL;
            if (self != NULL) {
                own = SelfFilter(self);
            }
            CAIFilterRealPosition pos;
            node->GetPosition(objPos);
            pos.field_0x0 = objPos.x;
            pos.field_0x4 = objPos.y;
            pos.field_0x8 = objPos.z;
            result = own->m_pAIObject->IsObjectInLineOfSight(pos, arg, false);
        }
        break;
    case 0x1B:  // object in field of view
    case 0x1C:
        if (obj != NULL) {
            go = obj->m_gameObject;
        }
        if (go != NULL) {
            node = go->GetSceneNode();
        }
        if (node != NULL) {
            CAIFilterObject* own = NULL;
            if (self != NULL) {
                own = SelfFilter(self);
            }
            CAIFilterRealPosition pos;
            node->GetPosition(objPos);
            pos.field_0x0 = objPos.x;
            pos.field_0x4 = objPos.y;
            pos.field_0x8 = objPos.z;
            result = own->m_pAIObject->IsObjectInFieldOfView(pos, arg, 50.0f, false);
        }
        break;
    case 0x1D: {  // scene node in field of view
        if (self != NULL) {
            filter = SelfFilter(self);
        }
        CAIFilterRealPosition pos;
        (ADDR_TO_PTR(ISceneNode, BIFArg(sp, 1)))->GetPosition(objPos);
        pos.field_0x0 = objPos.x;
        pos.field_0x4 = objPos.y;
        pos.field_0x8 = objPos.z;
        result = filter->m_pAIObject->IsObjectInFieldOfView(pos, arg, 50.0f, false);
        break;
    }
    case 0x1E: {  // scene node reachable
        if (self != NULL) {
            filter = SelfFilter(self);
        }
        CAIFilterRealPosition pos;
        (ADDR_TO_PTR(ISceneNode, BIFArg(sp, 1)))->GetPosition(objPos);
        pos.field_0x0 = objPos.x;
        pos.field_0x4 = objPos.y;
        pos.field_0x8 = objPos.z;
        result = filter->m_pAIObject->IsObjectReachable(pos);
        break;
    }
    case 0x23:  // how much the object faces self (dot of its forward with the direction)
        if (obj != NULL) {
            go = obj->m_gameObject;
        }
        if (go != NULL) {
            node = go->GetSceneNode();
        }
        if (node != NULL) {
            CVector3 forward;
            SelfPosition(self, selfPos);
            node->GetForward(forward);
            node->GetPosition(objPos);
            float dy = selfPos.y - objPos.y;
            float dx = selfPos.x - objPos.x;
            float dz = selfPos.z - objPos.z;
            float len = Length3(dx, dy, dz);
            if (len != 0.0f) {
                float inv = 1.0f / len;
                dx *= inv;
                dy *= inv;
            }
            isFloat = true;
            value = dy * forward.y + dx * forward.x;
        } else {
            isFloat = true;
            value = 0.0f;
        }
        break;
    case 0x25:  // object is behind self
        if (obj != NULL) {
            go = obj->m_gameObject;
        }
        if (go != NULL) {
            node = go->GetSceneNode();
        }
        if (node != NULL) {
            CVector3 forward;
            ((ISceneNode*)self)->GetForward(forward);
            ((ISceneNode*)self)->GetPosition(selfPos);
            node->GetPosition(objPos);
            result = (objPos.y - selfPos.y) * forward.y + (objPos.x - selfPos.x) * forward.x < 0.0f;
        }
        break;
    case 0x26:  // how much the object's right side faces self
        if (obj != NULL) {
            go = obj->m_gameObject;
        }
        if (go != NULL) {
            node = go->GetSceneNode();
        }
        if (node != NULL) {
            CVector3 right;
            node->GetRightward(right);
            ((ISceneNode*)self)->GetPosition(selfPos);
            node->GetPosition(objPos);
            float dy = selfPos.y - objPos.y;
            float dx = selfPos.x - objPos.x;
            float dz = selfPos.z - objPos.z;
            float len = Length3(dx, dy, dz);
            if (len != 0.0f) {
                float inv = 1.0f / len;
                dx *= inv;
                dy *= inv;
            }
            isFloat = true;
            value = dy * right.y + dx * right.x;
        } else {
            isFloat = true;
            value = 0.0f;
        }
        break;
    case 0x2C: {  // the player's script data word 0
        BSObject* player = g_scene.GetPlayer(0)->GetScriptObject();
        if (player != NULL) {
            int* data = (int*)player->m_gameObject->GetScriptData();
            if (data != NULL) {
                result = *data;
            }
        }
        break;
    }
    case 0x2D: {
        BSObject* player = g_scene.GetPlayer(0)->GetScriptObject();
        if (player != NULL) {
            TriggerObjectData_struct* data = player->m_trigger->m_data;
            if (data != NULL) {
                result = data->field_0x70;
            }
        }
        break;
    }
    case 0x3C:  // is a player
        if (obj != NULL) {
            if (g_bInMultiplayerMode) {
                BSGO_Basic* objGo;
                if (obj != NULL && (objGo = obj->m_gameObject) != NULL && objGo->GetSceneNode() != NULL &&
                    obj->m_gameObject->GetSceneNode()->AsPlayerObject() != NULL) {
                    result = 1;
                } else {
                    result = 0;
                }
            } else {
                result = obj == g_scene.GetPlayer(0)->GetScriptObject();
            }
        }
        break;
    case 0x3D:  // allied (the player counts)
        if (obj != NULL) {
            if (obj == g_scene.GetPlayer(0)->GetScriptObject()) {
                result = 1;
            } else {
                result = obj->m_trigger->m_data->m_team == 1;
            }
        }
        break;
    case 0x3E:  // enemy
        if (obj != NULL) {
            if (obj == g_scene.GetPlayer(0)->GetScriptObject()) {
                result = 0;
            } else {
                result = obj->m_trigger->m_data->m_team == 2;
            }
        }
        break;
    case 0x3F:  // neutral
        if (obj != NULL) {
            if (obj == g_scene.GetPlayer(0)->GetScriptObject()) {
                result = 0;
            } else {
                result = obj->m_trigger->m_data->m_team == 0;
            }
        }
        break;
    case 0x40:
        if (obj != NULL) {
            result = obj->m_trigger->m_data->field_0x38;
        }
        break;
    case 0x41:  // team
        if (obj != NULL) {
            if (obj == g_scene.GetPlayer(0)->GetScriptObject()) {
                result = 1;
            } else {
                result = obj->m_trigger->m_data->m_team;
            }
        }
        break;
    case 0x42:
        if (obj != NULL) {
            result = obj->m_trigger->m_data->m_flags & 1;
        }
        break;
    case 0x43:
        if (obj != NULL) {
            result = obj->m_trigger->m_data->field_0x3A;
        }
        break;
    case 0x44:
        if (obj != NULL) {
            result = (obj->m_trigger->m_data->m_flags >> 1) & 1;
        }
        break;
    case 0x45:
        if (obj != NULL) {
            result = obj->m_trigger->m_data->field_0x3C;
        }
        break;
    case 0x46:
    case 0x47:
        if (obj != NULL) {
            result = obj->m_trigger->m_data->field_0x70;
        }
        break;
    case 0x48:  // scene node
        if (obj != NULL) {
            BSGO_Basic* objGo = obj->m_gameObject;
            if (objGo != NULL) {
                result = (int)PTR_TO_ADDR(objGo->GetSceneNode());
            }
        }
        break;
    }
    BIFPop(sp);
    if (isFloat) {
        BIFReturnF(sp, value);
        return;
    }
    BIFReturn(sp, result);
}

// Script setters: 2 adds to the player's script data word 0 (clamped to
// 0..trigger byte 0x70), 4 switches the soldier's weapon (bit 7 picks the
// other hand), 5 dump-object collisions, 6 a soldier flag, 7 gives the AI a
// last known target position half a unit above `obj`.
void BIFunc_SetObjInfo(int** sp, void* self) {
    bool result = false;
    int info = BIFArg(sp, 0);
    BSObject* obj = ADDR_TO_PTR(BSObject, BIFArg(sp, 1));
    int value = BIFArg(sp, 2);
    switch (info) {
    case 2: {
        BSObject* player = g_scene.GetPlayer(0)->GetScriptObject();
        if (player != NULL) {
            BSGO_Basic* go = player->m_gameObject;
            TriggerObjectData_struct* data = player->m_trigger->m_data;
            int* scriptData = (int*)go->GetScriptData();
            if (data != NULL && scriptData != NULL) {
                int max = data->field_0x70;
                int n = *scriptData + value;
                if (n > max) {
                    n = max;
                }
                if (n < 0) {
                    n = 0;
                }
                *scriptData = n;
                result = true;
            }
        }
        break;
    }
    case 4: {
        CSoldierObject* soldier = ((ISceneNode*)self)->AsSoldierObject();
        CWeapon* weapon = soldier->m_pWeapons[soldier->m_curWeapon];
        if (weapon != NULL) {
            weapon->Update(1.0f, false);
            soldier->m_curWeapon = value & ~0x80;
            char* hand;
            int handCrc;
            if (!((value / 128 + soldier->m_pWeapons[soldier->m_curWeapon]->m_pProperties->m_hand) & 1)) {
                hand = "lt_hand";
                handCrc = 0x4C83C769;
            } else {
                hand = "rt_hand";
                handCrc = 0xD58A96D3;
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
            soldier->m_curWeapon = value & ~0x80;
        }
        result = true;
        break;
    }
    case 5:
        EnableDumpCollisionsByType((EDumpType)0, -1, value != 0);
        break;
    case 6: {
        CSoldierObject* soldier = ((ISceneNode*)self)->AsSoldierObject();
        if (soldier != NULL) {
            soldier->field_0x3161 = value != 0;
        }
        break;
    }
    case 7:
        if (self != NULL && obj != NULL && ((ISceneNode*)self)->AsSoldierObject() != NULL) {
            CAIFilterObject* filter = NULL;
            if (self != NULL) {
                filter = SelfFilter(self);
            }
            if (filter != NULL) {
                ISceneNode* node = ScriptNode(obj);
                if (node != NULL) {
                    CVector3 pos;
                    node->GetPosition(pos);
                    pos.z += 0.5f;
                    CAIObject* ai = filter->m_pAIObject;
                    ai->m_lastSeenPosition.field_0x0 = pos.x;
                    ai->m_lastSeenPosition.field_0x4 = pos.y;
                    ai->m_lastSeenPosition.field_0x8 = pos.z;
                    ai->m_targetSeen = true;
                }
            }
        }
        break;
    }
    BIFPop(sp);
    BIFReturn(sp, result);
}

// Footstep-like noises (0x33) are silent while the player is disguised.
static inline bool PlayerDisguised() {
    u32* data = (u32*)g_scene.GetPlayer(0)->GetScriptObject()->m_gameObject->GetScriptData();
    return data[4] != 0 && data[5] != 0;
}

static inline float ClampVolume(float volume) {
    if (volume < 0.0f) {
        volume = 0.0f;
    }
    if (volume > 6.0f) {
        volume = 6.0f;
    }
    return volume;
}

// Lets the AIs hear a noise of `type` at `pos`.
void MakeNoise(void* source, CAIFilterObject* filter, const CVector3& pos, int type, float volume, bool flag,
               int arg) {
    if (type != 0x33 || !PlayerDisguised()) {
        volume = ClampVolume(volume);
        CAIFilterSoundInfo info;
        info.m_source = source;
        info.m_sourceFilter = filter;
        info.m_type = type;
        info.m_position.field_0x0 = pos.x;
        info.m_position.field_0x4 = pos.y;
        info.m_position.field_0x8 = pos.z;
        BSObject* script;
        if (filter != NULL && (script = ((CAIFilterSoldierObject*)filter)->m_pBSObject) != NULL) {
            info.m_sourceScript = script;
        } else {
            info.m_sourceScript = NULL;
        }
        info.m_volume = volume;
        info.m_radius = 10.0f;
        info.field_0x28 = flag;
        info.field_0x2C = arg;
        CAIFilterGlobal::PropagateSoundToAllObjects(info);
    }
}

void MakeNoise(ISceneNode* node, int type, float volume, int arg) {
    CAIFilterObject* filter = NULL;
    CVector3 pos;
    if (node != NULL && node->GetAIDoodad() != NULL) {
        filter = node->GetAIDoodad()->m_pFilter;
    }
    if (node != NULL) {
        node->GetPosition(pos);
    }
    if (type != 0x33 || !PlayerDisguised()) {
        volume = ClampVolume(volume);
        CAIFilterSoundInfo info;
        info.m_source = node;
        info.m_sourceFilter = filter;
        info.m_type = type;
        info.m_position.field_0x0 = pos.x;
        info.m_position.field_0x4 = pos.y;
        info.m_position.field_0x8 = pos.z;
        BSObject* script;
        if (filter != NULL && (script = ((CAIFilterSoldierObject*)filter)->m_pBSObject) != NULL) {
            info.m_sourceScript = script;
        } else {
            info.m_sourceScript = NULL;
        }
        info.m_volume = volume;
        info.m_radius = 10.0f;
        info.field_0x28 = true;
        info.field_0x2C = arg;
        CAIFilterGlobal::PropagateSoundToAllObjects(info);
    }
}

// A noise at trigger `trigger`'s position (0x33 when `loud` is 0).
void BIFunc_MakeNoiseFromInputTrigger(int** sp, void* self) {
    int type = 0x34;
    float volume = BIFArgF(sp, 2);
    if (BIFArg(sp, 1) == 0) {
        type = 0x33;
    }
    TriggerObjectData_struct* data = (ADDR_TO_PTR(TriggerObject_struct, BIFArg(sp, 0)))->m_data;
    float z = data->m_position[2];
    float y = data->m_position[1];
    float x = data->m_position[0];
    if (type != 0x33 || !PlayerDisguised()) {
        volume = ClampVolume(volume);
        CAIFilterSoundInfo info;
        info.m_source = self;
        info.m_sourceFilter = NULL;
        info.m_type = type;
        info.m_position.field_0x0 = x;
        info.m_position.field_0x4 = y;
        info.m_position.field_0x8 = z;
        info.m_sourceScript = NULL;
        info.m_volume = volume;
        info.m_radius = 10.0f;
        info.field_0x28 = true;
        info.field_0x2C = 1;
        CAIFilterGlobal::PropagateSoundToAllObjects(info);
    }
    BIFPop(sp);
}

// A noise at the running object (its node, or its trigger).
void BIFunc_MakeNoise(int** sp, void* self) {
    int type = 0x34;
    float volume = BIFArgF(sp, 1);
    if (BIFArg(sp, 0) == 0) {
        type = 0x33;
    }
    if (g_pBSObject != NULL && g_pBSObject->m_gameObject != NULL &&
        g_pBSObject->m_gameObject->GetSceneNode() != NULL) {
        ISceneNode* node = g_pBSObject->m_gameObject->GetSceneNode();
        CAIFilterObject* filter = NULL;
        CVector3 pos;
        if (node != NULL && node->GetAIDoodad() != NULL) {
            filter = node->GetAIDoodad()->m_pFilter;
        }
        if (node != NULL) {
            node->GetPosition(pos);
        }
        if (type != 0x33 || !PlayerDisguised()) {
            volume = ClampVolume(volume);
            CAIFilterSoundInfo info;
            info.m_source = node;
            info.m_sourceFilter = filter;
            info.m_type = type;
            info.m_position.field_0x0 = pos.x;
            info.m_position.field_0x4 = pos.y;
            info.m_position.field_0x8 = pos.z;
            BSObject* script;
            if (filter != NULL && (script = ((CAIFilterSoldierObject*)filter)->m_pBSObject) != NULL) {
                info.m_sourceScript = script;
            } else {
                info.m_sourceScript = NULL;
            }
            info.m_volume = volume;
            info.m_radius = 10.0f;
            info.field_0x28 = true;
            info.field_0x2C = type == 0x33 ? 1 : 3;
            CAIFilterGlobal::PropagateSoundToAllObjects(info);
        }
    } else if (g_pBSObject != NULL) {
        TriggerObject_struct* trigger = g_pBSObject->m_trigger;
        if (trigger != NULL) {
            TriggerObjectData_struct* data = trigger->m_data;
            if (data != NULL) {
                float z = data->m_position[2];
                float y = data->m_position[1];
                float x = data->m_position[0];
                if (type != 0x33 || !PlayerDisguised()) {
                    volume = ClampVolume(volume);
                    CAIFilterSoundInfo info;
                    info.m_source = self;
                    info.m_sourceFilter = NULL;
                    info.m_type = type;
                    info.m_position.field_0x0 = x;
                    info.m_position.field_0x4 = y;
                    info.m_position.field_0x8 = z;
                    info.m_sourceScript = NULL;
                    info.m_volume = volume;
                    info.m_radius = 10.0f;
                    info.field_0x28 = true;
                    info.field_0x2C = type == 0x33 ? 1 : 3;
                    CAIFilterGlobal::PropagateSoundToAllObjects(info);
                }
            }
        }
    }
    BIFPop(sp);
}

void BIFunc_GetVoiceHandle(int** sp, void* self) {
    int handle = -1;
    CSoldierObject* soldier = ((ISceneNode*)self)->AsSoldierObject();
    if (soldier != NULL) {
        handle = soldier->m_soundHandle;
    }
    BIFPop(sp);
    BIFReturn(sp, handle);
}

void BIFunc_StopCurrentVoice(int** sp, void* self) {
    CSoldierObject* soldier = ((ISceneNode*)self)->AsSoldierObject();
    if (soldier != NULL) {
        int handle = soldier->m_soundHandle;
        if (handle != -1) {
            g_SoundSchedule.RemoveSound(handle, true);
            soldier->m_soundHandle = -1;
        }
    }
    BIFPop(sp);
    BIFReturn(sp, 1);
}

// The BSP leaf the running object is in (from its proximity data).
static inline _PropBSPLeaf* ProximityLeaf(void* data) {
    return (_PropBSPLeaf*)(const _PropBSPLeaf*)((_ProximityData*)data)->m_newLeaf;
}

// The script object of the nearest waypoint of `type` within `dist`.
void BIFunc_GetClosestValidWaypoint(int** sp, void*) {
    BSGO_Basic* go = g_pBSObject->m_gameObject;
    int any = BIFArg(sp, 4);
    int type = BIFArg(sp, 0);
    float dist = BIFArgF(sp, 2);
    void* prox = go->GetProximityData();
    int result;
    if (ProximityLeaf(prox) != NULL) {
        CVector3 pos;
        g_pBSObject->m_gameObject->GetSceneNode()->GetPosition(pos);
        TriggerObject_struct* waypoint = g_aigAIFilterGlobalObject.GetClosestWaypointByType(
            ProximityLeaf(prox), *(CAIFilterRealPosition*)&pos, (DWI_OBJECT_CRC_ENUM)type, any == 0, dist * dist);
        if (waypoint != NULL) {
            result = (int)PTR_TO_ADDR(waypoint->m_object);
        } else {
            result = -1;
        }
    } else {
        result = -1;
    }
    BIFPop(sp);
    BIFReturn(sp, result);
}

// The index of the nearest cover point within `dist` with a free peek
// position, or -1.
void BIFunc_AIGetClosestCoverPoint(int** sp, void*) {
    BSGO_Basic* go = g_pBSObject->m_gameObject;
    int result = -1;
    float dist = BIFArgF(sp, 0);
    void* prox = go->GetProximityData();
    if (ProximityLeaf(prox) != NULL) {
        CVector3 pos;
        g_pBSObject->m_gameObject->GetSceneNode()->GetPosition(pos);
        TriggerObject_struct* cover = g_aigAIFilterGlobalObject.GetClosestWaypointByType(
            ProximityLeaf(prox), *(CAIFilterRealPosition*)&pos, (DWI_OBJECT_CRC_ENUM)0x9101C906, true, dist * dist);
        if (cover != NULL && ((CCoverPoint*)cover)->GetAvailablePeekPosition() != 0) {
            result = cover - g_pTriggerObjects;
        }
    }
    BIFPop(sp);
    BIFReturn(sp, result);
}

void BIFunc_DisplayLetterbox(int** sp, void*) {
    bool on = BIFArg(sp, 0) != 0;
    g_scene.GetPlayer(0)->m_ui.m_letterbox = on;
    BIFPop(sp);
}

void BIFunc_LoadOneShotSoundBank(int** sp, void*) {
    int bank = BIFArg(sp, 0);
    int mission;
    int stage;
    if (g_bInMultiplayerMode) {
        mission = g_Shell.m_mpMission;
        stage = g_Shell.m_mpStage;
    } else {
        mission = g_Shell.Get_currentMission();
        stage = g_Shell.Get_currentStage();
    }
    LoadOneShotSound(bank, mission, stage);
    g_CurrentMission = mission;
    g_CurrentLevel = stage;
    g_CurrentSoundBank = bank;
    BIFPop(sp);
}

void BIFunc_DebugInitFromTrigger(int** sp, void*) {
    BIFPop(sp);
}

// Prompt text: lines are built in a ring of 20 buffers (wrapped at
// g_promptLineWidth / g_promptMaxSize) and shown on the HUD message handlers
// when complete. Hints use a ring of 8.
#define PROMPT_LINES 20
#define HINT_LINES 8
#define PROMPT_LINE_SIZE 0x50

static PopUpMessage g_popUpMessage;
static char g_szHintBuffer[HINT_LINES][PROMPT_LINE_SIZE];
static char g_promptBuffer[PROMPT_LINES][PROMPT_LINE_SIZE];
static int g_promptstart = PROMPT_LINES - 1;  // the newest line shown
static char* g_promptCursor = g_promptBuffer[0];
static int g_promptLineWidth = 0x28;
static int g_promptMaxSize = 0x30;
static int g_iHintBufferIndex;
static int g_prompt;  // the line being written
static int g_promptColumn;

static inline void SetupPromptMessage(u8 r, u8 g, u8 b, u8 a) {
    g_popUpMessage.Set_placement(4);
    g_popUpMessage.Set_screenLocs(320, 12.0f * g_screen.m_heightScale);
    g_popUpMessage.Set_property(1);
    g_popUpMessage.Set_onScreenTime(5.0f);
    g_popUpMessage.Set_textSize(0x16);
    g_popUpMessage.Set_rgba(r, g, b, a);
}

// Shows the prompt lines from the newest back to g_promptstart (to player
// `player`'s HUD, or every HUD with -1 in multiplayer) and starts a new one.
void displayPromptBuffer(int player) {
    int line = g_prompt;
    while (line != g_promptstart) {
        SetupPromptMessage(0x3F, 0x3F, 0x3F, 0x3F);
        char* text = g_promptBuffer[line];
        g_popUpMessage.SetString(text);
        if (!g_bInMultiplayerMode) {
            player = 0;
        }
        if (player == -1) {
            for (int i = 0; i < 4; i++) {
                PromptUserMessage(text, g_pPopUpMessageHandler[i], &g_popUpMessage);
            }
        } else {
            PromptUserMessage(text, g_pPopUpMessageHandler[player], &g_popUpMessage);
        }
        line = line != 0 ? line - 1 : PROMPT_LINES - 1;
    }
    g_promptstart = g_prompt;
    g_promptColumn = 0;
    g_prompt = (g_prompt + 1) % PROMPT_LINES;
    g_promptCursor = g_promptBuffer[g_prompt];
}

// Appends text to the prompt, word-wrapping into new lines.
void appendMessageToPrompt(const char* text, int) {
    int len = strlen(text);
    while (len > g_promptMaxSize - g_promptColumn) {
        const char* end = text;
        while (end - text <= g_promptLineWidth - g_promptColumn) {
            const char* space = strchr(end, ' ');
            if (space == NULL || space + 1 - text > g_promptMaxSize - g_promptColumn) {
                break;
            }
            end = space + 1;
        }
        int n = end - text;
        strncpy(g_promptCursor, text, n);
        text = end;
        g_promptCursor[n] = 0;
        g_promptColumn = 0;
        g_prompt = (g_prompt + 1) % PROMPT_LINES;
        g_promptCursor = g_promptBuffer[g_prompt];
        len = strlen(end);
    }
    strcpy(g_promptCursor, text);
    g_promptCursor = &g_promptCursor[len];
    g_promptColumn += len;
}

static inline void NextHintLine() {
    g_iHintBufferIndex = g_iHintBufferIndex == HINT_LINES - 1 ? 0 : g_iHintBufferIndex + 1;
}

// Shows a hint (single player), word-wrapped into the hint lines.
void DisplayHintText(const char* text) {
    int len = strlen(text);
    int lines = 0;
    if (g_bInMultiplayerMode) {
        return;
    }
    while (len > g_promptLineWidth) {
        const char* end = text;
        while (end - text <= g_promptLineWidth) {
            const char* space = strchr(end, ' ');
            if (space != NULL) {
                if (space + 1 - text > g_promptMaxSize) {
                    break;
                }
                end = space + 1;
            } else {
                if (len <= g_promptMaxSize) {
                    end = &text[len];
                }
                break;
            }
        }
        int n = end - text;
        strncpy(g_szHintBuffer[g_iHintBufferIndex], text, n);
        text = end;
        lines++;
        g_szHintBuffer[g_iHintBufferIndex][n] = 0;
        NextHintLine();
        len = strlen(end);
    }
    if (len > 0) {
        strcpy(g_szHintBuffer[g_iHintBufferIndex], text);
        lines++;
        g_szHintBuffer[g_iHintBufferIndex][len] = 0;
        NextHintLine();
    }
    int line = g_iHintBufferIndex != 0 ? g_iHintBufferIndex - 1 : HINT_LINES - 1;
    for (int i = lines - 1; i >= 0; i--) {
        SetupPromptMessage(0x3C, 0x3C, 0x28, 0x80);
        char* hint = g_szHintBuffer[line];
        g_popUpMessage.SetString(hint);
        PromptUserMessage(hint, g_pPopUpMessageHandler[0], &g_popUpMessage);
        line = line != 0 ? line - 1 : HINT_LINES - 1;
    }
}

// Shows user message `id` (from the message table) on player 0's HUD.
void BIFunc_DisplayPrompt(int** sp, void*) {
    int id = BIFArg(sp, 0);
    if (id != -1) {
        PromptUserMessage(id, g_pPopUpMessageHandler[0]);
    }
    BIFPop(sp);
}

// Appends user message `id` to the prompt; `show` displays the prompt.
void BIFunc_DisplayPromptText(int** sp, void*) {
    int id = BIFArg(sp, 0);
    bool show = BIFArg(sp, 1) != 0;
    char* text = GetUMString(id);
    if (text != NULL) {
        appendMessageToPrompt(text, 0);
        if (show && g_promptCursor != g_promptBuffer[g_prompt]) {
            displayPromptBuffer(-1);
        }
    }
    BIFPop(sp);
}

void BIFunc_DisplayPromptInteger(int** sp, void*) {
    char text[16];
    int show = BIFArg(sp, 1);
    sprintf(text, "%d", BIFArg(sp, 0));
    appendMessageToPrompt(text, 0);
    if (show != 0 && g_promptCursor != g_promptBuffer[g_prompt]) {
        displayPromptBuffer(-1);
    }
    BIFPop(sp);
}

void BIFunc_SEDEvent(int** sp, void*) {
    BIFPop(sp);
}

// Plays scripted sound `sound` (with the object's sound groups from its
// trigger: category 0xB the bank, 0xC the variant) at the object or trigger,
// offset by `offset`; the player's sound 0x52 uses the bank of their
// multiplayer character. Returns the sound handle, or -1.
int PlaySoundFromTriggerOrObject(TriggerObject_struct* trigger, BSObject* script, void* self, int sound, int flags,
                                 CVector3& offset) {
    int handle = -1;
    u32 bank = -1;
    u32 variant = -1;
    TriggerObjectData_struct* data = trigger->m_data;
    u32* groups = data->m_groups;
    if (groups != NULL) {
        u32* end = groups + 1 + groups[0];
        for (u32* g = groups + 1; g <= end; g++) {
            u32 group = *g;
            u32 category = group >> 16;
            if (category == 0xB) {
                bank = (u16)group;
            } else if (category == 0xC) {
                variant = (u16)group;
            }
        }
    }
    CVector3 pos;
    if (self != NULL) {
        ((ISceneNode*)self)->GetPosition(pos);
    } else {
        pos.x = data->m_position[0];
        pos.y = data->m_position[1];
        pos.z = data->m_position[2];
    }
    pos.x += offset.x;
    pos.y += offset.y;
    pos.z += offset.z;
    if ((int)bank == -1 && sound == 0x52 && self != NULL && ((ISceneNode*)self)->AsPlayerObject() != NULL) {
        if (g_bInMultiplayerMode) {
            int character = g_Shell.m_players[((ISceneNode*)self)->AsPlayerObject()->field_0x768].m_character;
            if (character == 5 || character == 7 || character == 8) {
                bank = 0x1C1;
            } else {
                bank = 0x1C6;
            }
        } else {
            bank = 0x1C6;
        }
    }
    CSoundTable::SSoundParms parms;
    if ((int)bank != -1 && g_soundMapSingleton.GetSoundEvent(bank, variant, sound, &parms)) {
        handle = PlayScriptedSound((ISceneNode*)self, pos, parms, flags, script);
    }
    return handle;
}

void BIFunc_PlaySoundFromInputTrigger(int** sp, void* self) {
    CVector3 offset(0.0f, 0.0f, 0.0f);
    int handle = PlaySoundFromTriggerOrObject(ADDR_TO_PTR(TriggerObject_struct, BIFArg(sp, 0)), g_pBSObject, self, BIFArg(sp, 1),
                                              BIFArg(sp, 2), offset);
    BIFPop(sp);
    BIFReturn(sp, handle);
}

void BIFunc_PlaySoundAtOffset(int** sp, void* self) {
    CVector3 offset(BIFArgF(sp, 2), BIFArgF(sp, 3), BIFArgF(sp, 4));
    int handle =
        PlaySoundFromTriggerOrObject(g_pBSObject->m_trigger, g_pBSObject, self, BIFArg(sp, 0), BIFArg(sp, 1), offset);
    BIFPop(sp);
    BIFReturn(sp, handle);
}

void BIFunc_PlaySound(int** sp, void* self) {
    int handle = -1;
    int sound = BIFArg(sp, 0);
    CVector3 offset(0.0f, 0.0f, 0.0f);
    if (sound != -1) {
        handle = PlaySoundFromTriggerOrObject(g_pBSObject->m_trigger, g_pBSObject, self, sound, BIFArg(sp, 1), offset);
    }
    BIFPop(sp);
    BIFReturn(sp, handle);
}

void BIFunc_EndSound(int** sp, void*) {
    int handle = BIFArg(sp, 0);
    if (handle != -1) {
        g_SoundSchedule.RemoveSound(handle, true);
    }
    BIFPop(sp);
}

// Sends AEMS (audio event) `data` (mode 0 send, 1 begin, 2 update, 3 end
// `handle`), with its volume field scaled by `scale`. Positional or tracked
// sounds (types 7/8/14/15 on a soldier) are also kept in g_SoundSchedule.
void BIFunc_AEMSEvent(int** sp, void* self) {
    int handle = -1;
    int mode = BIFArg(sp, 0);
    int* data = ADDR_TO_PTR(int, BIFArg(sp, 1));
    int eventHandle = BIFArg(sp, 2);
    int positional = BIFArg(sp, 3);
    int event = BIFArg(sp, 4);
    float scale = BIFArgF(sp, 5);
    CSoldierObject* soldier;
    if (self != NULL) {
        soldier = ((ISceneNode*)self)->AsSoldierObject();
    } else {
        soldier = NULL;
    }
    AEMS_SetObject(soldier);
    int a;
    int volumeIndex;
    int c;
    int d;
    GetAEMSStructureValues(data[0], &a, &volumeIndex, &c, &d);
    int type = 0;
    data[volumeIndex] = (float)data[volumeIndex] * scale;
    CVector3 pos;
    if (positional != 0 && self == NULL) {
        type = 1;
        TriggerObjectData_struct* trigger = g_pBSObject->m_trigger->m_data;
        pos.x = trigger->m_position[0];
        pos.y = trigger->m_position[1];
        pos.z = trigger->m_position[2];
    }
    bool tracked = false;
    if (soldier != NULL) {
        switch (data[0]) {
        case 15:
        case 8:
            tracked = true;
            break;
        case 14:
        case 7:
            tracked = data[1] == 1;
            break;
        }
    }
    switch (mode) {
    case 0:
        handle = AEMS_SendEvent(data);
        if (positional != 0 || tracked) {
            GetAEMSStructureValues(data[0], &a, &volumeIndex, &c, &d);
            g_SoundSchedule.AddSound((SoundSchedule::ESoundScheduleObjectType)type, (ISceneNode*)self, &pos, data, d,
                                     handle, a, volumeIndex, c, g_pBSObject, (EBSEventEnum)event, scale, tracked, 0);
        }
        break;
    case 1:
        handle = AEMS_BeginEvent(data);
        if (positional != 0 || tracked) {
            GetAEMSStructureValues(data[0], &a, &volumeIndex, &c, &d);
            g_SoundSchedule.AddSound((SoundSchedule::ESoundScheduleObjectType)type, (ISceneNode*)self, &pos, data, d,
                                     handle, a, volumeIndex, c, g_pBSObject, (EBSEventEnum)event, scale, tracked, 0);
        }
        break;
    case 2:
        handle = AEMS_UpdateEvent(data, eventHandle);
        if (positional != 0 || tracked) {
            GetAEMSStructureValues(data[0], &a, &volumeIndex, &c, &d);
            g_SoundSchedule.UpdateSound(eventHandle, data, d);
        }
        break;
    case 3:
        handle = AEMS_EndEvent(data, eventHandle);
        if (positional != 0 || tracked) {
            g_SoundSchedule.RemoveSound(eventHandle, false);
        }
        break;
    }
    BIFPop(sp);
    BIFReturn(sp, handle);
}

void BIFunc_PathfinderSetLevel(int** sp, void*) {
    MUSIC_SetLevel(BIFArg(sp, 0));
    BIFPop(sp);
}

void BIFunc_PathfinderEvent(int** sp, void*) {
    MUSIC_SendEvent(BIFArg(sp, 0));
    BIFPop(sp);
}

void BIFunc_PathfinderFadeVolume(int** sp, void*) {
    MUSIC_Fade(BIFArg(sp, 0) / 100.0f, BIFArg(sp, 1));
    BIFPop(sp);
}

void BIFunc_PathfinderSetLatency(int** sp, void*) {
    MUSIC_SetLatency(BIFArg(sp, 0));
    BIFPop(sp);
}

u8 g_pGenericSoundMemory[0x50];

void BIFunc_GetGenericSoundStructureMemory(int** sp, void*) {
    BIFPop(sp);
    BIFReturn(sp, (int)PTR_TO_ADDR(g_pGenericSoundMemory));
}

void BIFunc_FloatToInt(int** sp, void*) {
    int* top = *sp;
    BSBuiltInFunction_struct& bif = BIFCurrent();
    *sp = top - bif.m_popCount;
    BIFReturn(sp, *(float*)(top - (bif.m_numArgs - 1)));
}

void BIFunc_InitToInvalid(int** sp, void*) {
    BIFPop(sp);
    BIFReturn(sp, 0);
}

// Keeps a particle system's position but resets its orientation.
void BIFunc_ResetParticleSystemRotation(int** sp, void*) {
    CParticleSystem* ps = ADDR_TO_PTR(CParticleSystem, BIFArg(sp, 0));
    if (ps != NULL) {
        CMatrix m;
        ps->GetLocalToWorld(m);
        CVector3 pos = *(CVector3*)m.m[3];
        m.Ident();
        m.SetPos(pos);
        ps->SetLocalToWorld(m);
    }
    BIFPop(sp);
}

// Starts particle effect `type` at the object (its bounding-volume centre
// for plain static objects) or the trigger, offset by (x, y, z).
void BIFunc_CreateParticleSystemByType(int** sp, void* self) {
    CVector3 offset;
    int type = BIFArg(sp, 0);
    offset.x = BIFArgF(sp, 1);
    offset.y = BIFArgF(sp, 2);
    offset.z = BIFArgF(sp, 3);
    CMatrix m;
    if (self != NULL) {
        ISceneNode* node = (ISceneNode*)self;
        node->GetTMLocalToWorld(m);
        if (node->AsStaticObject() != NULL && node->AsStaticObject()->AsThrownObject() == NULL &&
            node->AsStaticObject()->AsHierTankObject() == NULL) {
            const IVolume* vol = node->GetWorldBoundingVolume((ISceneNode::EVolumeType)3);
            if (vol != NULL) {
                m.SetPos(((const CVolBox*)vol)->m_center);
            }
        }
    } else {
        TriggerObjectData_struct* data = g_pBSObject->m_trigger->m_data;
        CVector3 pos(data->m_position[0], data->m_position[1], data->m_position[2]);
        ((CQuaternion*)data->m_orientation)->GetMatrix(m);
        m.SetPos(pos);
    }
    m.PreTranslate(offset);
    CParticleSystem* ps = CParticleSystem::Create(type);
    if (ps != NULL) {
        ps->SetLocalToWorld(m);
    }
    BIFPop(sp);
    BIFReturn(sp, (int)PTR_TO_ADDR(ps));
}

void BIFunc_SetParticleSystemVelocity(int** sp, void*) {
    CParticleSystem* ps = ADDR_TO_PTR(CParticleSystem, BIFArg(sp, 0));
    if (ps != NULL) {
        CVector3 vel;
        vel.z = BIFArgF(sp, 3);
        vel.x = BIFArgF(sp, 1);
        vel.y = BIFArgF(sp, 2);
        ps->SetSystemInitialVelocity(vel);
    }
    BIFPop(sp);
}

void BIFunc_DestroyParticleSystem(int** sp, void*) {
    CParticleSystem* ps = ADDR_TO_PTR(CParticleSystem, BIFArg(sp, 0));
    if (ps != NULL) {
        ps->DeActivate();
    }
    BIFPop(sp);
}

// Creates light `id`, attached to the object at (x, y, z) or placed at that
// offset from the trigger.
void BIFunc_CreateLightByType(int** sp, void* self) {
    int attach = BIFArg(sp, 4);
    CVector3 offset;
    offset.x = BIFArgF(sp, 1);
    offset.y = BIFArgF(sp, 2);
    offset.z = BIFArgF(sp, 3);
    CLight* light = CLight::Create((u32)BIFArg(sp, 0));
    if (self != NULL && attach != 0) {
        ((ISceneNode*)self)->SetAttachedLight(light, offset);
    } else {
        TriggerObjectData_struct* data = g_pBSObject->m_trigger->m_data;
        CVector3 pos(data->m_position[0], data->m_position[1], data->m_position[2]);
        CMatrix m;
        ((CQuaternion*)data->m_orientation)->GetMatrix(m);
        m.SetPos(pos);
        m.PreTranslate(offset);
        light->SetTMLocalToWorld(m);
    }
    BIFPop(sp);
    BIFReturn(sp, (int)PTR_TO_ADDR(light));
}

void BIFunc_DestroyLight(int** sp, void*) {
    (ADDR_TO_PTR(CLight, BIFArg(sp, 0)))->MarkForDestruction(0);
    BIFPop(sp);
}

void BIFunc_GetEventSystemMemBlock(int** sp, void*) {
    void* block = BSEventGetEventMemoryBlock(0x20);
    BIFPop(sp);
    BIFReturn(sp, (int)PTR_TO_ADDR(block));
}

void BIFunc_GetEmbeddedHitReactionInfo(int** sp, void*) {
    void* info = g_pBSObject->m_gameObject->GetEmbeddedHitReactionInfo();
    BIFPop(sp);
    BIFReturn(sp, (int)PTR_TO_ADDR(info));
}

int g_iHighestObjectiveNumber;

void BIFunc_AddObjective(int** sp, void*) {
    int index = BIFArg(sp, 0);
    AddObjective(index, BIFArg(sp, 1));
    if (index > g_iHighestObjectiveNumber) {
        g_iHighestObjectiveNumber = index;
    }
    BIFPop(sp);
}

// The running object's scene node as a static or hierarchical object.
static inline CStaticObject* ScriptStaticObject() {
    return g_pBSObject->m_gameObject->GetSceneNode()->AsStaticObject();
}

static inline CHierObject* ScriptHierObject() {
    return g_pBSObject->m_gameObject->GetSceneNode()->AsHierObject();
}

void BIFunc_StaticObjectPlayAnimation(int** sp, void*) {
    int a = BIFArg(sp, 0);
    int b = BIFArg(sp, 1);
    int c = BIFArg(sp, 2);
    int event = BIFArg(sp, 3);
    ScriptStaticObject()->PlayAnimation(a, b, c, (EBSEventEnum)event);
    BIFPop(sp);
}

void BIFunc_HierObjectPlayAnimation(int** sp, void*) {
    int index = BIFArg(sp, 0);
    int a = BIFArg(sp, 1);
    int b = BIFArg(sp, 2);
    int c = BIFArg(sp, 3);
    int event = BIFArg(sp, 4);
    ScriptStaticObject();
    ScriptHierObject()->GetSubObject(index)->PlayAnimation(a, b, c, (EBSEventEnum)event);
    BIFPop(sp);
}

// Plays a recorded motion on the object (index < 0) or sub-object `index`.
void BIFunc_StartMotionPlayback(int** sp, void*) {
    int index = BIFArg(sp, 0);
    int a = BIFArg(sp, 1);
    int b = BIFArg(sp, 2);
    int event = BIFArg(sp, 3);
    if (index < 0) {
        ScriptStaticObject()->StartMotionPlayback(a, b, (EBSEventEnum)event);
    } else {
        ScriptHierObject()->GetSubObject(index)->StartMotionPlayback(a, b, (EBSEventEnum)event);
    }
    BIFPop(sp);
}

void BIFunc_StopMotionPlayback(int** sp, void*) {
    int index = BIFArg(sp, 0);
    if (index < 0) {
        ScriptStaticObject()->StopMotionPlayback();
    } else {
        ScriptHierObject()->GetSubObject(index)->StopMotionPlayback();
    }
    BIFPop(sp);
}

// Moves the object (index -1) or a sub-object at `speed` (per second, per
// axis) until it has gone `distance` along the first axis to arrive.
void BIFunc_TranslateRel(int** sp, void*) {
    int index = BIFArg(sp, 0);
    int event = BIFArg(sp, 7);
    CVector3 speed;
    CVector3 distance;
    speed.x = BIFArgF(sp, 1);
    speed.y = BIFArgF(sp, 2);
    speed.z = BIFArgF(sp, 3);
    distance.x = BIFArgF(sp, 4);
    distance.y = BIFArgF(sp, 5);
    distance.z = BIFArgF(sp, 6);
    if (index != -1) {
        ScriptHierObject()->GetSubObject(index)->TranslateFromScript(speed, distance, (EBSEventEnum)event);
    } else {
        ScriptStaticObject()->TranslateFromScript(speed, distance, (EBSEventEnum)event);
    }
    BIFPop(sp);
}

// Rotates at a rate until it has turned by an angle (both in degrees).
void BIFunc_RotateRel(int** sp, void*) {
    float index = BIFArg(sp, 0);
    int event = BIFArg(sp, 7);
    CVector3 rate;
    CVector3 angles;
    rate.x = 3.1415927f * BIFArgF(sp, 1) / 180.0f;
    rate.y = 3.1415927f * BIFArgF(sp, 2) / 180.0f;
    rate.z = 3.1415927f * BIFArgF(sp, 3) / 180.0f;
    angles.x = 3.1415927f * BIFArgF(sp, 4) / 180.0f;
    angles.y = 3.1415927f * BIFArgF(sp, 5) / 180.0f;
    angles.z = 3.1415927f * BIFArgF(sp, 6) / 180.0f;
    if (index != -1.0f) {
        ScriptHierObject()->GetSubObject(index)->RotateFromScript(rate, angles, (EBSEventEnum)event);
    } else {
        ScriptStaticObject()->RotateFromScript(rate, angles, (EBSEventEnum)event);
    }
    BIFPop(sp);
}

void BIFunc_SetObjective(int** sp, void*) {
    SetObjectiveStatus(BIFArg(sp, 0), BIFArg(sp, 1) != 0);
    BIFPop(sp);
}

void BIFunc_GetObjective(int** sp, void*) {
    int status = GetObjectiveStatus(BIFArg(sp, 0));
    BIFPop(sp);
    BIFReturn(sp, status);
}

// A script result word: an int or a float.
union BIFValue {
    int i;
    float f;
};

// Script queries about a waypoint (`waypoint` is a trigger index with mode
// 0, a script object with mode 1) relative to the running object.
void BIFunc_GetWayPointInfo(int** sp, void* self) {
    BIFValue result;
    result.f = 0.0f;
    TriggerObjectData_struct* waypoint = NULL;
    int mode = BIFArg(sp, 1);
    int arg = BIFArg(sp, 0);
    u32 info = BIFArg(sp, 2);
    switch (mode) {
    case 0:
        waypoint = g_pTriggerObjects[arg].m_data;
        break;
    case 1:
        waypoint = (ADDR_TO_PTR(BSObject, arg))->m_trigger->m_data;
        break;
    }
    switch (info) {
    case 0: {  // which side of the object the waypoint is on (3 behind/side, 2 in front)
        CVector3 forward;
        CVector3 right;
        CVector3 pos;
        ScriptNode(g_pBSObject)->GetForward(forward);
        ScriptNode(g_pBSObject)->GetRightward(right);
        ScriptNode(g_pBSObject)->GetPosition(pos);
        float dy = waypoint->m_position[1] - pos.y;
        float dx = waypoint->m_position[0] - pos.x;
        float dz = waypoint->m_position[2] - pos.z;
        float len = Length3(dx, dy, dz);
        if (len != 0.0f) {
            float inv = 1.0f / len;
            dx *= inv;
            dy *= inv;
            dz *= inv;
        }
        float side = right.z * dz + (right.x * dx + right.y * dy);
        if (side < 0.7071f) {
            result.i = 3;
        } else if (side > -0.7071f) {
            result.i = 2;
        } else if (forward.z * dz + (forward.x * dx + forward.y * dy) >= 0.0f) {
            result.i = 0;
        } else {
            result.i = 1;
        }
        break;
    }
    case 1: {  // distance
        float x = waypoint->m_position[0];
        float y = waypoint->m_position[1];
        float z = waypoint->m_position[2];
        CVector3 pos;
        ScriptNode(g_pBSObject)->GetPosition(pos);
        result.f = Length3(x - pos.x, y - pos.y, z - pos.z);
        break;
    }
    case 2: {  // angle to the waypoint
        CVector3 pos;
        CVector3 forward;
        CVector3 right;
        ScriptNode(g_pBSObject)->GetPosition(pos);
        float dx = waypoint->m_position[0] - pos.x;
        float dy = waypoint->m_position[1] - pos.y;
        ScriptNode(g_pBSObject)->GetForward(forward);
        ScriptNode(g_pBSObject)->GetRightward(right);
        result.f = MathFunAtan2F(0.0f * right.z + (dx * right.x + dy * right.y),
                                 0.0f * forward.z + (dx * forward.x + dy * forward.y));
        break;
    }
    case 3: {  // angle from the AI's facing to its stored point
        CAIObject* ai = SelfAI(self);
        float yaw;
        float pitch;
        ((CAIFilterRealEulerDirection*)&ai->field_0x60)
            ->CalculateAngleDiff(ai->m_position, ai->m_coverPosition, &yaw, &pitch);
        result.f = yaw;
        break;
    }
    case 4:
        ((ISceneNode*)self)->GetAIDoodad();
        result.i = 0;
        break;
    case 5: {  // angle from the object's facing to the waypoint's facing
        CMatrix m;
        ((CQuaternion*)waypoint->m_orientation)->GetMatrix(m);
        CVector3 dir = *(CVector3*)m.m[1];
        CVector3 forward;
        CVector3 right;
        ScriptNode(g_pBSObject)->GetForward(forward);
        ScriptNode(g_pBSObject)->GetRightward(right);
        dir.z = 0.0f;
        result.f = MathFunAtan2F(0.0f * right.z + (dir.x * right.x + dir.y * right.y),
                                 0.0f * forward.z + (dir.x * forward.x + dir.y * forward.y));
        break;
    }
    case 6:
        if ((waypoint->field_0x70 >> 2) & 1) {
            result.i = 2;
        } else {
            result.i = 1;
        }
        break;
    case 7:
        result.i = IsMGUsed((ADDR_TO_PTR(BSObject, arg))->m_trigger);
        break;
    case 8:
        if (waypoint != NULL) {
            result.i = waypoint->field_0x3A;
        } else {
            result.f = 0.0f;
        }
        break;
    case 9: {  // angle from a cover's back direction to the object, clamped to +-90 degrees
        CMatrix m;
        ((CQuaternion*)waypoint->m_orientation)->GetMatrix(m);
        float wy = waypoint->m_position[1];
        float wx = waypoint->m_position[0];
        CVector3 pos;
        ScriptNode(g_pBSObject)->GetPosition(pos);
        CVector3 back = *(CVector3*)m.m[1];
        float bx = -back.x;
        float by = -back.y;
        pos.z = 0.0f;
        pos.x -= wx;
        pos.y -= wy;
        float coverAngle = MathFunAtan2F(bx, by);
        float targetAngle = MathFunAtan2F(pos.x, pos.y);
        float diff = MathFunNormalizeAngleNegativePiToPi(targetAngle - coverAngle);
        if (diff > 1.5707964f) {
            diff = 1.5707964f;
        }
        if (diff < -1.5707964f) {
            diff = -1.5707964f;
        }
        printf("******WAYPOINTINFO_TARGET_ANGLE_TO_COVER *****\n");
        printf("CoverInvAngle: %f, CoverToTargAngle: %f, AngleDiff: %f\n", coverAngle, targetAngle, diff);
        result.f = diff;
        break;
    }
    case 10: {  // cover peek type from the waypoint flags
        u8 flags = waypoint->field_0x70;
        if (((flags >> 2) & 1) || ((flags >> 3) & 1)) {
            result.i = 4;
        } else if ((flags & 1) || ((flags >> 4) & 1) || ((flags >> 6) & 1)) {
            result.i = 1;
        } else if (((flags >> 1) & 1) || ((flags >> 5) & 1) || ((flags >> 7) & 1)) {
            result.i = 2;
        } else {
            result.f = 0.0f;
        }
        break;
    }
    }
    BIFPop(sp);
    BIFReturn(sp, result.i);
}

// Script queries about sounds: the source (`source`: 0 the player weapon, 1
// the trigger, 2 a soldier's weapon, 5 the object) and what to return
// (`info`: 0 azimuth from the player, 1 distance from the player in 1/256
// units, 2 the weapon's sound type, 3 the object's speed as a pitch).
void BIFunc_GetSoundInfo(int** sp, void*) {
    CWeapon* weapon = NULL;
    int result = 0;
    int source = BIFArg(sp, 1);
    int info = BIFArg(sp, 0);
    CVector3 pos;
    switch (source) {
    case 0: {
        CPlayerObject* player = (CPlayerObject*)g_pBSObject->m_gameObject->GetSceneNode();
        player->GetPosition(pos);
        weapon = player->GetCurrentWeapon();
        break;
    }
    case 2: {
        CSoldierObject* soldier = (CSoldierObject*)g_pBSObject->m_gameObject->GetSceneNode();
        soldier->GetPosition(pos);
        weapon = soldier->m_pWeapons[soldier->m_curWeapon];
        break;
    }
    case 5:
        g_pBSObject->m_gameObject->GetSceneNode()->GetPosition(pos);
        break;
    case 1: {
        TriggerObjectData_struct* data = g_pBSObject->m_trigger->m_data;
        pos.x = data->m_position[0];
        pos.y = data->m_position[1];
        pos.z = data->m_position[2];
        break;
    }
    }
    switch (info) {
    case 0:
        result = GetAzimuth(pos, g_scene.GetPlayer(0));
        break;
    case 1: {
        CPlayerObject* player = g_scene.GetPlayer(0);
        CVector3 playerPos;
        if (player != NULL) {
            player->GetPosition(playerPos);
        } else {
            playerPos.z = 0.0f;
            playerPos.y = 0.0f;
            playerPos.x = 0.0f;
        }
        pos.x -= playerPos.x;
        pos.y -= playerPos.y;
        pos.z -= playerPos.z;
        result = 256.0f * Length3(pos.x, pos.y, pos.z);
        break;
    }
    case 2:
        result = weapon->GetWeaponSoundType();
        break;
    case 3: {
        CStaticObject* obj = g_pBSObject->m_gameObject->GetSceneNode()->AsStaticObject();
        bool animated;
        if (g_Shell.Get_currentMission() == 4 && g_Shell.Get_currentStage() == 2 && !g_bInMultiplayerMode) {
            u32 type = obj->m_pathState;
            animated = false;
            if (type != 0 && type != 1) {
                animated = true;
            }
            if (animated) {
                result = (0.16f + obj->m_speed) * (220.0f / (0.16f + obj->m_maxSpeed));
            } else {
                f32* vel = &obj->m_velocity.x;
                result = Length3(vel[0], vel[1], vel[2]) * (220.0f * obj->m_soundPitchScale);
            }
        } else {
            u32 type = obj->m_pathState;
            animated = false;
            if (type != 0 && type != 1) {
                animated = true;
            }
            if (animated) {
                result = obj->m_speed * (220.0f / obj->m_maxSpeed);
            } else {
                f32* vel = &obj->m_velocity.x;
                result = Length3(vel[0], vel[1], vel[2]) * (220.0f * obj->m_soundPitchScale);
            }
        }
        break;
    }
    }
    BIFPop(sp);
    BIFReturn(sp, result);
}

// The weapon type of the running player (0) or soldier (2) weapon `index`
// (< 0 the current one), -1 for none; 2 for anything else.
void BIFunc_GetWeaponType(int** sp, void*) {
    int result;
    int owner = BIFArg(sp, 0);
    int index = BIFArg(sp, 1);
    switch (owner) {
    case 0: {
        CPlayerObject* player = (CPlayerObject*)g_pBSObject->m_gameObject->GetSceneNode();
        if (index < 0) {
            CWeapon* weapon = player->GetCurrentWeapon();
            if (weapon != NULL) {
                result = weapon->m_weaponType;
            } else {
                result = -1;
            }
        } else {
            CWeapon* weapon = player->GetWeapon(index)->m_pWeapon;
            if (weapon != NULL) {
                result = weapon->m_weaponType;
            } else {
                result = -1;
            }
        }
        break;
    }
    case 2: {
        CSoldierObject* soldier = (CSoldierObject*)g_pBSObject->m_gameObject->GetSceneNode();
        CWeapon* weapon;
        if (index < 0) {
            weapon = soldier->m_pWeapons[soldier->m_curWeapon];
        } else {
            weapon = soldier->m_pWeapons[index];
        }
        if (weapon != NULL) {
            result = weapon->m_weaponType;
        } else {
            result = -1;
        }
        break;
    }
    default:
    case 1:
        result = 2;
        break;
    }
    BIFPop(sp);
    BIFReturn(sp, result);
}

// A soldier's (owner 2) weapon: 0 clip ammo, 1 max ammo, 2 current index.
void BIFunc_GetWeaponInfo(int** sp, void*) {
    int result = -1;
    int info = BIFArg(sp, 1);
    if (BIFArg(sp, 0) != 2) {
    } else {
        CSoldierObject* soldier = (CSoldierObject*)g_pBSObject->m_gameObject->GetSceneNode();
        if (info == 2) {
            result = soldier->m_curWeapon;
        } else {
            CWeapon* weapon = soldier->m_pWeapons[soldier->m_curWeapon];
            s16 value;
            switch (info) {
            case 0:
                value = weapon->m_clipAmmo;
                break;
            case 1:
                value = weapon->m_pProperties->m_maxAmmo;
                break;
            default:
                value = -1;
                break;
            }
            result = value;
        }
    }
    BIFPop(sp);
    BIFReturn(sp, result);
}

// Stores the index of the soldier's grenade slot (weapon type 0x21, 0x22,
// 0x26 or 0x28) in its script data, or -1.
void BIFunc_SetGrenadeSlot(int** sp, void* self) {
    int slot = -1;
    for (int i = 0; i < 9; i++) {
        CSoldierObject* soldier = (CSoldierObject*)g_pBSObject->m_gameObject->GetSceneNode();
        CWeapon* weapon;
        if (i < 0) {
            weapon = soldier->m_pWeapons[soldier->m_curWeapon];
        } else {
            // Reads one past the 8 slots, as the original does.
            weapon = ((CWeapon**)soldier->m_pWeapons)[i];
        }
        u32 type;
        if (weapon != NULL && ((type = weapon->m_weaponType) - 0x21 <= 1 || type == 0x26 || type == 0x28)) {
            slot = i;
            break;
        }
    }
    ((int*)((ISceneNode*)self)->GetScriptObject()->m_gameObject->GetScriptData())[0x80 / 4] = slot;
    BIFPop(sp);
}

void BIFunc_CatchGrenade(int** sp, void*) {
    (ADDR_TO_PTR(CThrownBullet, BIFArg(sp, 0)))->SomebodyCaughtYou();
    BIFPop(sp);
}

// Writes the grenade's fuse time to the old stack top before popping (as
// the original does).
void BIFunc_GetGrenadeTimer(int** sp, void*) {
    int* top = *sp;
    int timer = (ADDR_TO_PTR(CThrownBullet, BIFArg(sp, 0)))->field_0x90;
    *top = timer;
    BIFPop(sp);
}

void BIFunc_SetGrenadeTimer(int** sp, void*) {
    (ADDR_TO_PTR(CThrownBullet, BIFArg(sp, 0)))->field_0x90 = BIFArg(sp, 1);
    BIFPop(sp);
}

// A player weapon slot's ammo: `query` & 0x1FF is the slot; flag 0x200 the
// max ammo, 0x400 whether selectable, 0x800 the clip, 0x1000 a full clip,
// else clip + reserve. -1 for an empty slot.
void BIFunc_GetPlayerAmmo(int** sp, void* self) {
    int result = 0;
    int query = BIFArg(sp, 0);
    CPlayerObject* player;
    if (self != NULL && ((ISceneNode*)self)->AsPlayerObject() != NULL) {
        player = ((ISceneNode*)self)->AsPlayerObject();
    } else {
        player = g_scene.GetPlayer(0);
    }
    CPlayerWeaponObject* slot = player->GetWeapon(query & 0x1FF);
    if (slot != NULL) {
        if (query & 0x200) {
            result = slot->m_pWeapon->m_pProperties->m_maxAmmo;
        } else if (query & 0x400) {
            result = slot->IsSelectable() != 0;
        } else if (query & 0x800) {
            result = slot->m_pWeapon->m_clipAmmo;
        } else if (query & 0x1000) {
            CWeapon* weapon = slot->m_pWeapon;
            result = weapon->m_clipAmmo == weapon->m_pProperties->m_clipSize;
        } else {
            CWeapon* weapon = slot->m_pWeapon;
            result = weapon->m_reserveAmmo + weapon->m_clipAmmo;
        }
    } else {
        result = -1;
    }
    BIFPop(sp);
    BIFReturn(sp, result);
}

// Gives player 0 ammo for weapon slot `slot`; a newly picked up weapon
// (with `select`) becomes selectable and shows message `msg`. A player with
// no weapon switches to the next.
void BIFunc_AddPlayerAmmo(int** sp, void*) {
    int msg = BIFArg(sp, 3);
    int slotIndex = BIFArg(sp, 0);
    bool select = BIFArg(sp, 2) != 0;
    int amount = BIFArg(sp, 1);
    CPlayerObject* player = g_scene.GetPlayer(0);
    CPlayerWeaponObject* slot = player->GetWeapon(slotIndex);
    slot->m_pWeapon->AddAmo(amount);
    if (!slot->IsSelectable() && select) {
        slot->m_selectable = 1;
        slot->DoneReloading();
        if (msg >= 0) {
            appendMessageToPrompt(GetUMString(msg), 0);
            displayPromptBuffer(-1);
        }
    }
    if (player->GetCurrentWeapon() == NULL) {
        slot->m_hideFrames = 3;
        player->TriggerScriptEvent(0xA2, NULL, false);
        player->CycleWeapon(1, -1);
    }
    BIFPop(sp);
}

void BIFunc_PlayerSuperAimMode(int** sp, void*) {
    int on = BIFArg(sp, 0) != 0;
    CPlayerObject* player = g_scene.GetPlayer(0);
    player->m_superAim = on;
    BIFPop(sp);
}

void BIFunc_IsPlayerInvincible(int** sp, void*) {
    BIFPop(sp);
    BIFReturn(sp, g_cheats.m_invincible);
}

// Ejects a shell casing of `type` from the player's weapon.
void BIFunc_PlayerEjectShell(int** sp, void* self) {
    int type = BIFArg(sp, 0);
    CPlayerObject* player = ((ISceneNode*)self)->AsPlayerObject();
    CPlayerWeaponObject* weapon = player->GetCurrentPlayerWeapon();
    int crc;
    switch (type) {
    case 1:
        crc = 0x997035A9;
        break;
    case 2:
        crc = 0x37AB0F02;
        break;
    case 3:
        crc = 0x9AB689E6;
        break;
    case 4:
        crc = 0x4375A62A;
        break;
    case 5:
        crc = 0xB2134D62;
        break;
    default:
        crc = 0x997035A9;
        break;
    }
    if (weapon != NULL && !player->m_scopeView) {
        weapon->CreateEjectedShell(crc);
    }
    BIFPop(sp);
}

void BIFunc_DisablePlayerShoot(int** sp, void* self) {
    CPlayerObject* player = ((ISceneNode*)self)->AsPlayerObject();
    if (player->GetCurrentWeapon() != NULL) {
        player->GetCurrentWeapon()->Shoot((EWeaponShootType)1, 0.0f, -1.0f);
    }
    BIFPop(sp);
}

void BIFunc_SetWeaponDraw(int** sp, void* self) {
    int draw = BIFArg(sp, 0);
    CSoldierObject* soldier = ((ISceneNode*)self)->AsSoldierObject();
    CWeapon* weapon = soldier->m_pWeapons[soldier->m_curWeapon];
    if (weapon != NULL) {
        weapon->m_drawEnabled = draw != 0;
    }
    BIFPop(sp);
}

// Leaves the object's dump object (a body or debris placed by its trigger)
// at its position.
void BIFunc_CreateDumpObject(int** sp, void* self) {
#ifdef FRONTLINE_NATIVE
    // Rewritten: not again for an enemy that dropped its item as it died.
    if (NativeEnemyDropDone((ISceneNode*)self)) {
        BIFPop(sp);
        return;
    }
#endif
    TriggerObjectData_struct* data = ((ISceneNode*)self)->GetScriptObject()->m_trigger->m_data;
    int dumpType;
    int id;
    switch (data->m_type) {
    case 3:
        dumpType = data->field_0x71;
        id = data->field_0x90;
        break;
    case 8:
        dumpType = data->field_0x71;
        id = data->field_0x74;
        break;
    case 2:
        dumpType = data->field_0x71;
        id = data->field_0x74;
        break;
    default:
        dumpType = 0;
        id = 0;
        break;
    }
    if (id != 0 && id != -1) {
        CQuaternion q = *(CQuaternion*)data->m_orientation;
        CVector3 pos;
        ((ISceneNode*)self)->GetPosition(pos);
        CreateDumpObject(pos, q, id, dumpType);
    }
    BIFPop(sp);
}

// Randomly deflects unit direction `dir` within a square of side `spread`
// across it and renormalizes.
static void AdjustAim(CVector3& dir, float spread) {
    // side = dir x up(0, 0, 1)
    float zz = dir.z * 0.0f;
    float sy = -(dir.x * 1.0f - zz);
    float sx = dir.y * 1.0f - zz;
    float sz = dir.x * 0.0f - dir.y * 0.0f;
    float len = Length3(sx, sy, sz);
    if (len != 0.0f) {
        float inv = 1.0f / len;
        sx *= inv;
        sy *= inv;
        sz *= inv;
    }
    // up' = side x dir
    float ux = sy * dir.z - sz * dir.y;
    float uy = sz * dir.x - sx * dir.z;
    float uz = sx * dir.y - sy * dir.x;
    len = Length3(ux, uy, uz);
    if (len != 0.0f) {
        float inv = 1.0f / len;
        ux *= inv;
        uy *= inv;
        uz *= inv;
    }
    float half = 0.5f * spread;
    float a = MathFunRandomReal(-half, half);
    float b = MathFunRandomReal(-half, half);
    dir.x += sx * a;
    dir.y += sy * a;
    dir.z += sz * a;
    dir.x += ux * b;
    dir.y += uy * b;
    dir.z += uz * b;
    len = Length3(dir.x, dir.y, dir.z);
    if (len != 0.0f) {
        float inv = 1.0f / len;
        dir.x *= inv;
        dir.y *= inv;
        dir.z *= inv;
    }
}

// Fires weapon `crc`'s projectile from script object `from` (at its
// attachment `attach`, or its position, or its trigger) at `to` (leading a
// moving AI target), with `spread`.
void BIFunc_FireProjectile(int** sp, void* self) {
    float speed = 5.0f;
    int bulletType = 0;
    float spread = BIFArgF(sp, 1);
    bool projectile = true;
    BSObject* from = ADDR_TO_PTR(BSObject, BIFArg(sp, 2));
    BSObject* to = ADDR_TO_PTR(BSObject, BIFArg(sp, 3));
    int attach = BIFArg(sp, 4);
    int index = 0;
    WeaponProperties_struct* props =
        (WeaponProperties_struct*)g_pWeaponFactory->GetWeaponPropertiesByCRC(BIFArg(sp, 0), &index);
    if (props != NULL) {
        bulletType = props->m_bulletType;
        speed = props->m_speed;
        projectile = props->m_isProjectile != 0;
    }
    CVector3 start;
    if (from != NULL && from->m_gameObject != NULL && from->m_gameObject->GetSceneNode() != NULL) {
        if (attach != 0) {
            StaticMesh* model = from->m_gameObject->GetSceneNode()->AsStaticObject()->m_pMesh->m_pData;
            u8* point = (u8*)model->m_hardPoints;
            int count = model->m_numHardPoints;
            for (int i = 0; i < count; i++) {
                if (*(int*)(point + 0x40) == attach) {
                    break;
                }
                point += 0x50;
            }
            CMatrix tm;
            from->m_gameObject->GetSceneNode()->GetTMLocalToWorld(tm);
            tm.Multiply(*(CMatrix*)point, tm);
            start = *(CVector3*)tm.m[3];
        } else {
            from->m_gameObject->GetSceneNode()->GetPosition(start);
        }
    } else if (from != NULL) {
        TriggerObject_struct* trigger = from->m_trigger;
        if (trigger != NULL) {
            TriggerObjectData_struct* data = trigger->m_data;
            if (data != NULL) {
                start = CVector3(data->m_position[0], data->m_position[1], data->m_position[2]);
            }
        }
    }
    CVector3 lead;
    lead.x = 0.0f;
    lead.y = 0.0f;
    lead.z = 0.0f;
    CVector3 target;
    if (to != NULL && to->m_gameObject != NULL && to->m_gameObject->GetSceneNode() != NULL) {
        ISceneNode* node = to->m_gameObject->GetSceneNode();
        node->GetPosition(target);
        CAIObject* ai = node->GetAIDoodad()->m_pFilter->m_pAIObject;
        lead.x = ai->m_velocity.field_0x0;
        lead.y = ai->m_velocity.field_0x4;
        lead.z = ai->m_velocity.field_0x8;
    } else if (to != NULL) {
        TriggerObject_struct* trigger = to->m_trigger;
        if (trigger != NULL) {
            TriggerObjectData_struct* data = trigger->m_data;
            if (data != NULL) {
                target = CVector3(data->m_position[0], data->m_position[1], data->m_position[2]);
            }
        }
    }
    target.x -= start.x;
    target.y -= start.y;
    target.z -= start.z;
    float time = Length3(target.x, target.y, target.z) / speed;
    lead.x *= time;
    lead.y *= time;
    lead.z *= time;
    target.x += lead.x;
    target.y += lead.y;
    target.z += lead.z;
    AdjustAim(target, spread);
    g_pBulletFactory->CreateBullet(bulletType, projectile, start, target, speed, (EClsnId)4, 512.0f,
                                   (ISceneNode*)self);
    BIFPop(sp);
}

// Attaches `obj` (or self) to prop `prop` at attachment `point`.
void BIFunc_AttachToProp(int** sp, void* self) {
    BSObject* obj = ADDR_TO_PTR(BSObject, BIFArg(sp, 0));
    int point = BIFArg(sp, 1);
    BSObject* propObj = ADDR_TO_PTR(BSObject, BIFArg(sp, 3));
    bool flag = BIFArg(sp, 2) != 0;
    ISceneNode* node;
    if (obj != NULL) {
        node = obj->m_gameObject->GetSceneNode();
    } else {
        node = (ISceneNode*)self;
    }
    CAttachableObject* prop = (CAttachableObject*)propObj->m_gameObject->GetSceneNode();
    bool isAnim = node->AsAnimObject() != NULL;
    if (node->AsPlayerObject() != NULL) {
        prop->AttachObject(node->AsPlayerObject(), point, flag);
    } else if (isAnim) {
        prop->AttachObject(node->AsAnimObject(), point);
    } else {
        prop->AttachObject(node->AsStaticObject(), point);
    }
    BIFPop(sp);
}

// Moves `obj` (or self) to waypoint `waypoint` and holds it there.
void BIFunc_AttachToWaypoint(int** sp, void* self) {
    BSObject* obj = ADDR_TO_PTR(BSObject, BIFArg(sp, 0));
    BSObject* waypoint = ADDR_TO_PTR(BSObject, BIFArg(sp, 1));
    ISceneNode* node;
    if (obj != NULL) {
        node = obj->m_gameObject->GetSceneNode();
    } else {
        node = (ISceneNode*)self;
    }
    CAnimObject* anim = node->AsAnimObject();
    CMatrix m;
    if (waypoint != NULL) {
        TriggerObjectData_struct* data = waypoint->m_trigger->m_data;
        CVector3 pos(data->m_position[0], data->m_position[1], data->m_position[2]);
        ((CQuaternion*)data->m_orientation)->GetMatrix(m);
        m.SetPos(pos);
        if (anim->GetCollisionId() == 5) {
            anim->SetCollisionId((EClsnId)27);
        }
        anim->AttachToLocation(m);
    }
    BIFPop(sp);
}

void BIFunc_DetachFromWaypoint(int** sp, void* self) {
    BSObject* obj = ADDR_TO_PTR(BSObject, BIFArg(sp, 0));
    ISceneNode* node;
    if (obj != NULL) {
        node = obj->m_gameObject->GetSceneNode();
    } else {
        node = (ISceneNode*)self;
    }
    CAnimObject* anim = node->AsAnimObject();
    if (anim->GetCollisionId() == 27) {
        anim->SetCollisionId((EClsnId)5);
    }
    anim->SetAttached(false);
    BIFPop(sp);
}

void BIFunc_AttachPlayerToProp(int** sp, void*) {
    BSObject* propObj = ADDR_TO_PTR(BSObject, BIFArg(sp, 2));
    int point = BIFArg(sp, 0);
    bool flag = BIFArg(sp, 1) != 0;
    if (propObj == NULL) {
        propObj = g_pBSObject;
    }
    CAttachableObject* prop = (CAttachableObject*)propObj->m_gameObject->GetSceneNode();
    prop->AttachObject(g_scene.GetPlayer(0), point, flag);
    BIFPop(sp);
}

void BIFunc_DetachFromProp(int** sp, void* self) {
    BSObject* obj = ADDR_TO_PTR(BSObject, BIFArg(sp, 0));
    CAttachableObject* prop = (CAttachableObject*)(ADDR_TO_PTR(BSObject, BIFArg(sp, 1)))->m_gameObject->GetSceneNode();
    ISceneNode* node;
    if (obj != NULL) {
        node = obj->m_gameObject->GetSceneNode();
    } else {
        node = (ISceneNode*)self;
    }
    prop->DetachObject(node);
    BIFPop(sp);
}

void BIFunc_DetachPlayerFromProp(int** sp, void*) {
    BSObject* propObj = ADDR_TO_PTR(BSObject, BIFArg(sp, 0));
    if (propObj == NULL) {
        propObj = g_pBSObject;
    }
    CAttachableObject* prop = (CAttachableObject*)propObj->m_gameObject->GetSceneNode();
    prop->DetachObject(g_scene.GetPlayer(0));
    g_scene.GetPlayer(0)->m_ui.m_fadeIn = 1.0f;
    BIFPop(sp);
}

// Puts object `obj` in the soldier's left (0) or right (1) hand.
void BIFunc_AttachToSoldier(int** sp, void* self) {
    int hand = BIFArg(sp, 1);
    int arg = BIFArg(sp, 2);
    CStaticObject* obj = (CStaticObject*)(ADDR_TO_PTR(BSObject, BIFArg(sp, 0)))->m_gameObject->GetSceneNode();
    switch (hand) {
    case 0:
        ((CSoldierObject*)self)->AttachObject(obj, "lt_hand", arg);
        break;
    case 1:
        ((CSoldierObject*)self)->AttachObject(obj, "rt_hand", arg);
        break;
    }
    g_scene.Remove(*obj);
    BIFPop(sp);
}

// Puts prop `obj` in the soldier's free hand (right first); returns the
// hand (1 right, 0 left) or -1.
void BIFunc_AttachToSoldierEmptyHand(int** sp, void* self) {
    int result = -1;
    BSObject* obj = ADDR_TO_PTR(BSObject, BIFArg(sp, 0));
    int arg = BIFArg(sp, 1);
    CStaticObject* prop = (CStaticObject*)obj->m_gameObject->GetSceneNode();
    if (!g_scene.IsNodeInScene(prop)) {
        if (obj != NULL) {
            DebugMsg("Prop %d has already been attached or destroyed. Check with Designers for possible duplicate ID.\n",
                     obj->m_trigger->m_data->field_0x38);
        }
    } else if (!((CSoldierObject*)self)->AttachObject(prop, "rt_hand", arg)) {
        if (((CSoldierObject*)self)->AttachObject(prop, "lt_hand", arg)) {
            result = 0;
            g_scene.Remove(*prop);
        }
    } else {
        result = 1;
        g_scene.Remove(*prop);
    }
    BIFPop(sp);
    BIFReturn(sp, result);
}

// Takes the object out of the soldier's left (0) or right (1) hand: kept in
// the scene with `keep` (a weapon is marked dropped), else destroyed.
void BIFunc_DetachFromSoldier(int** sp, void* self) {
    CStaticObject* obj = NULL;
    int hand = BIFArg(sp, 0);
    bool keep = BIFArg(sp, 1) != 0;
    switch (hand) {
    case 0:
        obj = ((ISceneNode*)self)->AsSoldierObject()->DetachObject("lt_hand");
        break;
    case 1:
        obj = ((ISceneNode*)self)->AsSoldierObject()->DetachObject("rt_hand");
        break;
    }
    if (obj != NULL && keep) {
        g_scene.Add(*obj);
        CWeapon* weapon = obj->AsWeaponObject();
        if (weapon != NULL) {
            weapon->m_bDropped = true;
        }
    } else if (obj != NULL && obj->GetScriptObject() != NULL) {
        BSGO_Basic* go = obj->GetScriptObject()->m_gameObject;
        if (go != NULL) {
            DelayDestroyObject(go);
        }
    }
    BIFPop(sp);
}

// The nearest box or tank prop (other than `near`) with prop id `id`.
void BIFunc_GetPropById(int** sp, void*) {
    BSObject* near = ADDR_TO_PTR(BSObject, BIFArg(sp, 1));
    int id = BIFArg(sp, 0);
    if (near == NULL) {
        near = g_pBSObject;
    }
    CVector3 nearPos;
    BSGO_Basic* nearGo = near->m_gameObject;
    if (nearGo != NULL) {
        nearGo->GetSceneNode()->GetPosition(nearPos);
    } else {
        TriggerObjectData_struct* data = near->m_trigger->m_data;
        nearPos.x = data->m_position[0];
        nearPos.y = data->m_position[1];
        nearPos.z = data->m_position[2];
    }
    BSObject* best = NULL;
    float bestDist = 1000.0f;
    BSGO_Basic* go;
    for (go = g_pBoxList; go != NULL; go = go->m_pNext) {
        if (go->GetSceneNode() != NULL && go->GetSceneNode()->GetScriptObject() != NULL &&
            id == go->GetSceneNode()->GetScriptObject()->m_trigger->m_data->field_0x38) {
            ISceneNode* node = go->GetSceneNode();
            BSObject* script = go->GetSceneNode()->GetScriptObject();
            if (script != near) {
                CVector3 d;
                node->GetPosition(d);
                d.x -= nearPos.x;
                d.y -= nearPos.y;
                d.z -= nearPos.z;
                if (Length3(d.x, d.y, d.z) < bestDist) {
                    best = script;
                    bestDist = Length3(d.x, d.y, d.z);
                }
            }
        }
    }
    if (best == NULL) {
        for (go = g_pTankList; go != NULL; go = go->m_pNext) {
            if (go->GetSceneNode() != NULL && go->GetSceneNode()->GetScriptObject() != NULL &&
                id == go->GetSceneNode()->GetScriptObject()->m_trigger->m_data->field_0x38) {
                ISceneNode* node = go->GetSceneNode();
                BSObject* script = go->GetSceneNode()->GetScriptObject();
                if (script != near) {
                    CVector3 d;
                    node->GetPosition(d);
                    d.x -= nearPos.x;
                    d.y -= nearPos.y;
                    d.z -= nearPos.z;
                    if (Length3(d.x, d.y, d.z) < bestDist) {
                        best = script;
                        bestDist = Length3(d.x, d.y, d.z);
                    }
                }
            }
        }
    }
    BIFPop(sp);
    BIFReturn(sp, (int)PTR_TO_ADDR(best));
}

// Mood presets: an expression (some random), a jaw motion and blinking.
void CSoldierObject::SetMood(EMoodType mood) {
    if ((u32)mood <= 6) {
        switch (mood) {
        case 6:
            SetExpression(8, 1.0f);
            GetJaw()->SetMotionType((EJawType)7);
            SetBlink(1.0f, 2.0f, 0.017f, 0.017f, 0.017f);
            m_blinkTimer = 0.0f;
            return;
        case 0:
            SetExpression(4, 1.0f);
            if (rand() % 100 < 50) {
                GetJaw()->SetMotionType((EJawType)5);
            } else {
                GetJaw()->SetMotionType((EJawType)6);
            }
            SetBlink(2.0f, 4.0f, 0.1f, 0.1f, 0.1f);
            m_blinkTimer = 0.0f;
            return;
        case 1:
            if (rand() % 100 < 50) {
                SetExpression(3, 1.0f);
                if (rand() % 100 < 50) {
                    GetJaw()->SetMotionType((EJawType)5);
                } else {
                    GetJaw()->SetMotionType((EJawType)6);
                }
            } else {
                SetExpression(1, 1.0f);
                if (rand() % 100 < 30) {
                    GetJaw()->SetMotionType((EJawType)6);
                } else {
                    GetJaw()->SetMotionType((EJawType)5);
                }
            }
            SetBlink(1.0f, 2.0f, 0.017f, 0.017f, 0.017f);
            m_blinkTimer = 0.0f;
            return;
        case 2: {
            SetExpression(m_defaultExpression, 1.0f);
            int r = rand() % 100;
            if (r < 20) {
                GetJaw()->SetMotionType((EJawType)3);
            } else if (r < 60) {
                GetJaw()->SetMotionType((EJawType)2);
            } else {
                GetJaw()->SetMotionType((EJawType)6);
            }
            SetBlink(0.5f, 2.0f, 0.017f, 0.017f, 0.017f);
            m_blinkTimer = 0.0f;
            return;
        }
        case 3:
            if (rand() % 100 < 50) {
                SetExpression(11, 1.0f);
            } else {
                SetExpression(8, 0.7f);
            }
            GetJaw()->SetMotionType((EJawType)1);
            SetBlink(0.0f, 0.0f, 0.0f, 0.0f, 86400.0f);
            m_blinkTimer = 0.0f;
            return;
        case 4:
            SetExpression(8, 0.7f);
            SetBlink(0.0f, 0.0f, 86400.0f, 0.0f, 0.5f);
            m_blinkTimer = 0.0f;
            GetJaw()->SetMotionType((EJawType)0);
            return;
        case 5: {
            int r = rand() % 100;
            if (r < 20) {
                SetExpression(8, 0.7f);
                SetBlink(0.0f, 0.0f, 0.0f, 0.0f, 86400.0f);
                m_blinkTimer = 0.0f;
                GetJaw()->SetMotionType((EJawType)4);
                return;
            }
            if (r < 60) {
                SetExpression(8, 0.7f);
                SetBlink(0.0f, 0.0f, 86400.0f, 0.0f, 0.5f);
                m_blinkTimer = 0.0f;
                GetJaw()->SetMotionType((EJawType)0);
                return;
            }
            SetExpression(11, 0.8f);
            SetBlink(0.0f, 0.0f, 0.0f, 0.0f, 86400.0f);
            m_blinkTimer = 0.0f;
            GetJaw()->SetMotionType((EJawType)1);
            return;
        }
        }
    } else {
        SetExpression(0, 1.0f);
        GetJaw()->SetMotionType((EJawType)0);
        SetBlink(2.0f, 4.0f, 0.1f, 0.1f, 0.1f);
        m_blinkTimer = 0.0f;
    }
}

void BIFunc_SetMood(int** sp, void* self) {
    int mood = BIFArg(sp, 0);
    ((ISceneNode*)self)->AsSoldierObject()->SetMood((EMoodType)mood);
    BIFPop(sp);
}

void BIFunc_SetExpression(int** sp, void* self) {
    int expr = BIFArg(sp, 0);
    float weight = BIFArgF(sp, 1);
    ((ISceneNode*)self)->AsSoldierObject()->SetExpression(expr, weight);
    BIFPop(sp);
    BIFReturn(sp, -1);
}

void BIFunc_SetBlinkState(int** sp, void* self) {
    int state = BIFArg(sp, 0);
    CSoldierObject* soldier = ((ISceneNode*)self)->AsSoldierObject();
    switch (state) {
    case 0:
        soldier->SetBlink(0.0f, 0.0f, 0.0f, 0.0f, 86400.0f);
        break;
    case 1:
        soldier->SetBlink(2.0f, 4.0f, 0.1f, 0.1f, 0.1f);
        break;
    case 6:
        soldier->SetBlink(2.0f, 4.0f, 1.5f, 0.5f, 0.5f);
        break;
    case 2:
    case 3:
        soldier->SetBlink(1.0f, 2.0f, 0.017f, 0.017f, 0.017f);
        break;
    case 4:
        soldier->SetBlink(0.5f, 2.0f, 0.017f, 0.017f, 0.017f);
        break;
    case 5:
        soldier->SetBlink(2.0f, 6.0f, 0.017f, 0.017f, 0.017f);
        break;
    case 8:
        soldier->SetBlink(0.0f, 0.0f, 0.0f, 0.0f, 86400.0f);
        break;
    case 7:
        soldier->SetBlink(0.0f, 0.0f, 86400.0f, 0.0f, 0.5f);
        break;
    default:
        soldier->SetBlink(2.0f, 5.0f, 0.017f, 0.017f, 0.017f);
        break;
    }
    soldier->m_blinkTimer = 0.0f;
    BIFPop(sp);
}

void BIFunc_SetJawMotion(int** sp, void* self) {
    int type = BIFArg(sp, 0);
    ((ISceneNode*)self)->AsSoldierObject()->GetJaw()->SetMotionType((EJawType)type);
    BIFPop(sp);
}

// Sets the sound-effects volume (0..1 of the shell's volume setting).
void BIFunc_AEMSSetMasterVolume(int** sp, void*) {
    AEMS_SetVolume((double)(BIFArgF(sp, 0) * (float)(int)(g_Shell.field_0x28 - 1)) / 6.0);
    BIFPop(sp);
}

void BIFunc_AmbientTrack_Select(int** sp, void*) {
    AmbientTrack_Select(BIFArg(sp, 0));
    BIFPop(sp);
}

void BIFunc_AmbientTrack_Volume(int** sp, void*) {
    AmbientTrack_Volume(BIFArg(sp, 0) / 1000.0f);
    BIFPop(sp);
}

void BIFunc_SetMuzzleFireForPlayer(int** sp, void*) {
    SetMuzzleStatusForPlayer(BIFArg(sp, 0) != 0);
    BIFPop(sp);
}

// Places the ambient track at the trigger (or nowhere).
void BIFunc_AmbientTrack_Azimuth(int** sp, void*) {
    if (BIFArg(sp, 0) != 0) {
        TriggerObjectData_struct* data = g_pBSObject->m_trigger->m_data;
        CVector3 pos;
        pos.x = data->m_position[0];
        pos.y = data->m_position[1];
        pos.z = data->m_position[2];
        AmbientTrack_SetLocation(&pos);
    } else {
        AmbientTrack_SetLocation(NULL);
    }
    BIFPop(sp);
}

// The running object's position, or its trigger's.
static inline void ObjectOrTriggerPosition(void* self, CVector3& pos) {
    if (self != NULL) {
        ((ISceneNode*)self)->GetPosition(pos);
    } else {
        TriggerObjectData_struct* data = g_pBSObject->m_trigger->m_data;
        pos.x = data->m_position[0];
        pos.y = data->m_position[1];
        pos.z = data->m_position[2];
    }
}

void BIFunc_CreateExplosion(int** sp, void* self) {
    float radius = BIFArgF(sp, 0);
    float loudness = BIFArgF(sp, 1);
    float dx = BIFArgF(sp, 2);
    float dy = BIFArgF(sp, 3);
    float dz = BIFArgF(sp, 4);
    float damage = BIFArgF(sp, 5);
    CVector3 pos;
    ObjectOrTriggerPosition(self, pos);
    pos.x += dx;
    pos.y += dy;
    pos.z += dz;
    CExplosion explosion(g_scene, pos, radius, loudness, damage, (EScriptBulletType)0x15, NULL, true);
    BIFPop(sp);
}

// Damages the nearest object within `radius` of the (offset) position.
void BIFunc_CreateImpact(int** sp, void* self) {
    float radius = BIFArgF(sp, 0);
    float damage = BIFArgF(sp, 1);
    float dx = BIFArgF(sp, 2);
    float dy = BIFArgF(sp, 3);
    float dz = BIFArgF(sp, 4);
    float a = BIFArgF(sp, 5);
    float b = BIFArgF(sp, 6);
    CVector3 pos;
    ObjectOrTriggerPosition(self, pos);
    pos.x += dx;
    pos.y += dy;
    pos.z += dz;
    CImpact impact(pos, radius, damage, a, b, (EScriptBulletType)0x15, (ISceneNode*)self);
    g_scene.VisitAllNodesInVolume(impact, impact.m_volume, (ISceneNode::EVolumeType)0, true);
    if (impact.m_pClosest != NULL) {
        impact.DamageClosestObject();
    }
    BIFPop(sp);
}

// Switches the soldier to weapon slot `index`, holding it in the hand its
// properties (plus `handOffset`) pick.
static inline void SwitchSoldierWeapon(CSoldierObject* soldier, int index, int handOffset) {
    CWeapon* weapon = soldier->m_pWeapons[soldier->m_curWeapon];
    if (weapon != NULL) {
        weapon->Update(1.0f, false);
        soldier->m_curWeapon = index;
        char* hand;
        int handCrc;
        if (!((handOffset + soldier->m_pWeapons[soldier->m_curWeapon]->m_pProperties->m_hand) & 1)) {
            hand = "lt_hand";
            handCrc = 0x4C83C769;
        } else {
            hand = "rt_hand";
            handCrc = 0xD58A96D3;
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
        soldier->m_curWeapon = index;
    }
}

// Gives the soldier the grenade the script picked up (`grenade`'s bullet
// type 24 is the other grenade model) in slot 3 and switches to it.
void BIFunc_GiveEnemyGrenadeWeapon(int** sp, void* self) {
    CBullet* grenade = ADDR_TO_PTR(CBullet, BIFArg(sp, 0));
    int crc;
    switch (grenade->GetScriptBulletType()) {
    case 3:
        crc = 0xA55DDA2D;
        break;
    case 24:
        crc = 0x83674487;
        break;
    default:
        crc = 0xA55DDA2D;
        break;
    }
    CSoldierObject* soldier = (CSoldierObject*)self;
    soldier->AddWeapon(3, crc);
    CWeapon* weapon = soldier->m_pWeapons[soldier->m_curWeapon];
    // (Reads the current weapon's hand before checking it exists, as the
    // original does.)
    int curHand = weapon->m_pProperties->m_hand;
    SwitchSoldierWeapon(soldier, 3, curHand == 0 ? 0 : 1);
    BIFPop(sp);
}

// Fires weapon `crc`'s projectile from `from` (as BIFunc_FireProjectile) at
// the point `t` (0..1) along spline path `path`.
void BIFunc_FireProjectileAtPath(int** sp, void* self) {
    float speed = 5.0f;
    int bulletType = 0;
    float spread = BIFArgF(sp, 1);
    BSObject* from = ADDR_TO_PTR(BSObject, BIFArg(sp, 2));
    bool projectile = true;
    u32 pathId = BIFArg(sp, 3);
    float t = BIFArgF(sp, 4);
    int attach = BIFArg(sp, 5);
    int index = 0;
    WeaponProperties_struct* props =
        (WeaponProperties_struct*)g_pWeaponFactory->GetWeaponPropertiesByCRC(BIFArg(sp, 0), &index);
    if (props != NULL) {
        bulletType = props->m_bulletType;
        speed = props->m_speed;
        projectile = props->m_isProjectile != 0;
    }
    CVector3 start;
    if (from != NULL && from->m_gameObject != NULL && from->m_gameObject->GetSceneNode() != NULL) {
        if (attach != 0) {
            StaticMesh* model = from->m_gameObject->GetSceneNode()->AsStaticObject()->m_pMesh->m_pData;
            u8* point = (u8*)model->m_hardPoints;
            int count = model->m_numHardPoints;
            for (int i = 0; i < count; i++) {
                if (*(int*)(point + 0x40) == attach) {
                    break;
                }
                point += 0x50;
            }
            CMatrix tm;
            from->m_gameObject->GetSceneNode()->GetTMLocalToWorld(tm);
            tm.Multiply(*(CMatrix*)point, tm);
            start = *(CVector3*)tm.m[3];
        } else {
            from->m_gameObject->GetSceneNode()->GetPosition(start);
        }
    } else if (from != NULL) {
        TriggerObject_struct* trigger = from->m_trigger;
        if (trigger != NULL) {
            TriggerObjectData_struct* data = trigger->m_data;
            if (data != NULL) {
                start = CVector3(data->m_position[0], data->m_position[1], data->m_position[2]);
            }
        }
    }
    CAISplinePath* path = FindSplinePath(pathId);
    float s = t * (float)path->m_lastSegment;
    int segment = s;
    CVector3 target;
    path->m_segments[segment].Expand(s - segment, target);
    target.x -= start.x;
    target.y -= start.y;
    target.z -= start.z;
    AdjustAim(target, spread);
    g_pBulletFactory->CreateBullet(bulletType, projectile, start, target, speed, (EClsnId)4, 1.0f,
                                   (ISceneNode*)self);
    BIFPop(sp);
}

// The pickup prompt (user message) for a multiplayer weapon, or -1.
int GetMPWeaponPrompt(EScriptBulletType type) {
    if ((u32)type <= 25) {
        switch (type) {
        case 7:
            return 0x197;
        case 0:
            return 0x198;
        case 4:
            return 0x199;
        case 8:
            return 0x19A;
        case 9:
            return 0x19B;
        case 10:
            return 0x19C;
        case 11:
            return 0x19F;
        case 12:
            return 0x1A0;
        case 5:
            return 0x1A1;
        case 13:
            return 0x1A2;
        case 14:
            return 0x1A3;
        case 15:
            return 0x1A4;
        case 16:
            return 0x1A7;
        case 17:
            return 0x1A8;
        case 6:
            return 0x1A9;
        case 18:
            return 0x1AA;
        case 19:
            return 0x1AD;
        case 20:
            return 0x1B0;
        case 2:
            return 0x1B1;
        case 1:
            return 0x1B3;
        case 21:
            return 0x1B5;
        case 22:
            return 0x1B6;
        case 23:
            return 0x1B7;
        case 3:
            return 0x1B8;
        case 24:
            return 0x1B9;
        case 25:
            return 0x2FF;
        }
    }
    return -1;
}

// Appends `text` to the prompt and shows it to player `player`'s HUD.
static inline void PromptPlayer(const char* text, int player) {
    appendMessageToPrompt(text, 0);
    displayPromptBuffer(player);
}

// Multiplayer kill: records it and tells the killer and the victim (team
// kill, suicide or kill messages, with the weapon when known).
void BIFunc_UpdateFragCount(int** sp, void*) {
    char text[128];
    BSObject* killerObj = ADDR_TO_PTR(BSObject, BIFArg(sp, 0));
    int bulletType = BIFArg(sp, 1);
    if (g_bInMultiplayerMode) {
        if (killerObj == NULL) {
            killerObj = g_pBSObject;
        }
        CPlayerObject* victim = g_pBSObject->m_gameObject->GetSceneNode()->AsPlayerObject();
        CPlayerObject* killer = killerObj->m_gameObject->GetSceneNode()->AsPlayerObject();
        g_Shell.Add_MPPlayerKilled(killer->m_playerIndex, victim->m_playerIndex);
        int weaponPrompt = GetMPWeaponPrompt((EScriptBulletType)bulletType);
        if (killer->m_team == victim->m_team) {
            if (killer != victim) {
                sprintf(text, "%s%s", GetUMString(0x2FC), victim->GetPlayerName());
                PromptPlayer(text, killer->field_0x768);
                if (weaponPrompt >= 0) {
                    sprintf(text, "%s%s's %s", GetUMString(0x2FE), killer->GetPlayerName(),
                            GetUMString(weaponPrompt));
                } else {
                    sprintf(text, "%s%s", GetUMString(0x2FE), killer->GetPlayerName());
                }
                PromptPlayer(text, victim->field_0x768);
            } else {
                PromptPlayer(GetUMString(0x300), killer->field_0x768);
            }
        } else {
            sprintf(text, "%s%s", GetUMString(0x2FB), victim->GetPlayerName());
            PromptPlayer(text, killer->field_0x768);
            if (weaponPrompt >= 0) {
                sprintf(text, "%s%s's %s", GetUMString(0x2FD), killer->GetPlayerName(), GetUMString(weaponPrompt));
            } else {
                sprintf(text, "%s%s", GetUMString(0x2FD), killer->GetPlayerName());
            }
            PromptPlayer(text, victim->field_0x768);
        }
    }
    BIFPop(sp);
}

void BIFunc_UpdatePlayerStats(int** sp, void*) {
    int stat = BIFArg(sp, 0);
    u32 value = BIFArg(sp, 1);
    switch (stat) {
    case 0:
        g_Shell.Set_shotsFired(value, false);
        if (g_bInMultiplayerMode) {
            g_Shell.Add_MPShotFired(g_pBSObject->m_gameObject->GetSceneNode()->AsPlayerObject()->m_playerIndex,
                                    value);
        }
        break;
    case 1:
        g_Shell.Set_hits(value, false);
        break;
    case 2:
        g_Shell.Set_hitsTaken(value, false);
        break;
    case 3:
        g_Shell.Set_enemiesKilled(value, false);
        break;
    case 4:
        g_Shell.Set_shotLocation(value, (EShotLocations)0, false);
        break;
    case 5:
        g_Shell.Set_shotLocation(value, (EShotLocations)1, false);
        break;
    case 6:
        g_Shell.Set_shotLocation(value, (EShotLocations)2, false);
        break;
    case 7:
        g_Shell.Set_shotLocation(value, (EShotLocations)3, false);
        break;
    case 8:
        g_Shell.Set_shotLocation(value, (EShotLocations)4, false);
        break;
    case 9:
        g_Shell.Set_shotLocation(value, (EShotLocations)5, false);
        break;
    case 10:
        g_Shell.Set_shotLocation(value, (EShotLocations)6, false);
        break;
    }
    BIFPop(sp);
}

// Two hints per objective, then the index of the one to show next.
const char* g_HintTextArray[20][3];

// A duplicate enemy (an enemy-team soldier spawner) adds to the spawned count.
void BIFunc_ModifyTotalKillableEnemyCount(int** sp, void*) {
    TriggerObjectData_struct* data = g_pBSObject->m_trigger->m_data;
    int count = BIFArg(sp, 0);
    if (data->m_type == 3 && data->m_team == 2) {
        DebugMsg("-------------- Duplicate Creation:  Ignoring for total enemy count\n");
        g_iNumberOfEnemiesSpawned += count;
    }
    BIFPop(sp);
}

void BIFunc_ChangePlayersTeam(int** sp, void* self) {
    int team = BIFArg(sp, 0);
    CAIFilterObject* filter = ((ISceneNode*)self)->AsPlayerObject()->GetAIDoodad()->m_pFilter;
    filter->m_pAIObject->m_status.m_vars[AISTATUS_TEAM] = team;
    filter->m_pAIObject->field_0x14 = team;
    BIFPop(sp);
}

void BIFunc_TogglePlayerVsTriggerChecks(int** sp, void* self) {
    int on = BIFArg(sp, 0);
    CPlayerObject* player = ((ISceneNode*)self)->AsPlayerObject();
    player->m_triggerChecks = on != 0;
    BIFPop(sp);
}

// Sets hint `slot` (1..3) of level `level` (1..20; 0x7FFFFFFF means 20) to
// user message `msg`.
void BIFunc_AddHint(int** sp, void*) {
    int level = BIFArg(sp, 0);
    int slot = BIFArg(sp, 1);
    int msg = BIFArg(sp, 2);
    if (level == 0x7FFFFFFF) {
        level = 20;
    }
    g_HintTextArray[level - 1][slot - 1] = GetUMString(msg);
    BIFPop(sp);
}

// Ends the level (`done`: completed rather than quit).
void BIFunc_EndLevel(int** sp, void*) {
    g_Shell.field_0xF = BIFArg(sp, 0) != 0;
    DestroyBuddyDataStructures();
    g_uiGameState |= GAMESTATE_LEVEL_DONE;
    BIFPop(sp);
}

void BIFunc_ScaleRel(int** sp, void* self) {
    float scale = BIFArgF(sp, 0);
    float time = BIFArgF(sp, 1);
    int event = BIFArg(sp, 2);
    ((ISceneNode*)self)->AsStaticObject()->ScaleFromScript(scale, time, (EBSEventEnum)event);
    BIFPop(sp);
}

// Puts the player on (or takes them off) the mounted gun of script object
// `gun`.
void BIFunc_AttachPlayerToMountedMachineGun(int** sp, void* self) {
    int on = BIFArg(sp, 1);
    ISceneNode* gun = FindMountedGun((ADDR_TO_PTR(BSObject, BIFArg(sp, 0)))->m_trigger);
    ((ISceneNode*)self)->AsPlayerObject()->SetUsingMountedMachineGun(on != 0, (CStaticObject*)gun);
    BIFPop(sp);
}

// The script object of scene node `node` (unless `flag`); written before
// popping, as the original does.
void BIFunc_GetBSObject(int** sp, void*) {
    int obj = 0;
    if (BIFArg(sp, 1) != 0) {
    } else {
        obj = (int)PTR_TO_ADDR((ADDR_TO_PTR(ISceneNode, BIFArg(sp, 0)))->GetScriptObject());
    }
    BIFReturn(sp, obj);
    BIFPop(sp);
}

// Sets or clears bits of the running mounted-gun waypoint's trigger word 0.
void BIFunc_SetMGWaypointFlag(int** sp, void*) {
    u32* flags = (u32*)g_pBSObject->m_trigger;
    u32 bits = BIFArg(sp, 0);
    if (BIFArg(sp, 1) != 0) {
        *flags |= bits;
    } else {
        *flags &= ~bits;
    }
    BIFPop(sp);
}

void BIFunc_PlayerForceToWeapon(int** sp, void* self) {
    int weapon = BIFArg(sp, 0);
    ((ISceneNode*)self)->AsPlayerObject()->CycleWeapon(1, weapon);
    BIFPop(sp);
}

// Pushes object `obj` along the running object's facing (mode 0: the player
// by default, mode 4: with a random lift).
void BIFunc_ApplyForceToObject(int** sp, void* self) {
    int mode = BIFTop(sp, 2);
    IMovingSceneNode* obj = ADDR_TO_PTR(IMovingSceneNode, BIFTop(sp, 1));
    float force = *(float*)&BIFTop(sp, 0);
    CVector3 dir;
    ((ISceneNode*)self)->GetForward(dir);
    switch (mode) {
    case 2:
        break;
    case 4: {
        float len = Length3(dir.x, dir.y, dir.z);
        if (len != 0.0f) {
            float inv = 1.0f / len;
            dir.x *= inv;
            dir.y *= inv;
            dir.z *= inv;
        }
        dir.z += 0.5f + (float)(rand() % 100) / 500.0f;
        obj->ApplyForceTo(dir, force);
        break;
    }
    case 0: {
        if (obj == NULL) {
            obj = g_scene.GetPlayer(0);
        }
        float len = Length3(dir.x, dir.y, dir.z);
        if (len != 0.0f) {
            float inv = 1.0f / len;
            dir.x *= inv;
            dir.y *= inv;
            dir.z *= inv;
        }
        dir.z += MathFunRandomReal(0.5f, 0.65f);
        obj->ApplyForceTo(dir, force);
        break;
    }
    }
    BIFPop(sp);
}

// Moves the mechanic along path `path` (speed in units per second; over
// 1000 keeps the current speed instead of starting from rest; rotation
// speeds in degrees per second).
void BIFunc_MechFollowPath(int** sp, void* self) {
    CStaticObject* mech = ((ISceneNode*)self)->AsStaticObject();
    int speed = BIFArg(sp, 3);
    bool keepSpeed;
    if (speed > 1000) {
        keepSpeed = true;
        speed -= 1000;
    } else {
        keepSpeed = false;
    }
    int accel = BIFArg(sp, 4) + 1;
    mech->SetMovementPath(BIFArg(sp, 0), BIFArg(sp, 1) != 0, BIFArg(sp, 2) != 0, 0.016683351f * speed,
                          0.001f * (accel * accel), 0.017453292f * (0.016683351f * BIFArgF(sp, 5)),
                          0.017453292f * (0.016683351f * BIFArgF(sp, 6)), ADDR_TO_PTR(BSObject, BIFArg(sp, 7)),
                          BIFArg(sp, 8), keepSpeed);
    BIFPop(sp);
}

void BIFunc_MechSetPathFlag(int** sp, void* self) {
    CStaticObject* mech = ((ISceneNode*)self)->AsStaticObject();
    int value = BIFArg(sp, 1);
    if (BIFArg(sp, 0) != 1) {
        mech->m_turnSlowdown = 0.01f * value;
    } else {
        mech->m_pathNoTurn = value != 0;
    }
    BIFPop(sp);
}

void BIFunc_MovingMechForceUpdateTrigger(int** sp, void*) {
    ForceUpdateProximityTriggerStatus(g_pBSObject->m_gameObject, false);
    BIFPop(sp);
}

void BIFunc_MechSetFlag(int** sp, void* self) {
    CStaticObject* mech = ((ISceneNode*)self)->AsStaticObject();
    int bit = BIFArg(sp, 0);
    if (BIFArg(sp, 1) != 0) {
        mech->m_flags |= 1 << bit;
    } else {
        mech->m_flags &= ~(1 << bit);
    }
    BIFPop(sp);
}

// Rumbles the running player's controller (single player: player 0's).
void BIFunc_PlayVibrationEvent(int** sp, void*) {
    int type = BIFArg(sp, 0);
    float a = BIFArgF(sp, 1);
    float b = BIFArgF(sp, 2);
    int count = BIFArg(sp, 3);
    u32 port;
    if (g_bInMultiplayerMode) {
        port = g_pBSObject->m_gameObject->GetSceneNode()->AsPlayerObject()->m_controllerPort;
    } else {
        port = g_scene.GetPlayer(0)->m_controllerPort;
    }
    g_inputMgr.SetFeedback(port, type, count, b, a);
    BIFPop(sp);
}

void BIFunc_BlockNewVibrations(int** sp, void*) {
    int player = 0;
    bool block = BIFArg(sp, 0) != 0;
    if (g_bInMultiplayerMode) {
        player = g_pBSObject->m_gameObject->GetSceneNode()->AsPlayerObject()->m_playerIndex;
    }
    g_inputMgr.m_feedbackBlocked[player] = block;
    BIFPop(sp);
}

void BIFunc_FadeOut(int** sp, void* self) {
    ((CSoldierObject*)self)->FadeOut(BIFArgF(sp, 0));
    BIFPop(sp);
}

// Puts mounted gun `arg0` in the running soldier's left (0) or right (1)
// hand.
void BIFunc_AttachObjectToFloatingBone(int** sp, void* self) {
    char* bone = NULL;
    int hand = BIFArg(sp, 1);
    int flags = BIFArg(sp, 2);
    ISceneNode* gun = FindMountedGun((ADDR_TO_PTR(BSObject, BIFArg(sp, 0)))->m_trigger);
    switch (hand) {
    case 0:
        bone = "lt_hand";
        break;
    case 1:
        bone = "rt_hand";
        break;
    }
    ((CSoldierObject*)self)->AttachObjectToFloatingBone((CStaticObject*)gun, bone, flags);
    BIFPop(sp);
}

void BIFunc_DetachObjectFromFloatingBone(int** sp, void* self) {
    ((CSoldierObject*)self)->DetachObjectFromFloatingBone();
    BIFPop(sp);
}

void BIFunc_EnableFloatingBone(int** sp, void* self) {
    ((CSoldierObject*)self)->AttachFloatingBone();
    BIFPop(sp);
}

void BIFunc_DisableFloatingBone(int** sp, void* self) {
    ((CSoldierObject*)self)->DetachFloatingBone();
    BIFPop(sp);
}

// Seconds until grenade `arg0` goes off.
void BIFunc_SetDetonationTime(int** sp, void*) {
    (ADDR_TO_PTR(CThrownBullet, BIFArg(sp, 0)))->SetDetonationTime(60.0f * BIFArgF(sp, 1));
    BIFPop(sp);
}

void BIFunc_SetSingleAnimEndsEventDelivery(int** sp, void* self) {
    ((CAnimObject*)self)->field_0x2452 = BIFArg(sp, 0) != 0;
    BIFPop(sp);
}

// Level settings fields: fog color and distances.
#define LS_FOG_R 0x12
#define LS_FOG_G 0x13
#define LS_FOG_B 0x14
#define LS_FOG_NEAR 0x15
#define LS_FOG_FAR 0x16

// Sets the fog distances and color (0..1); a negative color component
// keeps the level's fog color.
void BIFunc_SetFogParams(int** sp, void*) {
    float r = BIFArgF(sp, 2);
    float fogNear = BIFArgF(sp, 0);
    float fogFar = BIFArgF(sp, 1);
    float g = BIFArgF(sp, 3);
    float b = BIFArgF(sp, 4);
    CColor color(0, 0, 0, 0x80);
    if (r < 0.0f || g < 0.0f || b < 0.0f) {
        LevelFileContentsStruct_* settings = TLT_FindNextResourceByType((TLTResourceID)0xD, NULL);
        if (settings != NULL) {
            TLTResourceField* fields = settings->fields;
            color.r = fields[LS_FOG_R].value;
            color.g = fields[LS_FOG_G].value;
            color.b = fields[LS_FOG_B].value;
        } else {
            color.r = 0xFF;
            color.g = 0;
            color.b = 0xFF;
        }
    } else {
        color.r = 255.5f * r;
        color.g = 255.5f * g;
        color.b = 255.5f * b;
    }
    SetFog(color, fogNear, fogFar);
    BIFPop(sp);
}

// Restores the level's own fog.
void BIFunc_SetDefaultFogParams(int** sp, void*) {
    LevelFileContentsStruct_* settings = TLT_FindNextResourceByType((TLTResourceID)0xD, NULL);
    if (settings != NULL) {
        CColor color(0, 0, 0, 0x80);
        TLTResourceField* fields = settings->fields;
        color.r = fields[LS_FOG_R].value;
        color.g = fields[LS_FOG_G].value;
        color.b = fields[LS_FOG_B].value;
        SetFog(color, *(float*)&fields[LS_FOG_NEAR].value, *(float*)&fields[LS_FOG_FAR].value);
    }
    BIFPop(sp);
}

void BIFunc_StopCorpseSearch(int** sp, void* self) {
    StopCheck(sp, self, g_CorpseSearchSchedule);
}

// Copies the AI properties of the running object's trigger into its script
// data.
void BIFunc_SetAIObjectSharedDataPropVars(int** sp, void*) {
    BSAIScriptData* data = (BSAIScriptData*)g_pBSObject->m_gameObject->GetScriptData();
    TriggerObjectData_struct* trigger = g_pBSObject->m_trigger->m_data;
    data->field_0x34 = trigger->m_ai.field_0x77;
    data->field_0x38 = trigger->field_0x78;
    data->field_0x48 = trigger->field_0x7E >> 4;
    data->field_0x4C = trigger->m_ai.field_0x76;
    int a = trigger->field_0x80 >> 4;
    int b = trigger->field_0x88 >> 4;
    if (a > b) {
        data->field_0x50 = a;
    } else {
        data->field_0x50 = b;
    }
    data->field_0x54 = trigger->field_0x8A >> 4;
    data->field_0x58 = trigger->field_0x98;
    data->field_0x5C = trigger->field_0x84;
    data->field_0x60 = trigger->field_0x7A;
    if (data->field_0x50 <= 0.001f) {
        data->field_0x50 = 100.0f;
    }
    if (data->field_0x54 <= 0.001f) {
        data->field_0x54 = 50.0f;
    }
    BIFPop(sp);
}

void BIFunc_HierObjectSetLookObject(int** sp, void* self) {
    u32 look = BIFArg(sp, 0);
    ((ISceneNode*)self)->AsHierObject()->AsHierTankObject()->m_lookPart = look;
    BIFPop(sp);
}

// Maps weapon slot `arg0` to sub-object `arg1` (1-based).
void BIFunc_HierObjectMapWeapon(int** sp, void* self) {
    int slot = BIFArg(sp, 0);
    int sub = BIFArg(sp, 1);
    ((ISceneNode*)self)->AsHierObject()->AsHierTankObject()->MapWeaponSlot(slot, sub - 1);
    BIFPop(sp);
}

// The pan (or, with `arg1`, tilt) angle in degrees from sub-object `arg0`
// to the tank's pan (or, with `arg2`, tilt) aim target raised by `arg3`.
void BIFunc_HierObjectGetAimAngle(int** sp, void* self) {
    int tilt = BIFArg(sp, 1);
    int useTiltTarget = BIFArg(sp, 2);
    int index = BIFArg(sp, 0);
    float raise = BIFArgF(sp, 3);
    CTankObject* tank = ((ISceneNode*)self)->AsHierObject()->AsHierTankObject();
    CStaticObject* sub = tank->GetSubObject(index);
    CVector3 pos;
    sub->GetPosition(pos);
    CVector3 target;
    if (useTiltTarget != 0) {
        target = tank->m_tiltAimTarget;
    } else {
        target = tank->m_panAimTarget;
    }
    target.z += raise;
    CVector3 right;
    sub->GetRightward(right);
    float angle;
    if (tilt != 0) {
        CVector3 forward;
        sub->GetForward(forward);
        angle = -MathFunGetTiltAngleDiffNoRoll(&pos, &target, &right, &forward);
    } else {
        angle = MathFunGetPanAngleDiffNoRoll(&pos, &target, &right);
    }
    angle *= 57.295776f;
    BIFPop(sp);
    BIFReturnF(sp, angle);
}

void BIFunc_HierObjectFire(int** sp, void* self) {
    int sub = BIFArg(sp, 0);
    CTankObject* tank = ((ISceneNode*)self)->AsHierObject()->AsHierTankObject();
    if (tank != NULL) {
        tank->FireSubObject(sub);
    }
    BIFPop(sp);
}

// A negative multiplier `-n` means 1/n.
void BIFunc_HierObjectRegisterBulletDamage(int** sp, void* self) {
    int a = BIFArg(sp, 0);
    int b = BIFArg(sp, 1);
    int c = BIFArg(sp, 2);
    float multiplier = BIFArgF(sp, 3);
    CHierObject* hier = ((ISceneNode*)self)->AsHierObject();
    if (multiplier < 0.0f) {
        multiplier = 1.0f / -multiplier;
    }
    hier->RegisterBulletDamage(a, b, c, multiplier);
    BIFPop(sp);
}

// `angle` (arg0) adjusted so that `base` + angle stays within [min, max].
void BIFunc_GetConstrainedRotateAngle(int** sp, void*) {
    float angle = BIFArgF(sp, 0);
    float base = BIFArgF(sp, 1);
    float result = angle;
    float sum = base + angle;
    float max = BIFArgF(sp, 3);
    float min = BIFArgF(sp, 2);
    if (sum > max) {
        result = max - base;
    } else if (sum < min) {
        result = min - base;
    }
    BIFPop(sp);
    BIFReturnF(sp, result);
}

// What a hit event passes to the script.
struct BSHitInfo {
    /* 0x0 */ u8 unk_0x0[0x4];
    /* 0x4 */ s32 m_damage;
    /* 0x8 */ u8 unk_0x8[0x4];
    /* 0xC */ u32 m_bulletType;
    /* 0x10 */ s32 m_subObject;
};

// Health `arg2` less the damage of hit `arg0` if it hit sub-object `arg1`
// (scaled by the running hier object's multiplier for the bullet's class).
void BIFunc_DoDamageIfLocationMatches(int** sp, void* self) {
    BSHitInfo* hit = ADDR_TO_PTR(BSHitInfo, BIFArg(sp, 0));
    int sub = BIFArg(sp, 1);
    int health = BIFArg(sp, 2);
    if (hit->m_subObject == sub) {
        CHierObject* hier = ((ISceneNode*)self)->AsHierObject();
        if (hier != NULL) {
            int damageClass;
            switch (hit->m_bulletType) {
            case 26:
            case 27:
                damageClass = 0;
                break;
            case 10:
            case 11:
            case 17:
            case 18:
            case 25:
                damageClass = 1;
                break;
            case 1:
            case 4:
            case 6:
            case 7:
            case 8:
            case 9:
                damageClass = 2;
                break;
            case 0:
            case 14:
            case 15:
            case 19:
            case 20:
                damageClass = 3;
                break;
            case 12:
                damageClass = 4;
                break;
            case 5:
            case 13:
            case 16:
                damageClass = 5;
                break;
            case 2:
                damageClass = 6;
                break;
            case 3:
            case 21:
            case 22:
            case 23:
            case 24:
                damageClass = 7;
                break;
            default:
                damageClass = 0;
                break;
            }
            hit->m_damage = hit->m_damage * hier->GetBulletDamageMultiplierByClass(sub, damageClass);
        }
        health -= hit->m_damage;
        if (health < 0) {
            health = 0;
        }
    }
    BIFPop(sp);
    BIFReturn(sp, health);
}

void BIFunc_SetHeadTrackPlayer(int** sp, void* self) {
    int track = BIFArg(sp, 0);
    float time = BIFArg(sp, 1);
    ((ISceneNode*)self)->AsSoldierObject()->SetHeadTrackPlayer(track != 0, time);
    BIFPop(sp);
}

void BIFunc_GetMySoundInfoStruct(int** sp, void* self) {
    CSoldierSoundInfo* info = &((ISceneNode*)self)->AsSoldierObject()->m_soundInfo;
    BIFPop(sp);
    BIFReturn(sp, (int)PTR_TO_ADDR(info));
}

void BIFunc_SetMySoundInfoStruct(int** sp, void* self) {
    CSoldierSoundInfo* info = ADDR_TO_PTR(CSoldierSoundInfo, BIFArg(sp, 0));
    CSoldierObject* soldier = ((ISceneNode*)self)->AsSoldierObject();
    soldier->m_soundInfo.field_0x0 = info->field_0x0;
    soldier->m_soundInfo.field_0x4 = info->field_0x4;
    soldier->m_soundInfo.field_0x8 = info->field_0x8;
    soldier->m_soundInfo.field_0xC = info->field_0xC;
    soldier->m_soundInfo.field_0x10 = info->field_0x10;
    soldier->m_soundInfo.field_0x14 = info->field_0x14;
    soldier->m_soundInfo.field_0x18 = info->field_0x18;
    soldier->m_soundInfo.field_0x1C = info->field_0x1C;
    BIFPop(sp);
}

// With `arg0` 1: the pan angle from the running object's facing to point
// `arg1`.
void BIFunc_GetNoiseInfo(int** sp, void* self) {
    float result = 0.0f;
    BS_STRUCT_Vector_struct* point = ADDR_TO_PTR(BS_STRUCT_Vector_struct, BIFArg(sp, 1));
    if (BIFArg(sp, 0) == 1) {
        CVector3 pos;
        CVector3 right;
        ((ISceneNode*)self)->GetPosition(pos);
        ((ISceneNode*)self)->GetRightward(right);
        CVector3 target;
        target.x = point->x;
        target.y = point->y;
        target.z = point->z;
        result = MathFunGetPanAngleDiffNoRoll(&pos, &target, &right);
    }
    BIFPop(sp);
    BIFReturnF(sp, result);
}

// Knocks the helmet off upwards at speed `arg0`.
void BIFunc_EjectHelmet(int** sp, void* self) {
    CVector3 velocity(0.0f, 0.0f, 0.0f);
    velocity.z = BIFArgF(sp, 0);
    ((ISceneNode*)self)->AsSoldierObject()->EjectHelmet(&velocity);
    BIFPop(sp);
}

void BIFunc_RegisterMMGCheck(int** sp, void*) {
    BSScheduleRegistrationRecord_struct* rec =
        g_MMGPointCheckSchedule.Register(g_pBSObject, 0x102, true, BIFArg(sp, 0), 0, 0);
    BIFPop(sp);
    BIFReturn(sp, (int)PTR_TO_ADDR(rec));
}

// (With NULL the original unregisters from the buddy schedule.)
void BIFunc_StopMMGCheck(int** sp, void* self) {
    BSScheduleRegistrationRecord_struct* rec = ADDR_TO_PTR(BSScheduleRegistrationRecord_struct, BIFArg(sp, 0));
    if (rec != NULL) {
        g_MMGPointCheckSchedule.Unregister(rec);
    } else {
        g_BuddyPlayerCheckSchedule.Unregister(((ISceneNode*)self)->GetScriptObject());
    }
    BIFPop(sp);
}

void BIFunc_AIChooseMMGPoint(int** sp, void* self) {
    int point = 0;
    point = (int)PTR_TO_ADDR(SelfFilter(self)->ChooseMMGPoint());
    BIFPop(sp);
    BIFReturn(sp, point);
}

// Puts mounted gun `arg0` back where the level placed it.
void BIFunc_ResetMMG(int** sp, void*) {
    BSObject* obj = ADDR_TO_PTR(BSObject, BIFArg(sp, 0));
    if (obj != NULL) {
        CStaticObject* gun = (CStaticObject*)FindMountedGun(obj->m_trigger);
        CMatrix m;
        TriggerObjectData_struct* data = gun->GetScriptObject()->m_trigger->m_data;
        ((CQuaternion*)data->m_orientation)->GetMatrix(m);
        CVector3 pos;
        pos.x = data->m_position[0];
        pos.y = data->m_position[1];
        pos.z = data->m_position[2];
        m.SetPos(pos);
        gun->SetTMLocalToWorld(m);
    }
    BIFPop(sp);
}

// The multiplayer weapon set (level resource): field 1 is the number of
// weapons; weapon set ID `i` (1-based) has weapon CRC field 0x13 + i and
// ammo CRC field 0x25 + i.
static LevelFileContentsStruct_* g_pMPWeaponSet;

#define MPWS_COUNT 1
#define MPWS_WEAPON 0x13
#define MPWS_AMMO 0x25

static inline int MPWeaponCRC(int id) {
    return g_pMPWeaponSet->fields[MPWS_WEAPON + id].value;
}

static inline int MPAmmoCRC(int id) {
    return g_pMPWeaponSet->fields[MPWS_AMMO + id].value;
}

// Creates the pickup the running trigger is set up for, as weapon `crc`.
static inline int CreateMPPickup(int crc) {
    g_pBSObject->m_trigger->m_data->field_0x48 = crc;
    TriggerObject_struct* trigger = g_pBSObject->m_trigger;
    return GO_SCRIPT_WORD(CreateObject(trigger, trigger->m_data->field_0x50, NULL));
}

void BIFunc_MPCreateWeaponByWeaponSetID(int** sp, void*) {
    int result = 0;
    int id = BIFArg(sp, 0);
    if (id > 0 && id <= g_pMPWeaponSet->fields[MPWS_COUNT].value) {
        result = CreateMPPickup(MPWeaponCRC(id));
    }
    BIFPop(sp);
    BIFReturn(sp, result);
}

void BIFunc_MPCreateAmmoByWeaponSetID(int** sp, void*) {
    int result = 0;
    int id = BIFArg(sp, 0);
    if (id > 0 && id <= g_pMPWeaponSet->fields[MPWS_COUNT].value) {
        result = CreateMPPickup(MPAmmoCRC(id));
    }
    BIFPop(sp);
    BIFReturn(sp, result);
}

// Creates the trigger's weapon pickup if its weapon is in the set.
void BIFunc_MPCreateWeaponByType(int** sp, void*) {
    TriggerObject_struct* trigger = g_pBSObject->m_trigger;
    int count = g_pMPWeaponSet->fields[MPWS_COUNT].value;
    TriggerObjectData_struct* data = trigger->m_data;
    int result = 0;
    for (int i = 0; i < count; i++) {
        if (data->field_0x48 == MPWeaponCRC(i + 1)) {
            result = GO_SCRIPT_WORD(CreateObject(trigger, data->field_0x50, NULL));
            break;
        }
    }
    BIFPop(sp);
    BIFReturn(sp, result);
}

void BIFunc_MPCreateAmmoByType(int** sp, void*) {
    TriggerObject_struct* trigger = g_pBSObject->m_trigger;
    int count = g_pMPWeaponSet->fields[MPWS_COUNT].value;
    TriggerObjectData_struct* data = trigger->m_data;
    int result = 0;
    for (int i = 0; i < count; i++) {
        if (data->field_0x48 == MPAmmoCRC(i + 1)) {
            result = GO_SCRIPT_WORD(CreateObject(trigger, data->field_0x50, NULL));
            break;
        }
    }
    BIFPop(sp);
    BIFReturn(sp, result);
}

void BIFunc_MPGetWeaponPickupPromptFromWeaponSetID(int** sp, void*) {
    int weaponPrompt = 0;
    int ammoPrompt = 0;
    CPlayerObject::GetWeaponInfoByCRC(MPWeaponCRC(BIFArg(sp, 0)), &weaponPrompt, &ammoPrompt);
    BIFPop(sp);
    BIFReturn(sp, weaponPrompt);
}

void BIFunc_MPGetAmmoPickupPromptFromWeaponSetID(int** sp, void*) {
    int weaponPrompt = 0;
    int ammoPrompt = 0;
    CPlayerObject::GetWeaponInfoByCRC(MPWeaponCRC(BIFArg(sp, 0)), &weaponPrompt, &ammoPrompt);
    BIFPop(sp);
    BIFReturn(sp, ammoPrompt);
}

void BIFunc_MPGetWeaponTypeFromWeaponSetID(int** sp, void*) {
    int type = CPlayerObject::GetWeaponInfoByCRC(MPWeaponCRC(BIFArg(sp, 0)), NULL, NULL);
    BIFPop(sp);
    BIFReturn(sp, type);
}

// Player `arg0` picks up the running trigger's ammo (amount `arg1`).
void BIFunc_MPAddPlayerAmmo(int** sp, void*) {
    BSObject* obj = ADDR_TO_PTR(BSObject, BIFArg(sp, 0));
    int amount = BIFArg(sp, 1);
    bool taken = obj->m_gameObject->GetSceneNode()->AsPlayerObject()->AddAmmoOrWeapon(
        g_pBSObject->m_trigger->m_data->field_0x48, amount, NULL);
    BIFPop(sp);
    BIFReturn(sp, taken);
}

// Player `arg0` picks up the running trigger's health (amount `arg1`) and
// is told which kind it was.
void BIFunc_MPAddPlayerHealth(int** sp, void*) {
    BSObject* obj = ADDR_TO_PTR(BSObject, BIFArg(sp, 0));
    int amount = BIFArg(sp, 1);
    CPlayerObject* player = obj->m_gameObject->GetSceneNode()->AsPlayerObject();
    bool taken = player->AddHealth(g_pBSObject->m_trigger->m_data->field_0x48, amount, NULL);
    if (taken) {
        int message = -1;
        switch (g_pBSObject->m_trigger->m_data->field_0x48) {
        case 0x598E4105:
        case 0x72441AA1:
            message = 0x10A;
            break;
        case 0x10EFEBA2:
            message = 0x10B;
            break;
        case 0xF7BADDB9:
            message = 0x10C;
            break;
        }
        if (message >= 0) {
            PromptPlayer(GetUMString(message), player->field_0x768);
        }
    }
    BIFPop(sp);
    BIFReturn(sp, taken);
}

void BIFunc_MPGetPlayerBody(int** sp, void*) {
    CPlayerObject* player = g_pBSObject->m_gameObject->GetSceneNode()->AsPlayerObject();
    BIFPop(sp);
    BIFReturn(sp, (int)PTR_TO_ADDR(player->m_pBody));
}

void BIFunc_MPGetPlayerVelocity(int** sp, void*) {
    float velocity;
    long unused;
    CPlayerObject* player = g_pBSObject->m_gameObject->GetSceneNode()->AsPlayerObject();
    BIFPop(sp);
    player->m_pBody->GetFlocoParams(velocity, unused);
    BIFReturnF(sp, velocity);
}

// Schedule callbacks. Each returns true when the scheduled event should be
// sent, with `*data` its data and `*delay` the delay before the next check.

// The AI filter of a scheduled object.
static inline CAIFilterObject* ScheduledFilter(BSObject* obj) {
    return obj->m_gameObject->GetSceneNode()->GetAIDoodad()->m_pFilter;
}

// Fires when a soldier at a mounted gun should leave it (`gun` is the gun).
bool DoMMGPointCheck(BSObject* obj, int gun, int, int, int*, int* delay) {
    if (gun == 0) {
        return false;
    }
    CAIFilterObject* filter = ScheduledFilter(obj);
    *delay = 0;
    return filter->ShouldILeaveMMG(ADDR_TO_PTR(BSObject, gun));
}

// Fires with the distance to the target when the squared distance leaves
// (`inner`, `outer`); without a target, sends 0x45 and stops.
bool DoDistanceToTarget(BSObject* obj, int inner, int outer, int, int* data, int* delay) {
    float distSq = 0.0f;
    bool hasTarget = false;
    CAIObject* ai = ScheduledFilter(obj)->m_pAIObject;
    if (ai != NULL && (ai->m_nonAITarget || ai->m_pTarget != NULL)) {
        hasTarget = true;
    }
    if (hasTarget) {
        float d;
        if (ai->m_nonAITarget) {
            d = ai->GetDistanceSquaredXYZReal();
        } else {
            d = ai->GetDistanceSquaredXYZReal(ai->m_pTarget);
        }
        distSq = d;
    } else {
        g_DistanceToTargetSchedule.Unregister(obj);
        BSObjectTriggerEvent(obj, 0x45, NULL, obj, true);
    }
    if (distSq > (float)inner && distSq < (float)outer) {
        return false;
    }
    float dist;
    if (distSq > 0.0f) {
        dist = sqrtf(distSq);
    } else {
        dist = distSq;
    }
    *delay = 0;
    *data = (int)dist;
    return true;
}

// Fires with the first object found by field of view (mode 2) or line of
// sight (mode 3).
bool DoTargetSearch(BSObject* obj, int mode, int, int, int* data, int* delay) {
    BSObject* found = NULL;
    CAIFilterObject* filter = ScheduledFilter(obj);
    switch (mode) {
    case 2: {
        CAIObject* seen = filter->m_pAIObject->CheckFieldOfViewModeList();
        found = seen != NULL ? AIScript(seen) : NULL;
        break;
    }
    case 3: {
        CAIObject* seen = filter->m_pAIObject->CheckLineOfSightModeList();
        found = seen != NULL ? AIScript(seen) : NULL;
        break;
    }
    }
    if (found == NULL) {
        return false;
    }
    *delay = (int)MathFunRandomI64(1, 9) * obj->m_trigger->m_data->field_0x79;
#ifdef FRONTLINE_NATIVE
    if (NativeAIOn()) {
        *delay = NativeAIReactionFrames(*delay);
    }
#endif
    *data = (int)PTR_TO_ADDR(found);
    return true;
}

// Fires when the watched object `other` is lost: out of `range` (mode 1),
// out of field of view (2) or line of sight (3, 5 inverted), or vision
// seeking finds nothing (4).
bool DoTargetCheck(BSObject* obj, int mode, int range, int other, int*, int* delay) {
    bool fire = true;
    ISceneNode* node = obj->m_gameObject->GetSceneNode();
    CAIFilterObject* filter = node->GetAIDoodad()->m_pFilter;
    switch (mode) {
    case 1: {
        ISceneNode* otherNode = (ADDR_TO_PTR(BSObject, other))->m_gameObject->GetSceneNode();
        if (otherNode != NULL) {
            CVector3 otherPos;
            otherNode->GetPosition(otherPos);
            CVector3 pos;
            node->GetPosition(pos);
            float dx = pos.x - otherPos.x;
            float dy = pos.y - otherPos.y;
            fire = dx * dx + dy * dy > (float)(range * range);
        }
        break;
    }
    case 2: {
        CAIObject* seen = filter->m_pAIObject->CheckFieldOfViewModeObject();
        if ((seen != NULL ? AIScript(seen) : NULL) != NULL) {
            fire = false;
        }
        break;
    }
    case 3: {
        CAIObject* seen = filter->m_pAIObject->CheckLineOfSightModeObject();
        if ((seen != NULL ? AIScript(seen) : NULL) != NULL) {
            fire = false;
        }
        break;
    }
    case 4: {
        CAIObject* seen = filter->m_pAIObject->CheckVisionSeekingMode();
        fire = (seen != NULL ? AIScript(seen) : NULL) != NULL;
        break;
    }
    case 5: {
        CAIObject* seen = filter->m_pAIObject->CheckLineOfSightModeObject();
        if ((seen != NULL ? AIScript(seen) : NULL) == NULL) {
            fire = false;
        }
        break;
    }
    }
    *delay = 0;
    return fire;
}

// Fires with the first corpse in view.
bool DoCorpseSearch(BSObject* obj, int, int, int, int* data, int* delay) {
    CAIObject* seen = ScheduledFilter(obj)->m_pAIObject->CheckFieldOfViewCorpse();
    BSObject* found = seen != NULL ? AIScript(seen) : NULL;
    if (found == NULL) {
        return false;
    }
    *delay = 0;
    *data = (int)PTR_TO_ADDR(found);
    return true;
}

// Fires when player `other` comes into (`lost` 0) or leaves (`lost` 1)
// line of sight within `range`.
bool DoBuddyPlayerCheck(BSObject* obj, int lost, int range, int other, int*, int* delay) {
    CAIFilterObject* filter = ScheduledFilter(obj);
    CAIFilterRealPosition pos;
    (ADDR_TO_PTR(BSObject, other))->m_gameObject->GetSceneNode()->GetPosition(*(CVector3*)&pos);
    *delay = 0;
    if (lost != 0) {
        return !filter->m_pAIObject->IsObjectInLineOfSight(pos, range, false);
    }
    return filter->m_pAIObject->IsObjectInLineOfSight(pos, range, false);
}

// Scheduling parameters for an AEMS sound event of type `type`.
void GetAEMSStructureValues(int type, int* a, int* volumeIndex, int* c, int* size) {
    switch (type) {
    case 1:
        *a = 3;
        *volumeIndex = 5;
        *c = -1;
        *size = 0x20;
        break;
    case 3:
        *a = 3;
        *volumeIndex = 2;
        *c = -1;
        *size = 0x14;
        break;
    case 4:
        *a = 4;
        *volumeIndex = 5;
        *c = -1;
        *size = 0x1C;
        break;
    case 6:
        *a = 2;
        *volumeIndex = 3;
        *c = -1;
        *size = 0x10;
        break;
    case 7:
        *a = 4;
        *volumeIndex = 5;
        *c = -1;
        *size = 0x28;
        break;
    case 8:
        *a = 3;
        *volumeIndex = 4;
        *c = -1;
        *size = 0x18;
        break;
    case 14:
        *a = 4;
        *volumeIndex = 5;
        *c = -1;
        *size = 0x28;
        break;
    case 15:
        *a = 3;
        *volumeIndex = 4;
        *c = -1;
        *size = 0x18;
        break;
    case 16:
        *a = 3;
        *volumeIndex = 2;
        *c = 1;
        *size = 0x1C;
        break;
    }
}

#define UM_NO_HINT 0x2D9

// Shows the next hint for the first incomplete objective (alternating
// between its two hints) and plays the hint sound.
int DisplayHint() {
    if (g_iHighestObjectiveNumber == 0) {
        DisplayHintText(GetUMString(UM_NO_HINT));
        return 1;
    }
    int objective = 1;
    while (objective <= g_iHighestObjectiveNumber && GetObjectiveStatus(objective)) {
        objective++;
    }
    if (objective > g_iHighestObjectiveNumber) {
        objective = 20;
    }
    const char** hints = g_HintTextArray[objective - 1];
    int next = (int)PTR_TO_ADDR(hints[2]);
    if (hints[next] != NULL) {
        DisplayHintText(hints[next]);
        do {
            next++;
            if (next >= 2) {
                next -= 2;
            }
        } while (hints[next] == NULL);
        hints[2] = ADDR_TO_PTR(const char, next);
    } else {
        DisplayHintText(GetUMString(UM_NO_HINT));
    }
    int sound[10] = {7, 3, 0, 0x1000, 0, 0, 0x9C4, 0, 0, 0x7FFF};
    AEMS_SendEvent(sound);
    return 1;
}

// Weak methods (header virtuals emitted by this unit).
void* BSGO_Basic::GetScriptData() {
    return NULL;
}

ISceneNode* BSGO_Basic::GetSceneNode() {
    return NULL;
}

void* BSGO_Basic::GetProximityData() {
    return NULL;
}

void* BSGO_Basic::GetEmbeddedHitReactionInfo() {
    return NULL;
}

BSObject* CStaticObject::GetScriptObject() const {
    return m_pScriptObject;
}

void CStaticObject::SetCollisionId(EClsnId id) {
    m_collisionId = id;
}

CThrownObject* CStaticObject::AsThrownObject() {
    return NULL;
}

CWeapon* CStaticObject::AsWeaponObject() {
    return NULL;
}

CTankObject* CStaticObject::AsHierTankObject() {
    return NULL;
}

BSObject* CAnimObject::GetScriptObject() const {
    return m_pScriptObject;
}

void CAnimObject::SetCollisionId(EClsnId id) {
    m_collisionId = id;
}

// (The AI doodads are embedded in the objects.)
CAIDoodad* CPlayerObject::GetAIDoodad() {
    return (CAIDoodad*)&m_aiDoodad;
}

void CPlayerObject::SetCollisionId(EClsnId id) {
    m_collisionId = id;
}

CAIDoodad* CSoldierObject::GetAIDoodad() {
    return &m_aiDoodad;
}

void CBullet::GetPosition(CVector3& pos) const {
    pos = m_position;
}

// Header of the built-in function table the script compiler writes
// (little-endian).
struct BIFDataHeader {
    /* 0x0 */ u8 unk_0x0[0x4];
    /* 0x4 */ int m_version;
    /* 0x8 */ int m_checksum;
    /* 0xC */ int m_numFunctions;
    /* 0x10 */ u32 m_functions;  // offset, then BSBuiltInFunction_struct*
};

#define BIF_VERSION 0x1D
#define BIF_CHECKSUM 0x3352B

// Maps the CRC of each built-in function's name to the function.
struct BIFCRCEntry {
    u32 m_crc;
    BSBuiltInFunc m_func;
};

#define BIF_CRC_COUNT 288
extern BIFCRCEntry g_BIFCRCArray[BIF_CRC_COUNT];

// Loads the level's built-in function table (resource 1).
static inline BIFDataHeader* LoadBIFData() {
    TLT_FindNextResourceByType((TLTResourceID)0xB, NULL);
    BIFDataHeader* header = (BIFDataHeader*)TLT_LoadFileFromLevelBigFile(
        ADDR_TO_PTR(const char, TLT_FindNextResourceByType((TLTResourceID)1, NULL)->fields[0].value), NULL);
    EndianSwap(header->m_version);
    EndianSwap(header->m_checksum);
    EndianSwap(header->m_numFunctions);
    EndianSwap(header->m_functions);
    return header;
}

static inline void InitBIFSchedules() {
    g_SoundSchedule.Init();
#ifdef FRONTLINE_NATIVE
    // Rewritten's AI polls more at once, so checks don't lag behind with many soldiers.
    g_DistanceToTargetSchedule.Init(DoDistanceToTarget, NativeAIScheduleBatch(5), 1);
    g_TargetSearchSchedule.Init(DoTargetSearch, NativeAIScheduleBatch(5), 1);
    g_TargetCheckSchedule.Init(DoTargetCheck, NativeAIScheduleBatch(5), 7);
    g_CorpseSearchSchedule.Init(DoCorpseSearch, NativeAIScheduleBatch(5), 0x1D);
#else
    g_DistanceToTargetSchedule.Init(DoDistanceToTarget, 5, 1);
    g_TargetSearchSchedule.Init(DoTargetSearch, 5, 1);
    g_TargetCheckSchedule.Init(DoTargetCheck, 5, 7);
    g_CorpseSearchSchedule.Init(DoCorpseSearch, 5, 0x1D);
#endif
    g_BuddyPlayerCheckSchedule.Init(DoBuddyPlayerCheck, 1, 0x78);
    g_MMGPointCheckSchedule.Init(DoMMGPointCheck, 5, 7);
}

static inline void ResetHints() {
    g_iHighestObjectiveNumber = 0;
    ((u32*)g_pGenericSoundMemory)[0x13] = 0xAEAEAEAE;
    for (int i = 0; i < 20; i++) {
        g_HintTextArray[i][0] = NULL;
        g_HintTextArray[i][1] = NULL;
        g_HintTextArray[i][2] = NULL;
    }
    g_iHintBufferIndex = 0;
}

int BSInitBIFuncs() {
    BIFDataHeader* header = LoadBIFData();
    if (header->m_version != BIF_VERSION) {
        DebugMsg("Version error on bif data file\n");
        DebugMsg("  engine version = %d.  file version = %d\n", BIF_VERSION, header->m_version);
        DebugMsg("  The script compiler and/or the engine needs to be updated\n");
    }
    if (header->m_checksum != BIF_CHECKSUM) {
        DebugMsg("Checksum error on bif data file\n");
        DebugMsg("  engine checksum = %d.  file checksum = %d\n", BIF_CHECKSUM, header->m_checksum);
        DebugMsg("  Scripts need to be rebuilt or the engine needs to be built with a new bsstruct.h\n");
    }
    header->m_functions = (u32)PTR_TO_ADDR(header) + header->m_functions;
    g_pBuiltInFunctions = ADDR_TO_PTR(BSBuiltInFunction_struct, header->m_functions);
    g_pNumBuiltInFunctions = header->m_numFunctions;
    int i;
    for (i = 0; i < header->m_numFunctions; i++) {
        BSBuiltInFunction_struct* bif = &g_pBuiltInFunctions[i];
        EndianSwap(bif->m_crc);
        EndianSwap(bif->field_0x8);
        EndianSwap(bif->m_popCount);
        EndianSwap(bif->m_numArgs);
        g_pBuiltInFunctions[i].m_func = BSBifuncInvalid;
    }
    for (i = 0; i < header->m_numFunctions; i++) {
        BSBuiltInFunction_struct* bif = &g_pBuiltInFunctions[i];
        for (int j = 0; j < BIF_CRC_COUNT; j++) {
            if (g_BIFCRCArray[j].m_crc == bif->m_crc) {
                bif->m_func = g_BIFCRCArray[j].m_func;
                break;
            }
        }
    }
    InitBIFSchedules();
    for (int p = 0; p < 4; p++) {
        g_pPopUpMessageHandler[p] = new PopUpMessageHandler("data/chars.bum", "data/binum.bum");
    }
    ResetHints();
    if (g_bInMultiplayerMode) {
        g_pMPWeaponSet = TLT_FindNextResourceByTypeAndVal((TLTResourceID)0x16, 0, g_levelWeaponSetID, NULL);
        if (g_NumPlayers <= 2) {
            g_promptLineWidth = 0x24;
            g_promptMaxSize = 0x30;
        } else {
            g_promptLineWidth = 0x18;
            g_promptMaxSize = 0x18;
        }
    } else {
        g_pMPWeaponSet = NULL;
    }
    return 0;
}

// (Swaps the table header again but keeps the function table.)
int BSResetBIFuncs() {
    LoadBIFData();
    g_SoundSchedule.Init();
#ifdef FRONTLINE_NATIVE
    // Rewritten's AI polls more at once, so checks don't lag behind with many soldiers.
    g_DistanceToTargetSchedule.Reset(DoDistanceToTarget, NativeAIScheduleBatch(5), 1);
    g_TargetSearchSchedule.Reset(DoTargetSearch, NativeAIScheduleBatch(5), 1);
    g_TargetCheckSchedule.Reset(DoTargetCheck, NativeAIScheduleBatch(5), 7);
    g_CorpseSearchSchedule.Reset(DoCorpseSearch, NativeAIScheduleBatch(5), 0x1D);
#else
    g_DistanceToTargetSchedule.Reset(DoDistanceToTarget, 5, 1);
    g_TargetSearchSchedule.Reset(DoTargetSearch, 5, 1);
    g_TargetCheckSchedule.Reset(DoTargetCheck, 5, 7);
    g_CorpseSearchSchedule.Reset(DoCorpseSearch, 5, 0x1D);
#endif
    g_BuddyPlayerCheckSchedule.Reset(DoBuddyPlayerCheck, 1, 0x78);
    g_MMGPointCheckSchedule.Reset(DoMMGPointCheck, 5, 7);
    ResetHints();
    return 0;
}

int BSCleanUpBIFuncs() {
    g_DistanceToTargetSchedule.Shutdown();
    g_TargetSearchSchedule.Shutdown();
    g_TargetCheckSchedule.Shutdown();
    g_SoundSchedule.Shutdown();
    g_CorpseSearchSchedule.Shutdown();
    g_BuddyPlayerCheckSchedule.Shutdown();
    g_MMGPointCheckSchedule.Shutdown();
    for (int p = 0; p < 4; p++) {
        delete g_pPopUpMessageHandler[p];
    }
    return 0;
}

int BSUpdateBIFuncs() {
    g_DistanceToTargetSchedule.Update();
    g_TargetSearchSchedule.Update();
    g_TargetCheckSchedule.Update();
    g_SoundSchedule.Update(1);
    g_CorpseSearchSchedule.Update();
    g_BuddyPlayerCheckSchedule.Update();
    g_MMGPointCheckSchedule.Update();
    return 0;
}

BIFCRCEntry g_BIFCRCArray[BIF_CRC_COUNT] = {
    {0x12F32503, BIFunc_AIGetSoldierMatchListStruct},
    {0xCC919301, BIFunc_AIGetSoldierPointer},
    {0x964DA7FB, BIFunc_AISetupTargetMatchList},
    {0xD1644323, BIFunc_AIResetTargetMatchList},
    {0x627F97D7, BIFunc_AISetTargetMode},
    {0xD8716F9E, BIFunc_SetAIObjectSharedDataPropVars},
    {0x2F8A0D98, BIFunc_RegisterDistanceToTarget},
    {0x21C8D9A4, BIFunc_RegisterTargetSearch},
    {0xBBC62DFB, BIFunc_RegisterTargetCheck},
    {0xB8E276BF, BIFunc_RegisterCorpseSearch},
    {0xE4192FEB, BIFunc_RegisterBuddyPlayerCheck},
    {0x6B064677, BIFunc_RegisterMMGCheck},
    {0xBA10C813, BIFunc_StopDistanceToTarget},
    {0x7BECE1AC, BIFunc_StopTargetSearch},
    {0x9A213E05, BIFunc_StopTargetCheck},
    {0xE2C64EB7, BIFunc_StopCorpseSearch},
    {0x7183EA60, BIFunc_StopBuddyPlayerCheck},
    {0x6D559D81, BIFunc_StopMMGCheck},
    {0xB8361E70, BIFunc_AIChooseTarget},
    {0x121FAF7D, BIFunc_AIChooseTargetByID},
    {0x78DB2174, BIFunc_AISetHearingRadius},
    {0x210F63A1, BIFunc_AIGetDistanceToTargetXY},
    {0xF84CCB7A, BIFunc_AIGetDistanceToTargetXYZ},
    {0x6BBA973D, BIFunc_AIEnableTargetAim},
    {0xA739F867, BIFunc_AIIsItAGoodTimeToFire},
    {0xB3FB9A02, BIFunc_AIFireAtCurrentTarget},
    {0xD9B324F1, BIFunc_AIFireMMGAtCurrentTarget},
    {0xF16918D9, BIFunc_FireProjectile},
    {0x3359980C, BIFunc_FireProjectileAtPath},
    {0x140A2FF2, BIFunc_HierObjectSetLookObject},
    {0x15EBDF6A, BIFunc_HierObjectMapWeapon},
    {0xC42E27E3, BIFunc_HierObjectGetAimAngle},
    {0x2BBA76BF, BIFunc_HierObjectFire},
    {0x92A89039, BIFunc_HierObjectRegisterBulletDamage},
    {0xD4B2FC4F, BIFunc_GetConstrainedRotateAngle},
    {0xA0CEBC0B, BIFunc_DoDamageIfLocationMatches},
    {0x1861C7FA, BIFunc_AIStartSplinePathWalk},
    {0xA9C7C76E, BIFunc_AIStopSplinePathWalk},
    {0x5334541A, BIFunc_AIContinueSplinePathWalk},
    {0x0FDBDABF, BIFunc_AIStartAStarPathWalk},
    {0x0A8EDB49, BIFunc_AIStartAStarPathWalkWithCurrentTarget},
    {0xA2326A84, BIFunc_AIStopAStarPathWalk},
    {0x5E9C7D9A, BIFunc_AIContinueAStarPathWalk},
    {0x70ABD31D, BIFunc_AIEngageObstacleAvoidance},
    {0xE7C1CBA0, BIFunc_GetWayPointInfo},
    {0x1D1E1A02, BIFunc_AIStoreBestCoverPoint},
    {0x54CE5B0E, BIFunc_AIGetClosestCoverPoint},
    {0x4B7D4FE5, BIFunc_AIStartCoverPointWalk},
    {0xF098E139, BIFunc_AIStartCoverPointWalkToStored},
    {0xB86E2DB5, BIFunc_AICommitCoverPointWalkToStored},
    {0xFADB4F71, BIFunc_AIStopCoverPointWalk},
    {0x0028DC05, BIFunc_AIContinueCoverPointWalk},
    {0xC0E04120, BIFunc_AIArrivedAtCoverpoint},
    {0x881551DB, BIFunc_AILeaveCoverPoint},
    {0xE3E5DAF6, BIFunc_AIStartOrientTowardsCoverPoint},
    {0x8171EF40, BIFunc_AIStopOrientTowardsCoverPoint},
    {0x6CAED4B0, BIFunc_AIContinueOrientTowardsCoverPoint},
    {0x632749DA, BIFunc_AISnapToCoverPointPosition},
    {0x65B6E865, BIFunc_AIInvalidateCoverPointByID},
    {0x95845412, BIFunc_GetClosestValidWaypoint},
    {0xC8D951B3, BIFunc_AIChooseMMGPoint},
    {0x2D335AAA, BIFunc_AISetMovePoint},
    {0x722BC49A, BIFunc_AIStartMovePointWalk},
    {0xDFC274A1, BIFunc_AIStopMovePointWalk},
    {0x236C63BF, BIFunc_AIContinueMovePointWalk},
    {0x4859DE77, BIFunc_AISnapToMovePoint},
    {0x2F7481C6, BIFunc_AISnapToMovePointAndAngle},
    {0x84E500C0, BIFunc_AIGetDistanceToMovePoint},
    {0x4140AE20, BIFunc_AIStartAStarPathWalkToMovePoint},
    {0x5B939840, BIFunc_AIChooseCoverPoint},
    {0xA99221A5, BIFunc_AISetCoverPoint},
    {0x39D6ED49, BIFunc_AISetRunAwayMovepoint},
    {0x4EC96EB2, BIFunc_PlayAnimation},
    {0x5D8AAE9F, BIFunc_PlayLocomotion},
    {0xE8892478, BIFunc_PlayAnimationOnOther},
    {0xC81719BA, BIFunc_PlayFacialAnimation},
    {0x04BE72F0, BIFunc_PlayAnimationNoTranslation},
    {0xFF3071C7, BIFunc_AICollisionTest},
    {0x0CF4EA8A, BIFunc_SetGravity},
    {0xC67460C7, BIFunc_SetGroundCheck},
    {0x63312223, BIFunc_InduceGroundCheck},
    {0x17D2A2C5, BIFunc_SetHDCollision},
    {0x10937C0E, BIFunc_DisableCollision},
    {0xB338CCA7, BIFunc_SetMechanicCollision},
    {0xE7F48A50, BIFunc_SetCanBePushed},
    {0x5BA09DD4, BIFunc_PathMechAllowCollisionDamage},
    {0x2D1A465F, BIFunc_SetHeadAsRoot},
    {0xCC5908EC, BIFunc_SetCollisionToPlayer},
    {0x468EDCC0, BIFunc_SendEvent},
    {0x85BFA7E5, BIFunc_SendEventSelf},
    {0xE90A1B5F, BIFunc_SendEventParent},
    {0x56E2C2EB, BIFunc_SendEventImmediate},
    {0x4A8CFEE0, BIFunc_SendEventDelayed},
    {0x5D5BC3B4, BIFunc_SendMessage},
    {0x0B379420, BIFunc_GetEventSystemMemBlock},
    {0x07B7FC8D, BIFunc_RegisterTimerEvent},
    {0x54ED2131, BIFunc_EnableNonVisDestruct},
    {0x9A32C852, BIFunc_GetMyInstanceData},
    {0x8F774186, BIFunc_GetEmbeddedHitReactionInfo},
    {0xE44D223B, BIFunc_GetObjInfo},
    {0xCA0803A9, BIFunc_SetObjInfo},
    {0x24C0749A, BIFunc_GetWeaponType},
    {0xFEDB1263, BIFunc_SetWeaponEnable},
    {0x620DB9B9, BIFunc_GetRandomInteger},
    {0x796B0CDE, BIFunc_GetRandomFloat},
    {0x9CFECB1B, BIFunc_GetMySoundInfoStruct},
    {0x32D58B67, BIFunc_SetMySoundInfoStruct},
    {0xD9E19D6E, BIFunc_GetNoiseInfo},
    {0x6351C832, BIFunc_TranslateRel},
    {0xA58B3453, BIFunc_RotateRel},
    {0xA3F9CC03, BIFunc_ScaleRel},
    {0x777723A8, BIFunc_InitToInvalid},
    {0xCFCB99AA, BIFunc_DisplayPrompt},
    {0x1635ADBC, BIFunc_DisplayLetterbox},
    {0x3A8E0B07, BIFunc_AddObjective},
    {0x3C8D264C, BIFunc_SetObjective},
    {0x3393F0CF, BIFunc_GetObjective},
    {0x65BD26BB, BIFunc_AddHint},
    {0xBE056BA6, BIFunc_FloatToInt},
    {0x73342BB3, BIFunc_CreateChild},
    {0x78E03854, BIFunc_DestroyObject},
    {0x22F18E41, BIFunc_CanICreateChild},
    {0x3852A2F2, BIFunc_CreateChildThrownObject},
    {0xD7AB0BB9, BIFunc_DestroyThrownObject},
    {0x31A4A1D5, BIFunc_CreateChildFallingObject},
    {0xD94717AE, BIFunc_DropFallingObject},
    {0xB4D3FCCA, BIFunc_CreateChildRootHierObject},
    {0xA48346DC, BIFunc_CreateChildSubHierObject},
    {0xEEAC8BC5, BIFunc_SwapHierObject},
    {0x4562276D, BIFunc_DestroySubHierObject},
    {0x654BC7E2, BIFunc_CreateParticleSystemByType},
    {0x39DCB6B7, BIFunc_ResetParticleSystemRotation},
    {0x7551C57B, BIFunc_SetParticleSystemVelocity},
    {0x27515812, BIFunc_DestroyParticleSystem},
    {0x0983D290, BIFunc_CreateLightByType},
    {0x469F1332, BIFunc_DestroyLight},
    {0x273F0933, BIFunc_CreateDumpObject},
    {0x98FDEC8D, BIFunc_GetPlayerAmmo},
    {0x0D4BBBF4, BIFunc_AddPlayerAmmo},
    {0xB51DF512, BIFunc_PlayerEjectShell},
    {0x3E80717F, BIFunc_DisablePlayerShoot},
    {0x6BD753B6, BIFunc_SetPlayerCollision},
    {0xAC5252F0, BIFunc_SetWeaponDraw},
    {0x5281731A, BIFunc_PlayerIsMyWeaponAProjectile},
    {0x9839FEDC, BIFunc_PlayerFireMyWeapon},
    {0x1A4E1A2B, BIFunc_PlayerStartReloading},
    {0xC346E9E3, BIFunc_PlayerReloadSingle},
    {0xABF4BA02, BIFunc_PlayerClearReloading},
    {0x46945630, BIFunc_PlayerFinishedReloading},
    {0xDEF42097, BIFunc_PlayerSwitchWeapons},
    {0x957B67EE, BIFunc_PlayerDeath},
    {0x13E17EF7, BIFunc_PlayerGetWeaponRefireRate},
    {0xE92C0317, BIFunc_PlayerAddHitDirection},
    {0xE7CCE2F1, BIFunc_PlayerCanSwitchWeapon},
    {0x7D6A0760, BIFunc_PlayerCanCookGrenades},
    {0x20DCBAF5, BIFunc_PlayerHasMeleeWeapon},
    {0x111337AF, BIFunc_IsPlayerAiming},
    {0x1B0A9E98, BIFunc_IsPlayerDisguised},
    {0x8C8A2096, BIFunc_IsPlayerCrouching},
    {0x72A819B5, BIFunc_IsPlayerInvincible},
    {0xAD078995, BIFunc_SetPlayerWielding},
    {0x691EE6C9, BIFunc_SetPlayerFallingDamage},
    {0xA8F1CBD7, BIFunc_PlayerRespawn},
    {0x355829BA, BIFunc_GetGenericSoundStructureMemory},
    {0xA638F607, BIFunc_GetSoundInfo},
    {0xEA6C53A1, BIFunc_PlaySound},
    {0x4D5AA397, BIFunc_EndSound},
    {0x2637782C, BIFunc_PlaySoundAtOffset},
    {0xD36CD3FE, BIFunc_PlaySoundFromInputTrigger},
    {0x36A906E2, BIFunc_SEDEvent},
    {0xEB839E89, BIFunc_AEMSEvent},
    {0xD941EFAC, BIFunc_PathfinderSetLevel},
    {0xC0D84AD7, BIFunc_PathfinderEvent},
    {0xC3BA7E0E, BIFunc_PathfinderFadeVolume},
    {0x58E8F73C, BIFunc_PathfinderSetLatency},
    {0xF9A52B55, BIFunc_MakeNoise},
    {0x2842FAF8, BIFunc_MakeNoiseFromInputTrigger},
    {0x86004007, BIFunc_GetVoiceHandle},
    {0x8CE20CDD, BIFunc_StopCurrentVoice},
    {0xD112DFAB, BIFunc_CreateBuddyDataStructures},
    {0x8B49B6FE, BIFunc_AddBuddy},
    {0x8B2BD60E, BIFunc_GetInitialBuddyOrders},
    {0x8DA731CB, BIFunc_BuddyDied},
    {0x96690017, BIFunc_UpdateBuddyProgress},
    {0xA9E23D9B, BIFunc_UpdateBuddyCheckpoints},
    {0xDB65B79B, BIFunc_SetBuddyPathGroup},
    {0xB8BDDEDF, BIFunc_DebugTTYPrint},
    {0x4D412CD6, BIFunc_ScriptError},
    {0xAD26B30A, BIFunc_DoSomethingInteresting},
    {0x452C5879, BIFunc_BreakPoint},
    {0x32952724, BIFunc_PlayerStartCameraShake},
    {0x4A90F4BF, BIFunc_PlayerStopCameraShake},
    {0x7D1760B3, BIFunc_PlayerSuperAimMode},
    {0x3E612CCA, BIFunc_PlayerCanCrouch},
    {0x297BF47F, BIFunc_PlayerMotionShake},
    {0x94CAA3F6, BIFunc_PlayerScreenFlash},
    {0x01AECD70, BIFunc_PlayerStartPath},
    {0x8B2ED074, BIFunc_PlayerStopPath},
    {0x1F77E7C9, BIFunc_PlayerStartGrenadeCook},
    {0xFEDFE1C9, BIFunc_DisplayPromptText},
    {0x0C13B920, BIFunc_DisplayPromptInteger},
    {0x735D53E8, BIFunc_AISendFlag},
    {0xA9BDA7AA, BIFunc_AISetStatusVar},
    {0x3464B177, BIFunc_LoadOneShotSoundBank},
    {0x639712E4, BIFunc_GetWeaponInfo},
    {0x44972E0B, BIFunc_AttachToProp},
    {0xAC1A7982, BIFunc_DetachFromProp},
    {0xD1716AAB, BIFunc_AttachPlayerToProp},
    {0x71B4AF7B, BIFunc_DetachPlayerFromProp},
    {0xF723B56B, BIFunc_AttachToSoldier},
    {0x7F34A182, BIFunc_DetachFromSoldier},
    {0xBC9BCD2D, BIFunc_AttachToWaypoint},
    {0x65C580E5, BIFunc_DetachFromWaypoint},
    {0xC70C360B, BIFunc_AttachToSoldierEmptyHand},
    {0xE6D2E978, BIFunc_GetPropById},
    {0xEBBCB029, BIFunc_AmbientTrack_Select},
    {0x19D49737, BIFunc_AmbientTrack_Volume},
    {0xFF624707, BIFunc_AmbientTrack_Azimuth},
    {0xCA318634, BIFunc_AEMSSetMasterVolume},
    {0x52DFFFCE, BIFunc_SetupGrenadeShot},
    {0x4EBF673D, BIFunc_CatchGrenade},
    {0xD1AA7E61, BIFunc_GetGrenadeTimer},
    {0x1FB4844D, BIFunc_SetGrenadeTimer},
    {0xAFA4973F, BIFunc_GiveEnemyGrenadeWeapon},
    {0x09BF42DE, BIFunc_SetMood},
    {0xEBB65033, BIFunc_SetExpression},
    {0xACE69EEE, BIFunc_SetBlinkState},
    {0xDBED8612, BIFunc_SetJawMotion},
    {0xCC745189, BIFunc_SetMuzzleFireForPlayer},
    {0xD5C54BC3, BIFunc_UpdateFragCount},
    {0x0C31D8C4, BIFunc_UpdatePlayerStats},
    {0x5BDB25C8, BIFunc_HierObjectPlayAnimation},
    {0x548FB1CD, BIFunc_StaticObjectPlayAnimation},
    {0xF3DCCBC9, BIFunc_StartMotionPlayback},
    {0xD413FA51, BIFunc_StopMotionPlayback},
    {0x2F3EAC00, BIFunc_EndLevel},
    {0xE76F6E9D, BIFunc_AttachPlayerToMountedMachineGun},
    {0xA2F7EB85, BIFunc_SetGrenadeSlot},
    {0x9652E0D5, BIFunc_GetGrenadeHoldTime},
    {0x87EF51F3, BIFunc_StartGrenadeHold},
    {0xD42BC517, BIFunc_StopGrenadeHold},
    {0xA2FCCDEB, BIFunc_CreateExplosion},
    {0xEFEC1DEA, BIFunc_CreateImpact},
    {0x53CB9A6D, BIFunc_GetBSObject},
    {0xA2A416F9, BIFunc_ApplyForceToObject},
    {0xAB822024, BIFunc_MechFollowPath},
    {0x2A86C58A, BIFunc_MechSetPathFlag},
    {0x290A77F0, BIFunc_MovingMechForceUpdateTrigger},
    {0x55B117B6, BIFunc_MechSetFlag},
    {0x6E063060, BIFunc_DebugInitFromTrigger},
    {0xBF10D8E0, BIFunc_PlayVibrationEvent},
    {0xCABD719F, BIFunc_BlockNewVibrations},
    {0xAF06E673, BIFunc_FadeOut},
    {0x960E13BC, BIFunc_AttachObjectToFloatingBone},
    {0xF7F6D2F2, BIFunc_DetachObjectFromFloatingBone},
    {0x5436D716, BIFunc_EnableFloatingBone},
    {0x5453739B, BIFunc_DisableFloatingBone},
    {0x2A3733B8, BIFunc_SetDetonationTime},
    {0x86D203B7, BIFunc_SetSingleAnimEndsEventDelivery},
    {0x35BD54E1, BIFunc_SetFogParams},
    {0x4037D02C, BIFunc_SetDefaultFogParams},
    {0xBB77CE51, BIFunc_SetHeadTrackPlayer},
    {0xC1A14EC2, BIFunc_SetMGWaypointFlag},
    {0x070E747A, BIFunc_PlayerForceToWeapon},
    {0xDB912483, BIFunc_RegisterAlarm},
    {0x340A992E, BIFunc_UnregisterAlarm},
    {0x94777A7F, BIFunc_GetClosestAlarm},
    {0x006D68E1, BIFunc_AlarmIsAvailable},
    {0xD7FF37A1, BIFunc_SetAlarmOccupy},
    {0x094CE95B, BIFunc_IsAlarmActivated},
    {0x09B6286C, BIFunc_SetAlarmActivated},
    {0x9A1D5549, BIFunc_EjectHelmet},
    {0xCB5CE5BD, BIFunc_SetInShadow},
    {0xEF59BF71, BIFunc_ResetMMG},
    {0xF5DB6B1C, BIFunc_ModifyTotalKillableEnemyCount},
    {0x8F0DE4A2, BIFunc_ChangePlayersTeam},
    {0xEFFB33A0, BIFunc_TogglePlayerVsTriggerChecks},
    {0x0FB928A0, BIFunc_MPCreateWeaponByWeaponSetID},
    {0xFDA7339B, BIFunc_MPCreateAmmoByWeaponSetID},
    {0x41F750E0, BIFunc_MPCreateWeaponByType},
    {0xCA02A7B6, BIFunc_MPCreateAmmoByType},
    {0xE0C8E4FD, BIFunc_MPGetWeaponPickupPromptFromWeaponSetID},
    {0x22AA672F, BIFunc_MPGetAmmoPickupPromptFromWeaponSetID},
    {0x12283241, BIFunc_MPGetWeaponTypeFromWeaponSetID},
    {0xC4123C7D, BIFunc_MPAddPlayerAmmo},
    {0x0D18E7CB, BIFunc_MPAddPlayerHealth},
    {0x65831E9C, BIFunc_MPGetPlayerBody},
    {0x88E4E169, BIFunc_MPGetPlayerVelocity},
};
