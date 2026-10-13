#include "ea/snd/smix.h"

extern "C" void SNDSYS_linkmaincpumixer(void) {
    MIXinitfn = MIX_create;
    MIXrestorefn = MIX_destroy;
    MIXaudioslicefn = MIX_audioslice;
    MIXplayinitfn = MIX_playinit;
    MIXplayfn = MIX_play;
    MIXstopfn = MIX_stop;
    MIXsetpitchfn = MIX_setpitch;
}
