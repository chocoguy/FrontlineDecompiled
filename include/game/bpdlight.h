// Level light data (BPD = level geometry file). Only fields decompiled code
// uses so far. Hand-maintained.
#ifndef _GAME_BPDLIGHT_H_
#define _GAME_BPDLIGHT_H_

#include "types.h"

#define BPD_LIGHT_AMBIENT 1
#define BPD_LIGHT_DIRECTIONAL 2

// size 0x30
struct BPDLight {
    /* 0x00 */ int type;  // BPD_LIGHT_AMBIENT, or a directional light
    /* 0x04 */ u8 unk_0x4[0xC];
    /* 0x10 */ f32 dir[3];
    /* 0x1C */ f32 color[3];
    /* 0x28 */ f32 intensity;
    /* 0x2C */ u8 unk_0x2C[0x4];
};

// A set of lights for a region (size 0x30).
// A plane: points with n.p + d >= 0 are inside.
struct BPDPlane {
    f32 n[3];
    f32 d;
};

struct BPDLightVolume {
    /* 0x00 */ u8 unk_0x0[0x18];
    /* 0x18 */ int numPlanes;  // the volume is the intersection of these
    /* 0x1C */ BPDPlane* planes;
    /* 0x20 */ int numLights;
    /* 0x24 */ BPDLight* lights;
    /* 0x28 */ int priority;
    /* 0x2C */ u8 unk_0x2C[0x4];
};

#endif
