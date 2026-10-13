#include "ea/snd/smix.h"

// The voice's decoder position, or -1.
extern "C" int MIX_getframe(int voice) {
    SNDMIXVOICE* mv = &sndmix.voices[voice];
    if (mv->getframe != NULL) {
        return mv->getframe(mv->decoder);
    }
    return -1;
}
