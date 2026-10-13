// Game-side callbacks for the IStudio UI runtime: transforms, screen
// resources and the shell BIG file. Hand-maintained.
#ifndef _GAME_ISTUDIOFUNCS_H_
#define _GAME_ISTUDIOFUNCS_H_

#include "types.h"
#include "game/uistypes.h"

struct UISParam_t;

void DummyPluginProcessFnc(void*, unsigned long, unsigned long, UISParam_t*, UISParam_t*);  // 0x8008BCD0
void LoadShellBigFile();  // 0x8008BC90
void UnloadShellBigFile();  // 0x8008BC5C
void* ResLoadScreen(unsigned short group, unsigned short screen);  // 0x8008BC10
void ResUnloadScreen(unsigned short group, unsigned short screen, void* data);  // 0x8008BBE4
void iSirenTransformfnc(UISTransformAction action, UISTransform_t* xf);  // 0x8008B914

// Scale of the transform applied last.
extern float g_fScaleValueX;  // 0x8034EAA0
extern float g_fScaleValueY;  // 0x8034EAA4
extern float g_fScaleValueZ;  // 0x8034EAA8

#endif
