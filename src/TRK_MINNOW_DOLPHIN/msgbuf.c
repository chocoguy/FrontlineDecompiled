#include <TRK_MINNOW_DOLPHIN/trk.h>

TRKBuffer gTRKMsgBufs[TRK_MSG_BUFFER_COUNT];

static inline void TRKSetBufferUsed(TRKBuffer* buf, BOOL state) {
    buf->isInUse = state;
}

DSError TRKInitializeMessageBuffers(void) {
    int i;

    for (i = 0; i < TRK_MSG_BUFFER_COUNT; i++) {
        TRKInitializeMutex(&gTRKMsgBufs[i]);
        TRKAcquireMutex(&gTRKMsgBufs[i]);
        TRKSetBufferUsed(&gTRKMsgBufs[i], FALSE);
        TRKReleaseMutex(&gTRKMsgBufs[i]);
    }

    return kNoError;
}

DSError TRKGetFreeBuffer(MessageBufferID* msgID, TRKBuffer** outMsg) {
    TRKBuffer* buf;
    DSError error;
    int i;

    error = kNoMessageBufferAvailable;
    *outMsg = NULL;

    for (i = 0; i < TRK_MSG_BUFFER_COUNT; i++) {
        buf = TRKGetBuffer(i);

        TRKAcquireMutex(buf);
        if (!buf->isInUse) {
            TRKResetBuffer(buf, TRUE);
            TRKSetBufferUsed(buf, TRUE);
            error = kNoError;
            *outMsg = buf;
            *msgID = i;
            i = TRK_MSG_BUFFER_COUNT;  // exit the loop
        }
        TRKReleaseMutex(buf);
    }

    if (error == kNoMessageBufferAvailable) {
        usr_puts_serial("ERROR : No buffer available\n");
    }

    return error;
}

TRKBuffer* TRKGetBuffer(MessageBufferID idx) {
    TRKBuffer* buf;

    buf = NULL;
    if (idx >= 0 && idx < TRK_MSG_BUFFER_COUNT) {
        buf = &gTRKMsgBufs[idx];
    }
    return buf;
}

void TRKReleaseBuffer(MessageBufferID idx) {
    TRKBuffer* msg;

    if (idx != -1 && idx >= 0 && idx < TRK_MSG_BUFFER_COUNT) {
        msg = &gTRKMsgBufs[idx];
        TRKAcquireMutex(msg);
        TRKSetBufferUsed(msg, FALSE);
        TRKReleaseMutex(msg);
    }
}

void TRKResetBuffer(TRKBuffer* msg, u8 keepData) {
    msg->length = 0;
    msg->position = 0;
    if (!keepData) {
        TRK_memset(msg->data, 0, TRK_MSG_BUFFER_SIZE);
    }
}

DSError TRKSetBufferPosition(TRKBuffer* msg, u32 pos) {
    DSError error;

    error = kNoError;
    if (pos > TRK_MSG_BUFFER_SIZE) {
        error = kMessageBufferOverflow;
    } else {
        msg->position = pos;
        if (pos > msg->length) {
            msg->length = pos;
        }
    }
    return error;
}

#pragma auto_inline off
DSError TRKAppendBuffer(TRKBuffer* msg, const void* data, u32 length) {
    DSError error;
    u32 bytesLeft;

    error = kNoError;
    if (length == 0) {
        return kNoError;
    }

    bytesLeft = TRK_MSG_BUFFER_SIZE - msg->position;
    if (bytesLeft < length) {
        error = kMessageBufferOverflow;
        length = bytesLeft;
    }

    if (length == 1) {
        msg->data[msg->position] = ((const u8*)data)[0];
    } else {
        TRK_memcpy(msg->data + msg->position, data, length);
    }

    msg->position += length;
    msg->length = msg->position;
    return error;
}
#pragma auto_inline reset

DSError TRKReadBuffer(TRKBuffer* msg, void* data, u32 length) {
    DSError error;
    u32 bytesLeft;

    error = kNoError;
    if (length == 0) {
        return kNoError;
    }

    bytesLeft = msg->length - msg->position;
    if (length > bytesLeft) {
        error = kMessageBufferReadError;
        length = bytesLeft;
    }

    TRK_memcpy(data, msg->data + msg->position, length);
    msg->position += length;
    return error;
}

// The wire format is big-endian; values are swapped when the host is not.

DSError TRKAppendBuffer1_ui16(TRKBuffer* buffer, const u16 data) {
    u8* bigEndianData;
    u8* byteData;
    u8 swapBuffer[sizeof(data)];

    if (gTRKBigEndian) {
        bigEndianData = (u8*)&data;
    } else {
        byteData = (u8*)&data;
        bigEndianData = swapBuffer;
        bigEndianData[0] = byteData[1];
        bigEndianData[1] = byteData[0];
    }

    return TRKAppendBuffer(buffer, bigEndianData, sizeof(data));
}

DSError TRKAppendBuffer1_ui32(TRKBuffer* buffer, const u32 data) {
    u8* bigEndianData;
    u8* byteData;
    u8 swapBuffer[sizeof(data)];

    if (gTRKBigEndian) {
        bigEndianData = (u8*)&data;
    } else {
        byteData = (u8*)&data;
        bigEndianData = swapBuffer;
        bigEndianData[0] = byteData[3];
        bigEndianData[1] = byteData[2];
        bigEndianData[2] = byteData[1];
        bigEndianData[3] = byteData[0];
    }

    return TRKAppendBuffer(buffer, bigEndianData, sizeof(data));
}

DSError TRKAppendBuffer1_ui64(TRKBuffer* buffer, const u64 data) {
    u8* bigEndianData;
    u8* byteData;
    u8 swapBuffer[sizeof(data)];

    if (gTRKBigEndian) {
        bigEndianData = (u8*)&data;
    } else {
        byteData = (u8*)&data;
        bigEndianData = swapBuffer;
        bigEndianData[0] = byteData[7];
        bigEndianData[1] = byteData[6];
        bigEndianData[2] = byteData[5];
        bigEndianData[3] = byteData[4];
        bigEndianData[4] = byteData[3];
        bigEndianData[5] = byteData[2];
        bigEndianData[6] = byteData[1];
        bigEndianData[7] = byteData[0];
    }

    return TRKAppendBuffer(buffer, bigEndianData, sizeof(data));
}

DSError TRKAppendBuffer_ui8(TRKBuffer* buffer, const u8* data, int count) {
    DSError err;
    int i;

    for (i = 0, err = kNoError; err == kNoError && i < count; i++) {
        err = TRKAppendBuffer1_ui8(buffer, data[i]);
    }
    return err;
}

DSError TRKAppendBuffer_ui32(TRKBuffer* buffer, const u32* data, int count) {
    DSError err;
    int i;

    for (i = 0, err = kNoError; err == kNoError && i < count; i++) {
        err = TRKAppendBuffer1_ui32(buffer, data[i]);
    }
    return err;
}

DSError TRKReadBuffer1_ui8(TRKBuffer* buffer, u8* data) {
    return TRKReadBuffer(buffer, data, 1);
}

DSError TRKReadBuffer1_ui16(TRKBuffer* buffer, u16* data) {
    DSError err;
    u8* bigEndianData;
    u8* byteData;
    u8 swapBuffer[sizeof(*data)];

    if (gTRKBigEndian) {
        bigEndianData = (u8*)data;
    } else {
        bigEndianData = swapBuffer;
    }

    err = TRKReadBuffer(buffer, bigEndianData, sizeof(*data));

    if (!gTRKBigEndian && err == kNoError) {
        byteData = (u8*)data;
        byteData[0] = bigEndianData[1];
        byteData[1] = bigEndianData[0];
    }

    return err;
}

DSError TRKReadBuffer1_ui32(TRKBuffer* buffer, u32* data) {
    DSError err;
    u8* bigEndianData;
    u8* byteData;
    u8 swapBuffer[sizeof(*data)];

    if (gTRKBigEndian) {
        bigEndianData = (u8*)data;
    } else {
        bigEndianData = swapBuffer;
    }

    err = TRKReadBuffer(buffer, bigEndianData, sizeof(*data));

    if (!gTRKBigEndian && err == kNoError) {
        byteData = (u8*)data;
        byteData[0] = bigEndianData[3];
        byteData[1] = bigEndianData[2];
        byteData[2] = bigEndianData[1];
        byteData[3] = bigEndianData[0];
    }

    return err;
}

DSError TRKReadBuffer1_ui64(TRKBuffer* buffer, u64* data) {
    DSError err;
    u8* bigEndianData;
    u8* byteData;
    u8 swapBuffer[sizeof(*data)];

    if (gTRKBigEndian) {
        bigEndianData = (u8*)data;
    } else {
        bigEndianData = swapBuffer;
    }

    err = TRKReadBuffer(buffer, bigEndianData, sizeof(*data));

    if (!gTRKBigEndian && err == kNoError) {
        byteData = (u8*)data;
        byteData[0] = bigEndianData[7];
        byteData[1] = bigEndianData[6];
        byteData[2] = bigEndianData[5];
        byteData[3] = bigEndianData[4];
        byteData[4] = bigEndianData[3];
        byteData[5] = bigEndianData[2];
        byteData[6] = bigEndianData[1];
        byteData[7] = bigEndianData[0];
    }

    return err;
}

DSError TRKReadBuffer_ui8(TRKBuffer* buffer, u8* data, int count) {
    DSError err;
    int i;

    for (i = 0, err = kNoError; err == kNoError && i < count; i++) {
        err = TRKReadBuffer1_ui8(buffer, &data[i]);
    }
    return err;
}

DSError TRKReadBuffer_ui32(TRKBuffer* buffer, u32* data, int count) {
    DSError err;
    int i;

    for (i = 0, err = kNoError; err == kNoError && i < count; i++) {
        err = TRKReadBuffer1_ui32(buffer, &data[i]);
    }
    return err;
}
