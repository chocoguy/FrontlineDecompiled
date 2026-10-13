// From tools/gen_headers.py, now hand-maintained.
// Tables exported by IStudio (the UI tool) for the shell and pause screens:
// which image libraries and screens each UI uses.
#ifndef _GAME_ISEXPORTDEFS_H_
#define _GAME_ISEXPORTDEFS_H_

#include "types.h"
#include "game_types.h"

class CSprite;

// An image library: file names and the sprites loaded from them.
struct IStudioLibrary {
    /* 0x0 */ int numImages;
    /* 0x4 */ char** imageNames;
    /* 0x8 */ CSprite** images;
};

// A group of screen definition files.
struct IStudioScreens {
    /* 0x0 */ int numScreens;
    /* 0x4 */ char** screenNames;
};

// Set by ShellIs*/PauseIs* to select the active UI.
extern IStudioScreens* ScreenInfo;
extern IStudioLibrary* LibraryInfo;

void NullifyScreenAndLibrary();  // 0x8008B00C (isexportdefs.cpp)
void ShellIsScreen();            // 0x8008B000 (isShellGroup.cpp)
void ShellIsLibrary();           // 0x8008AFF0 (isShellLibrary.cpp)
void PauseIsScreen();            // 0x8008AFE4 (ispausegroup.cpp)
void PauseIsLibrary();           // 0x8008AFD4 (ispauselibrary.cpp)

#endif
