#include "ea/snd/sndi.h"

// Hand each user data block of a timbre to the user data clients (last first).
void SNDBANKI_userdatacallback(SNDIPATCHHEADER* header, int type) {
    while (header->numUserData > 0) {
        SNDUSERDATACBINFO info;
        int i;
        header->numUserData--;
        info.m_type = type;
        info.m_data = header->userData[header->numUserData];
        info.m_size = header->userDataSize[header->numUserData];
        for (i = 0; i < sndgs.numUserDataClients; i++) {
            sndgs.userDataClients[i](&info);
        }
    }
}

// Register a bank; its sample data is copied to ARAM 4K at a time and its timbres resolved
// against it. Returns 7 (data in ARAM: the header alone may be kept, see SNDbankheadercopy) or 8.
extern "C" int SNDbankadd(int* bank, void* data) {
    BANKVER5* hdr = (BANKVER5*)data;
    SNDBANKSLOT* slot;

#ifdef FRONTLINE_NATIVE
    if (SNDBankHeaderFromBigEndian(hdr)) {
        SNDBankFromBigEndian(hdr);
    }
#endif
    sndgs.restoreHook3 = (void (*)(int))SNDbankremove;
    *bank = SNDBANKI_alloc();
    slot = &sndgs.banks[*bank];
    slot->data = hdr;
    slot->busy = 1;
    slot->aram = NULL;
    if (hdr->sampleSize != 0) {
        int offset;
        int i;
        int state;

        slot->aram = SNDPLATFORM_memalloc(0x200, hdr->sampleSize + 0x30);
        if (slot->aram == NULL) {
            slot->data = NULL;
            slot->busy = 0;
            return -6;
        }
        for (offset = 0; offset < hdr->sampleSize; offset += 0x1000) {
            int size = hdr->sampleSize - offset;
            int handle;
            if (size > 0x1000) {
                size = 0x1000;
            }
            handle = SNDPLATFORM_download(0x200, (u8*)hdr + hdr->headerSize + offset, (u8*)slot->aram + offset, size);
            while (SNDPLATFORM_downloadcomplete(handle) == 0) {
            }
        }
        state = 0;
        for (i = 0; i < hdr->numPatches; i++) {
            if (hdr->patchOffsets[i] != 0) {
                SNDBANKI_asyncresolvepatch(0x200, (TAGGEDPATCH*)((u8*)&hdr->patchOffsets[i] + hdr->patchOffsets[i]),
                                           (char*)slot->aram, &state);
            }
        }
    }
    slot->busy = 0;
    if (hdr->sampleSize != 0) {
        return 7;
    }
    return 8;
}
