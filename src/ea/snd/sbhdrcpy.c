#include "ea/snd/sndi.h"
#include <string.h>

// Move the bank's header to `dest` (the sample data is in ARAM), e.g. to free the load buffer.
extern "C" int SNDbankheadercopy(void* dest, int bank) {
    int result = 0;
    int size = SNDbankheadersize(bank);
    if (size < 0) {
        result = size;
    } else {
        memmove(dest, sndgs.banks[bank].data, size);
        sndgs.banks[bank].data = (BANKVER5*)dest;
    }
    return result;
}
