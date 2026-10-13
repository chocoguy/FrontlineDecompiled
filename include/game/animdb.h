// CAnimDatabase: the loaded animation files, grouped by object type (two
// types at most). Hand-maintained.
#ifndef _GAME_ANIMDB_H_
#define _GAME_ANIMDB_H_

#include "types.h"
#include "game_types.h"

struct AnimFileFormat_t;
struct AnimFileStateAnimList_t;
struct AnimTranTransListArray_t;
struct CAnimDebugFile;

#define ANIMDB_NUM_GROUPS 2
#define ANIMDB_FILES_PER_GROUP 20

// size 0x58
struct CAnimDatabaseGroup {
    /* 0x0 */ u16 objectType;  // 0xFFFF when unused
    /* 0x4 */ u32 numFiles;
    /* 0x8 */ AnimFileFormat_t* files[ANIMDB_FILES_PER_GROUP];
};

// size 0xB0
class CAnimDatabase {
public:
    /* 0x00 */ CAnimDatabaseGroup m_groups[ANIMDB_NUM_GROUPS];

    CAnimDatabase();  // 0x800621D0
    ~CAnimDatabase();  // 0x80062194
    void Init();  // 0x800620CC
    void AddFile(AnimFileFormat_t* file, CAnimDebugFile* debug);  // 0x80061FC0
    s32 GetStateCallbackID(unsigned long state, unsigned short objectType);  // 0x80061F04
    void* GetStateInfo(unsigned long state, unsigned short objectType);  // 0x80061E48
    void GetStateAnimList(unsigned long objectType, unsigned long state,
                          AnimFileStateAnimList_t* list);  // 0x80061D98
    void GetTransList(unsigned long objectType, unsigned long state,
                      AnimTranTransListArray_t* list);  // 0x80061CE4
    AnimFileFormat_t* GetFileFromFileNum(unsigned long objectType, unsigned short fileNum);  // 0x80061C44

private:
    CAnimDatabaseGroup* FindGroup(unsigned long objectType);
};

extern void* g_ARAM_poolman;  // 0x8034E930
extern void* g_ARAM_pool;  // 0x8034E934

#endif
