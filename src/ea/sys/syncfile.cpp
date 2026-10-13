#include "ea/sys/filesys.h"
#include "ea/real.h"

typedef int (*BLOCKIOFUNC)(int file, int offset, void* buffer, int size, int priority, void* data);

// A synchronous transfer, done 32K at a time. size 0x24
struct SYNCBLOCKIO {
    /* 0x00 */ int priority;
    /* 0x04 */ int file;
    /* 0x08 */ int offset;
    /* 0x0C */ int remaining;
    /* 0x10 */ int total;
    /* 0x14 */ int size;  // of the current block
    /* 0x18 */ u8* buffer;
    /* 0x1C */ BLOCKIOFUNC func;
    /* 0x20 */ int op;
};

static void synccallback(int op, int status, void* data) {
    SYNCBLOCKIO* io = (SYNCBLOCKIO*)data;
    int n = FILESYS_completeop(op);

    io->op = 0;
    if (status == 1) {
        io->offset += n;
        io->total += n;
        io->buffer += n;
        if (n < io->size) {
            io->remaining = 0;
        } else {
            io->remaining -= n;
        }
        if (io->remaining > 0) {
            if (io->remaining > 0x8000) {
                io->size = 0x8000;
            } else {
                io->size = io->remaining;
            }
            io->op = io->func(io->file, io->offset, io->buffer, io->size, io->priority, io);
            if (io->op != 0) {
                FILESYS_callbackop(io->op, synccallback);
            } else {
                io->remaining = 0;
            }
        }
    } else {
        io->remaining = 0;
    }
}

static int syncblockio(int file, int offset, void* buffer, int size, int priority, BLOCKIOFUNC func) {
    SYNCBLOCKIO io;

    io.priority = priority;
    io.file = file;
    io.offset = offset;
    io.remaining = size;
    io.total = 0;
    io.size = size;
    io.buffer = (u8*)buffer;
    io.func = func;
    if (io.size > 0x8000) {
        io.size = 0x8000;
    }
    if ((io.op = func(file, offset, io.buffer, io.size, priority, &io)) != 0) {
        FILESYS_callbackop(io.op, synccallback);
        do {
            FILESYS_waitop(io.op);
        } while (io.remaining != 0 || io.op != 0);
    }
    return io.total;
}

extern "C" {

int FILESYS_opensync(const char* path, int mode, int priority, int* handle) {
    int ok = 0;
    int op = FILESYS_open(path, mode, priority, NULL);
    if (op != 0) {
        FILESYS_waitop(op);
        ok = FILESYS_opstatus(op) == 1;
        *handle = FILESYS_completeop(op);
    } else {
        *handle = 0;
    }
    return ok;
}

int FILESYS_readsync(int handle, int offset, void* buffer, int size, int priority) {
    return syncblockio(handle, offset, buffer, size, priority, FILESYS_read);
}

int FILESYS_closesync(int handle, int priority) {
    int ok = 0;
    int op = FILESYS_close(handle, priority, NULL);
    if (op != 0) {
        FILESYS_waitop(op);
        ok = FILESYS_completeop(op) != 0;
    }
    return ok;
}

int FILESYS_sizesync(int handle, int priority) {
    int size = 0;
    int op = FILESYS_size(handle, priority, NULL);
    if (op != 0) {
        FILESYS_waitop(op);
        size = FILESYS_completeop(op);
    }
    return size;
}

int FILESYS_addbigsync(const char* path, int flags, int priority, int* handle) {
    int ok = 0;
    int op = FILESYS_addbig(path, flags, priority, NULL);
    if (op != 0) {
        FILESYS_waitop(op);
        ok = FILESYS_opstatus(op) == 1;
        *handle = FILESYS_completeop(op);
    } else {
        *handle = 0;
    }
    return ok;
}

int FILESYS_delbigsync(int handle, int priority) {
    int ok = 0;
    int op = FILESYS_delbig(handle, priority, NULL);
    if (op != 0) {
        FILESYS_waitop(op);
        ok = FILESYS_completeop(op) != 0;
    }
    return ok;
}

int FILESYS_existssync(const char* path, int priority) {
    int ok = 0;
    int op = FILESYS_exists(path, priority, NULL);
    if (op != 0) {
        FILESYS_waitop(op);
        ok = FILESYS_completeop(op) != 0;
    }
    return ok;
}

}  // extern "C"
