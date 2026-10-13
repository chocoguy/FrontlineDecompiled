#ifndef _GAME_MAINLOOP_H_
#define _GAME_MAINLOOP_H_

#include "types.h"
#include "game_types.h"
#include "game/AnimObjFactory.h"
#include "game/bulletfactory.h"
#include "game/decal.h"
#include "game/weaponfactory.h"
#include "game/world_object.h"

class CPlayerObject;
class CSprite;

// Loading progress callback: (unused, loading screen sprite, 0..1).
typedef void (*GameloopProgressFn)(CSprite*, CSprite*, float);

void Gameloop_Restart();  // 0x8007A5F8
void Gameloop_Loop(float);  // 0x8007A810
void Gameloop_Shutdown();  // 0x8007A9E0
void Gameloop_Init(GameloopProgressFn, CSprite*, CSprite*);  // 0x8007AAE8
void LoadLevelSettings();  // 0x8007B350

// Other code reaches these through g_AnimObjFactory at fixed offsets
// (players +0x1C, bullets +0x38, weapons +0x78, decals +0xA0): keep the
// definition order in mainloop.cpp.
extern CAnimObjectFactory g_AnimObjFactory;  // 0x801DDDE0
extern CPlayerObject* g_pPlayers[4];  // 0x801DDDFC
extern CBulletFactory g_BulletFactory;  // 0x801DDE18
extern CWeaponFactory g_WeaponFactory;  // 0x801DDE58
extern CBulletDecalManager g_BulletDecalManager;  // 0x801DDE80
extern char g_soundStreamFilename[0x100];  // 0x801DE44C
extern int g_soundStreamRequest;  // 0x8034DC80
extern CWorldObject::SAcquireSet g_idSet;  // 0x80184730
extern CWorldObject* g_pWorld;  // 0x8034E9DC
extern CBulletFactory* g_pBulletFactory;  // 0x8034E9E0
extern CWeaponFactory* g_pWeaponFactory;  // 0x8034E9E4
extern int g_mpTimeLimit;  // 0x8034E9E8 (in vsyncs, -1 = none)
extern int g_bMPEndLevel;  // 0x8034E9EC

#endif
