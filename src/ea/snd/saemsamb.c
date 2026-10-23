#include "ea/snd/aems.h"

int (*SNDAEMS_addmodulebank)(void* data, const char* streamFile, int, void* (*alloc)(void*, int, int)) =
    (int (*)(void*, const char*, int, void* (*)(void*, int, int)))SNDAEMSI_addmodulebank;

// Add a module bank already in memory: register its sample banks, let `alloc` move it (keeping
// only the bank headers when the samples went to ARAM), then resolve it. Returns the slot.
int SNDAEMSI_addmodulebank(void* data, char* streamFile, int streamOffset, void* (*alloc)(void*, int, int)) {
    MODULEBANK* bank = (MODULEBANK*)data;
    int sfx = -1;
    int midi = -1;
    int size = bank->size;
    int headerSize = size - (bank->sfxBankSize + bank->midiBankSize);
    int sfxResult = 0;
    int midiResult = 0;
    int keep;
    int slot;
    MODULEBANK* moved;

#ifdef FRONTLINE_NATIVE
    AEMSBankHeaderFromBigEndian(bank);
    size = bank->size;
    headerSize = size - (bank->sfxBankSize + bank->midiBankSize);
#endif
    for (slot = 0; slot < 16; slot++) {
        if (sndaems.banks[slot] == NULL) {
            break;
        }
    }
    if (bank->sfxBankOffset != 0) {
        sfxResult = SNDbankadd(&sfx, (u8*)bank + bank->sfxBankOffset);
        if (sfxResult < 0) {
            return sfxResult;
        }
    }
    if (bank->midiBankOffset != 0) {
        midiResult = SNDbankadd(&midi, (u8*)bank + bank->midiBankOffset);
        if (midiResult < 0) {
            return midiResult;
        }
    }
    if (sfxResult != 0 && midiResult != 0) {
        keep = size;
    } else if (sfxResult == 7) {
        keep = headerSize + SNDbankheadersize(sfx);
    } else if (midiResult == 7) {
        keep = headerSize + SNDbankheadersize(midi);
    } else {
        keep = size;
    }
    moved = (MODULEBANK*)alloc(bank, keep, size);
    if (moved != bank) {
        u8* headers;
        SNDI_memcpy(moved, bank, headerSize);
        bank = moved;
        headers = (u8*)moved + headerSize;
        if (moved->sfxBankOffset != 0) {
            SNDbankheadercopy(headers, sfx);
        }
        if (bank->midiBankOffset != 0 && midiResult >= 0) {
            SNDbankheadercopy(headers + SNDbankheadersize(midi), midi);
        }
    }
    SNDAEMSI_resolvemodulebank(bank, streamFile, streamOffset);
    bank->rt->sfxBank = sfx;
    bank->rt->midiBank = midi;
    sndaems.banks[slot] = bank;
    return slot;
}
