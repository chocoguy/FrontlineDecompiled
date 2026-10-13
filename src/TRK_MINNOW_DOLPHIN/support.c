#include <TRK_MINNOW_DOLPHIN/trk.h>
#include <string.h>

#define kParameterError 2
#define kWaitACKError 0x800

#define DSMSG_WriteFile 0xD0
#define DSMSG_ReadFile 0xD1
#define DSMSG_OpenFile 0xD2
#define DSMSG_CloseFile 0xD3
#define DSMSG_PositionFile 0xD4

#define DS_IONoError 0
#define DS_IOError 1

// Reads or writes a file on the host in chunks of at most 0x800 bytes.
DSError TRKSuppAccessFile(u32 file_handle, u8* data, u32* count, u8* io_result, BOOL need_reply, BOOL read) {
    DSError error;
    MessageBufferID replyBufferId;
    TRKBuffer* replyBuffer;
    MessageBufferID bufferId;
    TRKBuffer* buffer;
    u16 length;
    u32 done;
    u8 replyIOResult;
    u16 replyLength;
    BOOL exit;

    if (data == NULL || *count == 0) {
        return kParameterError;
    }

    exit = FALSE;
    *io_result = DS_IONoError;
    done = 0;
    error = kNoError;

    while (!exit && done < *count && error == kNoError && *io_result == DS_IONoError) {
        length = *count - done;
        if (length > 0x800) {
            length = 0x800;
        }

        error = TRKGetFreeBuffer(&bufferId, &buffer);

        if (error == kNoError) {
            error = TRKAppendBuffer1_ui8(buffer, read ? DSMSG_ReadFile : DSMSG_WriteFile);
        }
        if (error == kNoError) {
            error = TRKAppendBuffer1_ui32(buffer, file_handle);
        }
        if (error == kNoError) {
            error = TRKAppendBuffer1_ui16(buffer, length);
        }
        if (!read && error == kNoError) {
            error = TRKAppendBuffer_ui8(buffer, data + done, length);
        }

        if (error == kNoError) {
            if (need_reply) {
                replyLength = 0;
                replyIOResult = 0;

                // Reads from the console (handle 0) may wait for input indefinitely.
                error = TRKRequestSend(buffer, &replyBufferId, 5, 3, !(read && file_handle == 0));
                if (error == kNoError) {
                    replyBuffer = TRKGetBuffer(replyBufferId);
                    TRKSetBufferPosition(replyBuffer, 2);
                }
                if (error == kNoError) {
                    error = TRKReadBuffer1_ui8(replyBuffer, &replyIOResult);
                }
                if (error == kNoError) {
                    error = TRKReadBuffer1_ui16(replyBuffer, &replyLength);
                }

                if (read && error == kNoError) {
                    if (replyBuffer->length != replyLength + 5) {
                        replyLength = replyBuffer->length - 5;
                        if (replyIOResult == 0) {
                            replyIOResult = 1;
                        }
                    }

                    if (replyLength <= length) {
                        error = TRKReadBuffer_ui8(replyBuffer, data + done, replyLength);
                    }
                }

                if (replyLength != length) {
                    if ((!read || replyLength >= length) && replyIOResult == 0) {
                        replyIOResult = 1;
                    }
                    length = replyLength;
                    exit = TRUE;
                }

                *io_result = replyIOResult;
                TRKReleaseBuffer(replyBufferId);
            } else {
                error = TRKMessageSend(buffer);
            }
        }

        TRKReleaseBuffer(bufferId);
        done += length;
    }

    *count = done;
    return error;
}

// Sends a request and waits for its ACK, processing any requests from the
// host that arrive meanwhile. Retries p2 times; p3 enables a timeout.
DSError TRKRequestSend(TRKBuffer* msgBuf, MessageBufferID* bufferId, u32 p1, u32 p2, int p3) {
    DSError error;
    TRKBuffer* buffer;
    u32 timer;
    int tries;
    u8 msg_command;
    u8 msg_error;
    BOOL badReply;

    error = kNoError;
    badReply = TRUE;
    *bufferId = -1;

    for (tries = p2 + 1; tries != 0 && *bufferId == -1 && error == kNoError; tries--) {
        error = TRKMessageSend(msgBuf);
        if (error == kNoError) {
            if (p3) {
                timer = 0;
            }

            while (TRUE) {
                do {
                    *bufferId = TRKTestForPacket();
                    if (*bufferId != -1) {
                        break;
                    }
                } while (!p3 || ++timer < 79999980);

                if (*bufferId == -1) {
                    break;
                }

                badReply = FALSE;

                buffer = TRKGetBuffer(*bufferId);
                TRKSetBufferPosition(buffer, 0);

                if ((error = TRKReadBuffer1_ui8(buffer, &msg_command)) != kNoError) {
                    break;
                }
                if (msg_command >= kDSReplyACK) {
                    break;
                }

                TRKProcessInput(*bufferId);
                *bufferId = -1;
            }

            if (*bufferId != -1) {
                if (buffer->length < p1) {
                    badReply = TRUE;
                }
                if (error == kNoError && !badReply) {
                    error = TRKReadBuffer1_ui8(buffer, &msg_error);
                }
                if (error == kNoError && !badReply) {
                    if (msg_command != kDSReplyACK || msg_error != 0) {
                        badReply = TRUE;
                    }
                }
                if (error != kNoError || badReply) {
                    TRKReleaseBuffer(*bufferId);
                    *bufferId = -1;
                }
            }
        }
    }

    if (*bufferId == -1) {
        error = kWaitACKError;
    }

    return error;
}

DSError HandleOpenFileSupportRequest(const char* path, u8 replyError, u32* param_3, u8* ioResult) {
    MessageBufferID replyBufferId;
    MessageBufferID bufferId;
    TRKBuffer* buffer;
    TRKBuffer* replyBuffer;
    DSError error;

    *param_3 = 0;
    error = TRKGetFreeBuffer(&bufferId, &buffer);
    if (error == kNoError) {
        error = TRKAppendBuffer1_ui8(buffer, DSMSG_OpenFile);
    }
    if (error == kNoError) {
        error = TRKAppendBuffer1_ui8(buffer, replyError);
    }
    if (error == kNoError) {
        error = TRKAppendBuffer1_ui16(buffer, strlen(path) + 1);
    }
    if (error == kNoError) {
        error = TRKAppendBuffer_ui8(buffer, (const u8*)path, strlen(path) + 1);
    }

    if (error == kNoError) {
        *ioResult = 0;
        error = TRKRequestSend(buffer, &replyBufferId, 7, 3, 0);
        if (error == kNoError) {
            replyBuffer = TRKGetBuffer(replyBufferId);
            TRKSetBufferPosition(replyBuffer, 2);
        }
        if (error == kNoError) {
            error = TRKReadBuffer1_ui8(replyBuffer, ioResult);
        }
        if (error == kNoError) {
            error = TRKReadBuffer1_ui32(replyBuffer, param_3);
        }
        TRKReleaseBuffer(replyBufferId);
    }

    TRKReleaseBuffer(bufferId);
    return error;
}

DSError HandleCloseFileSupportRequest(u32 file_handle, u8* ioResult) {
    MessageBufferID replyBufferId;
    MessageBufferID bufferId;
    TRKBuffer* buffer;
    TRKBuffer* replyBuffer;
    DSError error;

    error = TRKGetFreeBuffer(&bufferId, &buffer);
    if (error == kNoError) {
        error = TRKAppendBuffer1_ui8(buffer, DSMSG_CloseFile);
    }
    if (error == kNoError) {
        error = TRKAppendBuffer1_ui32(buffer, file_handle);
    }

    if (error == kNoError) {
        *ioResult = 0;
        error = TRKRequestSend(buffer, &replyBufferId, 3, 3, 0);
        if (error == kNoError) {
            replyBuffer = TRKGetBuffer(replyBufferId);
            TRKSetBufferPosition(replyBuffer, 2);
        }
        if (error == kNoError) {
            error = TRKReadBuffer1_ui8(replyBuffer, ioResult);
        }
        TRKReleaseBuffer(replyBufferId);
    }

    TRKReleaseBuffer(bufferId);
    return error;
}

DSError HandlePositionFileSupportRequest(u32 file_handle, u32* position, u8 mode, u8* ioResult) {
    MessageBufferID replyBufferId;
    MessageBufferID bufferId;
    TRKBuffer* buffer;
    TRKBuffer* replyBuffer;
    DSError error;

    error = TRKGetFreeBuffer(&bufferId, &buffer);
    if (error == kNoError) {
        error = TRKAppendBuffer1_ui8(buffer, DSMSG_PositionFile);
    }
    if (error == kNoError) {
        error = TRKAppendBuffer1_ui32(buffer, file_handle);
    }
    if (error == kNoError) {
        error = TRKAppendBuffer1_ui32(buffer, *position);
    }
    if (error == kNoError) {
        error = TRKAppendBuffer1_ui8(buffer, mode);
    }

    if (error == kNoError) {
        *ioResult = 0;
        error = TRKRequestSend(buffer, &replyBufferId, 3, 3, 0);
        if (error == kNoError) {
            replyBuffer = TRKGetBuffer(replyBufferId);
            TRKSetBufferPosition(replyBuffer, 2);
        }
        if (error == kNoError) {
            error = TRKReadBuffer1_ui8(replyBuffer, ioResult);
        }
        if (error == kNoError) {
            error = TRKReadBuffer1_ui32(replyBuffer, position);
        } else {
            *position = -1;
        }
        TRKReleaseBuffer(replyBufferId);
    }

    TRKReleaseBuffer(bufferId);
    return error;
}
