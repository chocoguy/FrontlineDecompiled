// PATH, EA's interactive music player (see path.h). The API locks the
// player with `pfactive` (the timer and task services skip a locked
// player) and works on the projects a handle selects; `pfstate` is the
// project being worked on. Tracks are serviced either by the 100 Hz timer
// (low latency) or by the sound task.
#include "game/path.h"
#include "game/pathxSND.h"
#include "stdio.h"
#include "string.h"

PATHPROJECT pfstates[PATH_MAXPROJECTS];
s8 volscale = 100;
u8 bankservice = 0x20;  // 0x20 task, 0x42 timer
PATHPROJECT* pfstate;
void (*songprogress)(u32 id, s16 node);
s8 inited;
s8 paused;
u32 pfactive;
u32 milliseconds;

#define PATH_NODE(i) ((PATHFINDNODE*)((u32*)pfstate->map + pfstate->nodeOffsets[i]))

// Locks the player; false when it isn't set up or is already locked.
#define PATH_LOCK() (inited && pfactive == 0 && (pfactive = (u32)-1) != 0)

static inline void PATH_UNLOCK() {
    pfactive = 0;
    pfstate = NULL;
}

// Makes project `i` the one worked on; returns its id (0: none).
static inline u32 PATHI_setproject(int i) {
    if (pfactive == 0) {
        pfactive = (u32)-1;
    }
    if (i >= 0 && i < PATH_MAXPROJECTS && pfstates[i].map != NULL) {
        u32 id = pfstates[i].id;
        pfstate = &pfstates[i];
        pfactive = id;
        return id;
    }
    if (i != -1) {
        pfactive = 0;
    }
    return pfactive;
}

static inline int PATHI_matches(PATHPROJECT* p, u32 handle) {
    return p->map != NULL && (handle & PATH_INSTANCEMASK & p->id) && (handle & PATH_PROJECTMASK & p->id);
}

// The tracks of projects whose bits are in `projects`.
static inline int PATHI_counttracks(u32 projects) {
    int count = 0;
    for (int i = 0; i < PATH_MAXPROJECTS; i++) {
        if (((0x01000000 << i) & projects) && pfstates[i].map != NULL) {
            for (int j = 0; j < PATH_MAXTRACKS; j++) {
                if (pfstates[i].tracks[j] != NULL) {
                    count++;
                }
            }
        }
    }
    return count;
}

// The first track of `handle`, NULL if none.
static inline PATHTRACK* PATHI_findtrack(u32 handle) {
    for (u32 i = 0; i < PATH_MAXPROJECTS; i++) {
        PATHPROJECT* p = &pfstates[i];
        if (p->map != NULL && (p->id & (handle & PATH_INSTANCEMASK)) && (p->id & (handle & PATH_PROJECTMASK))) {
            for (int j = 0; j < PATH_MAXTRACKS; j++) {
                if ((1 << j) & (handle & PATH_TRACKMASK)) {
                    return pfstates[i].tracks[j];
                }
            }
        }
    }
    return NULL;
}

// The lead track: the playing (unpaused, on a node) track with the most
// latency, and its node's beat.
static inline void PATHI_findlead() {
    pfstate->leadLatency = 0;
    pfstate->leadTrack = -1;
    pfstate->leadBeat = 0;
    for (int i = 0; i < PATH_MAXTRACKS; i++) {
        PATHTRACK* t = pfstate->tracks[i];
        if (t != NULL && t->paused == 0) {
            u8 playing = 0;
            if (t->cur.node >= 0 && t->node != NULL) {
                playing = 1;
            }
            if (playing && pfstate->leadLatency < (u32)t->latency) {
                pfstate->leadLatency = t->latency;
                pfstate->leadTrack = i;
                pfstate->leadBeat = t->node->beat;
            }
        }
    }
}

int PATH_fade(int track, int volume, int ms) {
    if (!PATH_LOCK()) {
        return;
    }
    s8 level = 1.27f * (float)volume;
    for (int i = 0; i < PATH_MAXPROJECTS; i++) {
        if (PATHI_matches(&pfstates[i], track) && PATHI_setproject(i) != 0) {
            for (int j = 0; j < PATH_MAXTRACKS; j++) {
                PATHTRACK* t = pfstate->tracks[j];
                if (t != NULL && (track & (1 << j))) {
                    int curve = pfstate->fadeCurve;
                    char skip = 0;
                    t->fadeTo = volume;
                    t->cur.level = level;
                    int v = t->cur.level * t->volume / 100;
                    if (curve >= 0) {
                        t->fadeCurve = curve;
                        skip = 1;
                    } else if ((u32)pfstate->fadeCurve > 1) {
                        t->fadeCurve = 0;
                        skip = 1;
                    }
                    PATHX_ifade(t->pathx, v, ms, skip);
                }
            }
        }
    }
    PATH_UNLOCK();
}

int PATH_volume(int track, char volume) {
    if (!PATH_LOCK()) {
        return;
    }
    if (volume > 100) {
        volume = 100;
    }
    if (volume < 0) {
        volume = 0;
    }
    if (track == -1) {
        volscale = volume;
    }
    for (int i = 0; i < PATH_MAXPROJECTS; i++) {
        if (PATHI_matches(&pfstates[i], track) && PATHI_setproject(i) != 0) {
            for (int j = 0; j < PATH_MAXTRACKS; j++) {
                PATHTRACK* t = pfstate->tracks[j];
                if (t != NULL && (track & (1 << j))) {
                    t->volume = volume;
                    if (t->fadeTo < 0) {
                        PATHX_volume(t->pathx, t->cur.level * volume / 100);
                    }
                }
            }
        }
    }
    PATH_UNLOCK();
}

// Stops every track of the matching projects.
int PATH_stop(int track) {
    if (!PATH_LOCK()) {
        return;
    }
    for (int i = 0; i < PATH_MAXPROJECTS; i++) {
        if (PATHI_matches(&pfstates[i], track) && PATHI_setproject(i) != 0) {
            for (int j = 0; j < PATH_MAXTRACKS; j++) {
                PATHTRACK* t = pfstate->tracks[j];
                if (t != NULL) {
                    PATHX_stop(t->pathx);
                    t->cur.node = -1;
                }
            }
        }
    }
    PATH_UNLOCK();
}

int PATH_pause(int track, char pause) {
    if (!PATH_LOCK()) {
        return;
    }
    if (track == -1) {
        paused = pause;
    }
    for (u8 i = 0; i < PATH_MAXPROJECTS; i++) {
        if (PATHI_matches(&pfstates[i], track) && PATHI_setproject(i) != 0) {
            for (int j = 0; j < PATH_MAXTRACKS; j++) {
                PATHTRACK* t = pfstate->tracks[j];
                if (t != NULL && (track & (1 << j))) {
                    PATHX_pause(t->pathx, pause);
                    t->paused = pause;
                }
            }
        }
    }
    PATH_UNLOCK();
}

// Destroys the first matching track (and its project once it has none).
// As the original: with no matching track, returns the last track slot it
// looked at.
void* PATH_destroy(int track) {
    PATHTRACK* t = NULL;
    if (!PATH_LOCK()) {
        return NULL;
    }
    for (u8 i = 0; i < PATH_MAXPROJECTS; i++) {
        if (PATHI_matches(&pfstates[i], track) && PATHI_setproject(i) != 0) {
            for (int j = 0; j < PATH_MAXTRACKS; j++) {
                t = pfstate->tracks[j];
                if (t != NULL && (track & (1 << j))) {
                    pfstate->tracks[j] = NULL;
                    PATHX_destroytrack(t->pathx);
                    if (PATHI_counttracks(0x01000000 << i) == 0) {
                        pfstate->map = NULL;
                        PATHI_sortprojects();
                    }
                    goto done;
                }
            }
        }
    }
done:
    if (inited && PATHI_counttracks(PATH_PROJECTMASK) == 0) {
        PATHX_endservice();
    }
    PATH_UNLOCK();
    return t;
}

// Queues a jump of the node's track to `node`.
int PATH_jump(int track, int node, char sync) {
    PATHFINDEVENT ev;
    if (!PATH_LOCK()) {
        return -0x12;
    }
    for (u32 i = 0; i < PATH_MAXPROJECTS; i++) {
        if (PATHI_matches(&pfstates[i], track) && PATHI_setproject(i) != 0) {
            if (pfstate->leadTrack < 0) {
                sync = 0x40;
            }
            if (node < 0 || node > pfstate->map->numNodes) {
                break;
            }
            for (int j = 0; j < PATH_MAXTRACKS; j++) {
                u32 bit = 1 << j;
                if (track & bit) {
                    PATHFINDNODE* n = PATH_NODE(node);
                    if (n == NULL || (u32)(1 << n->track) != bit) {
                        goto done;
                    }
                    ev.priority = 1;
                    ev.hold = 0;
                    ev.event = -1 - node;
                    ev.track = j;
                    ev.mask = ~bit;
                    ev.time = 0;
                    ev.expire = 0;
                    switch (sync) {
                    case 0:
                        ev.sync = 0;
                        break;
                    case 0x10:
                        ev.sync = 0x10;
                        break;
                    case 0x20:
                        ev.sync = 0x20;
                        break;
                    default:
                        ev.sync = 0x40;
                        break;
                    }
                    pfstate->events[0] = ev;
                    pfstate->events[1].mask = 0;
                }
            }
        }
    }
done:
    PATH_UNLOCK();
    return 0;
}

// Queues `event` (by priority) for the matching tracks, with the levels
// and nodes the map gives each track for the lead track's beat. Priority
// 2 stops and unpauses everything first when it is the next event.
int PATH_event(int track, short event, int flags) {
    PATHFINDEVENT ev;
    PATHEVENTVALUE value;
    int priority = flags & ~0x78;
    s8 hold = flags & 8;
    s8 sync = flags & 0x70;
    int slot = PATH_MAXEVENTS;
    u8 index = event;
    if (!PATH_LOCK()) {
        return -0x12;
    }
    u32 projectBit = 0x01000000 << ((event >> 8) & 0xF);
    for (u32 i = 0; i < PATH_MAXPROJECTS; i++) {
        PATHPROJECT* p = &pfstates[i];
        if (p->map == NULL || !(projectBit & p->id) || !(track & PATH_PROJECTMASK & p->id) ||
            !(track & PATH_INSTANCEMASK & p->id)) {
            continue;
        }
        if (PATHI_setproject(i) == 0) {
            continue;
        }
        if (index >= pfstate->map->numEvents) {
            break;
        }
        PATHI_findlead();
        ev.priority = priority;
        ev.hold = 0;
        ev.event = index;
        ev.sync = sync;
        ev.track = 0;
        ev.time = 0;
        ev.expire = 0;
        ev.mask = (u32)-1;
        for (int j = 0; j < PATH_MAXTRACKS; j++) {
            if (pfstate->tracks[j] == NULL) {
                continue;
            }
            u32 bit = 1 << j;
            if (!(track & bit)) {
                continue;
            }
            PATHMAP* map = pfstate->map;
            u8 sections = map->numSections;
            value = pfstate->values[pfstate->eventTable[index * sections + sections * (j * map->numEvents) +
                                                        pfstate->leadBeat]];
            if ((value.flags & 3) || value.level >= 0) {
                ev.mask -= bit;
            }
            if (hold != 0 || (value.flags & 8)) {
                ev.hold = 1;
            }
            if (sync == 0) {
                ev.sync = value.flags & 0x70;
            }
        }
        if (flags == -1) {
            PATHI_eventrestore(index);
            break;
        }
        for (int k = 0; k < PATH_MAXEVENTS; k++) {
            if (pfstate->events[k].mask == 0 || pfstate->events[k].priority < ev.priority) {
                slot = k;
                break;
            }
        }
        if (slot < PATH_MAXEVENTS) {
            for (int k = PATH_MAXEVENTS - 1; k > slot; k--) {
                pfstate->events[k] = pfstate->events[k - 1];
            }
            pfstate->events[slot] = ev;
            if (ev.priority == 2) {
                pfstate->events[slot].priority = 1;
                if (slot == 0) {
                    PATH_UNLOCK();
                    PATH_stop(-1);
                    PATH_pause(-1, 0);
                }
            }
        }
    }
    PATH_UNLOCK();
    return 0;
}

int PATH_control(int track, int level) {
    if (!PATH_LOCK()) {
        return -0x12;
    }
    if (level >= 0 && level <= 0x7F) {
        for (u8 i = 0; i < PATH_MAXPROJECTS; i++) {
            if (PATHI_matches(&pfstates[i], track) && PATHI_setproject(i) != 0) {
                for (int j = 0; j < PATH_MAXTRACKS; j++) {
                    if (pfstate->tracks[j] != NULL) {
                        pfstate->tracks[j]->control = level;
                    }
                }
            }
        }
    }
    PATH_UNLOCK();
    return 0;
}

int PATH_latency(int track, unsigned short ms) {
    PATHTRACK* t = PATHI_findtrack(track);
    if (t == NULL) {
        return -8;
    }
    t->latency = ms;
    return 0;
}

// The buffer size a track needs: the track and its request ring, plus 16
// (as the original, even when already aligned).
int PATH_overheadtrack(int numRequests, int stream) {
    int size = PATHX_overheadtrack(numRequests, stream) + sizeof(PATHTRACK) + numRequests * sizeof(PATHREQUEST);
    return size + (0x10 - size % 0x10);
}

int PATH_createtrackfrombuf(int id, int index, const char* file, int latency, SNDPLAYOPTS* opts, int numRequests,
                            int stream, void* buffer, int bufferSize, void* bank) {
    int result;
    if (!PATH_LOCK()) {
        return -0x12;
    }
    for (int i = 0; i < PATH_MAXPROJECTS; i++) {
        if (pfstates[i].map != NULL && (u32)id == pfstates[i].id) {
            PATHI_setproject(i);
            break;
        }
    }
    if (pfstate->tracks[index] != NULL) {
        result = -0xD;
    } else {
        // The track, its request ring, then (16-byte aligned) PATHX's part.
        PATHTRACK* t = (PATHTRACK*)buffer;
        u8* mem = (u8*)buffer + sizeof(PATHTRACK);
        t->requests = (PATHREQUEST*)mem;
        mem += numRequests * sizeof(PATHREQUEST);
        mem += 0x10 - (int)(mem - (u8*)buffer) % 0x10;
        bufferSize -= mem - (u8*)buffer;
        for (int k = 0; k < numRequests; k++) {
            t->requests[k].request = -1;
            t->requests[k].node = 0;
            t->requests[k].length = 0;
            t->requests[k].beat = 0;
        }
        PATHEVENTVALUE none;
        none.level = -1;
        none.flags = 0;
        none.node = -1;
        t->index = index;
        t->control = pfstate->control;
        t->paused = 0;
        t->volume = volscale;
        t->fadeStart = 0;
        t->fadeTo = -1;
        t->fadeFrom = -1;
        t->fadeCurve = -1;
        t->node = NULL;
        t->latency = (u16)latency;
        t->numRequests = numRequests;
        t->cur = none;
        t->cur.level = 0x7F;
        t->saved = none;
        t->unk_0x28 = none;
        t->eventId = -1;
        t->timerService = latency < 50;
        t->beat = -(t->timerService == 0);
        t->unk_bits = 0;
        t->loopNode = -1;
        t->loops = 0;
        t->startTime = 0;
        t->beatLength = 0;
        t->barLength = 0;
        t->releaseTime = 0;
        t->request = 0;
        t->requestIndex = 0;
        strcpy(t->file, file);

        // The PATHX name: the instance number and the file name, lower
        // case letters and digits only, 8 characters at most.
        const char* p = file + strlen(file);
        while (p >= file && *p != '\\' && *p != '/' && *p != ':') {
            p--;
        }
        int instance = 0;
        if (!((1 << 28) & id)) {
            instance = 1;
            if (!((1 << 29) & id)) {
                instance = 2;
                if (!((1 << 30) & id)) {
                    instance = 3;
                    if (!((1 << 31) & id)) {
                        instance = 4;
                    }
                }
            }
        }
        sprintf(t->name, "%x%s", instance + 1, p + 1);
        s8 c = 'a';
        for (char* q = t->name + 1; q < t->name + sizeof(t->name); q++) {
            u8 more = 0;
            if (c != 0 && *q != 0) {
                more = 1;
            }
            if (more) {
                c = *q | 0x20;
            } else {
                c = 0;
            }
            if ((c < 'a' || c > 'z') && (c < '0' || c > '9')) {
                c = 0;
                if (q >= t->name + 8) {
                    *q = 0;
                    break;
                }
                *q = ' ';
            }
        }
        pfstate->tracks[index] = t;
        if (PATHI_counttracks(PATH_PROJECTMASK) == 1) {
            PATHX_initservice(PATHI_servicetask, PATHI_servicetimer);
        }
        t->name[8] = 0;
        t->pathx = PATHX_createtrack(file, t->name, opts, numRequests, stream, mem, bufferSize, bank);
        if (t->pathx < 0) {
            pfstate->tracks[index] = NULL;
            result = t->pathx;
        } else {
            t->request = -1;
            result = id | (1 << index);
        }
    }
    PATH_UNLOCK();
    return result;
}

int PATH_createtrack(int id, int index, const char* file, int latency, SNDPLAYOPTS* opts, int numRequests,
                     int stream, void* buffer, int bufferSize) {
    return PATH_createtrackfrombuf(id, index, file, latency, opts, numRequests, stream, buffer, bufferSize, NULL);
}

// Registers a loaded map; another copy of a loaded project gets the next
// instance bit. Returns the project id, -1 when it can't be added.
int PATH_addmapfile(void* data) {
    PATHMAP* map = (PATHMAP*)data;
    int slot = PATH_MAXPROJECTS;
    int result = -1;
    if (!inited && pfactive == 0) {
        for (int i = 0; i < PATH_MAXPROJECTS; i++) {
            pfstates[i].map = NULL;
            for (int j = 0; j < PATH_MAXTRACKS; j++) {
                pfstates[i].tracks[j] = NULL;
            }
        }
        pfstate = NULL;
        inited = 1;
        paused = 0;
    }
    if (pfactive != 0) {
        return -0x12;
    }
    u8 id = map->id;
    int copies = 0;
    for (int i = 0; i < PATH_MAXPROJECTS; i++) {
        if (pfstates[i].map == NULL) {
            if (slot == PATH_MAXPROJECTS) {
                slot = i;
            }
        } else if (id == pfstates[i].map->id) {
            copies++;
        }
    }
    if (slot < PATH_MAXPROJECTS && map->magic == 0x50464478 && map->version == 3 && map->subversion == 4) {
        pfstates[slot].map = map;
        pfstates[slot].id = (0x01000000 << id) | (0x10000000 << copies);
        for (int i = 0; i < PATH_MAXPROJECTS; i++) {
            if (pfstates[i].map != NULL && pfstates[slot].id == pfstates[i].id) {
                PATHI_setproject(i);
                break;
            }
        }
        result = id;
        PATHPROJECT* p = pfstate;
        p->nodeOffsets = pfstate->map->nodeOffsets;
        pfstate->nodesEnd = pfstate->nodeOffsets + pfstate->map->numNodes;
        map = pfstate->map;
        PATHFINDNODE* last = (PATHFINDNODE*)((u32*)map + pfstate->nodeOffsets[map->numNodes - 1]);
        pfstate->eventTable = (u8*)last + last->numBranches * sizeof(PATHBRANCH) + 0xC;
        pfstate->values =
            (PATHEVENTVALUE*)(pfstate->eventTable + ((map->numTracks * (map->numEvents * map->numSections) + 3) & ~3));
        pfstate->tables = (u32*)(pfstate->values + pfstate->map->numValues);
        pfstate->fadeOffsets = (u32*)pfstate->map + pfstate->tables[pfstate->map->tableIndex];
        pfstate->waves = (s32(*)[2])(pfstate->fadeOffsets + pfstate->map->numTracks + 1);
        pfstate->fades = (u32*)pfstate->map + pfstate->fadeOffsets[pfstate->map->numTracks];
        pfstate->events[0].mask = 0;
        pfstate->events[1].mask = 0;
        pfstate->taskLatency = 50;
        pfstate->timerLatency = 10;
        pfstate->fadeTime = 1000;
        pfstate->fadeCurve = 1;
        pfstate->control = 0;
        for (int i = 0; i < pfstate->map->numNodes; i++) {
            PATHFINDNODE* n = PATH_NODE(i);
            if (n->bars == 0) {
                n->bars = 1;
            }
        }
    }
    PATH_UNLOCK();
    return result;
}

// The 100 Hz timer and the sound task: each services the tracks it owns
// (the lead track first).
static inline void PATHI_service(u8 service) {
    if (paused || !PATH_LOCK()) {
        return;
    }
    bankservice = service;
    milliseconds = PATHX_milliseconds();
    PATHX_statusall(0);
    for (u8 i = 0; i < PATH_MAXPROJECTS; i++) {
        if (pfstates[i].map != NULL && PATHI_setproject(i) != 0) {
            PATHI_findlead();
            PATHI_serviceproject(i, 1);
            PATHI_serviceproject(i, 0);
        }
    }
    PATHX_statusall(1);
    PATH_UNLOCK();
}

void PATHI_servicetimer() {
    PATHI_service(0x42);
}

void PATHI_servicetask() {
    PATHI_service(0x20);
}

// The ms of audio a track has queued.
static inline int PATHI_queuedtime(PATHTRACK* t) {
    int total = 0;
    for (u32 k = 0; k < t->numRequests; k++) {
        int left = PATHX_timeremaining(t->pathx, k);
        if (left >= 0) {
            total += left;
        }
    }
    return total;
}

// The ms until the track reaches the next point the event can start at:
// the next beat or bar, or the end of its node's remaining beats.
static inline int PATHI_synctime(PATHTRACK* t, int queued, PATHFINDEVENT* ev) {
    u32 sync = ev->sync;
    PATHFINDNODE* n = PATH_NODE(t->cur.node);
    if ((sync == 0x10 || sync == 0x20) && n->wave >= 0) {
        int played;
        int left;
        PATHX_status(t->pathx, &played, &left);
        queued = left % t->beatLength;
        if (ev->sync == 0x20) {
            queued = left % t->barLength;
        }
    } else if (t->beat > 0) {
        queued += t->beatLength * (n->beatsPerBar * n->bars - (t->beat - 1));
    }
    return queued;
}

// Services the project's tracks of this service (the lead track when
// `lead`): fades, starting the next event when its time comes (tracks of
// a held event wait for the time the others agree on), releasing held
// requests, and queueing the next node when a track runs low.
void PATHI_serviceproject(int project, int lead) {
    s8 timer = bankservice == 0x42;
    s32 window = timer ? pfstate->timerLatency : pfstate->taskLatency;
    PATHFINDEVENT* ev = &pfstate->events[0];
    if (pfstate->events[0].mask != 0 && pfstate->leadTrack == -1 && !ev->hold) {
        ev->sync = 0x40;
    }
    for (u32 i = 0; i < PATH_MAXTRACKS; i++) {
        if (lead != (u8)(pfstate->leadTrack == i)) {
            continue;
        }
        PATHTRACK* t = pfstate->tracks[i];
        if (t == NULL || t->paused != 0) {
            continue;
        }
        u8 p = 0;
        u32 deadline = milliseconds + t->latency;
        if (t->cur.node >= 0 && t->node != NULL) {
            p = 1;
        }
        s8 playing = p;
        if (playing && t->fadeCurve >= 0) {
            PATHI_setfadevolume(t);
        }
        if (t->timerService != (u32)timer) {
            continue;
        }
        u8 q = 0;
        if (ev->mask != 0 && !(ev->mask & (1 << i))) {
            q = 1;
        }
        s8 pending = q;
        if (pending) {
            u32 sync = ev->sync;
            if (sync == 0x40 || ev->time != 0) {
                int until = 0;
                if (ev->time != 0) {
                    until = ev->time - deadline;
                    if (sync == 0x10 || sync == 0x20) {
                        until = ev->time - milliseconds;
                    }
                }
                if (until <= window) {
                    if (sync == 0x10 || sync == 0x20) {
                        ev->sync = 0x40;
                    }
                    if (ev->hold && !playing) {
                        t->releaseTime = ev->time;
                    }
                    if (PATHI_eventstart(i) < 0) {
                        continue;
                    }
                    pending = 0;
                    playing = 1;
                }
            }
        }
        if (!playing) {
            continue;
        }
        if (t->releaseTime != 0) {
            if (t->releaseTime <= milliseconds + ((u32)window >> 1)) {
                if (t->cur.flags & 8) {
                    PATHI_crossfadetracks(project, i, ev);
                }
                PATHX_modifyhold(t->pathx, 0);
                t->releaseTime = 0;
            }
            continue;
        }
        if (!(pending && ev->sync != 0x40) && PATHX_readyfornewrequest(t->pathx) == 0) {
            continue;
        }
        int queued = PATHI_queuedtime(t);
        if (pending && ev->sync != 0x40) {
            pending = 0;
            u32 time = milliseconds + PATHI_synctime(t, queued, ev);
            int diff = ev->time - time;
            if (diff < 0) {
                diff = -diff;
            }
            if (time > ev->time || (ev->track == i && diff > 10)) {
                ev->time = time;
                ev->track = i;
                if (ev->hold) {
                    // Every queued copy of the event starts together.
                    s16 event = ev->event;
                    for (int k = 0; k < PATH_MAXPROJECTS; k++) {
                        if ((0x01000000 << k) != 0) {
                            for (int e = 0; e < PATH_MAXEVENTS; e++) {
                                if (pfstates[k].events[e].mask != 0 && pfstates[k].events[e].event == event) {
                                    pfstates[k].events[e].time = time;
                                }
                            }
                        }
                    }
                }
            }
        }
        if (t->startTime != 0) {
            if (t->startTime > deadline + window) {
                continue;
            }
        } else if (queued > t->latency) {
            continue;
        }
        if (!pending || t->beat > 0 || PATHI_eventstart(i) < 0) {
            if (t->fadeTo == 0 && PATHX_getvolume(t->pathx) == 0) {
                PATHX_stop(t->pathx);
                t->cur.node = -1;
                t->fadeTo = -1;
            } else {
                PATHI_seeknextnode(i);
            }
        }
    }

    // An event no track has timed yet starts when the last track gets to
    // its sync point.
    if (ev->mask != 0 && ev->mask != (u32)-1 && ev->track == 0 && !ev->hold && ev->sync != 0x40 && ev->time == 0) {
        s32 latest = 0;
        for (int j = 0; j < PATH_MAXTRACKS; j++) {
            PATHTRACK* t = pfstate->tracks[j];
            if (t != NULL && t->paused == 0 && t->cur.node >= 0) {
                s32 time = milliseconds + PATHI_synctime(t, PATHI_queuedtime(t), ev);
                if (latest < time) {
                    latest = time;
                }
            }
        }
        ev->time = latest;
    }

    // A pause (mask -1): once it expires, finish the fades and drop it.
    if (ev->mask == (u32)-1 && lead == 0 && ev->expire < milliseconds) {
        for (int j = 0; j < PATH_MAXTRACKS; j++) {
            PATHTRACK* t = pfstate->tracks[j];
            if (t != NULL && ((1 << j) & PATH_TRACKMASK) && t->cur.node >= 0) {
                int volume = t->cur.level * t->volume / 100;
                if (t->fadeTo < 0 || t->fadeTo * t->volume / 100 == PATHX_getvolume(t->pathx)) {
                    if (t->fadeTo == 0) {
                        PATHX_stop(t->pathx);
                        t->cur.node = -1;
                    } else if (volume >= 0) {
                        PATHX_volume(t->pathx, volume);
                    }
                    t->fadeTo = -1;
                }
            }
        }
        for (int k = 0; k < PATH_MAXEVENTS - 1; k++) {
            pfstate->events[k] = pfstate->events[k + 1];
        }
        pfstate->events[PATH_MAXEVENTS - 1].mask = 0;
    }
}

// Crossfades track `track` in from its copy in the other instances of the
// project, which fades out (with the node's fade curves, or the project's
// default fade).
void PATHI_crossfadetracks(int, unsigned int track, PATHFINDEVENT*) {
    PATHTRACK* t = pfstate->tracks[track];
    t->releaseTime = 0;
    int fadeTime = pfstate->fadeTime;
    PATHTRACK* other = PATHI_findtrack((pfstate->id + (1 << track)) ^ PATH_INSTANCEMASK);
    if (other == NULL) {
        return;
    }
    PATHX_getvolume(t->pathx);
    int target = (double)PATHX_getvolume(other->pathx) / 1.27;
    t->volume = other->volume;
    int curve = t->node->fadeIn;
    if (curve != 0) {
        t->fadeStart = milliseconds;
        t->fadeCurve = curve;
        t->fadeFrom = (int)((float)PATHX_getvolume(t->pathx) / 1.27f);
        t->fadeTo = target;
        t->cur.level = (int)(1.27f * (float)target);
    } else {
        char skip = 0;
        t->fadeTo = target;
        t->cur.level = (int)(1.27f * (float)target);
        int volume = t->cur.level * t->volume / 100;
        if ((u32)pfstate->fadeCurve > 1) {
            t->fadeCurve = 0;
            skip = 1;
        }
        PATHX_ifade(t->pathx, volume, fadeTime, skip);
    }
    // As the original: checks the other track's fade-out curve but uses
    // this one's.
    if (other->node->fadeOut != 0) {
        other->fadeStart = milliseconds;
        other->fadeCurve = t->node->fadeOut;
        other->fadeFrom = (int)((float)PATHX_getvolume(other->pathx) / 1.27f);
        other->fadeTo = 0;
        other->cur.level = (int)0.0f;
    } else {
        char skip = 0;
        other->fadeTo = 0;
        other->cur.level = (int)0.0f;
        int volume = other->cur.level * other->volume / 100;
        if ((u32)pfstate->fadeCurve > 1) {
            other->fadeCurve = 0;
            skip = 1;
        }
        PATHX_ifade(other->pathx, volume, fadeTime, skip);
    }
}

// When a track entering `node` (synced as `from` says) should start, in ms
// from now, against the lead track's position: at the end of the lead's
// node (sync 2), on its next beat (1), or on the next of `from`'s entry
// beats. Returns the track's starting beat (-1 none, 1 timer-serviced).
int PATHI_choosebeat(PATHFINDNODE* node, PATHFINDNODE* from, int* start, int* nextBeat) {
    int played;
    int left;
    *start = 0;
    int lead = pfstate->leadTrack;
    if (lead < 0) {
        return 0;
    }
    PATHTRACK* lt = pfstate->tracks[lead];
    if (lt->node == NULL) {
        return 0;
    }
    if (from->sync == 0) {
        return 0;
    }
    PATHFINDNODE* leadNode = PATH_NODE(lt->cur.node);
    int length = pfstate->waves[node->wave - 1][1];
    int leadLength = pfstate->waves[leadNode->wave - 1][1];
    int leadBeat = leadLength;
    if (leadNode->beatsPerBar != 0) {
        leadBeat = leadLength / (leadNode->beatsPerBar * leadNode->bars);
    }
    int beat = length;
    if (node->beatsPerBar != 0) {
        beat = length / (node->beatsPerBar * node->bars);
    }
    PATHX_status(lt->pathx, &played, &left);
    if (played < 0 || left < 0) {
        return 0;
    }
    if (lt->beat > 0) {
        left = left + lt->beatLength * (leadNode->beatsPerBar * leadNode->bars - (lt->beat - 1));
        played = leadLength - left;
    }
    int result = -1;
    PATHTRACK* t = pfstate->tracks[node->track];
    int at = played + t->latency;
    if (t->timerService != 0) {
        result = 1;
    }
    u32 sync = from->sync;
    if (sync == 2) {
        *start = left - length;
        while (*start < t->latency) {
            *start += leadLength;
        }
        return result;
    }
    if (sync == 1) {
        int into = at % length;
        result = into / beat + 2;
        *start = (result - 1) * beat - into;
        if (nextBeat != NULL) {
            *nextBeat = *start + leadBeat;
        }
        return result;
    }
    if (leadNode->beatsPerBar != 0 && from->beatStart != 0 && from->beatStep != 0) {
        int unit = (float)leadBeat * ((float)leadNode->beatUnit / (float)from->beatUnit);
        *start = (from->beatStart - 1) * unit;
        while (*start < at) {
            *start += from->beatStep * unit;
        }
        *start = *start - played;
    }
    return result;
}

// The track's position in its current beat, and the beat length.
static inline int PATHI_beatposition(PATHTRACK* t, int* beatLength) {
    int played;
    int left;
    if (t == NULL) {
        return 0;
    }
    PATHFINDNODE* n = PATH_NODE(t->cur.node);
    int length = pfstate->waves[n->wave - 1][1];
    if (n->beatsPerBar != 0) {
        length = length / (n->beatsPerBar * n->bars);
    }
    PATHX_status(t->pathx, &played, &left);
    int position = left % length;
    if (beatLength != NULL) {
        *beatLength = length;
    }
    return position;
}

// Queues the track's current node on its stream; returns the request.
// Task-serviced tracks hold their first request until released.
int PATHI_queuenode(PATHTRACK* t, unsigned int sync) {
    int file = 100;
    if (t->cur.node < 0) {
        t->request = -1;
        if (t->node != NULL) {
            PATHI_eventrestore(t->eventId);
        }
        return 0;
    }
    int queued = PATHI_queuedtime(t);
    if (t->timerService == 0) {
        if (queued <= 0) {
            if (t->releaseTime == 0) {
                t->releaseTime = milliseconds;
            }
            if (t->startTime != 0) {
                t->releaseTime = t->startTime;
            }
            file = -1;
        }
        t->beat = -1;
    } else if (t->beat <= 0) {
        t->beat = 1;
    }
    int beat = t->beat;
    int volume;
    if (pfstate->events[0].mask != 0 && queued != 0) {
        volume = -1;
    } else {
        volume = t->cur.level * t->volume / 100;
    }
    PATHFINDNODE* n = PATH_NODE(t->cur.node);
    int offset = 0;
    t->barLength = t->beatLength = pfstate->waves[n->wave - 1][1];
    if (t->cur.node < 0 || t->cur.node >= pfstate->map->numNodes) {
        offset = 0;
    } else {
        s16 wave = PATH_NODE(t->cur.node)->wave;
        if (wave > 0) {
            offset = pfstate->waves[wave - 1][0];
        }
    }
    t->request = PATHX_queuefile(t->pathx, file, t->file, offset, t->beatLength, volume, t->beat, sync);
    if (n->bars != 0) {
        t->barLength = t->barLength / n->bars;
    }
    if (n->beatsPerBar != 0) {
        t->beatLength = t->beatLength / (n->beatsPerBar * n->bars);
    }
    if (t->timerService != 0) {
        PATHTRACK* lead = pfstate->tracks[pfstate->leadTrack];
        if (lead != NULL && t != lead && t->node->alignBeat) {
            // Start on the lead track's next beat (at least half a beat on).
            int beatLength;
            int position = PATHI_beatposition(lead, &beatLength);
            if (position < t->beatLength / 2) {
                position += beatLength;
            }
            t->startTime = milliseconds + position;
        } else {
            t->startTime = 0;
        }
        t->releaseTime = 0;
        int b = t->beat;
        t->beat = b + 1;
        if (b >= n->beatsPerBar * n->bars) {
            t->beat = 0;
        }
    }
    if (t->request >= 0) {
        PATHREQUEST* r = &t->requests[t->requestIndex];
        r->request = t->request;
        r->node = t->cur.node;
        r->length = pfstate->waves[PATH_NODE(t->cur.node)->wave - 1][1];
        r->beat = beat;
        t->requestIndex++;
        if (t->requestIndex >= t->numRequests) {
            t->requestIndex = 0;
        }
    }
    return t->request;
}

// The branch of node `index` for the track's control level (a loop node
// uses its owner's remaining count instead, counting it down).
static inline s16 PATHI_branch(s16 index, s8 level) {
    s16 next = -1;
    PATHFINDNODE* n = PATH_NODE(index);
    PATHBRANCH* br = n->branches;
    PATHTRACK* owner = pfstate->tracks[n->track];
    if (owner->loopNode == index) {
        s8 count = owner->loops;
        level = count & 0x7F;
        if (n->loops > 0) {
            owner->loops = count - 1;
            if (count == 0) {
                owner->loopNode = -1;
            }
        }
    }
    for (u32 k = 0; k < n->numBranches; k++) {
        if (level >= br[k].lo && level <= br[k].hi) {
            next = br[k].node;
            break;
        }
    }
    return next;
}

// Moves the track on: to the next beat of its node, or to the node its
// branch picks, and queues that. Restores the track if that fails.
void PATHI_seeknextnode(int index) {
    PATHTRACK* t = pfstate->tracks[index];
    PATHTRACK saved = *t;
    s16 cur = t->cur.node;
    PATHFINDNODE* n = PATH_NODE(cur);
    s16 next = -1;
    s8 beat = -1;
    if (saved.timerService != 0) {
        beat = 1;
    }
    if (t->beat > 0 && t->beat <= n->beatsPerBar * n->bars) {
        next = cur;
        beat = t->beat;
    } else if (cur >= 0) {
        next = PATHI_branch(cur, t->control);
    }
    if (next >= 0) {
        next = PATHI_enternode(t->cur.node, next, t->control);
    }
    t->cur.node = next;
    t->beat = beat;
    t->releaseTime = t->startTime;
    t->startTime = 0;
    if (PATHI_queuenode(t, 0) < 0) {
        *t = saved;
    }
}

// Ends event `event` on the tracks it changed: back to their saved level,
// then stopped (restore mode 2) or back to the saved node (1).
void PATHI_eventrestore(short event) {
    for (int i = 0; i < PATH_MAXTRACKS; i++) {
        PATHTRACK* t = pfstate->tracks[i];
        if (t == NULL || t->node == NULL || event < 0 || t->eventId != event) {
            continue;
        }
        s8 level = t->saved.level & 0x7F;
        PATHX_volume(t->pathx, level * t->volume / 100);
        t->cur.level = level;
        t->eventId = -1;
        if (t->saved.flags & 2) {
            t->request = -1;
            t->cur.node = -1;
            t->startTime = 0;
            t->node = NULL;
        } else if (t->saved.flags & 1) {
            t->cur.node = PATHI_enternode(0, t->saved.node, t->control);
            s16 beat = -1;
            t->cur.flags = t->saved.flags;
            if (t->timerService != 0) {
                beat = 1;
            }
            t->beat = beat;
            t->startTime = 0;
            PATHI_queuenode(t, 0);
        }
    }
}

// Starts the next event on track `index`: a jump, or the map's value for
// the event (new level and node, crossfade, restore mode). Returns the
// track's new node, -1 when it can't start (the track is put back unless
// the event set a level).
int PATHI_eventstart(int index) {
    s8 queued = 1;
    s8 keep = 0;
    s8 restore = 0;
    int result = 0;
    s16 event = pfstate->events[0].event;
    PATHTRACK* t = pfstate->tracks[index];
    s8 beat = -1;
    if (t->timerService != 0) {
        beat = 1;
    }
    if (pfstate->events[0].sync == 0 && PATHX_readyfornewrequest(t->pathx) == 0) {
        return -1;
    }
    PATHTRACK saved = *t;
    PATHEVENTVALUE value = t->cur;  // what the event sets
    PATHEVENTVALUE back = t->cur;   // what restoring the event goes back to
    value.flags = 0;
    if (t->cur.node >= 0) {
        back.node = PATHI_branch(t->cur.node, t->control);
    }
    if (event < 0) {
        s16 node = -(event + 1);
        PATHFINDNODE* n = PATH_NODE(node);
        t->cur.node = PATHI_enternode(t->cur.node, node, t->control);
        result = PATHI_queuenode(t, pfstate->events[0].sync);
        queued = (u32)result >> 31 ^ 1;
        if (n->wave == 0) {
            t->node = n;
        }
    } else {
        PATHMAP* map = pfstate->map;
        u8 sections = map->numSections;
        int cell = event * sections + sections * (t->index * map->numEvents);
        if (pfstate->leadTrack < 0) {
            PATHI_findlead();
        }
        cell += pfstate->leadBeat;
        PATHEVENTVALUE tv = pfstate->values[pfstate->eventTable[cell]];
        if (tv.node >= 0 && PATH_NODE(tv.node)->eventNode) {
            tv.flags |= 4;
        }
        s16 node;
        if ((s16)cell < 0) {
            node = -1;
        } else {
            if ((tv.flags & 3) || tv.level >= 0) {
                value = tv;
            }
            node = tv.node;
        }
        if (pfstate->events[0].hold) {
            value.flags |= 8;
        }
        if (value.flags & 4) {
            keep = 1;
        }
        if (value.flags & 3) {
            restore = 1;
        }
        if (value.level >= 0) {
            if (!restore) {
                keep = 1;
            }
            t->cur.level = value.level;
        }
        if (value.flags & 8) {
            t->cur.flags |= 8;
            if (t->cur.node >= 0) {
                result = -1;
                goto done;
            }
            t->cur.level = 0;
        }
        if (!restore) {
            result = -1;
            goto done;
        }
        t->startTime = 0;
        t->unk_bits = 0;
        if (node >= 0) {
            node = PATHI_enternode(t->cur.node, node, t->control);
        }
        if (node >= 0) {
            PATHFINDNODE* n = PATH_NODE(node);
            if (t->node->eventNode && !keep) {
                keep = 1;
                back.node = value.node;
            }
            if (index != pfstate->leadTrack) {
                beat = PATHI_choosebeat(n, t->node, (int*)&t->startTime, NULL);
            }
            t->cur.node = node;
            t->beat = beat;
            if (t->startTime != 0) {
                t->startTime += milliseconds;
                if (pfstate->events[0].expire < t->startTime) {
                    pfstate->events[0].expire = t->startTime;
                }
            } else {
                result = PATHI_queuenode(t, pfstate->events[0].sync);
                queued = (u32)result >> 31 ^ 1;
            }
        } else {
            if (back.node >= 0) {
                if (pfstate->events[0].sync == 0x40) {
                    PATHX_stop(t->pathx);
                } else if (pfstate->events[0].time < pfstate->events[0].expire) {
                    // Wait for the tracks still to start.
                    pfstate->events[0].time = pfstate->events[0].expire;
                    *t = saved;
                    return -1;
                }
            }
            t->request = -1;
            t->node = NULL;
            t->cur.node = -1;
        }
    }
done:
    if (keep) {
        t->saved = back;
        if (value.flags & 3) {
            t->saved.flags = value.flags & 3;
        }
        t->eventId = event;
    } else {
        t->saved.flags = 0;
        t->saved.node = -1;
        t->eventId = -1;
    }
    if (queued) {
        pfstate->events[0].mask |= 1 << t->index;
    }
    if (result < 0) {
        if (value.level < 0) {
            *t = saved;
        }
        return -1;
    }
    return t->cur.node;
}

// Node `node` as remapped for arrivals from node `from` (its remap list
// pairs old -> new; every matching pair applies in turn).
static inline int PATHI_remap(int from, int node) {
    if (from >= 0) {
        u8 list = PATH_NODE(from)->remap;
        if (list != 0) {
            u32 first = pfstate->tables[list - 1];
            int count = pfstate->tables[list] - first;
            u32* pairs = (u32*)pfstate->map + first;
            for (int k = 0; k < count; k++) {
                if (node == (s32)pairs[k] >> 16) {
                    node = pairs[k] & 0xFFFF;
                }
            }
        }
    }
    return node;
}

// Enters node `node` from `from`: follows marker (wave 0: the track's
// current node, reported to songprogress), loop and random nodes by their
// branches until a node with a wave. Returns that node.
int PATHI_enternode(int from, int node, int level) {
    s8 control = level;
    s16 index = PATHI_remap(from, node);
    PATHFINDNODE* n = PATH_NODE(index);
    PATHTRACK* owner = pfstate->tracks[n->track];
    while (n->wave <= 0) {
        s16 wave = n->wave;
        if (wave == 0) {
            owner->node = n;
            if (n->type == 1) {
                level = ((int)milliseconds / 23) & 0x7F;
            }
            if (songprogress != NULL) {
                songprogress(pfactive, index);
            }
        } else if (wave == -1) {
            s8 count = n->loops;
            if (count != 0 && index != owner->loopNode) {
                owner->loops = count;
                owner->loopNode = index;
            }
        } else if (wave == -2) {
            level = ((int)milliseconds / 23) & 0x7F;
        }
        int next = PATHI_branch(index, level);
        next = PATHI_remap(from, next);
        level = control;
        index = next;
        n = PATH_NODE(index);
    }
    return (s16)PATHI_remap(from, index);
}

// A fade curve: the curve shape, run backwards when `invert`, over
// `duration` ms. size 0x4
typedef struct PATHFADE {
    /* 0x0 */ u8 type : 7;  // 1 linear, 2 quadratic, 3 1/x
    /* 0x0 */ u8 invert : 1;
    /* 0x1 */ u8 unk_0x1;
    /* 0x2 */ u16 duration;
} PATHFADE;

// Sets the volume a fading track is at; ends the fade (stopping a fade to
// 0) when its time is up.
void PATHI_setfadevolume(PATHTRACK* t) {
    PATHFADE fade;
    float shape = -1.0f;
    s8 to = t->fadeTo;
    s8 from = t->fadeFrom;
    s8 curve = t->fadeCurve;
    float range = (float)to - (float)from;
    if (curve > 0) {
        fade = *(PATHFADE*)&pfstate->fades[curve - 1];
    } else {
        fade.type = pfstate->fadeCurve;
        fade.duration = pfstate->fadeTime;
        fade.invert = range < 0.0f ? 0 : 1;
    }
    float time = (float)(milliseconds - t->fadeStart) / (float)fade.duration;
    if (time >= 1.0f) {
        PATHX_volume(t->pathx, t->cur.level);
        if (t->fadeTo == 0) {
            PATHX_stop(t->pathx);
            t->cur.node = -1;
        }
        t->fadeFrom = -1;
        t->fadeTo = -1;
        t->fadeCurve = -1;
        t->fadeStart = 0;
        t->cur.level = t->fadeTo;  // as the original: -1
        return;
    }
    if (fade.invert) {
        time = 1.0f - time;
    }
    switch (fade.type) {
    case 1:
        shape = 1.0f - time;
        break;
    case 2:
        shape = 1.0f - time * time;
        break;
    case 3:
        shape = (1.0f / time) / 25.0f;
        break;
    }
    float low = from;
    if (to < from) {
        low = to;
    }
    if (range < 0.0f) {
        range = -range;
    }
    if (shape >= 0.0f && shape <= 1.0f) {
        PATHX_volume(t->pathx, (int)(1.27f * (range * shape + low)));
    }
}

// Orders the projects by id.
void PATHI_sortprojects() {
    for (int i = 0; i < PATH_MAXPROJECTS; i++) {
        for (int j = i + 1; j < PATH_MAXPROJECTS; j++) {
            if (pfstates[i].id > pfstates[j].id) {
                PATHPROJECT swap = pfstates[i];
                pfstates[i] = pfstates[j];
                pfstates[j] = swap;
            }
        }
    }
}
