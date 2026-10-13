#include "ea/sys/filesys.h"
#include "ea/real.h"

// Synchronous whole-file helpers. Each runs as a FILESYS_atomic block, so
// only operations of sync_priority or better run meanwhile; its own
// operations use one better than that.

// size 0x14
struct FILEATOMARGS {
    /* 0x00 */ const char* name;
    /* 0x04 */ int unk_0x4;
    /* 0x08 */ int value;  // loadsize: the size read; loadbigheader: memory flags
    /* 0x0C */ int unk_0xC;  // 1 for FILE_size/loadsize/loadbigheader
    /* 0x10 */ int memflags;
};

static int sync_priority = 100;

extern "C" u8 FILE_exists(const char* name) {
    return FILESYS_existssync(name, sync_priority);
}

static int filesizeatom(int priority, void* data) {
    FILEATOMARGS* args = (FILEATOMARGS*)data;
    int file;
    int size;
    if ((u8)FILESYS_opensync(args->name, 1, priority, &file)) {
        size = FILESYS_sizesync(file, priority - 1);
        FILESYS_closesync(file, priority - 1);
        return size;
    }
    return 0;
}

extern "C" int FILE_size(const char* name) {
    FILEATOMARGS args;
    args.name = name;
    args.unk_0xC = 1;
    return FILESYS_atomic(filesizeatom, 0, sync_priority, &args);
}

extern "C" int FILE_sizez(const char* name) {
    FILEATOMARGS args;
    args.name = name;
    args.unk_0xC = 0;
    return FILESYS_atomic(filesizeatom, 0, sync_priority, &args);
}

static int loadfileatom(int priority, void* data) {
    FILEATOMARGS* args = (FILEATOMARGS*)data;
    int file;
    int size;
    void* buffer;
    if ((u8)FILESYS_opensync(args->name, 1, priority, &file)) {
        size = FILESYS_sizesync(file, priority - 1);
        buffer = gFileSysOpts.alloc(args->name, size, args->memflags);
        if (buffer != NULL) {
            FILESYS_readsync(file, 0, buffer, size, priority - 1);
            FILESYS_closesync(file, priority - 1);
            return (int)buffer;
        }
        FILESYS_closesync(file, priority - 1);
        return 0;
    }
    return 0;
}

extern "C" void* FILE_loadz(const char* name, int memflags) {
    FILEATOMARGS args;
    args.name = name;
    args.memflags = memflags;
    args.unk_0xC = 0;
    return (void*)FILESYS_atomic(loadfileatom, 0, sync_priority, &args);
}

static int loadfilesizeatom(int priority, void* data) {
    FILEATOMARGS* args = (FILEATOMARGS*)data;
    int file;
    int size;
    void* buffer;
    if ((u8)FILESYS_opensync(args->name, 1, priority, &file)) {
        size = FILESYS_sizesync(file, priority - 1);
        buffer = gFileSysOpts.alloc(args->name, size, args->memflags);
        if (buffer != NULL) {
            FILESYS_readsync(file, 0, buffer, size, priority - 1);
            FILESYS_closesync(file, priority - 1);
            args->value = size;
            return (int)buffer;
        }
        FILESYS_closesync(file, priority - 1);
        return 0;
    }
    return 0;
}

extern "C" void* FILE_loadsize(const char* name, int* size, int memflags) {
    FILEATOMARGS args;
    void* buffer;
    args.name = name;
    args.memflags = memflags;
    args.unk_0xC = 1;
    buffer = (void*)FILESYS_atomic(loadfilesizeatom, 0, sync_priority, &args);
    if (size != NULL) {
        *size = args.value;
    }
    return buffer;
}

// Loads a BIG archive's header: 0xA90 bytes first, then the rest if the
// header is bigger.
static int loadbigheaderatom(int priority, void* data) {
    FILEATOMARGS* args = (FILEATOMARGS*)data;
    int file;
    int size;
    u8* header;
    u8* bigger;
    if (!(u8)FILESYS_opensync(args->name, 1, priority, &file)) {
        return 0;
    }
    header = (u8*)gFileSysOpts.alloc(args->name, 0xA90, args->value);
    if (header == NULL) {
        FILESYS_closesync(file, priority - 1);
        return 0;
    }
    FILESYS_readsync(file, 0, header, 0xA90, priority - 1);
    if (BIG_typeofheader(header) == 0) {
        gFileSysOpts.free(header);
        FILESYS_closesync(file, priority - 1);
        return 0;
    }
    size = BIG_sizeofheader(header);
    if (size > 0xA90) {
        bigger = (u8*)gFileSysOpts.alloc(args->name, size, args->value);
        if (bigger == NULL) {
            gFileSysOpts.free(header);
            FILESYS_closesync(file, priority - 1);
            return 0;
        }
        MEM_copy(bigger, header, 0xA90);
        gFileSysOpts.free(header);
        header = bigger;
        FILESYS_readsync(file, 0xA90, bigger + 0xA90, size - 0xA90, priority - 1);
    }
    FILESYS_closesync(file, priority - 1);
    return (int)header;
}

extern "C" void* FILE_loadbigheader(const char* name, int memflags) {
    FILEATOMARGS args;
    args.name = name;
    args.value = memflags;
    args.unk_0xC = 1;
    return (void*)FILESYS_atomic(loadbigheaderatom, 0, sync_priority, &args);
}
