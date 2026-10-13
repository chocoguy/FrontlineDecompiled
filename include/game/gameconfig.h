// g_config: global option flags (defined in Moh2.cpp). Only the meaning of
// fields decompiled code uses is noted. Hand-maintained.
#ifndef _GAME_GAMECONFIG_H_
#define _GAME_GAMECONFIG_H_

#include "types.h"

struct GameConfig {
    GameConfig() {
        field_0xC = 0;
        noStaticMeshes = 0;
        field_0x1 = 0;
        noCharacterShadows = 1;
        field_0x3 = 0;
        field_0x4 = 0;
        field_0x5 = 0;
        noParticles = 0;
        field_0x7 = 0;
        noSprites = 0;
        field_0x9 = 0;
        field_0xA = 0;
    }

    /* 0x00 */ u8 noStaticMeshes;  // skip drawing static meshes
    /* 0x01 */ u8 field_0x1;
    /* 0x02 */ u8 noCharacterShadows;
    /* 0x03 */ u8 field_0x3;
    /* 0x04 */ u8 field_0x4;
    /* 0x05 */ u8 field_0x5;
    /* 0x06 */ u8 noParticles;
    /* 0x07 */ u8 field_0x7;
    /* 0x08 */ u8 noSprites;  // skip all 2D sprite drawing
    /* 0x09 */ u8 field_0x9;
    /* 0x0A */ u8 field_0xA;
    /* 0x0B */ u8 unk_0xB;
    /* 0x0C */ u32 field_0xC;
    /* 0x10 */ u8 unk_0x10[0x4];
};

extern GameConfig g_config;  // 0x80196A80

#endif
