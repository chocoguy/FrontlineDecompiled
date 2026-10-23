// The decoder's IDCT setup (MSSG idct.c). The transform itself is the
// hand-written paired-single code in gc_idct.
#include "movie/mpeg2dec.h"

static short iclip[1024];  // clipping table
static short* iclp;

void Initialize_Fast_IDCT() {
    int i;

    iclp = iclip + 512;
    for (i = -512; i < 512; i++) {
        iclp[i] = (i < -256) ? -256 : ((i > 255) ? 255 : i);
    }
}

#ifdef FRONTLINE_NATIVE
// GC_IDCT (gc_idct.asm on GameCube) in C: the same AAN-style float transform,
// rows then columns, with the same scaling. GQR7 is 0x04070007: the
// paired-single loads read the block as s16 / 16 (getblk.cpp's coefficients
// are 16 times MSSG's), and the final stores write s16(x) unscaled, truncated
// and saturated.
namespace {

const float C4 = 0.35355338f, C2 = 0.46193975f, C6 = 0.19134171f;
const float C1 = 0.49039263f, C3 = 0.4157348f, C5 = 0.27778512f, C7 = 0.09754516f;

// One 8-point pass, in the assembly's order of operations.
inline void IdctPass(const float x[8], float out[8]) {
    float e0, e1, e2, e3, o0, o1, o2, o3, t;
    t = C4 * x[0];
    e0 = C2 * x[2] + t;
    e1 = C6 * x[2] + t;
    e2 = C2 * x[6] + t;
    e3 = C4 * x[4] + t;
    e0 = C4 * x[4] + e0;
    e0 = C6 * x[6] + e0;
    e1 = e1 + -(C4 * x[4] + C2 * x[6]);
    e2 = e2 + -(C6 * x[2] + C4 * x[4]);
    e3 = e3 + -(C2 * x[2] + C6 * x[6]);

    o0 = C1 * x[1];
    o1 = C3 * x[1];
    o2 = C5 * x[1];
    o3 = C7 * x[1];
    o0 = C3 * x[3] + o0;
    o2 = C7 * x[5] + o2;
    o3 = C3 * x[5] + o3;
    o0 = C5 * x[5] + o0;
    o2 = C3 * x[7] + o2;
    o0 = C7 * x[7] + o0;
    o2 = o2 + -(C1 * x[3] + 0.0f);
    o3 = o3 + -(C5 * x[3] + C1 * x[7]);
    o1 = o1 + -(C7 * x[3] + (C1 * x[5] + C5 * x[7]));

    out[0] = e0 + o0;
    out[1] = e1 + o1;
    out[2] = e2 + o2;
    out[3] = e3 + o3;
    out[4] = e3 - o3;
    out[5] = e2 - o2;
    out[6] = e1 - o1;
    out[7] = e0 - o0;
}

inline short StoreS16(float v) {
    if (v >= 32767.0f) {
        return 32767;
    }
    if (v <= -32768.0f) {
        return -32768;
    }
    return (short)v;
}

}  // namespace

extern "C" void GC_IDCT(short* block) {
    float scratch[8][8];
    float x[8];
    for (int row = 0; row < 8; row++) {
        for (int k = 0; k < 8; k++) {
            x[k] = block[row * 8 + k] * (1.0f / 16.0f);
        }
        IdctPass(x, scratch[row]);
    }
    float out[8];
    for (int col = 0; col < 8; col++) {
        for (int k = 0; k < 8; k++) {
            x[k] = scratch[k][col];
        }
        IdctPass(x, out);
        for (int k = 0; k < 8; k++) {
            block[k * 8 + col] = StoreS16(out[k]);
        }
    }
}
#endif
