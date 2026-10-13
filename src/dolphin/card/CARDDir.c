#include <dolphin/card_priv.h>
#include <string.h>

void* __CARDGetDirBlock(CARDControl* card) {
    return card->currentDir;
}

static void WriteCallback(s32 chan, s32 result) {
    CARDControl* card;
    CARDCallback callback;
    CARDDir* dir0;
    CARDDir* dir1;

    card = &__CARDBlock[chan];
    if (result >= 0) {
        dir0 = (CARDDir*)((u8*)card->workArea + 0x2000);
        dir1 = (CARDDir*)((u8*)card->workArea + 0x4000);

        if (card->currentDir == dir0) {
            card->currentDir = dir1;
            memcpy(dir1, dir0, CARD_SYSTEM_BLOCK_SIZE);
        } else {
            card->currentDir = dir0;
            memcpy(dir0, dir1, CARD_SYSTEM_BLOCK_SIZE);
        }
    }

    if (card->apiCallback == NULL) {
        __CARDPutControlBlock(card, result);
    }
    callback = card->eraseCallback;
    if (callback) {
        card->eraseCallback = NULL;
        callback(chan, result);
    }
}

static void EraseCallback(s32 chan, s32 result) {
    CARDControl* card;
    CARDCallback callback;
    CARDDir* dir;
    u32 addr;

    card = &__CARDBlock[chan];
    if (result < 0) {
        goto error;
    }

    dir = card->currentDir;
    addr = ((u32)dir - (u32)card->workArea) / CARD_SYSTEM_BLOCK_SIZE * card->sectorSize;
    result = __CARDWrite(chan, addr, CARD_SYSTEM_BLOCK_SIZE, dir, WriteCallback);
    if (result < 0) {
        goto error;
    }
    return;

error:
    if (card->apiCallback == NULL) {
        __CARDPutControlBlock(card, result);
    }
    callback = card->eraseCallback;
    if (callback) {
        card->eraseCallback = NULL;
        callback(chan, result);
    }
}

s32 __CARDUpdateDir(s32 chan, CARDCallback callback) {
    CARDControl* card;
    CARDDirCheck* check;
    u32 addr;
    CARDDir* dir;

    card = &__CARDBlock[chan];
    if (!card->attached) {
        return CARD_RESULT_NOCARD;
    }

    dir = card->currentDir;
    check = __CARDGetDirCheck(dir);
    ++check->checkCode;
    __CARDCheckSum(dir, CARD_SYSTEM_BLOCK_SIZE - sizeof(u32), &check->checkSum, &check->checkSumInv);
    DCStoreRange(dir, CARD_SYSTEM_BLOCK_SIZE);

    card->eraseCallback = callback;
    addr = ((u32)dir - (u32)card->workArea) / CARD_SYSTEM_BLOCK_SIZE * card->sectorSize;
    return __CARDEraseSector(chan, addr, EraseCallback);
}
