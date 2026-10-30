// Native build only: Rewritten keeps the bodies of the dead, implemented in
// FrontlineRewritten's game/native/corpses.cpp.
//
// Originally a dead soldier's script fades the body out (FadeOut) and then
// destroys the soldier, giving its slots back for the soldiers still to come.
// In Rewritten the body stays: the soldier is destroyed as before (the
// soldier count, its script, AI, weapons and volumes go back), but its
// animated object is kept and drawn where it fell, frozen, until it fades out
// a minute and a half after the soldier began to. The extra object slots the
// bodies hold are added to the factory at level load, so the living get as
// many as ever. When they run out, the oldest body out of view goes.
//
// Every hook but NativeCorpseSlots does nothing unless NativeCorpsesOn().
#ifndef _NATIVE_CORPSES_H_
#define _NATIVE_CORPSES_H_

#ifdef FRONTLINE_NATIVE

class CDrawContext;
class CSoldierObject;

// Whether bodies stay this level: Rewritten, single player, and not turned
// off with FRONTLINE_CORPSES=off. Resolved by NativeCorpsesLevelStart.
extern bool g_nativeCorpsesOn;
inline bool NativeCorpsesOn() { return g_nativeCorpsesOn; }

// At level load, before the object factory is made.
void NativeCorpsesLevelStart(bool multiplayer);

// How many more animated object slots the factory makes for the bodies.
int NativeCorpseSlots();

// CSoldierObject::FadeOut: the body holds instead of fading (the bodies'
// update fades it later).
void NativeCorpseFadeOut(CSoldierObject* soldier);

// BSGO_Soldier::Destroy: keeps the soldier as a body if it was dying (its
// script had called FadeOut). Then the caller skips the soldier's CleanUp
// and ReleaseObject, which the body does its share of here.
bool NativeCorpseKeep(CSoldierObject* soldier);

// CScene::Update: ages the bodies (`dt` in frames), fades out the old ones
// and releases them once faded.
void NativeCorpsesUpdate(float dt);

// CScene::Draw, after the scene's nodes: draws the bodies in view.
void NativeCorpsesDraw(CDrawContext& ctx);

// The level restarts or ends: forgets the bodies (before the factory's
// Reset or Shutdown takes their slots back).
void NativeCorpsesReset();

#endif

#endif
