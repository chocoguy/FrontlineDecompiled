#include "game/isexportdefs.h"

IStudioScreens* ScreenInfo;
IStudioLibrary* LibraryInfo;

void NullifyScreenAndLibrary() {
    ScreenInfo = NULL;
    LibraryInfo = NULL;
}
