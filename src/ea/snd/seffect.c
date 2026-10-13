#include "ea/snd/sndi.h"

// Bus flags: 0x10 the type-4 effect, 0x08 the type-0x200 effect (both if neither is given);
// the remaining bits are the bus number.
#define SNDFX_TYPEMASK 0x1F8

SNDFXBUS* SNDCTRLI_getfxbus(int bus, int type) {
    switch (type & 0x71C) {
    case 4:
        return &sndgs.fxbus[bus];
    case 0x200:
        return &sndgs.fxbus[bus + 1];
    }
    return NULL;
}

extern "C" int SNDfxinitbus(int busflags, int level, int type, int param1, int param2) {
    int mask;
    int bus;
    int bit;

    if ((busflags & SNDFX_TYPEMASK) == 0) {
        busflags |= SNDFX_TYPEMASK;
    }
    mask = 0;
    if (busflags & 0x10) {
        mask |= 4;
    }
    if (busflags & 0x8) {
        mask |= 0x200;
    }
    bus = busflags & ~SNDFX_TYPEMASK;
    for (bit = 4; mask != 0; bit <<= 1) {
        if (bit & mask) {
            SNDFXBUS* fx;
            switch (bit & 0x71C) {
            case 4:
                fx = &sndgs.fxbus[bus];
                break;
            case 0x200:
                fx = &sndgs.fxbus[bus + 1];
                break;
            default:
                fx = NULL;
                break;
            }
            fx->type = type;
            fx->param1 = param1;
            fx->param2 = param2;
            SNDPLATFORM_fxinit(bus, bit);
            mask &= ~bit;
        }
    }
    if (type == 0) {
        level = 0;
    }
    SNDfxmasterlevel(busflags, level);
    return 0;
}

extern "C" int SNDfxmasterlevel(int busflags, int level) {
    int bus = busflags & ~SNDFX_TYPEMASK;
    int i;

    if ((busflags & SNDFX_TYPEMASK) == 0) {
        busflags |= SNDFX_TYPEMASK;
    }
    if (busflags & 0x10) {
        sndgs.fxbus[bus].level = level;
    }
    if (busflags & 0x8) {
        sndgs.fxbus[bus + 1].level = level;
    }
    SNDSYS_entercritical();
    for (i = 0; i < sndgs.numVoices; i++) {
        SNDVOICE* v = &sndgs.voices[i];
        SNDfxlevel(v->handle, bus, v->fxLevel[bus]);
    }
    SNDSYS_leavecritical();
    return 0;
}
