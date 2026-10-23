#include "ea/sys/filesys.h"
#include "ea/sys/mutex2.h"
#include "ea/real.h"

// Asynchronous reads and whole-file loads on top of FILESYS, in 32K blocks.
// Request ids are the pool slot in the low byte and a counter above it.

// size 0x30
struct ASYNCREQUEST {
    /* 0x00 */ int id;
    /* 0x04 */ ASYNCREQUEST* next;  // in the free queue
    /* 0x08 */ int total;  // bytes read
    /* 0x0C */ int releasing;  // ASYNCFILE_release is waiting on it
    /* 0x10 */ int cancelled;
    /* 0x14 */ u8* buffer;  // loads: 1 until allocated (once the size is known)
    /* 0x18 */ void (*callback)(int id);
    /* 0x1C */ int op;  // the FILESYS operation in progress
    /* 0x20 */ int file;
    /* 0x24 */ int offset;
    /* 0x28 */ int remaining;  // loads: the memory flags until the size is known
    /* 0x2C */ u8* dest;
};

static u8 mutex[0x1C];
static int numrequests;
static ASYNCREQUEST* request;
static struct {
    ASYNCREQUEST* head;
    ASYNCREQUEST* tail;
} freequeue;
static int requestidcounter;

static inline void freerequest(ASYNCREQUEST* r) {
    if (r->cancelled != 0 && (u32)PTR_TO_ADDR(r->buffer) > 1) {
        gFileSysOpts.free(r->buffer);
    }
    r->id &= 0xFF;
    r->op = 0;
    MUTEX_lock(mutex);
    if (freequeue.head == NULL) {
        freequeue.head = r;
    } else {
        freequeue.tail->next = r;
    }
    freequeue.tail = r;
    r->next = NULL;
    MUTEX_unlock(mutex);
}

static inline ASYNCREQUEST* allocrequest() {
    ASYNCREQUEST* r;
    MUTEX_lock(mutex);
    r = freequeue.head;
    if (r == NULL) {
        r = NULL;
    } else {
        freequeue.head = r->next;
    }
    MUTEX_unlock(mutex);
    return r;
}

static inline void newid(ASYNCREQUEST* r) {
    requestidcounter += 0x100;
    if (requestidcounter == 0) {
        requestidcounter = 0x100;
    }
    r->id = (r->id & 0xFF) | requestidcounter;
}

static inline ASYNCREQUEST* findrequest(int id) {
    ASYNCREQUEST* r;
    if (id < 0x100 || (id & 0xFF) >= numrequests) {
        r = NULL;
    } else {
        r = &request[id & 0xFF];
        if (r->id != id) {
            r = NULL;
        }
    }
    return r;
}

// The request is over: report it, or free it if it was cancelled (a
// release in progress frees it itself).
static inline void finish(ASYNCREQUEST* r) {
    void (*callback)(int);
    int cancelled;
    int releasing;

    MUTEX_lock(mutex);
    callback = r->callback;
    cancelled = r->cancelled;
    releasing = r->releasing;
    r->op = 0;
    MUTEX_unlock(mutex);
    if (cancelled != 0) {
        if (releasing == 0) {
            freerequest(r);
        }
    } else if (callback != NULL) {
        callback(r->id);
    }
}

static void loadfileclosecallback(int op, int status, void* data) {
    ASYNCREQUEST* r = (ASYNCREQUEST*)data;
    FILESYS_completeop(r->op);
    finish(r);
}

static void loadfilereadcallback(int op, int status, void* data) {
    ASYNCREQUEST* r = (ASYNCREQUEST*)data;
    int n = FILESYS_completeop(r->op);
    int size;

    r->total += n;
    r->dest += n;
    if (n < 0x8000 || r->cancelled != 0) {
        r->op = FILESYS_close(r->file, 99, r);
        if (r->op != 0) {
            FILESYS_callbackop(r->op, loadfileclosecallback);
        }
    } else {
        r->offset += n;
        r->remaining -= n;
        size = r->remaining;
        if (size > 0x8000) {
            size = 0x8000;
        }
        r->op = FILESYS_read(r->file, r->offset, r->dest, size, 99, r);
        if (r->op != 0) {
            FILESYS_callbackop(r->op, loadfilereadcallback);
        }
    }
}

// The size is known: allocate the buffer and read until a short block.
static void loadfilesizecallback(int op, int status, void* data) {
    ASYNCREQUEST* r = (ASYNCREQUEST*)data;
    int size = FILESYS_completeop(r->op);

    if (r->cancelled != 0) {
        r->op = FILESYS_close(r->file, 99, r);
        if (r->op != 0) {
            FILESYS_callbackop(r->op, loadfileclosecallback);
        }
    } else {
        int memflags = r->remaining;
        r->remaining = 0x7FFFFFFF;
        r->buffer = (u8*)gFileSysOpts.alloc("ASYNCFILEBUF", size, memflags);
        r->dest = r->buffer;
        r->op = FILESYS_read(r->file, r->offset, r->dest, 0x8000, 99, r);
        if (r->op != 0) {
            FILESYS_callbackop(r->op, loadfilereadcallback);
        }
    }
}

static void loadfileopencallback(int op, int status, void* data) {
    ASYNCREQUEST* r = (ASYNCREQUEST*)data;

    r->file = FILESYS_completeop(r->op);
    if (r->file == 0) {
        finish(r);
    } else if (r->cancelled != 0) {
        r->op = FILESYS_close(r->file, 99, r);
        if (r->op != 0) {
            FILESYS_callbackop(r->op, loadfileclosecallback);
        }
    } else if (r->buffer == NULL) {
        int size = r->remaining;
        if (size > 0x8000) {
            size = 0x8000;
        }
        r->op = FILESYS_read(r->file, r->offset, r->dest, size, 99, r);
        if (r->op != 0) {
            FILESYS_callbackop(r->op, loadfilereadcallback);
        }
    } else {
        r->op = FILESYS_size(r->file, 99, r);
        if (r->op != 0) {
            FILESYS_callbackop(r->op, loadfilesizecallback);
        }
    }
}

static void readfilereadcallback(int op, int status, void* data) {
    ASYNCREQUEST* r = (ASYNCREQUEST*)data;
    int n = FILESYS_completeop(r->op);
    int size;

    r->total += n;
    r->dest += n;
    if (n < 0x8000 || r->cancelled != 0) {
        finish(r);
    } else {
        r->offset += n;
        r->remaining -= n;
        size = r->remaining;
        if (size > 0x8000) {
            size = 0x8000;
        }
        r->op = FILESYS_read(r->file, r->offset, r->dest, size, 99, r);
        if (r->op != 0) {
            FILESYS_callbackop(r->op, readfilereadcallback);
        }
    }
}

extern "C" {

void ASYNCFILE_init(int num, int memflags) {
    int i;
    if (request == NULL && num <= 0x100) {
        numrequests = num;
        request = (ASYNCREQUEST*)gFileSysOpts.alloc("ASYNCFILE", num * sizeof(ASYNCREQUEST), memflags);
        freequeue.head = request;
        freequeue.tail = &request[num - 1];
        MUTEX_create(mutex);
        for (i = 0; i < num; i++) {
            request[i].id = i;
            request[i].op = 0;
            request[i].next = &request[i + 1];
        }
        request[num - 1].next = NULL;
    }
}

void ASYNCFILE_restore(void) {
    int i;
    int busy;

    if (request != NULL) {
        for (i = 0; i < numrequests; i++) {
            ASYNCFILE_cancel(request[i].id);
        }
        do {
            ASYNCREQUEST* r = request;
            busy = 0;
            for (i = 0; i < numrequests; i++, r++) {
                if (r->op != 0) {
                    busy = 1;
                }
            }
            if (busy) {
                if (THREAD_iscurrent(0)) {
                    SYNCTASK_run(0);
                }
                THREAD_yield(0);
            }
        } while (busy);
        gFileSysOpts.free(request);
        MUTEX_destroy(mutex);
        request = NULL;
    }
}

int ASYNCFILE_load(const char* name, int memflags) {
    ASYNCREQUEST* r = allocrequest();
    if (r == NULL) {
        return 0;
    }
    newid(r);
    r->total = 0;
    r->releasing = 0;
    r->cancelled = 0;
    r->buffer = (u8*)1;
    r->callback = NULL;
    r->offset = 0;
    r->remaining = memflags;
    r->dest = NULL;
    r->op = FILESYS_open(name, 1, 100, r);
    if (r->op == 0) {
        return 0;
    }
    FILESYS_callbackop(r->op, loadfileopencallback);
    return r->id;
}

int ASYNCFILE_read(int file, int offset, void* buffer, int size) {
    ASYNCREQUEST* r = allocrequest();
    if (r == NULL) {
        return 0;
    }
    newid(r);
    r->total = 0;
    r->releasing = 0;
    r->cancelled = 0;
    r->buffer = NULL;
    r->callback = NULL;
    r->file = file;
    r->offset = offset;
    r->remaining = size;
    r->dest = (u8*)buffer;
    if (size > 0x8000) {
        size = 0x8000;
    }
    r->op = FILESYS_read(file, offset, buffer, size, 100, r);
    if (r->op == 0) {
        return 0;
    }
    FILESYS_callbackop(r->op, readfilereadcallback);
    return r->id;
}

// 1 when done, 0 while running, -1 for a bad id.
int ASYNCFILE_getstatus(int id) {
    ASYNCREQUEST* r;
    int status;
    MUTEX_lock(mutex);
    r = findrequest(id);
    if (r == NULL) {
        status = -1;
    } else {
        status = (u8)(r->op == 0);
    }
    MUTEX_unlock(mutex);
    return status;
}

// Waits for the request and frees it: *buffer gets the start of the data
// (for loads, the allocated buffer), *bytes the bytes read. -1 if it was
// cancelled.
int ASYNCFILE_release(int id, int* buffer, int* bytes) {
    ASYNCREQUEST* r;
    int cancelled = 0;

    MUTEX_lock(mutex);
    r = findrequest(id);
    if (r != NULL) {
        cancelled = r->cancelled;
        if (cancelled == 0) {
            r->releasing = 1;
        }
    }
    MUTEX_unlock(mutex);
    if (r == NULL || cancelled != 0) {
        return -1;
    }
    while (r->op != 0) {
        FILESYS_waitop(r->op);
    }
    if (r->cancelled != 0) {
        if (buffer != NULL) {
            *buffer = 0;
        }
        if (bytes != NULL) {
            *bytes = 0;
        }
        freerequest(r);
        return -1;
    }
    if (buffer != NULL) {
        *buffer = (int)PTR_TO_ADDR((r->dest - r->total));
    }
    if (bytes != NULL) {
        *bytes = r->total;
    }
    freerequest(r);
    return 1;
}

int ASYNCFILE_cancel(int id) {
    ASYNCREQUEST* r;
    int op = 0;
    int cancelled = 0;
    int releasing = 0;

    MUTEX_lock(mutex);
    r = findrequest(id);
    if (r != NULL) {
        op = r->op;
        cancelled = r->cancelled;
        releasing = r->releasing;
        if (op != 0 || releasing == 0) {
            r->cancelled = 1;
        }
    }
    MUTEX_unlock(mutex);
    if (r == NULL || cancelled != 0 || r->cancelled == 0) {
        return -1;
    }
    if (op != 0) {
        FILESYS_cancelop(op);
    }
    if (op == 0 && releasing == 0) {
        freerequest(r);
    }
    return 1;
}

}  // extern "C"
