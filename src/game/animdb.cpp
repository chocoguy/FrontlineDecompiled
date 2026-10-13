#include "game/animdb.h"

#include <string.h>
#include <dolphin/ar.h>
#include "ea/sys/aramalloc.h"
#include "game/animemot.h"
#include "game/animfile.h"

void* g_ARAM_poolman;
void* g_ARAM_pool;
u32 g_ARAM_stack[2];

CAnimDatabase::CAnimDatabase() {}

CAnimDatabase::~CAnimDatabase() {}

void CAnimDatabase::Init() {
    for (u32 i = 0; i < ANIMDB_NUM_GROUPS; i++) {
        m_groups[i].objectType = 0xFFFF;
        m_groups[i].numFiles = 0;
        memset(m_groups[i].files, 0, sizeof(m_groups[i].files));
    }
    if (!ARCheckInit()) {
        ARInit(g_ARAM_stack, 2);
        ARQInit();
    }
    g_ARAM_poolman = ARAM_NEW_poolmanager(0x20, 10, 0x20, "MOHFL ARAM POOL MANAGER");
    g_ARAM_pool = ARAM_NEW_pool((ARAM_POOL_MANAGER*)g_ARAM_poolman, 0x800000, 0x800000, "Animation Pool");
}

// Note: with both groups taken by other types, the file goes into a third,
// out-of-range group (as in the original).
void CAnimDatabase::AddFile(AnimFileFormat_t* file, CAnimDebugFile*) {
    AnimFileFormat_t* copy = CopyAnimFileFormatStructureToDynMem(file);
    UploadFramesToARAM(copy, *file->frames, (u8*)file->frames);
    u16 type = AnimFileGetObjectType(copy);
    int g = 0;
    if (type != m_groups[0].objectType) {
        if (m_groups[0].objectType == 0xFFFF) {
            m_groups[g].objectType = type;
        } else {
            g = 1;
            if (type != m_groups[1].objectType) {
                if (m_groups[1].objectType == 0xFFFF) {
                    m_groups[g].objectType = type;
                } else {
                    g = 2;
                }
            }
        }
    }
    CAnimDatabaseGroup* group = &m_groups[g];
    u32 slot = group->numFiles;
    for (u32 i = 0; i < group->numFiles; i++) {
        if (group->files[i] == copy) {
            return;
        }
        if (group->files[i] == NULL) {
            slot = i;
        }
    }
    group->files[slot] = copy;
    if (slot == group->numFiles) {
        group->numFiles++;
    }
}

inline CAnimDatabaseGroup* CAnimDatabase::FindGroup(unsigned long objectType) {
    CAnimDatabaseGroup* group = NULL;
    for (int i = 0; i < ANIMDB_NUM_GROUPS; i++) {
        if (objectType == m_groups[i].objectType) {
            group = &m_groups[i];
            break;
        }
    }
    return group;
}

s32 CAnimDatabase::GetStateCallbackID(unsigned long state, unsigned short objectType) {
    s32 id = -1;
    CAnimDatabaseGroup* group = FindGroup(objectType);
    if (group == NULL) {
        return 0;
    }
    for (u32 i = 0; i < group->numFiles; i++) {
        if (group->files[i] != NULL) {
            s32 r = AnimFileGetStateCallbackID(group->files[i], state);
            if (r != -1) {
                id = r;
            }
        }
    }
    return id;
}

void* CAnimDatabase::GetStateInfo(unsigned long state, unsigned short objectType) {
    void* info = NULL;
    CAnimDatabaseGroup* group = FindGroup(objectType);
    if (group == NULL) {
        return NULL;
    }
    for (u32 i = 0; i < group->numFiles; i++) {
        if (group->files[i] != NULL) {
            void* r = AnimFileGetStateInfo(group->files[i], state);
            if (r != NULL) {
                info = r;
            }
        }
    }
    return info;
}

void CAnimDatabase::GetStateAnimList(unsigned long objectType, unsigned long state,
                                     AnimFileStateAnimList_t* list) {
    list->count = 0;
    list->capacity = 0xFA;
    CAnimDatabaseGroup* group = FindGroup(objectType);
    if (group != NULL) {
        for (u32 i = 0; i < group->numFiles; i++) {
            if (group->files[i] != NULL) {
                AnimFileGetStateAnimList(group->files[i], state, list);
            }
        }
    }
}

void CAnimDatabase::GetTransList(unsigned long objectType, unsigned long state,
                                 AnimTranTransListArray_t* list) {
    list->count = 0;
    list->capacity = 8;
    list->state = state;
    CAnimDatabaseGroup* group = FindGroup(objectType);
    if (group != NULL) {
        for (u32 i = 0; i < group->numFiles; i++) {
            if (group->files[i] != NULL) {
                AnimFileGetStateTransList(group->files[i], state, list);
            }
        }
    }
}

AnimFileFormat_t* CAnimDatabase::GetFileFromFileNum(unsigned long objectType,
                                                    unsigned short fileNum) {
    CAnimDatabaseGroup* group = FindGroup(objectType);
    if (group == NULL) {
        return NULL;
    }
    for (u32 i = 0; i < group->numFiles; i++) {
        if (group->files[i] != NULL && group->files[i]->fileNum == fileNum) {
            return group->files[i];
        }
    }
    return NULL;
}
