#include <dolphin/gx.h>
#include <dolphin/gx_priv.h>

void GXCallDisplayList(void* list, u32 nbytes) {
    if (gx->dirtyState != 0) {
        __GXSetDirtyState();
    }

    if (*(u32*)&gx->vNumNot == 0) {
        __GXSendFlushPrim();
    }

    GX_WRITE_U8(0x40);  // GX_CMD_CALL_DL
    GX_WRITE_U32(list);
    GX_WRITE_U32(nbytes);
}
