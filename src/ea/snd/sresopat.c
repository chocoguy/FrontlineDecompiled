#include "ea/snd/sndi.h"

// Resolve the timbres of a patch whose render mode matches the mask against bank data at `aram`.
int SNDBANKI_asyncresolvepatch(int mask, TAGGEDPATCH* patch, char* aram, int* state) {
    SNDIPATCHHEADER header;
    void* p = patch;
    int more;

    p = (u8*)patch + 4;
    do {
        more = SNDI_parsetimbre(&p, &header);
        if (header.renderMode & mask) {
            SNDPLATFORM_asyncresolvetimbre(&header, aram, state);
            SNDBANKI_userdatacallback(&header, 0);
        }
    } while (more != 0);
    return 0;
}
