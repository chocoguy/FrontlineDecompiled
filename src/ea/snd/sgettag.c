#include "ea/snd/sndi.h"

// Reads the next tag; 0 at the end of the stream.
int SNDI_gettag(SNDTAGINFO* info) {
    while (*info->ptr == 0xFC) {
        info->ptr++;
    }
    info->tag = *info->ptr;
    if (info->tag == 0xFF) {
        return 0;
    }
    info->ptr++;
    if (info->tag == 0xFD) {
        return 1;
    }
    if (info->tag == 0xFE) {
        return 1;
    }
    info->size = *info->ptr;
    if (info->size == 0xFF) {
        info->size = SNDI_getb(info->ptr + 1, 4);
        info->ptr += 4;
    }
    info->ptr++;
    info->data = info->ptr;
    if (info->size <= 4) {
        info->value = SNDI_getb(info->ptr, info->size);
    }
    info->ptr += info->size;
    return 1;
}
