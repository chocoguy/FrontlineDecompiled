#include <TRK_MINNOW_DOLPHIN/trk.h>

void* gTRKInputPendingPtr;
static TRKFramingState gTRKFramingState;

static inline void ReleaseFrameBuffer(void) {
    if (gTRKFramingState.msgBufID != -1) {
        TRKReleaseBuffer(gTRKFramingState.msgBufID);
        gTRKFramingState.msgBufID = -1;
    }
}

DSError TRKTerminateSerialHandler(void) {
    return kNoError;
}

DSError TRKInitializeSerialHandler(void) {
    gTRKFramingState.msgBufID = -1;
    gTRKFramingState.receiveState = kTRKWaitFlag;
    gTRKFramingState.isEscape = FALSE;
    return kNoError;
}

void TRKProcessInput(MessageBufferID bufferIdx) {
    TRKEvent event;

    TRKConstructEvent(&event, kRequestEvent);
    event.msgBufID = bufferIdx;
    gTRKFramingState.msgBufID = -1;
    TRKPostEvent(&event);
}

void TRKGetInput(void) {
    MessageBufferID id;
    TRKBuffer* msgBuffer;
    u8 command;

    id = TRKTestForPacket();
    if (id == -1) {
        return;
    }

    msgBuffer = TRKGetBuffer(id);
    TRKSetBufferPosition(msgBuffer, 0);
    TRKReadBuffer1_ui8(msgBuffer, &command);
    if (command < kDSReplyACK) {
        TRKProcessInput(id);
    } else {
        TRKReleaseBuffer(id);
    }
}

// Polls the UART and assembles a frame. Returns the buffer ID of a complete
// message, or -1.
MessageBufferID TRKTestForPacket(void) {
    DSError err;
    DSError readErr;
    u8 c;

    err = kNoError;
    readErr = TRKReadUARTPoll(&c);

    while (readErr == kNoError && err == kNoError) {
        if (gTRKFramingState.receiveState != kTRKInFrame) {
            gTRKFramingState.isEscape = FALSE;
        }

        switch (gTRKFramingState.receiveState) {
        case kTRKWaitFlag:
            if (c == TRK_FRAME_FLAG) {
                err = TRKGetFreeBuffer(&gTRKFramingState.msgBufID, &gTRKFramingState.buffer);
                gTRKFramingState.fcs = 0;
                gTRKFramingState.receiveState = kTRKFoundFlag;
            }
            break;
        case kTRKFoundFlag:
            if (c == TRK_FRAME_FLAG) {
                break;
            }
            gTRKFramingState.receiveState = kTRKInFrame;
            // fallthrough
        case kTRKInFrame:
            if ((s8)c == TRK_FRAME_FLAG) {
                BOOL done;

                if (gTRKFramingState.isEscape) {
                    TRKStandardACK(gTRKFramingState.buffer, 0xFF, 4);
                    ReleaseFrameBuffer();
                    gTRKFramingState.buffer = NULL;
                    gTRKFramingState.receiveState = kTRKWaitFlag;
                    break;
                }

                // The last byte is the checksum.
                if (gTRKFramingState.buffer->length < 2) {
                    TRKStandardACK(gTRKFramingState.buffer, 0xFF, 2);
                    ReleaseFrameBuffer();
                    done = FALSE;
                    gTRKFramingState.buffer = NULL;
                    gTRKFramingState.receiveState = kTRKWaitFlag;
                } else {
                    done = TRUE;
                    gTRKFramingState.buffer->position = 0;
                    gTRKFramingState.buffer->length--;
                }

                if (done) {
                    MessageBufferID id = gTRKFramingState.msgBufID;

                    gTRKFramingState.buffer = NULL;
                    gTRKFramingState.msgBufID = -1;
                    gTRKFramingState.receiveState = kTRKWaitFlag;
                    return id;
                }

                gTRKFramingState.receiveState = kTRKWaitFlag;
                break;
            }

            if (gTRKFramingState.isEscape) {
                c ^= 0x20;
                gTRKFramingState.isEscape = FALSE;
            } else if ((s8)c == TRK_FRAME_ESCAPE) {
                gTRKFramingState.isEscape = TRUE;
                break;
            }

            err = TRKAppendBuffer1_ui8(gTRKFramingState.buffer, c);
            gTRKFramingState.fcs += c;
            break;
        case kTRKFrameOverflow:
            if (c == TRK_FRAME_FLAG) {
                ReleaseFrameBuffer();
                gTRKFramingState.buffer = NULL;
                gTRKFramingState.receiveState = kTRKWaitFlag;
            }
            break;
        }

        readErr = TRKReadUARTPoll(&c);
    }

    return -1;
}
