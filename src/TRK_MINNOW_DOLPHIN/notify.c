#include <TRK_MINNOW_DOLPHIN/trk.h>

DSError TRKDoNotifyStopped(MessageCommandID cmd) {
    DSError err;
    MessageBufferID replyID;
    MessageBufferID bufferID;
    TRKBuffer* buffer;

    err = TRKGetFreeBuffer(&bufferID, &buffer);
    if (err == kNoError) {
        err = TRKAppendBuffer1_ui8(buffer, cmd);
        if (err == kNoError) {
            if (cmd == kDSNotifyStopped) {
                TRKTargetAddStopInfo(buffer);
            } else {
                TRKTargetAddExceptionInfo(buffer);
            }
        }

        err = TRKRequestSend(buffer, &replyID, 2, 3, 1);
        if (err == kNoError) {
            TRKReleaseBuffer(replyID);
        }
        TRKReleaseBuffer(bufferID);
    }

    return err;
}
