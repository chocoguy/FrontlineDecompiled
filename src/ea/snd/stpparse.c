#include "ea/snd/sndi.h"

SNDENVSEG snddefaultenvelope[1] = {{0x7FFFFFFF, 0x7F}};

// A tag's address: its data plus its value, or, when the timbre doesn't have
// the tag, its default value (an address, as for the envelope, or 0). On the
// console that's NULL + value; natively a null pointer plus a 32-bit address
// isn't one.
#ifdef FRONTLINE_NATIVE
#define TAG_ADDR(i) (ptr[i] != NULL ? PTR_TO_ADDR(ptr[i]) + value[i] : (u32)value[i])
#else
#define TAG_ADDR(i) PTR_TO_ADDR((ptr[i] + value[i]))
#endif

// Parses one timbre's tags at *data into header and advances *data past them.
// Returns 1 if another timbre follows (tag 0xFE), 0 at the end of the patch (0xFF).
int SNDI_parsetimbre(void** data, SNDIPATCHHEADER* header) {
    SNDTAGINFO info;
    int azimuth[4] = {0, 0, 0, 0};
    int value[0x26];  // tags 0x00-0x25
    u8* ptr[0x26];
    int value2[0x28];  // tags 0x80-0xA7
    u8* ptr2[0x28];
    int i;
    int more;

    info.ptr = (u8*)*data;
    header->start = *data;
    for (i = 0; i < 0x26; i++) {
        ptr[i] = NULL;
    }
    for (i = 0; i < 4; i++) {
        header->data[i] = NULL;
        header->dataSize[i] = 0;
    }
    for (i = 0; i < 4; i++) {
        header->adpcm[i] = NULL;
        header->adpcmLoop[i] = NULL;
    }
    for (i = 0; i < 0x28; i++) {
        ptr2[i] = NULL;
    }

    value[0x01] = 0;
    value[0x02] = 0x7F;
    value[0x03] = 0;
    value[0x04] = 0x7F;
    value[0x06] = 0;
    value[0x07] = 60;
    value[0x08] = -1;
    value[0x09] = 1;
    value[0x0A] = 0;
    value[0x0C] = 0x40;
    value[0x0D] = 0;
    value[0x0E] = 0x7F;
    value[0x0F] = 0;
    value[0x10] = 0;
    value[0x11] = 0;
    value[0x12] = 0;
    value[0x13] = 0;
    header->numUserData = 0;
    value[0x17] = 0;
    value[0x19] = (int)PTR_TO_ADDR(snddefaultenvelope);
    value[0x1A] = -1;
    value[0x1C] = 0x7F;
    value[0x1D] = 0;
    value[0x1E] = 0;
    value[0x1F] = 0;
    value[0x20] = 0;
    value[0x21] = 0;
    value[0x22] = 0;
    value[0x23] = 0;
    value[0x24] = 0;
    value[0x25] = 1;
    value2[0x80 - 0x80] = 2;
    value2[0x82 - 0x80] = 1;
    value2[0x84 - 0x80] = 24000;
    value2[0x85 - 0x80] = 0;
    value2[0x86 - 0x80] = -1;
    value2[0x87 - 0x80] = -1;
    value2[0x88 - 0x80] = 0;
    value2[0x9C - 0x80] = 0;
    value2[0x89 - 0x80] = 0;
    value2[0x9D - 0x80] = 0;
    value2[0x94 - 0x80] = 0;
    value2[0x9E - 0x80] = 0;
    value2[0x95 - 0x80] = 0;
    value2[0x9F - 0x80] = 0;
    value2[0xA0 - 0x80] = 7;
    value2[0x8C - 0x80] = 0x200;

    more = 0;
    while (SNDI_gettag(&info)) {
        if (info.tag < 0x26) {
            value[info.tag] = info.value;
            ptr[info.tag] = info.data;
            if (info.tag == 0x14) {
                header->userData[header->numUserData] = info.data;
                header->userDataSize[header->numUserData] = info.size;
                header->numUserData++;
            }
        } else if (info.tag >= 0x80 && info.tag < 0xA8) {
            value2[info.tag - 0x80] = info.value;
            ptr2[info.tag - 0x80] = info.data;
            switch (info.tag) {
            case 0x98:
                header->data[0] = info.data;
                header->dataSize[0] = info.size;
                break;
            case 0x99:
                header->data[1] = info.data;
                header->dataSize[1] = info.size;
                break;
            case 0x9A:
                header->data[2] = info.data;
                header->dataSize[2] = info.size;
                break;
            case 0x9B:
                header->data[3] = info.data;
                header->dataSize[3] = info.size;
                break;
            }
        } else if (info.tag == 0xFE) {
            more = 1;
            break;
        }
    }

    header->velLo = value[0x01];
    header->velHi = value[0x02];
    header->keyLo = value[0x03];
    header->keyHi = value[0x04];
    header->unk_0xC = value[0x06];
    header->rootKey = value[0x07];
    header->unk_0xE = value[0x08];
    header->unk_0xF = value[0x09];
    header->unk_0x10 = value[0x0A];
    header->pan = value[0x0C];
    header->panRand = value[0x0D];
    header->vol = value[0x0E];
    header->volRand = value[0x0F];
    header->unk_0x4 = value[0x10];
    header->unk_0x6 = value[0x11];
    header->unk_0x20 = (int)TAG_ADDR(0x12);
    header->unk_0x16 = value[0x13];
    header->unk_0x24 = (int)TAG_ADDR(0x17);
    header->env = ADDR_TO_PTR(SNDENVSEG, TAG_ADDR(0x19));
    header->unk_0x78 = value[0x1A];
    header->unk_0x1A = value[0x1C];
    header->unk_0x2C = (int)TAG_ADDR(0x1D);
    header->unk_0x1B = value[0x1E];
    header->unk_0x1C = value[0x1F];
    header->unk_0x30 = (int)TAG_ADDR(0x20);
    header->unk_0x1D = value[0x21];
    header->unk_0x54 = value[0x22];
    header->unk_0x1E = value[0x23];
    header->unk_0x0 = value[0x24];
    header->unk_0x1F = value[0x25];
    header->unk_0x2 = value2[0x80 - 0x80];
    header->channels = value2[0x82 - 0x80];
    header->unk_0x13 = value2[0xA0 - 0x80];
    header->rate = value2[0x84 - 0x80];
    header->samples = value2[0x85 - 0x80];
    header->loopStart = value2[0x86 - 0x80];
    header->loopEnd = value2[0x87 - 0x80];
    header->renderMode = value2[0x8C - 0x80];
    header->aramAddr = (s32*)ptr2[0x8A - 0x80];
    header->offset[0] = value2[0x88 - 0x80];
    header->offset[1] = value2[0x89 - 0x80];
    header->offset[2] = value2[0x94 - 0x80];
    header->offset[3] = value2[0x95 - 0x80];
    header->adpcm[0] = ptr2[0x8F - 0x80];
    header->adpcm[1] = ptr2[0x90 - 0x80];
    azimuth[0] = value2[0x9C - 0x80];
    azimuth[1] = value2[0x9D - 0x80];
    azimuth[2] = value2[0x9E - 0x80];
    azimuth[3] = value2[0x9F - 0x80];
    if ((s8)header->channels == 3) {
        header->adpcm[2] = ptr2[0x91 - 0x80];
    }
    header->adpcmLoop[0] = ptr2[0x8D - 0x80];
    header->adpcmLoop[1] = ptr2[0x8E - 0x80];
    if ((s8)header->channels == 3) {
        header->adpcmLoop[2] = ptr2[0x8B - 0x80];
    }
    for (i = 0; i < (s8)header->channels; i++) {
        // Stereo output plays stereo sounds at the speaker positions as they are.
        if (sndgs.opts.caps.speakers == 2 && (s8)header->channels == 2) {
            azimuth[i] = 0;
        }
        header->azimuth[i] = sndgs.chanAzimuth[(s8)header->channels - 1][i] + azimuth[i];
    }
    *data = info.ptr;
    return more;
}
