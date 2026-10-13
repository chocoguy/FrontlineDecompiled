#include <TRK_MINNOW_DOLPHIN/trk.h>

// Sends a message as an HDLC-style frame: flag, escaped payload, escaped
// one's-complement checksum, flag.
DSError TRKMessageSend(TRKBuffer* msg) {
    u8 fcs;
    u32 i;
    DSError err;
    u8 c;

    fcs = 0;
    for (i = 0; i < msg->length; i++) {
        fcs = (u8)(fcs + msg->data[i]);
    }
    fcs = fcs ^ 0xFF;

    err = WriteUART1(TRK_FRAME_FLAG);
    if (err == kNoError) {
        for (i = 0; i < msg->length; i++) {
            c = msg->data[i];
            if (c == TRK_FRAME_FLAG || c == TRK_FRAME_ESCAPE) {
                err = WriteUART1(TRK_FRAME_ESCAPE);
                c ^= 0x20;
                if (err != kNoError) {
                    break;
                }
            }
            err = WriteUART1(c);
            if (err != kNoError) {
                break;
            }
        }
    }

    if (err == kNoError) {
        c = fcs;
        if (c == TRK_FRAME_FLAG || c == TRK_FRAME_ESCAPE) {
            err = WriteUART1(TRK_FRAME_ESCAPE);
            c ^= 0x20;
            if (err != kNoError) {
                goto done;
            }
        }
        err = WriteUART1(c);
    }

done:
    if (err == kNoError) {
        err = WriteUART1(TRK_FRAME_FLAG);
    }
    if (err == kNoError) {
        err = WriteUARTFlush();
    }
    return err;
}
