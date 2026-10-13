// Skeleton for game/toilet (from tools/gen_headers.py, now hand-maintained).
//
// Engine side of "Toilet", Dreamworks Interactive's level data tool: loads a
// level's BIG archives (level.viv, comp.viv) and its contents table
// (levelfile.lfc), and finds resources in that table by type.
#ifndef _GAME_TOILET_H_
#define _GAME_TOILET_H_

#include "types.h"
#include "game_types.h"

// Version and checksum of the contents table format the engine understands.
#define TLT_CONTENTS_VERSION 1
#define TLT_CONTENTS_CHECKSUM 0x858CD

// Resource types (TLTResourceID values)
#define TLT_RESOURCE_COMPARTMENT ((TLTResourceID)0)

// A field of a resource. Kinds 0 and 4 hold offsets into the contents file,
// which are turned into pointers when the file is loaded.
struct TLTResourceField {
    /* 0x0 */ int kind;
    /* 0x4 */ int value;
};

// One resource in the level contents table (0xC bytes).
struct LevelFileContentsStruct_ {
    /* 0x0 */ TLTResourceID type;
    /* 0x4 */ int numFields;
    /* 0x8 */ TLTResourceField* fields;
};

// Header of levelfile.lfc. Every word is byte-swapped on load.
struct TLTLevelContentsHeader {
    /* 0x0 */ int version;
    /* 0x4 */ int checksum;
    /* 0x8 */ int numResources;
    /* 0xC */ u32 resources;  // offset, then pointer to LevelFileContentsStruct_[]
};

extern void* g_pLevelBigFile;
extern void* g_pShellBriefingLogBigFile;
extern int g_LevelBigFileSize;
extern int g_ShellBriefingLogBigFileSize;

void* TLT_LoadFromRepository(const char* path, int* size, bool* alreadyLoaded);  // 0x8001CCA8
void TLT_LoadLevelContentsFile(int mission, int level);                          // 0x8001CEE4
void* TLT_LoadFileNormal(const char* path, int* size, int flags);                // 0x8001D308
void TLT_InitResourceType(void (*init)(LevelFileContentsStruct_*), TLTResourceID type);  // 0x8001D394
int GetCompartmentSizeFromIndex(int index);                                      // 0x8001D4A4
void GetCompartmentCPTNameFromIndex(int index, char* out, int size);            // 0x8001D674
void GetCompartmentCDBNameFromIndex(int index, char* out, int size);            // 0x8001D74C
void TLT_CloseFile(void* data);                                                 // 0x8001D820
LevelFileContentsStruct_* TLT_FindNextResourceByTypeAndVal(TLTResourceID type, int field, int value,
                                                           LevelFileContentsStruct_* after);  // 0x8001D878
LevelFileContentsStruct_* TLT_FindNextResourceByType(TLTResourceID type,
                                                     LevelFileContentsStruct_* after);  // 0x8001D910
void TLT_UnLoadShellBriefingLogBigFile();                                       // 0x8001D978
void TLT_LoadShellBriefingLogBigFile(int mission, int level);                    // 0x8001D9B0
void* TLT_LoadFileFromShellBriefingLogBigFile(const char* path, int* size);       // 0x8001DA40
void* TLT_LoadFileFromSlidesBigFile(const char* path, int* size);                 // 0x8001DB10
void* TLT_LoadFileFromShellBigFile(const char* path, int* size);                  // 0x8001DBE4
void* TLT_LoadFileFromLevelBigFile(const char* path, int* size);                  // 0x8001DCB4
void TLT_Shutdown();                                                            // 0x8001DDE8
void TLT_Init(int mission, int level);                                          // 0x8001DE64

#endif
