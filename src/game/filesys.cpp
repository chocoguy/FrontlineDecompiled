#include "game/filesys.h"

#include "dolphin/dvd.h"

#define MAX_OPEN_FILES 32

struct OpenFile {
    /* 0x0 */ bool used;
    /* 0x4 */ DVDFileInfo info;
};

static bool s_bInitialized;
static int g_iNumOpenFiles;
static OpenFile g_openFiles[MAX_OPEN_FILES];

int File_Seek(int, int, ESeekOrigin) {
    return -1;
}

int File_Write(int, void*, int) {
    return -1;
}

int File_Close(int handle) {
    OpenFile* file = &g_openFiles[handle];
    DVDClose(&file->info);
    file->used = false;
    g_iNumOpenFiles--;
    return 0;
}

int File_Open(const char* path, EOpenMode) {
    int i;
    int handle;
    OpenFile* file;

    // Note: s_bInitialized is never set in the retail game, so every call
    // re-initializes DVD access and frees all slots.
    if (!s_bInitialized) {
        DVDInit();
        for (i = 0; i < MAX_OPEN_FILES; i++) {
            g_openFiles[i].used = false;
        }
    }

    handle = -1;
    for (i = 0; i < MAX_OPEN_FILES; i++) {
        if (!g_openFiles[i].used) {
            handle = i;
            break;
        }
    }

    file = &g_openFiles[handle];
    if (!DVDOpen(path, &file->info)) {
        return -1;
    }
    file->used = true;
    g_iNumOpenFiles++;
    return handle;
}
