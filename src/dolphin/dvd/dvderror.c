#include <dolphin/dvd_priv.h>

// Drive error codes, numbered for the error code saved in SRAM.
static u32 ErrorTable[18] = {
    0x00000000, 0x00023A00, 0x00062800, 0x00030200, 0x00031100, 0x00052000,
    0x00052001, 0x00052100, 0x00052400, 0x00052401, 0x00052402, 0x000B5A01,
    0x00056300, 0x00020401, 0x00020400, 0x00040800, 0x00100007, 0x00000000,
};

static u8 ErrorCode2Num(u32 errorCode) {
    u32 i;

    for (i = 0; i < sizeof(ErrorTable) / sizeof(ErrorTable[0]); i++) {
        if (ErrorTable[i] == errorCode) {
            return (u8)i;
        }
    }

    if ((0x00100000 <= errorCode) && (errorCode <= 0x00100008)) {
        return 17;
    }
    return 29;
}

void __DVDStoreErrorCode(u32 error) {
    OSSramEx* sram;
    u8 num;

    if (error == 0x01234567) {
        num = 255;
    } else if (error == 0x01234568) {
        num = 254;
    } else {
        u32 status = error >> 24;
        num = ErrorCode2Num(error & 0x00FFFFFF);
        if (status >= 6) {
            status = 6;
        }
        num = (u8)(num + status * 30);
    }

    sram = __OSLockSramEx();
    sram->dvdErrorCode = num;
    __OSUnlockSramEx(TRUE);
}
