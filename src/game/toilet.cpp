#include "game/toilet.h"
#include "game/endian.h"

#include <stdio.h>
#include <string.h>
#include "dwi/hash_map.h"
#include "ea/real.h"
#include "game/memory.h"
#include "game/Moh2.h"

// Functions are defined in the reverse of their order in the binary: the
// compiler emits them backwards, and callers can only inline what precedes them.

static char* szTLT_LevelContentsFileName = "levelfile.lfc";
static char* szTLT_LevelBigFileName = "level.viv";
static char* szTLT_CompartmentBigFileName = "comp.viv";
static char* szTLT_SlidesBigFileName = "slides.viv";

void* g_pLevelBigFile;
void* g_pShellBriefingLogBigFile;
int g_LevelBigFileSize;
int g_ShellBriefingLogBigFileSize;
static void* g_pCompartmentBigFileHeader;
static int g_CompartmentBigFile;
static LevelFileContentsStruct_* g_pLevelFileContentsArray;
static int g_pNumLevelFileContents;
static LevelFileContentsStruct_* g_pLevelFileContentsEndMarker;

// Files loaded through TLT_LoadFromRepository, by address.
static dwi::hash_map<char*, char*> g_Repository;

// Strip a "device:" prefix, which archives don't use.
inline const char* StripDevice(const char* path) {
    const char* colon = strchr(path, ':');
    if (colon != NULL) {
        path = colon + 1;
        DebugMsg("WARNING: When loading from a big file, strip off the %s part.\n"
                 "Since it's a DEBUG build, I'll do it for you\n",
                 "");
    }
    return path;
}

// File name without directories ('\' separated).
inline const char* StripDirectories(const char* path) {
    const char* name = path;
    const char* sep = strchr(path, '\\');
    while (sep != NULL) {
        name = sep + 1;
        sep = strchr(name, '\\');
    }
    return name;
}

inline void* LoadFromBig(const void* big, const char* name, int* size) {
    int offset;
    if (big != NULL && BIG_locateentryz(big, name, 0, &offset, size)) {
        return BIG_locate(big, name);
    }
    return NULL;
}

void TLT_Init(int mission, int level) {
    char compPath[128];
    char levelPath[128];
    char loadPath[256];

    if (g_pLevelBigFile != NULL) {
        MEM_free(g_pLevelBigFile);
    }
    sprintf(levelPath, "data/%d/%d_%d/%s", mission, mission, level, szTLT_LevelBigFileName);
    g_pLevelBigFile = TLT_LoadFileNormal(levelPath, &g_LevelBigFileSize, 0);
    TLT_LoadLevelContentsFile(mission, level);

    if (g_CompartmentBigFile != 0) {
        FILESYS_delbigsync(g_CompartmentBigFile, 100);
    }
    sprintf(compPath, "data/%d/%d_%d/%s", mission, mission, level, szTLT_CompartmentBigFileName);
    FILESYS_addbigsync(compPath, 0x400, 100, &g_CompartmentBigFile);
    g_pCompartmentBigFileHeader = FILE_loadbigheader(compPath, 0x400);

    g_Repository.reserve(BIG_count(g_pLevelBigFile) + 1);
}

void TLT_Shutdown() {
    if (g_pLevelBigFile != NULL) {
        MEM_free(g_pLevelBigFile);
        g_pLevelBigFile = NULL;
    }
    if (g_pCompartmentBigFileHeader != NULL) {
        MEM_free(g_pCompartmentBigFileHeader);
        g_pCompartmentBigFileHeader = NULL;
    }
    if (g_CompartmentBigFile != 0) {
        FILESYS_delbigsync(g_CompartmentBigFile, 100);
        g_CompartmentBigFile = 0;
    }
    g_pLevelFileContentsArray = NULL;
    g_pLevelFileContentsEndMarker = NULL;
    g_pNumLevelFileContents = 0;
    g_LevelBigFileSize = 0;
}

void* TLT_LoadFileNormal(const char* path, int* size, int flags) {
    char buf[256];
    const char* colon = strchr(path, ':');

    sprintf(buf, "%s", colon != NULL ? colon + 1 : path);
    if (size != NULL) {
        *size = FILE_sizez(buf);
    }
    return FILE_loadz(buf, flags | 0x400);
}

// Level archive first, then the disc.
void* TLT_LoadFileFromLevelBigFile(const char* path, int* size) {
    const char* file = StripDevice(path);
    void* data = LoadFromBig(g_pLevelBigFile, StripDirectories(file), size);

    if (data == NULL) {
        data = TLT_LoadFileNormal(file, size, 0);
    }
    return data;
}

void* TLT_LoadFileFromShellBigFile(const char* path, int* size) {
    return LoadFromBig(g_pLevelBigFile, StripDirectories(StripDevice(path)), size);
}

void* TLT_LoadFileFromSlidesBigFile(const char* path, int* size) {
    char bigPath[256];
    int handle;
    void* data;
    const char* file = StripDevice(path);

    sprintf(bigPath, "data/shell/%s", szTLT_SlidesBigFileName);
    FILESYS_addbigsync(bigPath, 0x400, 100, &handle);
    g_pCompartmentBigFileHeader = FILE_loadbigheader(bigPath, 0x400);
    data = FILE_loadsize(file, size, 0x400);
    FILESYS_delbigsync(handle, 100);
    MEM_free(g_pCompartmentBigFileHeader);
    g_pCompartmentBigFileHeader = NULL;
    return data;
}

void* TLT_LoadFileFromShellBriefingLogBigFile(const char* path, int* size) {
    return LoadFromBig(g_pShellBriefingLogBigFile, StripDirectories(StripDevice(path)), size);
}

void TLT_LoadShellBriefingLogBigFile(int mission, int level) {
    char path[128];

    sprintf(path, "data/shell/%d_%d_blog.viv", mission, level);
    g_pShellBriefingLogBigFile = TLT_LoadFileNormal(path, &g_ShellBriefingLogBigFileSize, 0);
}

void TLT_UnLoadShellBriefingLogBigFile() {
    if (g_pShellBriefingLogBigFile != NULL) {
        MEM_free(g_pShellBriefingLogBigFile);
        g_pShellBriefingLogBigFile = NULL;
        g_ShellBriefingLogBigFileSize = 0;
    }
}

LevelFileContentsStruct_* TLT_FindNextResourceByType(TLTResourceID type, LevelFileContentsStruct_* after) {
    LevelFileContentsStruct_* res;

    if (after >= g_pLevelFileContentsEndMarker) {
        return NULL;
    }
    for (res = after != NULL ? after + 1 : g_pLevelFileContentsArray; res < g_pLevelFileContentsEndMarker;
         res++) {
        if (res->type == type) {
            return res;
        }
    }
    return NULL;
}

// Note: `after` is ignored; the search always starts from the first resource.
LevelFileContentsStruct_* TLT_FindNextResourceByTypeAndVal(TLTResourceID type, int field, int value,
                                                           LevelFileContentsStruct_*) {
    LevelFileContentsStruct_* res = NULL;

    for (;;) {
        res = TLT_FindNextResourceByType(type, res);
        if (res == NULL) {
            return NULL;
        }
        if (value == res->fields[field].value) {
            return res;
        }
    }
}

// Frees `data` unless it lives inside one of the loaded archives.
void TLT_CloseFile(void* data) {
    if ((data < g_pLevelBigFile || data >= (u8*)g_pLevelBigFile + g_LevelBigFileSize) &&
        (data < g_pShellBriefingLogBigFile ||
         data >= (u8*)g_pShellBriefingLogBigFile + g_ShellBriefingLogBigFileSize)) {
        MEM_free(data);
    }
}

// Each compartment has two resources: its collision data (field 1 == 2 * index)
// and its geometry (field 1 == 2 * index + 1); field 0 is the file name.
void GetCompartmentCDBNameFromIndex(int index, char* out, int) {
    LevelFileContentsStruct_* res =
        TLT_FindNextResourceByTypeAndVal(TLT_RESOURCE_COMPARTMENT, 1, index * 2, NULL);

    if (res == NULL) {
        *out = '\0';
        return;
    }
    sprintf(out, "%s", (char*)res->fields[0].value);
}

void GetCompartmentCPTNameFromIndex(int index, char* out, int) {
    LevelFileContentsStruct_* res =
        TLT_FindNextResourceByTypeAndVal(TLT_RESOURCE_COMPARTMENT, 1, index * 2 + 1, NULL);

    if (res == NULL) {
        *out = '\0';
        return;
    }
    sprintf(out, "%s", (char*)res->fields[0].value);
}

int GetCompartmentSizeFromIndex(int index) {
    LevelFileContentsStruct_* cdb;
    LevelFileContentsStruct_* cpt;
    const char* cptName;
    int offset;
    int cdbSize;
    int cptSize;

    if (g_pCompartmentBigFileHeader == NULL) {
        return -1;
    }
    cdb = TLT_FindNextResourceByTypeAndVal(TLT_RESOURCE_COMPARTMENT, 1, index * 2, NULL);
    if (cdb == NULL) {
        return -1;
    }
    cpt = TLT_FindNextResourceByTypeAndVal(TLT_RESOURCE_COMPARTMENT, 1, index * 2 + 1, NULL);
    if (cpt == NULL) {
        return -1;
    }
    cptName = (const char*)cpt->fields[0].value;
    if (!BIG_locateentryz(g_pCompartmentBigFileHeader, (const char*)cdb->fields[0].value, 0, &offset,
                          &cdbSize)) {
        return -1;
    }
    if (!BIG_locateentryz(g_pCompartmentBigFileHeader, cptName, 0, &offset, &cptSize)) {
        return -1;
    }
    return cdbSize + cptSize;
}

// Calls `init` for every resource of the given type.
void TLT_InitResourceType(void (*init)(LevelFileContentsStruct_*), TLTResourceID type) {
    LevelFileContentsStruct_* res = TLT_FindNextResourceByType(type, NULL);

    while (res != NULL) {
        init(res);
        res = TLT_FindNextResourceByType(type, res);
    }
}

void TLT_LoadLevelContentsFile(int, int) {
    char path[256];
    int size;
    TLTLevelContentsHeader* header;
    LevelFileContentsStruct_* res;
    int i;
    int j;

    strcpy(path, szTLT_LevelContentsFileName);
    header = (TLTLevelContentsHeader*)TLT_LoadFileFromLevelBigFile(path, &size);

    EndianSwap(header->version);
    EndianSwap(header->checksum);
    EndianSwap(header->numResources);
    EndianSwap(header->resources);
    if (header->version != TLT_CONTENTS_VERSION) {
        DebugMsg("Error: version check failed on level contents file\n");
        DebugMsg("       Engine is version %d.  File is version %d.\n", TLT_CONTENTS_VERSION,
                 header->version);
    }
    if (header->checksum != TLT_CONTENTS_CHECKSUM) {
        DebugMsg("Error: checksum failed on level contents file\n");
        DebugMsg("       Engine checksum = %d.  File checksum = %d.\n", TLT_CONTENTS_CHECKSUM,
                 header->checksum);
        DebugMsg("Use Toilet to generate a new tltheader.h for the engine or get a new "
                 "definition.crd and reprocess your level\n");
    }

    header->resources += (u32)header;
    g_pLevelFileContentsArray = (LevelFileContentsStruct_*)header->resources;
    g_pNumLevelFileContents = header->numResources;
    for (i = 0; i < header->numResources; i++) {
        res = &g_pLevelFileContentsArray[i];
        EndianSwap(res->type);
        EndianSwap(res->numFields);
        EndianSwap(res->fields);
        res->fields = (TLTResourceField*)((u8*)header + (u32)res->fields);
        for (j = 0; j < res->numFields; j++) {
            EndianSwap(res->fields[j].kind);
            EndianSwap(res->fields[j].value);
            if (res->fields[j].kind == 0 || res->fields[j].kind == 4) {
                if (res->fields[j].value != 0) {
                    res->fields[j].value += (int)header;
                }
            }
        }
    }
    g_pLevelFileContentsEndMarker = g_pLevelFileContentsArray + header->numResources;
}

// Loads a file (archive first) and records it; `alreadyLoaded` tells whether
// the same data was loaded before.
void* TLT_LoadFromRepository(const char* path, int* size, bool* alreadyLoaded) {
    char* data = (char*)TLT_LoadFileFromLevelBigFile(path, size);

    *alreadyLoaded = false;
    if (data != NULL) {
        *alreadyLoaded = g_Repository.find(data) != g_Repository.end();
        if (!*alreadyLoaded) {
            g_Repository.insert(data, data);
        }
    }
    return data;
}
