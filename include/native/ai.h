// Native build only: Rewritten's enhanced enemy AI, implemented in
// FrontlineRewritten's game/native/ai/. The soldiers' decisions stay in their
// scripts (SoldierA_Roamer and its subclasses); these hooks change the
// engine services the scripts call: aim, perception, cover choice, grenades,
// and a squad director that shares what soldiers see.
//
// Every hook is called only when NativeAIOn(), so Classic (and Rewritten with
// FRONTLINE_AI=off) runs the original code.
#ifndef _NATIVE_AI_H_
#define _NATIVE_AI_H_

#ifdef FRONTLINE_NATIVE

#include "types.h"

class CAIObject;
class CAIFilterRealPosition;
class CAISoldierObject;
class CCoverPoint;
class CProjectileBullet;
class CThrownBullet;
class CVector3;
class ISceneNode;
struct _PropBSPLeaf;
struct BSObject;
struct CAIFilterSoundInfo;

// Whether the enhanced AI runs this level: Rewritten, single player, and not
// turned off with FRONTLINE_AI=off. Resolved by NativeAILevelStart.
extern bool g_nativeAIOn;
inline bool NativeAIOn() { return g_nativeAIOn; }

// At level load (before the scripts start): picks the profile from the
// difficulty (1 easy, 2 medium, 3 hard).
void NativeAILevelStart(int difficulty, bool multiplayer);

// The AI object list: an object was added (CAIObject::Init, after its
// one-time settings), removed, or the whole list was reset (level restart).
void NativeAIAdded(CAIObject* ai);
void NativeAIRemoved(CAIObject* ai);
void NativeAIReset();

// Each soldier's AI update, before it aims: refreshes whether the target is
// really visible and restarts the aim error after the target was hidden.
void NativeAISoldierUpdate(CAISoldierObject* ai);

// Once a frame, after every object's AI update: the squad director.
void NativeAIDirectorUpdate();

// UpdateTargetPosition, once the aim point is filtered: leads a moving
// target and adds the error of soldiers without an attack token. `dt` is the
// time since the last update (seconds), `dist` the distance to the target.
void NativeAIAdjustAim(CAIObject* ai, float dt, float dist);

// Whether a soldier should hold fire: a squadmate is in the line of fire.
bool NativeAIHoldFire(CAIObject* ai, const CVector3& from, const CVector3& to);

// CAISoldierObject::ChooseTarget: whether the current target keeps its
// preference (not when it is dead or has been out of sight for a while).
bool NativeAIKeepTarget(CAIObject* ai, CAIObject* target);

// Reaction delays (in frames) of target searches and heard sounds, scaled.
int NativeAIReactionFrames(int frames);

// The radius of the "the player is right there" check (5 originally).
float NativeAIProximityRange();

// How many registrations a perception schedule polls at once (5 originally).
int NativeAIScheduleBatch(int original);

// Cover: picks a cover point of `type` against `threat` by score (replaces
// CAIFilterGlobal::ChooseCoverPointByType); the peek side to claim at a
// cover point and where to peek from; whether the target has been able to
// see the soldier at its cover for a while (time to move).
CCoverPoint* NativeAIChooseCover(const CAIFilterRealPosition& pos, const CAIFilterRealPosition* threat, int type,
                                 float maxDistSq, float maxThreatDistSq, CAIObject* ai);
int NativeAIChoosePeek(CAIObject* ai, CCoverPoint* cover, int original);
CAIFilterRealPosition NativeAIPeekLocation(CCoverPoint* cover, int type);
bool NativeAICoverExposed(CAIObject* ai, const CAIFilterRealPosition& spot);

// A path node position to run to, at least `dist` from `threat` (replaces
// the greedy walk of FindRunAwayPoint). False if there is none.
bool NativeAIFindRunAwayPoint(CAIObject* ai, const ISceneNode* threat, float dist, CAIFilterRealPosition* out);

// Grenades: the launch speed for a throw at the aim point, or -1 (replaces
// CAIObject::TestGrenadeShot); the launch slope (tan of the angle) the
// soldier's next grenade leaves at (0.577, 30 degrees, originally).
float NativeAITestGrenadeShot(CAIObject* ai);
float NativeAIGrenadeLaunchTan(ISceneNode* thrower);

// A thrown grenade's update: soldiers near where it will land are warned
// early. NativeAIGrenadeWarned tells CheckEnemies to skip the original
// warning once it comes to rest.
void NativeAIWatchGrenade(CThrownBullet* grenade);
bool NativeAIGrenadeWarned(CThrownBullet* grenade);

// A projectile's update: soldiers it passes close to hear where it came from.
void NativeAIBulletFlying(CProjectileBullet* bullet, float dt);

// Script decision bias: a soldier script's GetRandomInteger result or
// RegisterTimerEvent delay, adjusted by profile at the call sites listed in
// game/native/ai/script_bias.cpp (found with tools/bsdis.py). Other calls
// keep their value.
int NativeAIScriptRandom(int lo, int hi, int value);
int NativeAIScriptTimer(int delay, int event);

// Rewritten (with or without the enhanced AI): an enemy's drop appears as it
// dies. AISetStatusVar marking a soldier dead calls NativeEnemyDied, which
// makes the drop at the soldier's death states that end in one
// (game/native/ai/drops.cpp); CreateDumpObject then skips it
// (NativeEnemyDropDone returns true once for that soldier).
void NativeEnemyDied(ISceneNode* self);
bool NativeEnemyDropDone(ISceneNode* self);

// Defined in bsmachin.cpp: the instruction (BIFNCALL) the running built-in
// was called from.
u32* NativeBSInstruction();

// Defined in AIObject.cpp: tells one object about a sound (as hearing it
// would, with the same event data), after `delay` frames.
void NativeAITellHeard(CAIObject* obj, const CAIFilterSoundInfo& info, int delay);

#endif

#endif
