#include "ea/snd/sndi.h"
#include "string.h"

// Reads a patch's first timbre into the stream/packet sample structures. Sample data in tags
// 0x98-0x9B is copied to sound memory.
void SNDI_patchtohdr(void* bank, TAGGEDPATCH* patch, SNDSAMPLEFORMAT* format, SNDSAMPLEATTR* attr,
                     SNDSAMPLEDESC* desc) {
    int azimuth[4] = {0, 0, 0, 0};
    int renderMode = 0x200;
    int* offsets = NULL;
    SNDTAGINFO info;
    int n = 0;

    memset(format, 0, sizeof(SNDSAMPLEFORMAT));
    memset(attr, 0, sizeof(SNDSAMPLEATTR));
    memset(desc, 0, sizeof(SNDSAMPLEDESC));
    SND_attrsetdef(attr);
    info.ptr = (u8*)patch + 4;
    format->rate = 24000;
    format->channels = 1;
    format->unk_0x3 = 7;
    desc->samples = 0;

    while (SNDI_gettag(&info)) {
        switch (info.tag) {
        case 0xA0:
            format->unk_0x3 = info.value;
            break;
        case 0x9C:
            azimuth[0] = info.value;
            break;
        case 0x9D:
            azimuth[1] = info.value;
            break;
        case 0x9E:
            azimuth[2] = info.value;
            break;
        case 0x9F:
            azimuth[3] = info.value;
            break;
        case 0x98:
            attr->data[0] = SNDMEMI_allocz(info.size);
            SNDI_memcpy(attr->data[0], info.data, info.size);
            attr->size[0] = info.size;
            break;
        case 0x99:
            attr->data[1] = SNDMEMI_allocz(info.size);
            SNDI_memcpy(attr->data[1], info.data, info.size);
            attr->size[1] = info.size;
            break;
        case 0x9A:
            attr->data[2] = SNDMEMI_allocz(info.size);
            SNDI_memcpy(attr->data[2], info.data, info.size);
            attr->size[2] = info.size;
            break;
        case 0x9B:
            attr->data[3] = SNDMEMI_allocz(info.size);
            SNDI_memcpy(attr->data[3], info.data, info.size);
            attr->size[3] = info.size;
            break;
        case 0x80:
            attr->unk_0x7 = info.value;
            break;
        case 0x82:
            format->channels = info.value;
            break;
        case 0x84:
            format->rate = info.value;
            break;
        case 0x85:
            desc->samples = info.value;
            break;
        case 0x8A:
            offsets = (int*)info.data;
            break;
        case 0x13:
            attr->unk_0x5 = info.value;
            break;
        case 0x0A:
            attr->unk_0x6 = info.value;
            break;
        case 0x06:
            attr->unk_0x2 = info.value;
            break;
        case 0x8C:
            renderMode = info.value;
            break;
        case 0x88:
            desc->offset[0] = info.value;
            break;
        case 0x89:
            desc->offset[1] = info.value;
            break;
        case 0x94:
            desc->offset[2] = info.value;
            break;
        case 0x95:
            desc->offset[3] = info.value;
            break;
        case 0x14:
            attr->userData[n] = info.data;
            attr->userDataSize[n] = info.size;
            n++;
            break;
        }
    }

    attr->renderMode = renderMode;
    for (int i = format->channels - 1; i >= 0; i--) {
        if (sndgs.opts.caps.speakers == 2 && format->channels == 2) {
            azimuth[i] = 0;
        }
        attr->unk_0xC[i] = sndgs.chanAzimuth[format->channels - 1][i] + azimuth[i];
        if ((attr->renderMode & 0x14) || attr->renderMode == 0) {
            desc->offset[i] = (int)PTR_TO_ADDR(bank) + desc->offset[i];
        }
        if ((attr->renderMode & 0x308) && offsets != NULL) {
            if (i == 0) {
                desc->offset[i] = *offsets;
            } else {
                desc->offset[i] = *offsets + (desc->offset[i] - desc->offset[0]);
            }
        }
    }
}
