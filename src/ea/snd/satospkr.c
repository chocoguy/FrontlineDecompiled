#include "ea/snd/sndi.h"

// Per-speaker volume (0..127) for each 1/256 turn of azimuth.
u8 sndaztospkr[256][4];

// vols[speaker] = 0..0x7FFF for the azimuth.
void SNDI_aztospkrvol(int azimuth, s16* vols) {
    u8* row = sndaztospkr[(azimuth >> 8) & 0xFF];
    for (int i = 0; i < sndgs.opts.caps.speakers; i++) {
        *vols++ = (s8)*row++ * 0x102;
    }
}

// Constant-power-ish pan law: 2g - g^2 / 127 for a linear gain g.
static inline u8 panlaw(int g) {
    return g + (g - g * g / 127);
}

// Pans between the two output speakers either side of each azimuth.
void SNDI_precalcaztospkrvol() {
    int az = 0;
    for (int r = 0; r < 256; r++, az += 0x100) {
        u8* row = sndaztospkr[r];
        if (sndgs.opts.caps.speakers == 1) {
            row[0] = 0x7F;
            continue;
        }
        for (int i = 0; i < sndgs.opts.caps.speakers; i++) {
            row[i] = 0;
        }
        u16* spk = sndgs.opts.caps.speakerAz[sndgs.opts.caps.speakers - 1];
        if (az <= spk[0]) {
            // Between the last speaker and the first, across the wrap.
            int toFirst = spk[0] - az;
            int fromLast = az + (0x10000 - spk[sndgs.opts.caps.speakers - 1]);
            int total = toFirst + fromLast;
            int g0 = fromLast * 127 / total;
            int g1 = toFirst * 127 / total;
            row[0] = panlaw(g0);
            row[sndgs.opts.caps.speakers - 1] = panlaw(g1);
        } else if (az >= spk[sndgs.opts.caps.speakers - 1]) {
            int fromLast = az - spk[sndgs.opts.caps.speakers - 1];
            int toFirst = (0x10000 - az) + spk[0];
            int total = toFirst + fromLast;
            int g0 = fromLast * 127 / total;
            int g1 = toFirst * 127 / total;
            row[0] = panlaw(g0);
            row[sndgs.opts.caps.speakers - 1] = panlaw(g1);
        } else {
            for (u8 i = 0; i < sndgs.opts.caps.speakers; i++) {
                if (az >= spk[i] && az <= spk[i + 1]) {
                    int toNext = spk[i + 1] - az;
                    int fromThis = az - spk[i];
                    int total = fromThis + toNext;
                    int g0 = toNext * 127 / total;
                    int g1 = fromThis * 127 / total;
                    row[i] = panlaw(g0);
                    row[i + 1] = panlaw(g1);
                }
            }
        }
    }
}
