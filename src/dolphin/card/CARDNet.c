#include <dolphin/card_priv.h>

u16 __CARDVendorID = 0xFFFF;

s32 CARDSetAttributes(s32 chan, s32 fileNo, u8 attr) {
    CARDDir dirent;
    s32 result;

    result = __CARDGetStatusEx(chan, fileNo, &dirent);
    if (result >= 0) {
        dirent.permission = attr;
        result = __CARDSetStatusExAsync(chan, fileNo, &dirent, __CARDSyncCallback);
    }
    if (result < 0) {
        return result;
    }
    return __CARDSync(chan);
}
