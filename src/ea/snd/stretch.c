#include "ea/snd/smix.h"
#include "ea/snd/spktplay.h"
#include <string.h>

int SFILTER_timestretch(void* self, int frames, void* in, void* out, int flag);
int stretchframesneeded(void* self, int frames);
int stretch(void* self, int frames, f32* in, f32* out);
void applyaction(TIMESTRETCHSTATE* s, f32** src, int action);
static int determineaction(int period, f32 ratio, u8** table, f32* err);
void crossfade(f32* a, f32* b, f32* out, int len, int shift);

// data: a byte, the period / 2, the total samples (4 bytes), then the table.
int SFILTER_timestretchinit(TIMESTRETCHSTATE* s, unsigned char* data, int voice) {
    s->f.process = SFILTER_timestretch;
    s->f.destroy = NULL;
    if (voice >= 0) {
        s->player = SNDPKTPLAYI_voicetopackethandle(SNDDRV_getmastervoice(voice));
    } else {
        s->player = -1;
    }
    s->table = data + 6;
    s->ratio = 1.0f;
    s->err = 0.0f;
    s->period = data[1] * 2;
    s->remaining = SNDI_getb(data + 2, 4);
    s->carry = 0;
    s->outAvail = 0;
    s->outPos = 0;
    return 0;
}

int SFILTER_timestretch(void* self, int frames, void* in, void* out, int flag) {
    TIMESTRETCHSTATE* s = (TIMESTRETCHSTATE*)self;
    if (s->remaining == 0) {
        return s->f.next->process(s->f.next, 200, out, in, s->f.port);
    }
    int player = s->player;
    int need = stretchframesneeded(s, frames);
    // Wait until the packet player has enough queued.
    if (player >= 0 && need > SNDPKTPLAY_framesoutstanding(player)) {
        return 0;
    }
    if (need > 0) {
        int got = s->f.next->process(s->f.next, need, out, in, s->f.port);
        if (got <= 0) {
            return got;
        }
    }
    int done = stretch(s, frames, (f32*)in, (f32*)out);
    if (done < frames) {
        memset((f32*)out + done, 0, (frames - done) * 4);
    }
    return frames;
}

// ratio: 4.12.
void SFILTER_timestretchsetratio(TIMESTRETCHSTATE* s, int ratio) {
    s->ratio = 0.00024414062f * ratio;
    if (s->ratio < 0.5) {
        s->ratio = 0.5f;
    } else if (s->ratio > 2.0) {
        s->ratio = 2.0f;
    }
}

int stretch(void* self, int frames, f32* in, f32* out) {
    TIMESTRETCHSTATE* s = (TIMESTRETCHSTATE*)self;
    f32* src = in;
    int done = s->outAvail;
    if (frames <= done) {
        done = frames;
    }
    SNDI_memcpy(out, &s->outBuf[s->outPos], done * 4);
    f32* dest = out + done;
    int left = frames - done;
    s->outPos += done;
    s->outAvail -= done;
    if (left == 0) {
        return done;
    }
    int action = 0;
    while (left > 0 && s->remaining >= s->period) {
        if (s->remaining < s->period * 2) {
            action = 0;
        } else {
            action = determineaction(s->period, s->ratio, &s->table, &s->err);
        }
        applyaction(s, &src, action);
        int n = s->outAvail;
        if (left <= n) {
            n = left;
        }
        SNDI_memcpy(dest, &s->outBuf[s->outPos], n * 4);
        left -= n;
        done += n;
        dest += n;
        s->outPos += n;
        s->outAvail -= n;
    }
    if (action > 0) {
        // The next period overlaps this one: keep it for the next call.
        SNDI_memcpy(s->carryBuf, src, s->period * 4);
        s->carry = s->period;
        src += s->period;
    }
    if (left > 0) {
        if (left > s->remaining) {
            left = s->remaining;
        }
        SNDI_memcpy(dest, src, left * 4);
        done += left;
        s->remaining -= left;
    }
    return done;
}

// Input frames stretch() will consume to produce `frames`.
int stretchframesneeded(void* self, int frames) {
    TIMESTRETCHSTATE* s = (TIMESTRETCHSTATE*)self;
    u8* table;
    f32 err;
    if (frames <= s->outAvail) {
        return 0;
    }
    int remaining = s->remaining;
    int left = frames - s->outAvail;
    int need = 0;
    int action = 0;
    table = s->table;
    err = s->err;
    while (left > 0 && remaining >= s->period) {
        if (remaining < s->period * 2) {
            action = 0;
        } else {
            action = determineaction(s->period, s->ratio, &table, &err);
        }
        int used;
        int produced;
        if (action >= 0) {
            used = s->period;
            produced = used + action;
        } else {
            used = s->period * 2;
            produced = action + used;
        }
        need += used;
        remaining -= used;
        if (left <= produced) {
            left = 0;
        } else {
            left -= produced;
        }
    }
    if (action > 0) {
        need += s->period;
    }
    if (left > 0) {
        if (left > remaining) {
            need += remaining;
        } else {
            need += left;
        }
    }
    int result = need - s->carry;
    return result > 0 ? result : 0;
}

void applyaction(TIMESTRETCHSTATE* s, f32** src, int action) {
    f32* cur;
    f32* next;
    if (s->carry > 0) {
        next = *src;
        cur = s->carryBuf;
    } else {
        cur = *src;
        next = cur + s->period;
    }
    if (action == 0) {
        SNDI_memcpy(s->outBuf, cur, s->period * 4);
        s->remaining -= s->period;
        s->outAvail = s->period;
        *src = next;
    } else if (action > 0) {
        // Repeat: the period plus `action` samples crossfaded into the next.
        SNDI_memcpy(s->outBuf, cur, action * 4);
        crossfade(cur, next, &s->outBuf[action], s->period, action);
        s->remaining -= s->period;
        s->outAvail = s->period + action;
        *src = next;
    } else {
        // Drop: crossfade the period into the next and skip -action samples.
        crossfade(cur, next, s->outBuf, s->period, action);
        SNDI_memcpy(&s->outBuf[s->period], next - action, (s->period + action) * 4);
        s->remaining -= s->period * 2;
        s->outAvail = action + s->period * 2;
        *src = next + s->period;
    }
    s->carry = 0;
    s->outPos = 0;
}

static inline f32 absf(f32 x) {
    return x < 0.0f ? -x : x;
}

// 0 to play the period as is, > 0 to repeat that many samples, < 0 to drop.
static int determineaction(int period, f32 ratio, u8** table, f32* err) {
    int shift = **table + period / 2;
    if (ratio < 1.0f) {
        f32 drop = *err + (f32)(period * 2 - shift) - 2.0f * ratio * period;
        f32 keep = period + *err - ratio * period;
        if (absf(keep) < absf(drop)) {
            *table += 1;
            *err = keep;
            return 0;
        }
        *table += 2;
        *err = drop;
        return -shift;
    }
    f32 e = *err;
    f32 rp = ratio * period;
    f32 repeat = e + (f32)(period + shift) - rp;
    f32 keep = period + e - rp;
    if (absf(keep) < absf(repeat)) {
        *table += 1;
        *err = keep;
        return 0;
    }
    *table += 1;
    *err = repeat;
    return shift;
}

// Linear crossfade over len - |shift| samples of a against a shifted by |shift|, then into b.
void crossfade(f32* a, f32* b, f32* out, int len, int shift) {
    f32 g;
    f32 step;
    int i;
    if (shift > 0) {
        g = 1.0f;
        step = -1.0f / len;
    } else {
        shift = -shift;
        g = 0.0f;
        step = 1.0f / len;
    }
    for (i = shift; i < len; i++) {
        out[i - shift] = g * a[i] + (1.0f - g) * a[i - shift];
        g += step;
    }
    for (i = 0; i < shift; i++) {
        out[len - shift + i] = g * b[i] + (1.0f - g) * a[len - shift + i];
        g += step;
    }
}
