#include "ea/snd/sndi.h"

// Big-endian signed value of 1..4 bytes.
int SNDI_getb(void* data, int size) {
    u8* p = (u8*)data;
    int value = 0;
    for (int n = size; n != 0; n--) {
        value = (value << 8) + *p++;
    }
    if (size == 1 && value > 0x7F) {
        value -= 0x100;
    } else if (size == 2 && value > 0x7FFF) {
        value -= 0x10000;
    } else if (size == 3 && value > 0x7FFFFF) {
        value -= 0x1000000;
    }
    return value;
}
