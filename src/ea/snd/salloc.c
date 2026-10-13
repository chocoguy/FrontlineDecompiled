#include "ea/snd/sndi.h"

s16 sndvoicereserved[4];

static inline int isreserved(int voice, int count) {
    for (int i = 0; i < count; i++) {
        if (voice == sndvoicereserved[i]) {
            return 1;
        }
    }
    return 0;
}

// Reserves `count` voices in lo..hi-1 for a sound: free voices first (longest free first),
// then steals from lower priority sounds (oldest first). Voices above priority 100 are never
// stolen. Returns the first voice (the master) or -9.
int SNDVOICEI_alloc(int count, int priority, int* handle, int lo, int hi) {
    static int lasthandle = 0;
    int result = -9;
    int found = 0;
    int i;
    s16 j;

    for (i = 0; i < count; i++) {
        sndvoicereserved[i] = -1;
    }
    lasthandle += 0x100;
    if (lasthandle < 0) {
        lasthandle = 0;
    }

    for (i = 0; i < count; i++) {
        s16 best = -1;
        u32 bestTime = 0xFFFFFFFF;
        SNDVOICE* v = &sndgs.voices[lo];
        for (j = lo; j < hi; j++, v++) {
            if (v->state == 0 && !isreserved(j, found) && v->allocTime < bestTime) {
                bestTime = v->allocTime;
                best = j;
            }
        }
        if (best >= 0) {
            sndvoicereserved[found++] = best;
        }
    }

    for (i = found; i < count; i++) {
        s8 bestPri = priority;
        if (!sndgs.opts.caps.stealEqual) {
            bestPri = priority - 1;
        }
        s16 best = -1;
        u32 bestTime = 0xFFFFFFFF;
        SNDVOICE* v = &sndgs.voices[lo];
        for (j = lo; j < hi; j++, v++) {
            if (!isreserved(j, found)) {
                u8 p = v->priority;
                if (p < 101) {
                    if (p < bestPri) {
                        bestTime = v->allocTime;
                        bestPri = p;
                        best = j;
                    } else if (p == bestPri && v->allocTime < bestTime) {
                        bestTime = v->allocTime;
                        best = j;
                    }
                }
            }
        }
        if (best >= 0) {
            sndvoicereserved[found++] = best;
            if (found >= count) {
                break;
            }
        }
    }

    if (found == count) {
        int sorted;
        do {
            sorted = 1;
            for (i = 0; i < count - 1; i++) {
                s16 a = sndvoicereserved[i];
                s16 b = sndvoicereserved[i + 1];
                if (a > b) {
                    sndvoicereserved[i] = b;
                    sorted = 0;
                    sndvoicereserved[i + 1] = a;
                }
            }
        } while (!sorted);

        *handle = lasthandle | sndvoicereserved[0];
        result = sndvoicereserved[0];
        for (i = 0; i < found; i++) {
            SNDVOICE* v = &sndgs.voices[sndvoicereserved[i]];
            if (v->state != 0) {
                int h = v->handle;
                if (h < 0) {
                    h = sndgs.voices[v->master].handle;
                }
                SNDstop(h);
            }
            v->state = 1;
            v->allocTime = sndgs.serverTicks;
            v->priority = priority;
        }
        sndgs.voices[sndvoicereserved[0]].handle = *handle;
        sndgs.voices[sndvoicereserved[0]].chanVoice[0] = sndvoicereserved[0];
        sndgs.voices[sndvoicereserved[0]].master = -1;
        for (i = 1; i < found; i++) {
            sndgs.voices[sndvoicereserved[0]].chanVoice[i] = sndvoicereserved[i];
            sndgs.voices[sndvoicereserved[i]].handle = -1;
            sndgs.voices[sndvoicereserved[i]].master = sndvoicereserved[0];
        }
    }
    return result;
}

static inline void release(SNDVOICE* v) {
    v->state = 0;
    v->unk_0x22 = 0;
    v->unk_0x23 = 0;
    v->allocTime = sndgs.serverTicks;
}

// A patch group's master voice is held (state 2) until its last other voice is freed.
void SNDVOICEI_free(int voice) {
    int count = 0;
    int master = -1;
    SNDVOICE* v = &sndgs.voices[voice];
    u8 group = v->unk_0x22;
    if (group != 0) {
        SNDVOICE* o = sndgs.voices;
        for (int i = 0; i < sndgs.numVoices; i++, o++) {
            if (o->unk_0x22 == group && o->handle >= 0 && (s8)o->state != 0) {
                count++;
                if (o->unk_0x23) {
                    master = i;
                }
            }
        }
        if (count == 1) {
            release(v);
            return;
        }
        SNDVOICE* m = &sndgs.voices[master];
        if (m->state == 2 && voice != master && count == 2) {
            release(v);
            release(&sndgs.voices[master]);
            return;
        }
        if (m->state == 1 && voice == master) {
            m->state = 2;
            return;
        }
    }
    release(v);
}

// Voice of a playing sound's handle, or -8.
int SNDVOICEI_get(int handle) {
    if (handle < 0) {
        return -8;
    }
    int v = handle & 0xFF;
    if (v >= sndgs.numVoices) {
        return -8;
    }
    SNDVOICE* voice = &sndgs.voices[v];
    if ((s8)voice->state == 0 || voice->handle != handle) {
        v = -8;
    }
    return v;
}
