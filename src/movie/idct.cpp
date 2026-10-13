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
