#include "ea/snd/sndi.h"

extern "C" int SND_attrsetdef(SNDSAMPLEATTR* attr) {
    attr->unk_0x2 = 0;
    attr->unk_0x0 = 0;
    attr->unk_0x3 = 0x7F;
    attr->unk_0x4 = 0x40;
    attr->unk_0x5 = 0;
    attr->unk_0x6 = 0;
    attr->unk_0x7 = 2;
    attr->renderMode = 0x200;
    for (int i = 0; i < 4; i++) {
        attr->userData[i] = NULL;
        attr->userDataSize[i] = 0;
    }
    for (int i = 0; i < 4; i++) {
        attr->unk_0xC[i] = 0;
        attr->data[i] = NULL;
    }
    return 0;
}
