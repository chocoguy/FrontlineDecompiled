#include "ea/sys/filesys.h"
#include "ea/sys/mutex2.h"
#include "ea/real.h"
#include "string.h"

// Streams: requests (files or memory images of chunks) are read in blocks
// into a ring buffer, parsed into chunks and handed out per track. A chunk
// header is a little-endian tag and size; once parsed, the size's top byte
// holds the chunk's track. Tag -1 marks where the data wraps to the buffer
// start, -2 a chunk that is free (released or dropped).

#define STREAM_MAGIC 0x4D525453  // "MRTS"
#define CHUNK_WRAP 0xFFFFFFFF
#define CHUNK_FREE 0xFFFFFFFE

enum {
    STREAMSTATE_IDLE = 0,
    STREAMSTATE_READING = 1,
    STREAMSTATE_FULL = 2,  // waiting for chunks to be released
};

enum {
    REQSTATE_FREE = 0,
    REQSTATE_QUEUED = 1,
    REQSTATE_READING = 2,
    REQSTATE_DONE = 3,
    REQSTATE_CANCELLED = 4,
};

struct STREAMHEADERtag;

// size 0x124
struct REQUESTSTRUCTtag {
    /* 0x000 */ int id;
    /* 0x004 */ int state;
    /* 0x008 */ REQUESTSTRUCTtag* prev;
    /* 0x00C */ REQUESTSTRUCTtag* next;
    /* 0x010 */ int type;  // 0 file, 1 memory
    /* 0x014 */ char name[0x100];
    /* 0x114 */ u8* mem;  // memory: the next byte to copy
    /* 0x118 */ int offset;  // file: the start offset; memory: the size
    /* 0x11C */ u32 tag;  // the chunk tag that ends the request
    /* 0x120 */ u8* start;  // where its data starts in the buffer
};

// A track: the stream handle callers use. size 0x10
struct STREAMTRACK {
    /* 0x0 */ STREAMHEADERtag* header;
    /* 0x4 */ int num;  // 1-based
    /* 0x8 */ int bytes;  // buffered for this track
    /* 0xC */ u8* next;  // its oldest chunk
};

// size 0xC
struct STREAMFILTER {
    /* 0x0 */ u32 mask;
    /* 0x4 */ u32 value;
    /* 0x8 */ int track;  // -1/-2: drop
};

// size 0x18C (requests, filters and tracks follow, then the buffer)
struct STREAMHEADERtag {
    /* 0x000 */ u32 magic;
    /* 0x004 */ u8 mutex[0x1C];
    /* 0x020 */ REQUESTSTRUCTtag* requests;
    /* 0x024 */ int numrequests;
    /* 0x028 */ STREAMFILTER* filters;
    /* 0x02C */ int numfilters;
    /* 0x030 */ STREAMTRACK* tracks;
    /* 0x034 */ int numtracks;
    /* 0x038 */ u8* bufstart;
    /* 0x03C */ u8* bufbase;  // where the data wrapped to (bufstart, or just after to keep alignment)
    /* 0x040 */ u8* bufend;
    /* 0x044 */ int state;
    /* 0x048 */ int priority;
    /* 0x04C */ int greedypriority;
    /* 0x050 */ int greedylevel;
    /* 0x054 */ int greedy;
    /* 0x058 */ int buffered;
    /* 0x05C */ u8* read;  // the oldest chunk still in use
    /* 0x060 */ u8* parse;  // the next chunk to parse
    /* 0x064 */ u8* write;  // the end of the data read
    /* 0x068 */ REQUESTSTRUCTtag* head;
    /* 0x06C */ REQUESTSTRUCTtag* cur;  // being read
    /* 0x070 */ REQUESTSTRUCTtag* tail;
    /* 0x074 */ REQUESTSTRUCTtag* free;
    /* 0x078 */ char name[0x100];  // the open file
    /* 0x178 */ int file;
    /* 0x17C */ int offset;  // of the next read
    /* 0x180 */ int op;
    /* 0x184 */ int readsize;
    /* 0x188 */ int blocksize;
};

static int requestidcounter;

static void restartstream(STREAMHEADERtag* h, int priority);
static void startnextrequest(STREAMHEADERtag* h, int priority);

static inline u32 getle(const u8* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static inline void putle(u8* p, u32 v) {
    p[0] = v;
    p[1] = v >> 8;
    p[2] = v >> 16;
    p[3] = v >> 24;
}

// The stream of a handle, or NULL.
static inline STREAMHEADERtag* getheader(int handle) {
    STREAMHEADERtag* h;
    int bad;
    if (handle == 0) {
        bad = 1;
    } else {
        h = (ADDR_TO_PTR(STREAMTRACK, handle))->header;
        if (h->magic != STREAM_MAGIC) {
            bad = 1;
        } else {
            bad = 0;
        }
    }
    return bad ? NULL : h;
}

// Whether p lies in the ring buffer span [lo, hi).
static inline u8 inspan(const u8* p, const u8* lo, const u8* hi) {
    u8 in;
    if (lo <= hi) {
        in = 0;
        if (p >= lo && p < hi) {
            in = 1;
        }
    } else {
        in = 0;
        if (p >= lo || p < hi) {
            in = 1;
        }
    }
    return in;
}

// Takes bytes off the buffered count; going below the greedy level makes
// the stream greedy (its read moves to the greedy priority).
static inline void unbuffer(STREAMHEADERtag* h, int n) {
    int old;
    int now;
    MUTEX_lock(h->mutex);
    old = h->buffered;
    now = old - n;
    h->buffered = now;
    MUTEX_unlock(h->mutex);
    if (old >= h->greedylevel && now < h->greedylevel) {
        h->greedy = 1;
        if (h->state == STREAMSTATE_READING) {
            FILESYS_priorityop(h->op, h->greedypriority);
        }
    }
}

static inline REQUESTSTRUCTtag* allocrequest(STREAMHEADERtag* h) {
    REQUESTSTRUCTtag* r;
    MUTEX_lock(h->mutex);
    r = h->free;
    if (r == NULL) {
        r = NULL;
    } else {
        h->free = r->next;
        requestidcounter += 0x100;
        if (requestidcounter == 0) {
            requestidcounter = 0x100;
        }
        r->id = (r->id & 0xFF) | requestidcounter;
    }
    MUTEX_unlock(h->mutex);
    return r;
}

// Queues a request and starts the stream if it is idle.
static inline void addrequest(STREAMHEADERtag* h, REQUESTSTRUCTtag* r) {
    MUTEX_lock(h->mutex);
    if (h->tail == NULL) {
        r->prev = NULL;
        h->head = r;
        h->cur = r;
        h->tail = r;
    } else {
        r->prev = h->tail;
        h->tail->next = r;
        h->tail = r;
    }
    MUTEX_unlock(h->mutex);
}

static void freerequest(STREAMHEADERtag* h, REQUESTSTRUCTtag* r) {
    if (r == h->head) {
        h->head = r->next;
    } else {
        r->prev->next = r->next;
    }
    if (r == h->tail) {
        h->tail = r->prev;
    } else {
        r->next->prev = r->prev;
    }
    if (r == h->cur) {
        if (r->next == NULL) {
            h->cur = r->prev;
        } else {
            h->cur = r->next;
        }
    }
    r->state = REQSTATE_FREE;
    r->next = h->free;
    h->free = r;
}

// Sorts the complete chunks read so far into tracks; returns 1 when the
// request's end chunk was reached.
static int parsechunks(STREAMHEADERtag* h) {
    REQUESTSTRUCTtag* req = h->cur;
    u8* p;
    u32 size;
    u32 tag;
    int track;
    int cancelled;
    int i;

    while (h->write - (p = h->parse) >= 8) {
        size = getle(p + 4);
        if (size & 0xFF000000) {
            putle(p, req->tag);
            putle(p + 4, 8);
            size = 8;
        }
        if (h->parse + size > h->write) {
            break;
        }
        tag = getle(p);
        if (req->tag == tag) {
            // Pad the end chunk so the next request starts aligned.
            u32 misalign = (uptr)(h->parse) & 31;
            size = ((size + misalign + 31) & ~31) - misalign;
        }

        for (i = 0; i < h->numfilters; i++) {
            STREAMFILTER* f = &h->filters[i];
            if (f->value == (tag & f->mask)) {
                track = f->track;
                goto found;
            }
        }
        track = -2;
    found:

        if (track < 0) {
            MUTEX_lock(h->mutex);
            cancelled = req->state == REQSTATE_CANCELLED;
            if (!cancelled) {
                putle(p, CHUNK_FREE);
                h->parse += size;
            }
            MUTEX_unlock(h->mutex);
        } else {
            putle(p + 4, size | (track << 24));
            MUTEX_lock(h->mutex);
            cancelled = req->state == REQSTATE_CANCELLED;
            if (!cancelled) {
                STREAMTRACK* t = &h->tracks[track - 1];
                int old;
                t->bytes += size;
                if (t->bytes == size) {
                    t->next = p;
                }
                h->parse += size;
                old = h->buffered;
                h->buffered = old + size;
                if (old < h->greedylevel && h->buffered >= h->greedylevel) {
                    h->greedy = 0;
                }
            }
            MUTEX_unlock(h->mutex);
        }

        if (cancelled) {
            if (getle(p) != req->tag) {
                u32 misalign = (uptr)(h->parse) & 31;
                putle(p + 4, (((size + misalign + 31) & ~31) - misalign) | (track << 24));
            }
            break;
        }
        if (getle(p) == req->tag) {
            return 1;
        }
    }
    return 0;
}

static void opencallback(int op, int status, void* data) {
    STREAMHEADERtag* h = (STREAMHEADERtag*)data;
    h->file = FILESYS_completeop(h->op);
    if (h->file != 0) {
        restartstream(h, h->greedypriority);
    }
}

static void closecallback(int op, int status, void* data) {
    STREAMHEADERtag* h = (STREAMHEADERtag*)data;
    FILESYS_completeop(h->op);
    h->op = FILESYS_open(h->name, 1, h->greedypriority, h);
    if (h->op != 0) {
        FILESYS_callbackop(h->op, opencallback);
    }
}

// A block has been read (or copied).
static void readcallback(int op, int status, void* data) {
    STREAMHEADERtag* h = (STREAMHEADERtag*)data;
    REQUESTSTRUCTtag* req = h->cur;
    int n;
    int done;
    int end;

    if (req->type == 1) {
        n = h->readsize;
        done = (u8)(h->offset + n >= req->offset);
    } else {
        n = FILESYS_completeop(h->op);
        done = n < h->readsize;
    }
    h->offset += n;
    h->write += n;
    end = parsechunks(h);
    if (req->state == REQSTATE_CANCELLED) {
        startnextrequest(h, h->greedypriority);
    } else if (done || end) {
        MUTEX_lock(h->mutex);
        if (req->state != REQSTATE_CANCELLED) {
            req->state = REQSTATE_DONE;
        }
        MUTEX_unlock(h->mutex);
        startnextrequest(h, h->greedypriority);
    } else {
        restartstream(h, h->greedypriority);
    }
}

static void startnextrequest(STREAMHEADERtag* h, int priority) {
    REQUESTSTRUCTtag* req = NULL;
    REQUESTSTRUCTtag* cur;
    int idle;

    MUTEX_lock(h->mutex);
    cur = h->cur;
    if (cur == NULL) {
        idle = 1;
    } else if (cur->state == REQSTATE_QUEUED) {
        idle = 0;
    } else if (cur->next == NULL) {
        idle = 1;
    } else {
        h->cur = cur->next;
        idle = 0;
    }
    if (idle) {
        h->state = STREAMSTATE_IDLE;
    } else {
        req = h->cur;
        req->start = h->parse;
        req->state = REQSTATE_READING;
    }
    MUTEX_unlock(h->mutex);

    if (idle) {
        return;
    }
    h->write = h->parse;
    if (req->type == 1) {
        h->offset = 0;
    } else {
        h->offset = req->offset;
        if (strcmp(req->name, h->name) != 0) {
            strcpy(h->name, req->name);
            if (h->file == 0) {
                h->op = FILESYS_open(h->name, 1, priority, h);
                if (h->op != 0) {
                    FILESYS_callbackop(h->op, opencallback);
                }
            } else {
                h->op = FILESYS_close(h->file, priority, h);
                if (h->op != 0) {
                    FILESYS_callbackop(h->op, closecallback);
                }
            }
            return;
        }
    }
    restartstream(h, priority);
}

// Frees what has been consumed and reads the next block if there is room
// (wrapping to the buffer start when the end is near); otherwise the stream
// waits (STREAMSTATE_FULL) until chunks are released.
static void restartstream(STREAMHEADERtag* h, int priority) {
    u8* p;
    int space;
    REQUESTSTRUCTtag* req;

    while ((p = h->read) != h->parse) {
        u32 tag = getle(p);
        if (tag == CHUNK_WRAP) {
            h->read = h->bufbase;
        } else if (tag == CHUNK_FREE) {
            h->read += getle(p + 4);
        } else {
            break;
        }
    }

    MUTEX_lock(h->mutex);
    for (;;) {
        REQUESTSTRUCTtag* r = h->head->next;
        if (r == NULL || r->state == REQSTATE_QUEUED) {
            break;
        }
        if (inspan(r->start - 1, h->read, h->write)) {
            break;
        }
        freerequest(h, h->head);
    }
    MUTEX_unlock(h->mutex);

    if (h->read > h->write) {
        space = h->read - h->write - 0x21;
    } else {
        space = h->bufend - h->write - 0x20;
        if (space < h->blocksize) {
            int partial = h->write - h->parse;
            int misalign;
            int type = h->cur->type;
            if (type == 1) {
                if (h->read - h->bufbase < partial + 1) {
                    h->state = STREAMSTATE_FULL;
                    return;
                }
            } else if (h->read - h->bufbase - 0x20 < partial + 1) {
                h->state = STREAMSTATE_FULL;
                return;
            }
            misalign = partial % 32;
            if (misalign == 0 || type == 1) {
                h->bufbase = h->bufstart;
            } else {
                h->bufbase = h->bufstart + 0x20 - misalign;
            }
            MEM_copy(h->bufbase, h->parse, partial);
            putle(h->parse, CHUNK_WRAP);
            putle(h->parse + 4, 8);
            h->parse = h->bufbase;
            h->write = h->parse + partial;
            p = h->read;
            if (getle(p) == CHUNK_WRAP) {
                h->read = h->bufbase;
                space = h->bufend - h->write - 0x20;
            } else {
                space = p - h->write - 1;
            }
        }
    }

    if (space < h->blocksize) {
        h->state = STREAMSTATE_FULL;
        return;
    }
    req = h->cur;
    if (req->type == 1) {
        if (h->offset + space > req->offset) {
            h->readsize = req->offset - h->offset;
        } else {
            h->readsize = space;
        }
        MEM_copy(h->write, req->mem, h->readsize);
        req->mem += h->readsize;
        readcallback(0, 0, h);
    } else {
        h->readsize = h->blocksize;
        h->op = FILESYS_read(h->file, h->offset, h->write, h->readsize, priority, h);
        if (h->op != 0) {
            FILESYS_callbackop(h->op, readcallback);
        }
    }
}

// The original doesn't inline these into each other.
#pragma push
#pragma auto_inline off

extern "C" {

int STREAM_overhead(int numrequests, int numfilters, int numtracks) {
    return numrequests * sizeof(REQUESTSTRUCTtag) + numfilters * sizeof(STREAMFILTER) +
           numtracks * sizeof(STREAMTRACK) + sizeof(STREAMHEADERtag) + 0x20;
}

int STREAM_create(int numrequests, int numfilters, int numtracks, void* buffer, int bufferSize) {
    STREAMHEADERtag* h = (STREAMHEADERtag*)buffer;
    int size = bufferSize - STREAM_overhead(numrequests, numfilters, numtracks);
    int i;

    if (size < 0x1800) {
        return 0;
    }
    if (numrequests < 2) {
        return 0;
    }
    if (numrequests > 0x100) {
        return 0;
    }
    if (numfilters < 1 || numfilters > 0x10) {
        return 0;
    }
    if (numtracks < 1 || numtracks > numfilters) {
        return 0;
    }

    h->magic = STREAM_MAGIC;
    MUTEX_create(h->mutex);
    h->requests = (REQUESTSTRUCTtag*)(h + 1);
    h->numrequests = numrequests;
    h->filters = (STREAMFILTER*)(h->requests + numrequests);
    h->numfilters = numfilters;
    h->tracks = (STREAMTRACK*)(h->filters + numfilters);
    h->numtracks = numtracks;
    h->bufstart = (u8*)(((uptr)((h->tracks + numtracks)) & ~31) + 0x20);
    h->bufbase = h->bufstart;
    h->bufend = (u8*)buffer + bufferSize;
    h->state = STREAMSTATE_IDLE;
    h->priority = 150;
    h->greedypriority = 50;
    h->greedylevel = 0;
    h->greedy = 0;
    h->buffered = 0;
    h->read = h->bufbase;
    h->parse = h->bufbase;
    h->write = h->bufbase;
    h->head = NULL;
    h->cur = NULL;
    h->tail = NULL;
    h->free = h->requests;
    MEM_clear(h->name, 0xFF);
    h->file = 0;

    if (size < 0x4000) {
        h->blocksize = 0x800;
    } else if (size < 0x8000) {
        h->blocksize = 0x1000;
    } else if (size < 0x10000) {
        h->blocksize = 0x2000;
    } else if (size < 0x20000) {
        h->blocksize = 0x4000;
    } else {
        h->blocksize = 0x8000;
    }

    for (i = 0; i < numrequests; i++) {
        h->requests[i].id = i;
        h->requests[i].state = REQSTATE_FREE;
        h->requests[i].next = &h->requests[i + 1];
    }
    h->requests[numrequests - 1].next = NULL;
    for (i = 0; i < numfilters; i++) {
        h->filters[i].mask = 0;
        h->filters[i].value = 0;
        h->filters[i].track = 1;
    }
    for (i = 0; i < numtracks; i++) {
        h->tracks[i].header = h;
        h->tracks[i].num = i + 1;
        h->tracks[i].bytes = 0;
    }
    return (int)PTR_TO_ADDR(h->tracks);
}

void STREAM_setfilter(int stream, int filter, int mask, int value, int dest) {
    STREAMHEADERtag* h = getheader(stream);
    STREAMFILTER* f;
    if (h == NULL) {
        return;
    }
    if (filter < 1 || filter > h->numfilters) {
        return;
    }
    if (filter == h->numfilters && (mask | value) != 0) {
        return;
    }
    if (!(dest >= 1 || dest == -1 || dest == -2)) {
        return;
    }
    if (dest > h->numtracks) {
        return;
    }
    if (h->state != STREAMSTATE_IDLE) {
        return;
    }
    f = &h->filters[filter - 1];
    f->mask = mask;
    f->value = value;
    f->track = dest;
}

void STREAM_destroy(int stream) {
    STREAMHEADERtag* h = getheader(stream);
    if (h == NULL) {
        return;
    }
    STREAM_kill(stream);
    while (h->state == STREAMSTATE_READING) {
        if (THREAD_iscurrent(0)) {
            SYNCTASK_run(0);
        }
        THREAD_yield(0);
    }
    h->magic = 0;
    MUTEX_destroy(h->mutex);
    if (h->file != 0) {
        FILESYS_closesync(h->file, 100);
    }
}

void STREAM_setpriority(int stream, int priority, int greedypriority) {
    STREAMHEADERtag* h = getheader(stream);
    if (h == NULL) {
        return;
    }
    h->priority = priority;
    h->greedypriority = greedypriority;
}

void STREAM_setgreedylevel(int stream, int level) {
    STREAMHEADERtag* h = getheader(stream);
    int old;
    if (h == NULL) {
        return;
    }
    old = h->greedylevel;
    h->greedylevel = level;
    if ((h->buffered < old) != (h->buffered < level)) {
        STREAM_setgreedystate(stream, h->buffered < level);
    }
}

void STREAM_setgreedystate(int stream, int greedy) {
    STREAMHEADERtag* h = getheader(stream);
    if (h == NULL) {
        return;
    }
    h->greedy = greedy;
    if (greedy != 0 && h->state == STREAMSTATE_READING) {
        FILESYS_priorityop(h->op, h->greedypriority);
    }
}

int STREAM_taphandle(int stream, int track) {
    STREAMHEADERtag* h = getheader(stream);
    if (h == NULL) {
        return 0;
    }
    if (track < 1 || track > h->numtracks) {
        return 0;
    }
    return (int)PTR_TO_ADDR(&h->tracks[track - 1]);
}

int STREAM_queuefile(int stream, const char* name, int offset, int tag) {
    STREAMHEADERtag* h = getheader(stream);
    REQUESTSTRUCTtag* r;
    int state;

    if (h == NULL) {
        return 0;
    }
    r = allocrequest(h);
    if (r == NULL) {
        return 0;
    }
    r->type = 0;
    strncpy(r->name, name, 0xFE);
    r->offset = offset;
    r->tag = tag;
    r->state = REQSTATE_QUEUED;
    r->next = NULL;
    addrequest(h, r);

    MUTEX_lock(h->mutex);
    state = h->state;
    if (state == STREAMSTATE_IDLE) {
        h->state = STREAMSTATE_READING;
    }
    MUTEX_unlock(h->mutex);
    if (state == STREAMSTATE_IDLE) {
        if (h->greedy != 0) {
            startnextrequest(h, h->greedypriority);
        } else {
            startnextrequest(h, h->priority);
        }
    }
    return r->id;
}

int STREAM_queuemem(int stream, void* data, int size, int tag) {
    STREAMHEADERtag* h = getheader(stream);
    REQUESTSTRUCTtag* r;
    int state;

    if (h == NULL) {
        return 0;
    }
    r = allocrequest(h);
    if (r == NULL) {
        return 0;
    }
    if (size == 0) {
        // Up to and including the end chunk.
        u8* p = (u8*)data;
        while (tag != (int)getle(p)) {
            size += getle(p + 4);
            p += getle(p + 4);
        }
        size += getle(p + 4);
    }
    r->type = 1;
    r->mem = (u8*)data;
    r->offset = size;
    r->tag = tag;
    r->state = REQSTATE_QUEUED;
    r->next = NULL;
    addrequest(h, r);

    MUTEX_lock(h->mutex);
    state = h->state;
    if (state == STREAMSTATE_IDLE) {
        h->state = STREAMSTATE_READING;
    }
    MUTEX_unlock(h->mutex);
    if (state == STREAMSTATE_IDLE) {
        startnextrequest(h, 0);
    }
    return r->id;
}

// Cancels a request: a queued one is just freed; for one being read or
// already read, its chunks are dropped from every track.
void STREAM_cancelrequest(int stream, int id) {
    STREAMHEADERtag* h = getheader(stream);
    REQUESTSTRUCTtag* r;
    u8* read = NULL;
    u8* start = NULL;
    u8* end = NULL;
    int skip;
    int i;

    if (h == NULL) {
        return;
    }
    MUTEX_lock(h->mutex);
    if ((id & 0xFF) >= h->numrequests) {
        r = NULL;
    } else {
        r = &h->requests[id & 0xFF];
        if (id != r->id) {
            r = NULL;
        } else if (r->state == REQSTATE_FREE) {
            r = NULL;
        }
    }
    if (r == NULL || r->state == REQSTATE_CANCELLED) {
        skip = 1;
    } else if (r->state == REQSTATE_QUEUED) {
        freerequest(h, r);
        skip = 1;
    } else {
        r->state = REQSTATE_CANCELLED;
        read = h->read;
        if (r == h->head) {
            start = read;
        } else {
            start = r->start;
        }
        if (r->next == NULL || r->next->state == REQSTATE_QUEUED) {
            end = h->parse;
        } else {
            end = r->next->start;
        }
        skip = 0;
    }
    MUTEX_unlock(h->mutex);
    if (skip) {
        return;
    }

    for (i = 0; i < h->numtracks; i++) {
        STREAMTRACK* t = &h->tracks[i];
        if (t->bytes <= 0) {
            continue;
        }
        if (inspan(t->next, read, start)) {
            // Older chunks come first: drop the request's chunks in place.
            u32 tracktag = t->num << 24;
            u8* p = start;
            while (p != end) {
                u32 size;
                u32 n;
                if (getle(p) == CHUNK_WRAP) {
                    p = h->bufbase;
                    continue;
                }
                size = getle(p + 4);
                n = size & 0xFFFFFF;
                if ((size & 0xFF000000) == tracktag) {
                    MUTEX_lock(h->mutex);
                    t->bytes -= n;
                    MUTEX_unlock(h->mutex);
                    unbuffer(h, n);
                    putle(p, CHUNK_FREE);
                    putle(p + 4, n);
                }
                p += n;
            }
        } else if (inspan(t->next, start, end)) {
            do {
                STREAM_release((int)PTR_TO_ADDR(t), STREAM_get((int)PTR_TO_ADDR(t)));
            } while (t->bytes > 0 && inspan(t->next, start, end));
        }
    }
}

// Cancels everything and drops all buffered data.
void STREAM_kill(int stream) {
    STREAMHEADERtag* h = getheader(stream);
    REQUESTSTRUCTtag* r;
    u8* p;
    u32 n = 0;
    int old;
    int now;
    int i;

    if (h == NULL) {
        return;
    }
    r = h->tail;
    if (r == NULL) {
        return;
    }
    while (r->state == REQSTATE_QUEUED || r->state == REQSTATE_READING) {
        STREAM_cancelrequest(stream, r->id);
        r = h->tail;
    }
    while (h->head != h->cur) {
        freerequest(h, h->head);
    }
    h->cur->state = REQSTATE_CANCELLED;
    for (i = 0; i < h->numtracks; i++) {
        h->tracks[i].bytes = 0;
    }

    old = h->buffered;
    MUTEX_lock(h->mutex);
    i = h->buffered;
    now = i - old;
    h->buffered = now;
    MUTEX_unlock(h->mutex);
    if (i >= h->greedylevel && now < h->greedylevel) {
        h->greedy = 1;
        if (h->state == STREAMSTATE_READING) {
            FILESYS_priorityop(h->op, h->greedypriority);
        }
    }

    p = h->read;
    while (p != h->parse) {
        u32 size;
        if (getle(p) == CHUNK_WRAP) {
            p = h->bufbase;
            continue;
        }
        size = getle(p + 4);
        putle(p, CHUNK_FREE);
        n = size & 0xFFFFFF;
        putle(p + 4, n);
        p += n;
    }

    if (h->state == STREAMSTATE_FULL) {
        if (p == h->bufbase) {
            h->parse = h->bufstart;
            h->bufbase = h->bufstart;
        } else {
            // Pad the last chunk so the next data starts aligned.
            u32 pad = 32 - ((uptr)(h->parse) & 31);
            if (pad == 32) {
                pad = 0;
            }
            n += pad;
            putle(h->parse - (n - pad) + 4, n);
            h->parse += pad;
        }
        h->state = STREAMSTATE_IDLE;
    }
}

// The track's next chunk (its header first), or NULL.
void* STREAM_get(int stream) {
    STREAMHEADERtag* h = getheader(stream);
    STREAMTRACK* t = ADDR_TO_PTR(STREAMTRACK, stream);
    u8* p;
    u32 n;
    int left;

    if (h == NULL) {
        return NULL;
    }
    if (t->bytes == 0) {
        return NULL;
    }
    p = t->next;
    n = getle(p + 4) & 0xFFFFFF;
    putle(p + 4, n);
    MUTEX_lock(h->mutex);
    left = t->bytes - n;
    t->bytes = left;
    MUTEX_unlock(h->mutex);
    if (left > 0) {
        u32 tracktag = t->num << 24;
        u8* q = p + n;
        u32 size;
        while (((size = getle(q + 4)) & 0xFF000000) != tracktag) {
            if (getle(q) == CHUNK_WRAP) {
                q = h->bufbase;
            } else {
                q += size & 0xFFFFFF;
            }
        }
        t->next = q;
    }
    return p;
}

void STREAM_release(int stream, void* chunk) {
    STREAMHEADERtag* h = getheader(stream);
    u8* p = (u8*)chunk;
    int state;

    if (h == NULL) {
        return;
    }
    if (p < h->bufbase || p > h->bufend - 8) {
        return;
    }
    if (getle(p) == CHUNK_FREE) {
        return;
    }
    putle(p, CHUNK_FREE);
    unbuffer(h, getle(p + 4));

    MUTEX_lock(h->mutex);
    state = h->state;
    if (state == STREAMSTATE_FULL) {
        h->state = STREAMSTATE_READING;
    }
    MUTEX_unlock(h->mutex);
    if (state == STREAMSTATE_FULL) {
        if (h->greedy != 0) {
            restartstream(h, h->greedypriority);
        } else {
            restartstream(h, h->priority);
        }
    }
}

int STREAM_gettable(int stream) {
    STREAMHEADERtag* h = getheader(stream);
    if (h == NULL) {
        return 0;
    }
    return (ADDR_TO_PTR(STREAMTRACK, stream))->bytes;
}

int STREAM_state(int stream) {
    STREAMHEADERtag* h = getheader(stream);
    if (h == NULL) {
        return 0;
    }
    return h->state;
}

u8 STREAM_isendofstream(int stream) {
    STREAMHEADERtag* h = getheader(stream);
    if (h == NULL) {
        return 0;
    }
    if (h->state != STREAMSTATE_IDLE) {
        return 0;
    }
    if ((ADDR_TO_PTR(STREAMTRACK, stream))->bytes != 0) {
        return 0;
    }
    return 1;
}

int STREAM_buffersize(int stream) {
    STREAMHEADERtag* h = getheader(stream);
    if (h == NULL) {
        return 0;
    }
    return h->bufend - h->bufstart;
}

}  // extern "C"

#pragma pop
