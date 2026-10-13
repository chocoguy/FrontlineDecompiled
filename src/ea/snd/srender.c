#include "ea/snd/sndi.h"


// Find the next output render mode (from *mode on) that the patch can use; *mode is left
// past it. Bits 0x71C and 0xE0 must overlap when the patch names any; 0x80 needs format 1.
int SNDI_validrendermode(int* mode, SNDIPATCHHEADER* header) {
    int result = 0;

    for (; *mode < sndgs.opts.caps.numRenderModes; (*mode)++) {
        u16 want = header->renderMode;
        u16 have = sndgs.opts.caps.renderModes[*mode];

        result = have & 0x71C;
        if (want & 0x71C) {
            result = want & result;
            if (result == 0) {
                result = 0;
                continue;
            }
        }
        if ((want & 0xE0) == 0) {
            result |= have & 0xE0;
        } else {
            int both = want & have & 0xE0;
            result |= both;
            if (both == 0) {
                result = 0;
                continue;
            }
        }
        if (header->channels != 1 && (result & 0x80)) {
            result = 0;
            continue;
        }
        if (result != 0) {
            (*mode)++;
            return result;
        }
    }
    return result;
}
