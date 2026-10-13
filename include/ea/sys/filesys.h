// EA's file system (FILESYS): a queue of file operations run one at a time,
// in priority order, by the file device (filedev.cpp: host files or the DVD,
// reads done on a thread). Files may also live inside BIG archives added
// with FILESYS_addbig. Operation handles pack the op's slot and a serial
// number; file handles are HANDLE_def pointers. Hand-maintained.
#ifndef _EA_SYS_FILESYS_H_
#define _EA_SYS_FILESYS_H_

#include "types.h"
#include "ea/real.h"
#include "dolphin/dvd.h"

struct _FILE;
struct BIGFILE_def;

// Operation types
enum {
    FILEOP_FREE = 0,
    FILEOP_RESERVED = 1,
    FILEOP_OPEN = 2,
    FILEOP_CLOSE = 3,
    FILEOP_READ = 4,
    FILEOP_WRITE = 5,
    FILEOP_SIZE = 6,
    FILEOP_7 = 7,
    FILEOP_EXISTS = 8,
    FILEOP_ADDBIG = 9,
    FILEOP_DELBIG = 10,
    FILEOP_DELETE = 11,
};

// Handle types
enum {
    FILEHANDLE_BIG = 1,   // an entry of a BIG archive
    FILEHANDLE_DVD = 2,
    FILEHANDLE_HOST = 3,  // a host (hd:) file through MSL stdio
};

// An operation handle: the first word of its FILEOPERATION_def.
union FILEOPHANDLE {
    int handle;
    struct {
        /* 0x0 */ u16 serial;
        /* 0x2 */ u8 type;
        /* 0x3 */ u8 index;
    } f;
};

// An open file. size 0x144
struct HANDLE_def {
    /* 0x000 */ int type;
    /* 0x004 */ u8 inuse;
    /* 0x005 */ char name[0xFF];
    /* 0x104 */ int size;
    /* 0x108 */ union {
        _FILE* fp;
        DVDFileInfo dvd;
        struct {
            BIGFILE_def* big;
            int offset;  // of the entry in the archive
        } big;
    } u;
};

typedef void (*FILEOPCALLBACK)(int op, int status, void* data);

// A queued operation. size 0x30
struct FILEOPERATION_def {
    /* 0x00 */ FILEOPHANDLE h;
    /* 0x04 */ int cancel;
    /* 0x08 */ int status;  // 0 pending, 1 done, -1 cancelled, -2 failed
    /* 0x0C */ int error;
    /* 0x10 */ int priority;  // lower runs first
    /* 0x14 */ void* data;  // passed to the callback
    /* 0x18 */ int arg;  // open mode, read offset; the result of size/exists
    /* 0x1C */ int size;
    /* 0x20 */ void* buffer;
    /* 0x24 */ HANDLE_def* file;
    /* 0x28 */ FILEOPCALLBACK callback;
    /* 0x2C */ FILEOPERATION_def* next;
};

// An added BIG archive. size 0x18
struct BIGFILE_def {
    /* 0x00 */ void* header;
    /* 0x04 */ int headersize;
    /* 0x08 */ int memflags;
    /* 0x0C */ HANDLE_def* file;
    /* 0x10 */ FILEOPERATION_def* op;  // the FILESYS_addbig operation
    /* 0x14 */ BIGFILE_def* next;
};

// size 0x28
struct FILEDEVICE_def {
    /* 0x00 */ int numops;
    /* 0x04 */ int numhandles;
    /* 0x08 */ int maxpriority;  // ops above it wait (FILESYS_atomic)
    /* 0x0C */ int pending;
    /* 0x10 */ int callbackdepth;
    /* 0x14 */ FILEOPERATION_def* current;
    /* 0x18 */ FILEOPERATION_def* ops;
    /* 0x1C */ HANDLE_def* handles;
    /* 0x20 */ FILEOPERATION_def* queue;
    /* 0x24 */ BIGFILE_def* bigs;
};

// size 0xC
struct FILESYSOPTS {
    /* 0x0 */ int size;
    /* 0x4 */ FILESYS_AllocFn alloc;
    /* 0x8 */ FILESYS_FreeFn free;
};

extern FILESYSOPTS gFileSysOpts;  // filesysopts.cpp

void iFILE_addbigreadcallback(int op, int status, void* data);  // 0x80148EA0
void iFILE_addbigopencallback(int op, int status, void* data);  // 0x801490C8
void iFILE_delbigclosecallback(int op, int status, void* data);  // 0x801494B4
void iFILE_CommandCompleteCallback(int ok);  // 0x80149B9C

#endif
