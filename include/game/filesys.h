// Skeleton for game/filesys (from tools/gen_headers.py, now hand-maintained).
// Minimal disc file access on top of the Dolphin DVD API (read-only).
#ifndef _GAME_FILESYS_H_
#define _GAME_FILESYS_H_

#include "types.h"
#include "game_types.h"

int File_Seek(int handle, int offset, ESeekOrigin origin);  // 0x8001C60C (unsupported)
int File_Write(int handle, void* data, int size);           // 0x8001C614 (unsupported)
int File_Close(int handle);                                 // 0x8001C61C
int File_Open(const char* path, EOpenMode mode);            // 0x8001C670

#endif
