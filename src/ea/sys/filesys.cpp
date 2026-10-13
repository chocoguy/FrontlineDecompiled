#include "ea/sys/filesys.h"
#include "ea/sys/filedev.h"
#include "ea/real.h"
#include "dolphin/os.h"
#include "string.h"

static OSMutex FileMutex;
static FILEDEVICE_def gFileDevice;

static void iFILE_ExecCommand(FILEOPERATION_def* op);
static void iFILE_perror(FILEOPERATION_def* op);
static FILEOPERATION_def* reserveop();
static void freeop(FILEOPERATION_def* op);
static HANDLE_def* reservehandle();
static void freehandle(HANDLE_def* file);

static void* DefaultFILE_malloc(const char* name, int size, int flags) {
    REAL_abortmessage("DefaultFILE_malloc - must set memory callbacks with FILESYS_setopts.\n");
    return NULL;
}

static int DefaultFILE_mfree(void* ptr) {
    REAL_abortmessage("DefaultFILE_mfree - must set memory callbacks with FILESYS_setopts.\n");
    return 0;
}

static inline FILEOPERATION_def* getop(int handle) {
    FILEOPHANDLE h;
    h.handle = handle;
    return &gFileDevice.ops[h.f.index];
}

// Whether the handle still names a live operation.
static inline u8 validop(int handle) {
    FILEOPHANDLE h;
    u8 valid = 0;
    h.handle = handle;
    if (handle != 0 && h.f.serial == gFileDevice.ops[h.f.index].h.f.serial) {
        valid = 1;
    }
    return valid;
}

// Puts an operation in the queue after those of the same or higher priority.
static inline void insertop(FILEOPERATION_def* op) {
    FILEOPERATION_def* prev = NULL;
    FILEOPERATION_def* p;
    op->next = gFileDevice.queue;
    while ((p = op->next) != NULL && p->priority <= op->priority) {
        prev = p;
        op->next = p->next;
    }
    if (prev != NULL) {
        prev->next = op;
    } else {
        gFileDevice.queue = op;
    }
}

// Takes an operation out of the queue; false if it isn't queued.
static inline u8 unlinkop(FILEOPERATION_def* op) {
    FILEOPERATION_def* prev = NULL;
    FILEOPERATION_def* p;
    for (p = gFileDevice.queue; p != NULL && p != op; p = p->next) {
        prev = p;
    }
    if (p == NULL) {
        return 0;
    }
    if (prev != NULL) {
        prev->next = op->next;
    } else {
        gFileDevice.queue = op->next;
    }
    return 1;
}

extern "C" {

int FILESYS_init(int numhandles, int unused, int numops) {
    if (gFileSysOpts.alloc == NULL) {
        gFileSysOpts.alloc = DefaultFILE_malloc;
        gFileSysOpts.free = (FILESYS_FreeFn)DefaultFILE_mfree;
    }
    if (gFileDevice.numops == 0) {
        return FILESYS_initadr(numhandles, unused, numops,
                               gFileSysOpts.alloc("File Sys", FILESYS_overhead(numhandles, unused, numops), 0));
    }
    return 0;
}

int FILESYS_initadr(int numhandles, int unused, int numops, void* mem) {
    if (numhandles == 0) {
        numhandles = 0x18;
    }
    if (unused == 0) {
        unused = 0x800;
    }
    if (numops == 0) {
        numops = 10;
    }
    if (gFileSysOpts.alloc == NULL) {
        gFileSysOpts.alloc = DefaultFILE_malloc;
        gFileSysOpts.free = (FILESYS_FreeFn)DefaultFILE_mfree;
    }
    if (gFileDevice.numops == 0) {
        gFileDevice.numops = numops;
        gFileDevice.numhandles = numhandles;
        gFileDevice.maxpriority = 0xFF;
        gFileDevice.ops = (FILEOPERATION_def*)mem;
        OSInitMutex(&FileMutex);
        MEM_fill(gFileDevice.ops, 0, FILESYS_overhead(numhandles, unused, numops));
        gFileDevice.handles = (HANDLE_def*)(gFileDevice.ops + gFileDevice.numops);
        initfiledev();
        return 1;
    }
    return 0;
}

int FILESYS_overhead(int numhandles, int unused, int numops) {
    if (numhandles == 0) {
        numhandles = 0x18;
    }
    if (numops == 0) {
        numops = 10;
    }
    return numops * sizeof(FILEOPERATION_def) + numhandles * sizeof(HANDLE_def);
}

void FILESYS_restore(void) {
    if (gFileDevice.numops != 0) {
        killfiledev();
        gFileSysOpts.free(gFileDevice.ops);
        gFileDevice.numops = 0;
    }
}

int FILESYS_opstatus(int op) {
    if (validop(op)) {
        return getop(op)->status;
    }
    return -3;
}

void FILESYS_callbackop(int op, FILEOPCALLBACK callback) {
    FILEOPERATION_def* o = getop(op);
    o->callback = callback;
    if (o->status != 0) {
        gFileDevice.callbackdepth++;
        callback(op, o->status, o->data);
        gFileDevice.callbackdepth--;
    }
}

void FILESYS_priorityop(int op, int priority) {
    FILEOPERATION_def* o = getop(op);
    int old;

    OSLockMutex(&FileMutex);
    old = o->priority;
    o->priority = priority;
    if (gFileDevice.pending >= 2 && o != gFileDevice.current && o->status == 0 && old != priority) {
        if (!unlinkop(o)) {
            OSUnlockMutex(&FileMutex);
            return;
        }
        insertop(o);
    }
    OSUnlockMutex(&FileMutex);
}

// Cancels a running or queued operation (closes and BIG removals can't be).
void FILESYS_cancelop(int op) {
    int how = 0;
    FILEOPHANDLE h;
    h.handle = op;

    OSLockMutex(&FileMutex);
    if (validop(op) && h.f.type != FILEOP_CLOSE && h.f.type != FILEOP_DELBIG) {
        FILEOPERATION_def* o = getop(op);
        if (gFileDevice.current == o) {
            how = 1;
            o->cancel = 1;
        } else {
            if (o->status != 1) {
                if (!unlinkop(o)) {
                    OSUnlockMutex(&FileMutex);
                    return;
                }
                how = 2;
                gFileDevice.pending--;
            }
            o->status = -1;
        }
        if (how == 1 && o->h.f.type == FILEOP_READ) {
            stopreadfile(o->file);
        } else if (how == 2 && o->callback != NULL) {
            o->callback(o->h.handle, -1, o->data);
        }
    }
    OSUnlockMutex(&FileMutex);
}

int FILESYS_waitop(int op) {
    FILEOPERATION_def* o = getop(op);
    if (!validop(op)) {
        return -3;
    }
    for (;;) {
        if (THREAD_iscurrent(0)) {
            SYNCTASK_run(0);
        } else {
            THREAD_yield(10);
        }
        if (!validop(op)) {
            return -3;
        }
        if (o->status != 0) {
            return o->status;
        }
    }
}

// Frees the operation and returns its result.
int FILESYS_completeop(int op) {
    FILEOPERATION_def* o = getop(op);
    int result = 0;

    switch (o->status) {
    case 1:
        switch (o->h.f.type) {
        case FILEOP_OPEN:
        case FILEOP_ADDBIG:
            result = (int)o->file;
            break;
        case FILEOP_CLOSE:
        case FILEOP_7:
        case FILEOP_DELBIG:
            result = o->status;
            break;
        case FILEOP_SIZE:
        case FILEOP_EXISTS:
            result = o->arg;
            break;
        case FILEOP_READ:
            result = o->size;
            break;
        case FILEOP_WRITE: {
            HANDLE_def* file = o->file;
            int end;
            result = o->size;
            end = o->size + o->arg;
            if (end > file->size) {
                file->size = end;
            }
            break;
        }
        case FILEOP_DELETE:
            result = 1;
            break;
        }
        break;
    case -1:
    case -2:
        if (o->h.f.type == FILEOP_OPEN || o->h.f.type == FILEOP_ADDBIG) {
            freehandle(o->file);
        }
        break;
    }
    freeop(o);
    return result;
}

int FILESYS_exists(const char* name, int priority, void* data) {
    FILEOPERATION_def* op = reserveop();
    op->h.f.type = FILEOP_EXISTS;
    op->data = data;
    op->arg = 1;
    op->priority = priority;
    op->file = reservehandle();
    if (op->file == NULL) {
        op->error = 2;
        iFILE_perror(op);
    }
    strncpy(op->file->name, name, 0xFF);
    iFILE_ExecCommand(op);
    return op->h.handle;
}

int FILESYS_open(const char* name, int mode, int priority, void* data) {
    FILEOPERATION_def* op = reserveop();
    op->h.f.type = FILEOP_OPEN;
    op->data = data;
    op->arg = mode;
    op->priority = priority;
    op->file = reservehandle();
    if (op->file == NULL) {
        op->error = 2;
        iFILE_perror(op);
    }
    strncpy(op->file->name, name, 0xFF);
    iFILE_ExecCommand(op);
    return op->h.handle;
}

// A BIG archive's own handle can't be closed this way (FILESYS_delbig).
int FILESYS_close(int file, int priority, void* data) {
    FILEOPERATION_def* op = reserveop();
    BIGFILE_def* big = gFileDevice.bigs;
    op->h.f.type = FILEOP_CLOSE;
    op->priority = priority;
    op->data = data;
    op->file = (HANDLE_def*)file;
    for (; big != NULL; big = big->next) {
        if (big->file == (HANDLE_def*)file) {
            op->error = 3;
            iFILE_perror(op);
            break;
        }
    }
    iFILE_ExecCommand(op);
    return op->h.handle;
}

int FILESYS_read(int file, int offset, void* buffer, int size, int priority, void* data) {
    FILEOPERATION_def* op = reserveop();
    op->h.f.type = FILEOP_READ;
    op->data = data;
    op->priority = priority;
    if (file == 0) {
        op->error = 6;
        iFILE_perror(op);
    }
    op->file = (HANDLE_def*)file;
    if (offset + size > op->file->size) {
        size = op->file->size - offset;
    }
    op->size = size;
    op->buffer = buffer;
    op->arg = offset;
    iFILE_ExecCommand(op);
    return op->h.handle;
}

int FILESYS_size(int file, int priority, void* data) {
    FILEOPERATION_def* op = reserveop();
    op->h.f.type = FILEOP_SIZE;
    op->data = data;
    op->priority = priority;
    if (file == 0) {
        op->error = 6;
        iFILE_perror(op);
    }
    op->file = (HANDLE_def*)file;
    iFILE_ExecCommand(op);
    return op->h.handle;
}

}  // extern "C"

// The archive header has been read (2K at first): read the rest if it is
// bigger, else add the archive and finish FILESYS_addbig.
void iFILE_addbigreadcallback(int op, int status, void* data) {
    BIGFILE_def* big = (BIGFILE_def*)data;
    FILEOPERATION_def* o = getop(op);
    int priority = o->priority;
    int size;

    big->file = o->file;
    big->op->file = o->file;
    FILESYS_completeop(op);
    if (BIG_typeofheader(big->header) == 0) {
        gFileSysOpts.free(big->header);
    }
    size = BIG_sizeofheader(big->header);
    if (size > big->headersize) {
        void* tmp = gFileSysOpts.alloc("tmp bigfile buf", size, big->memflags ^ 0x100);
        void* header;
        MEM_copy(tmp, big->header, big->headersize);
        gFileSysOpts.free(big->header);
        header = gFileSysOpts.alloc("bigfile buf", size, big->memflags);
        MEM_copy(header, tmp, big->headersize);
        gFileSysOpts.free(tmp);
        big->header = header;
        FILESYS_callbackop(FILESYS_read((int)big->file, big->headersize, (u8*)big->header + big->headersize,
                                        size - big->headersize, priority, big),
                           iFILE_addbigreadcallback);
        big->headersize = size;
    } else {
        big->next = gFileDevice.bigs;
        gFileDevice.bigs = big;
        iFILE_ExecCommand(big->op);
    }
}

// The archive is open: read its header.
void iFILE_addbigopencallback(int op, int status, void* data) {
    BIGFILE_def* big = (BIGFILE_def*)data;
    int priority = getop(op)->priority;
    int file = FILESYS_completeop(op);

    if (status != 1) {
        big->op->error = 4;
        iFILE_ExecCommand(big->op);
    } else {
        FILESYS_callbackop(FILESYS_read(file, 0, big->header, 0x800, priority, big), iFILE_addbigreadcallback);
    }
}

extern "C" int FILESYS_addbig(const char* name, int memflags, int priority, void* data) {
    BIGFILE_def* big = gFileDevice.bigs;
    FILEOPERATION_def* op = reserveop();
    op->h.f.type = FILEOP_ADDBIG;
    op->data = data;
    op->priority = priority;

    for (; big != NULL; big = big->next) {
        if (strncmp(name, big->file->name, 0xFF) == 0) {
            op->file = big->file;
            iFILE_ExecCommand(op);
            // Returns the operation itself here, not its handle.
            return (int)op;
        }
    }

    big = (BIGFILE_def*)gFileSysOpts.alloc(name, sizeof(BIGFILE_def), memflags);
    big->op = op;
    big->headersize = 0x800;
    big->header = gFileSysOpts.alloc("bigfile buf", 0x800, memflags);
    big->memflags = memflags;
    FILESYS_callbackop(FILESYS_open(name, 1, priority, big), iFILE_addbigopencallback);
    return op->h.handle;
}

void iFILE_delbigclosecallback(int op, int status, void* data) {
    FILESYS_completeop(op);
    iFILE_ExecCommand((FILEOPERATION_def*)data);
}

// Removes an archive (failing if a file in it is still open) and closes it.
extern "C" int FILESYS_delbig(int file, int priority, void* data) {
    BIGFILE_def* big = gFileDevice.bigs;
    BIGFILE_def* prev = NULL;
    FILEOPERATION_def* op = reserveop();
    HANDLE_def* h = gFileDevice.handles;
    int i;

    op->h.f.type = FILEOP_DELBIG;
    op->data = data;
    op->priority = priority;
    if (((HANDLE_def*)file)->type == FILEHANDLE_BIG) {
        op->error = 1;
    }
    for (i = 0; i < gFileDevice.numhandles; i++, h++) {
        if (h != NULL && h->type == FILEHANDLE_BIG && h->u.big.big->file == (HANDLE_def*)file) {
            op->status = -2;
            op->error = 1;
            break;
        }
    }
    for (; big != NULL && big->file != (HANDLE_def*)file; big = big->next) {
        prev = big;
    }
    if (big == NULL) {
        op->error = 1;
    }
    if (prev != NULL) {
        prev->next = big->next;
    } else {
        gFileDevice.bigs = gFileDevice.bigs->next;
    }
    gFileSysOpts.free(big->header);
    gFileSysOpts.free(big);
    FILESYS_callbackop(FILESYS_close(file, priority, op), iFILE_delbigclosecallback);
    return op->h.handle;
}

extern "C" int FILESYS_atomic(int (*func)(int priority, void* data), int unused, int priority, void* data) {
    int old = gFileDevice.maxpriority;
    int result;
    gFileDevice.maxpriority = priority;
    result = func(priority, data);
    gFileDevice.maxpriority = old;
    iFILE_ExecCommand(NULL);
    return result;
}

// The original doesn't inline these (they follow their callers).
#pragma push
#pragma auto_inline off

// Queues an operation (if any), then starts the next one if none is running.
static void iFILE_ExecCommand(FILEOPERATION_def* op) {
    char name[0x100];
    char bigname[0x100];
    int offset;
    int size;

    OSLockMutex(&FileMutex);
    if (op != NULL) {
        insertop(op);
        gFileDevice.pending++;
    }
    if (gFileDevice.current != NULL) {
        OSUnlockMutex(&FileMutex);
        return;
    }
    op = gFileDevice.queue;
    if (op != NULL && op->priority <= gFileDevice.maxpriority) {
        gFileDevice.queue = op->next;
        gFileDevice.pending--;
    } else {
        op = NULL;
    }
    gFileDevice.current = op;
    OSUnlockMutex(&FileMutex);

    if (op == NULL || op->cancel != 0) {
        return;
    }

    switch (op->h.f.type) {
    case FILEOP_OPEN:
    case FILEOP_EXISTS: {
        // "big|name" looks in that archive, "|name" in any; a plain name is
        // a file, or (opened for reading) an entry of any archive.
        int where;
        int ok;
        name[0] = 0;
        if (strchr(op->file->name, '|') != NULL) {
            if (op->file->name[0] == '|') {
                where = 2;
            } else {
                char* bar = strchr(op->file->name, '|');
                bigname[0] = 0;
                where = bar - op->file->name;
                strncpy(bigname, op->file->name, where);
                bigname[where] = 0;
                where = 4;
            }
            strcpy(name, strchr(op->file->name, '|') + 1);
        } else {
            where = 1;
            if (op->arg & 1) {
                where |= 2;
                strcpy(name, op->file->name);
            }
        }

        ok = 0;
        if (where & 1) {
            if (openfile(op->file->name, op->arg, op->file)) {
                ok = 1;
                if (op->h.f.type == FILEOP_EXISTS) {
                    closefile(op->file);
                } else {
                    op->file->size = getfilesize(op->file);
                }
            }
        }
        if (!ok && (where & 6)) {
            BIGFILE_def* big;
            int named = where & 4;
            for (big = gFileDevice.bigs; big != NULL && !ok; big = big->next) {
                if (named && strcmp(big->file->name, bigname) != 0) {
                    continue;
                }
                if (BIG_locateentryz(big->header, name, 0, &offset, &size)) {
                    ok = 1;
                    op->file->u.big.big = big;
                    op->file->size = size;
                    op->file->u.big.offset = offset;
                    op->file->type = FILEHANDLE_BIG;
                }
            }
        }
        if (op->h.f.type == FILEOP_EXISTS) {
            freehandle(op->file);
            op->arg = ok;
        }
        iFILE_CommandCompleteCallback(ok);
        break;
    }
    case FILEOP_CLOSE:
        op->error = 1;
        if (op->file != NULL) {
            if (op->file->type != FILEHANDLE_BIG) {
                op->error = closefile(op->file);
            }
            freehandle(op->file);
            op->file = NULL;
        }
        iFILE_CommandCompleteCallback((u8)(op->error == 0));
        break;
    case FILEOP_READ:
        if (op->size > 0) {
            HANDLE_def* file = op->file;
            if (file->type == FILEHANDLE_BIG) {
                readfile(file->u.big.big->file, op->buffer, file->u.big.offset + op->arg, op->size);
            } else {
                readfile(file, op->buffer, op->arg, op->size);
            }
        } else {
            iFILE_CommandCompleteCallback(1);
        }
        break;
    case FILEOP_WRITE:
        writefile(op->file, op->buffer, op->arg, op->size);
        iFILE_CommandCompleteCallback(1);
        break;
    case FILEOP_SIZE:
        op->arg = op->file->size;
        iFILE_CommandCompleteCallback(1);
        break;
    case FILEOP_7:
    case FILEOP_ADDBIG:
    case FILEOP_DELBIG:
        iFILE_CommandCompleteCallback((u8)(op->error == 0));
        break;
    case FILEOP_DELETE:
        deletefile(op->file->name);
        iFILE_CommandCompleteCallback(1);
        break;
    }
}

// The running operation has finished: report it and start the next.
void iFILE_CommandCompleteCallback(int ok) {
    FILEOPERATION_def* op = gFileDevice.current;
    if (op != NULL) {
        if (op->cancel != 0) {
            op->status = -1;
        } else {
            op->status = ok ? 1 : -2;
        }
        gFileDevice.current = NULL;
        if (op->callback != NULL) {
            gFileDevice.callbackdepth++;
            op->callback(op->h.handle, op->status, op->data);
            gFileDevice.callbackdepth--;
        }
        if (gFileDevice.callbackdepth == 0) {
            iFILE_ExecCommand(NULL);
        }
    }
}

static void iFILE_perror(FILEOPERATION_def* op) {}

// Hangs when all operations are in use.
static FILEOPERATION_def* reserveop() {
    static u16 opcount = 0;
    int i;

    OSLockMutex(&FileMutex);
    for (i = 0; i < gFileDevice.numops; i++) {
        if (gFileDevice.ops[i].h.f.type == FILEOP_FREE) {
            gFileDevice.ops[i].h.f.type = FILEOP_RESERVED;
            gFileDevice.ops[i].h.f.index = i;
            if (++opcount == 0) {
                opcount = 1;
            }
            gFileDevice.ops[i].h.f.serial = opcount;
            break;
        }
    }
    OSUnlockMutex(&FileMutex);
    if (i == gFileDevice.numops) {
        for (;;) {}
    }
    return &gFileDevice.ops[i];
}

static void freeop(FILEOPERATION_def* op) {
    OSLockMutex(&FileMutex);
    MEM_fill(op, 0, sizeof(FILEOPERATION_def));
    OSUnlockMutex(&FileMutex);
}

// Hangs when all handles are in use.
static HANDLE_def* reservehandle() {
    int i;

    OSLockMutex(&FileMutex);
    for (i = 0; i < gFileDevice.numhandles; i++) {
        if (gFileDevice.handles[i].inuse == 0) {
            gFileDevice.handles[i].inuse = 1;
            break;
        }
    }
    OSUnlockMutex(&FileMutex);
    if (i == gFileDevice.numhandles) {
        for (;;) {}
    }
    return &gFileDevice.handles[i];
}

static void freehandle(HANDLE_def* file) {
    OSLockMutex(&FileMutex);
    MEM_fill(file, 0, sizeof(HANDLE_def));
    OSUnlockMutex(&FileMutex);
}

#pragma pop

extern "C" void FILESYS_setmemcallbacks(FILESYS_AllocFn alloc, FILESYS_FreeFn free) {
    gFileSysOpts.alloc = alloc;
    gFileSysOpts.free = free;
}
