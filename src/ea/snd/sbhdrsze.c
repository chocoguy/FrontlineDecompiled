#include "ea/snd/sndi.h"

extern "C" int SNDbankheadersize(int bank) {
    return sndgs.banks[bank].data->headerSize;
}
