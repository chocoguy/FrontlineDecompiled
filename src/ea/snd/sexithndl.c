#include "ea/snd/sndi.h"

void SNDREAL_exithandler() {
    SNDSYS_restore();
}
