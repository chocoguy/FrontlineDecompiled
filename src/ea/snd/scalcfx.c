#include "ea/snd/sndi.h"

void SNDI_calcfxlevel(int bus, int voice) {
    SNDVOICE* v = &sndgs.voices[voice];
    SNDFXBUS* fx = SNDCTRLI_getfxbus(bus, v->renderMode);
    v->fxOut[bus] = ((s8)fx->level * (v->fxSend[bus] * v->fxLevel[bus])) >> 6;
}
