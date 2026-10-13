#include <dolphin/card_priv.h>
#include <string.h>

s32 __CARDGetStatusEx(s32 chan, s32 fileNo, CARDDir* dirent) {
    CARDControl* card;
    CARDDir* dir;
    CARDDir* ent;
    s32 result;

    if (fileNo < 0 || CARD_MAX_FILE <= fileNo) {
        return CARD_RESULT_FATAL_ERROR;
    }
    result = __CARDGetControlBlock(chan, &card);
    if (result < 0) {
        return result;
    }

    dir = __CARDGetDirBlock(card);
    ent = &dir[fileNo];
    result = __CARDAccess(card, ent);
    if (result == CARD_RESULT_NOPERM) {
        result = __CARDIsPublic(ent);
    }
    if (result >= 0) {
        memcpy(dirent, ent, sizeof(CARDDir));
    }
    return __CARDPutControlBlock(card, result);
}

s32 __CARDSetStatusExAsync(s32 chan, s32 fileNo, CARDDir* dirent, CARDCallback callback) {
    CARDControl* card;
    CARDDir* dir;
    CARDDir* ent;
    s32 result;
    u8* p;
    s32 i;

    if (fileNo < 0 || CARD_MAX_FILE <= fileNo || dirent->fileName[0] == 0xFF || dirent->fileName[0] == 0x00) {
        return CARD_RESULT_FATAL_ERROR;
    }
    result = __CARDGetControlBlock(chan, &card);
    if (result < 0) {
        return result;
    }

    dir = __CARDGetDirBlock(card);
    ent = &dir[fileNo];
    result = __CARDAccess(card, ent);
    if (result < 0) {
        return __CARDPutControlBlock(card, result);
    }

    // Zero everything after the name's terminator.
    for (p = dirent->fileName; p < (u8*)dirent->fileName + CARD_FILENAME_MAX; p++) {
        if (*p == '\0') {
            while (++p < (u8*)dirent->fileName + CARD_FILENAME_MAX) {
                *p = '\0';
            }
            break;
        }
    }

    if (memcmp(ent->fileName, dirent->fileName, CARD_FILENAME_MAX) != 0 ||
        memcmp(ent->gameName, dirent->gameName, sizeof(ent->gameName)) != 0 ||
        memcmp(ent->company, dirent->company, sizeof(ent->company)) != 0) {
        for (i = 0; i < CARD_MAX_FILE; i++) {
            if (i != fileNo) {
                CARDDir* other = &dir[i];
                if (other->gameName[0] != 0xFF &&
                    memcmp(other->gameName, dirent->gameName, sizeof(other->gameName)) == 0 &&
                    memcmp(other->company, dirent->company, sizeof(other->company)) == 0 &&
                    memcmp(other->fileName, dirent->fileName, CARD_FILENAME_MAX) == 0) {
                    return __CARDPutControlBlock(card, CARD_RESULT_EXIST);
                }
            }
        }
        memcpy(ent->fileName, dirent->fileName, CARD_FILENAME_MAX);
        memcpy(ent->gameName, dirent->gameName, sizeof(ent->gameName));
        memcpy(ent->company, dirent->company, sizeof(ent->company));
    }

    ent->time = dirent->time;
    ent->bannerFormat = dirent->bannerFormat;
    ent->iconAddr = dirent->iconAddr;
    ent->iconFormat = dirent->iconFormat;
    ent->iconSpeed = dirent->iconSpeed;
    ent->commentAddr = dirent->commentAddr;
    ent->permission = dirent->permission;
    ent->copyTimes = dirent->copyTimes;

    result = __CARDUpdateDir(chan, callback);
    if (result < 0) {
        __CARDPutControlBlock(card, result);
    }
    return result;
}
