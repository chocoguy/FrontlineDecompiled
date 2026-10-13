#include "ea/snd/sndi.h"
#include "ea/snd/sdecmut.h"

// EA MicroTalk (UTK) speech decoder: 432-sample frames of four 108-sample subframes, each a
// pitch (long-term) prediction plus an excitation, through a 12th order lattice LPC filter.

static int bitmask[9] = {0x0, 0x1, 0x3, 0x7, 0xf, 0x1f, 0x3f, 0x7f, 0xff};

// Reflection coefficients.
static f32 coeff_table[64] = {
    0.0f, -0.996776f, -0.990327f, -0.983879f, -0.977431f, -0.970982f, -0.964534f, -0.958085f,
    -0.951637f, -0.930754f, -0.90496f, -0.879167f, -0.853373f, -0.827579f, -0.801786f, -0.775992f,
    -0.750198f, -0.724405f, -0.698611f, -0.670635f, -0.619048f, -0.56746f, -0.515873f, -0.464286f,
    -0.412698f, -0.361111f, -0.309524f, -0.257937f, -0.206349f, -0.154762f, -0.103175f, -0.051587f,
    0.0f, 0.051587f, 0.103175f, 0.154762f, 0.206349f, 0.257937f, 0.309524f, 0.361111f,
    0.412698f, 0.464286f, 0.515873f, 0.56746f, 0.619048f, 0.670635f, 0.698611f, 0.724405f,
    0.750198f, 0.775992f, 0.801786f, 0.827579f, 0.853373f, 0.879167f, 0.90496f, 0.930754f,
    0.951637f, 0.958085f, 0.964534f, 0.970982f, 0.977431f, 0.983879f, 0.990327f, 0.996776f,
};

// Multipulse excitation: command by model and the next 8 bits.
static u8 index_table[2][256] = {
    {
        4, 6, 5, 9, 4, 6, 5, 13, 4, 6, 5, 10, 4, 6, 5, 17,
        4, 6, 5, 9, 4, 6, 5, 14, 4, 6, 5, 10, 4, 6, 5, 21,
        4, 6, 5, 9, 4, 6, 5, 13, 4, 6, 5, 10, 4, 6, 5, 18,
        4, 6, 5, 9, 4, 6, 5, 14, 4, 6, 5, 10, 4, 6, 5, 25,
        4, 6, 5, 9, 4, 6, 5, 13, 4, 6, 5, 10, 4, 6, 5, 17,
        4, 6, 5, 9, 4, 6, 5, 14, 4, 6, 5, 10, 4, 6, 5, 22,
        4, 6, 5, 9, 4, 6, 5, 13, 4, 6, 5, 10, 4, 6, 5, 18,
        4, 6, 5, 9, 4, 6, 5, 14, 4, 6, 5, 10, 4, 6, 5, 0,
        4, 6, 5, 9, 4, 6, 5, 13, 4, 6, 5, 10, 4, 6, 5, 17,
        4, 6, 5, 9, 4, 6, 5, 14, 4, 6, 5, 10, 4, 6, 5, 21,
        4, 6, 5, 9, 4, 6, 5, 13, 4, 6, 5, 10, 4, 6, 5, 18,
        4, 6, 5, 9, 4, 6, 5, 14, 4, 6, 5, 10, 4, 6, 5, 26,
        4, 6, 5, 9, 4, 6, 5, 13, 4, 6, 5, 10, 4, 6, 5, 17,
        4, 6, 5, 9, 4, 6, 5, 14, 4, 6, 5, 10, 4, 6, 5, 22,
        4, 6, 5, 9, 4, 6, 5, 13, 4, 6, 5, 10, 4, 6, 5, 18,
        4, 6, 5, 9, 4, 6, 5, 14, 4, 6, 5, 10, 4, 6, 5, 2,
    },
    {
        4, 11, 7, 15, 4, 12, 8, 19, 4, 11, 7, 16, 4, 12, 8, 23,
        4, 11, 7, 15, 4, 12, 8, 20, 4, 11, 7, 16, 4, 12, 8, 27,
        4, 11, 7, 15, 4, 12, 8, 19, 4, 11, 7, 16, 4, 12, 8, 24,
        4, 11, 7, 15, 4, 12, 8, 20, 4, 11, 7, 16, 4, 12, 8, 1,
        4, 11, 7, 15, 4, 12, 8, 19, 4, 11, 7, 16, 4, 12, 8, 23,
        4, 11, 7, 15, 4, 12, 8, 20, 4, 11, 7, 16, 4, 12, 8, 28,
        4, 11, 7, 15, 4, 12, 8, 19, 4, 11, 7, 16, 4, 12, 8, 24,
        4, 11, 7, 15, 4, 12, 8, 20, 4, 11, 7, 16, 4, 12, 8, 3,
        4, 11, 7, 15, 4, 12, 8, 19, 4, 11, 7, 16, 4, 12, 8, 23,
        4, 11, 7, 15, 4, 12, 8, 20, 4, 11, 7, 16, 4, 12, 8, 27,
        4, 11, 7, 15, 4, 12, 8, 19, 4, 11, 7, 16, 4, 12, 8, 24,
        4, 11, 7, 15, 4, 12, 8, 20, 4, 11, 7, 16, 4, 12, 8, 1,
        4, 11, 7, 15, 4, 12, 8, 19, 4, 11, 7, 16, 4, 12, 8, 23,
        4, 11, 7, 15, 4, 12, 8, 20, 4, 11, 7, 16, 4, 12, 8, 28,
        4, 11, 7, 15, 4, 12, 8, 19, 4, 11, 7, 16, 4, 12, 8, 24,
        4, 11, 7, 15, 4, 12, 8, 20, 4, 11, 7, 16, 4, 12, 8, 3,
    },
};

struct UTKCOMMAND {
    s32 next;  // model for the next command
    s32 size;  // bits used
    f32 value;  // pulse
};
static UTKCOMMAND decode_table[29] = {
    {1, 8, 0.0f},
    {1, 7, 0.0f},
    {0, 8, 0.0f},
    {0, 7, 0.0f},
    {0, 2, 0.0f},
    {0, 2, -1.0f},
    {0, 2, 1.0f},
    {0, 3, -1.0f},
    {0, 3, 1.0f},
    {1, 4, -2.0f},
    {1, 4, 2.0f},
    {1, 3, -2.0f},
    {1, 3, 2.0f},
    {1, 5, -3.0f},
    {1, 5, 3.0f},
    {1, 4, -3.0f},
    {1, 4, 3.0f},
    {1, 6, -4.0f},
    {1, 6, 4.0f},
    {1, 5, -4.0f},
    {1, 5, 4.0f},
    {1, 7, -5.0f},
    {1, 7, 5.0f},
    {1, 6, -5.0f},
    {1, 6, 5.0f},
    {1, 8, -6.0f},
    {1, 8, 6.0f},
    {1, 7, -6.0f},
    {1, 7, 6.0f},
};

static inline u32 getbits(UTALKSTATE* ut, int n) {
    u32 v = ut->bits;
    ut->bits = v >> n;
    ut->nbits -= n;
    if (ut->nbits < 8) {
        ut->bits |= *ut->ptr++ << ut->nbits;
        ut->nbits += 8;
    }
    return v & bitmask[n];
}

// Decodes a subframe's excitation into every other sample of out.
void readsamples(UTALKSTATE* ut, int multipulse, f32* out) {
    int i;
    if (multipulse) {
        int model = 0;
        i = 0;
        do {
            u32 bits = ut->bits;
            int cmd = index_table[model][bits & 0xFF];
            UTKCOMMAND* c = &decode_table[cmd];
            int size = c->size;
            model = c->next;
            ut->bits = bits >> size;
            ut->nbits -= size;
            if (ut->nbits < 8) {
                ut->bits |= *ut->ptr++ << ut->nbits;
                ut->nbits += 8;
            }
            if (cmd > 3) {
                // A pulse of up to 6.
                out[i] = c->value;
                i += 2;
            } else if (cmd > 1) {
                // 7 to 70 zeros.
                int count = getbits(ut, 6) + 7;
                if (i + count * 2 > 108) {
                    count = (108 - i) >> 1;
                }
                for (int j = 0; j < count; j++) {
                    out[i] = 0.0f;
                    i += 2;
                }
            } else {
                // A pulse of 7 or more: unary magnitude, then the sign.
                int x = 7;
                while (getbits(ut, 1) == 1) {
                    x++;
                }
                if (getbits(ut, 1) == 1) {
                    out[i] = x;
                } else {
                    out[i] = -x;
                }
                i += 2;
            }
        } while (i < 108);
        return;
    }
    // RELP: 0, -2 or +2 per sample.
    for (i = 0; i < 54; i++) {
        int b = ut->bits & 3;
        if (b == 1) {
            *out = -2.0f;
            getbits(ut, 2);
        } else if (b == 3) {
            *out = 2.0f;
            getbits(ut, 2);
        } else {
            *out = 0.0f;
            getbits(ut, 1);
        }
        out += 2;
    }
}

// Reflection coefficients to direct-form LPC coefficients.
static void reftolpc(f32* rc, f32* lpc) {
    f32 tmp1[12];
    f32 tmp2[12];
    int i;
    int j;
    for (i = 10; i >= 0; i--) {
        tmp2[i + 1] = rc[i];
    }
    tmp2[0] = 1.0f;
    for (i = 0; i < 12; i++) {
        f32 x = -rc[11] * tmp2[11];
        for (j = 10; j >= 0; j--) {
            x -= rc[j] * tmp2[j];
            tmp2[j + 1] = x * rc[j] + tmp2[j];
        }
        tmp2[0] = x;
        tmp1[i] = x;
        for (j = 0; j < i; j++) {
            x -= lpc[j] * tmp1[i - 1 - j];
        }
        lpc[i] = x;
    }
}

// Runs blocks of 12 samples of the frame from offset through the synthesis filter.
static void filter(UTALKSTATE* ut, int offset, int blocks) {
    f32 lpc[12];
    f32* ptr = &ut->frame[offset];
    reftolpc(ut->rc, lpc);
    for (int b = 0; b < blocks; b++) {
        for (int j = 0; j < 12; j++) {
            f32 x = ptr[j];
            int k;
            for (k = 0; k < j; k++) {
                x += lpc[k] * ut->history[k - j + 12];
            }
            for (; k < 12; k++) {
                x += lpc[k] * ut->history[k - j];
            }
            ut->history[11 - j] = x;
            ptr[j] = x;
        }
        ptr += 12;
    }
}

// Reads the stream header and resets the decoder.
void initmut(char* data, UTALKSTATE* ut) {
    int i;
    ut->bits = *(u8*)data;
    ut->nbits = 8;
    ut->ptr = (u8*)data + 1;
    getbits(ut, 1);
    ut->multipulseThresh = 32 - getbits(ut, 4);
    ut->gains[0] = 8.0f * (getbits(ut, 4) + 1);
    f32 mult = 1.04f + 0.001f * getbits(ut, 6);
    for (i = 1; i < 64; i++) {
        ut->gains[i] = ut->gains[i - 1] * mult;
    }
    for (i = 0; i < 12; i++) {
        ut->rc[i] = 0.0f;
        ut->history[i] = 0.0f;
    }
    for (i = 0; i < 324; i++) {
        ut->cb[i] = 0.0f;
    }
}

// Decodes the next 432 samples into ut->frame.
void decodemut(UTALKSTATE* ut) {
    f32 delta[12];
    f32 excitation[5 + 108 + 5];
    int i;
    int j;

    int idx = getbits(ut, 6);
    int multipulse = idx < ut->multipulseThresh;
    delta[0] = 0.25f * (coeff_table[idx] - ut->rc[0]);
    for (i = 1; i < 4; i++) {
        delta[i] = 0.25f * (coeff_table[getbits(ut, 6)] - ut->rc[i]);
    }
    for (i = 4; i < 12; i++) {
        delta[i] = 0.25f * (coeff_table[16 + getbits(ut, 5)] - ut->rc[i]);
    }

    for (i = 0; i < 4; i++) {
        int lag = getbits(ut, 8);
        int base = (i + 2) * 108 - lag;
        f32 pitchGain = 0.06666667f * getbits(ut, 4);
        f32 gain = ut->gains[getbits(ut, 6)];
        int align = getbits(ut, 1);
        int zero = getbits(ut, 1);
        readsamples(ut, multipulse, &excitation[5 + align]);
        if (zero) {
            for (j = 0; j < 54; j++) {
                excitation[5 + (1 - align) + 2 * j] = 0.0f;
            }
        } else {
            // Interpolate the other samples (low-pass), halving the gain to keep unit energy.
            excitation[0] = 0.0f;
            excitation[1] = 0.0f;
            excitation[2] = 0.0f;
            excitation[3] = 0.0f;
            excitation[4] = 0.0f;
            excitation[113] = 0.0f;
            excitation[114] = 0.0f;
            excitation[115] = 0.0f;
            excitation[116] = 0.0f;
            excitation[117] = 0.0f;
            f32* p = &excitation[5 + (1 - align)];
            for (j = 0; j < 108; j += 2) {
                p[j] = 0.01803268f * (p[j - 5] + p[j + 5]) + (0.59738594f * (p[j - 1] + p[j + 1]) +
                                                               -0.11459156f * (p[j - 3] + p[j + 3]));
            }
            gain *= 0.5f;
        }
        for (j = 0; j < 108; j++) {
            ut->frame[108 * i + j] = pitchGain * ut->cb[base + j] + gain * excitation[5 + j];
        }
    }

    SNDI_memcpy(ut->cb, &ut->frame[108], sizeof(ut->cb));
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 12; j++) {
            ut->rc[j] += delta[j];
        }
        filter(ut, 12 * i, i < 3 ? 1 : 33);
    }
}
