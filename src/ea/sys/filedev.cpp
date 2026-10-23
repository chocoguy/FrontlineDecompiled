#include "ea/sys/filedev.h"
#include "ea/sys/filesys.h"
#include "ea/real.h"
#include "dolphin/os.h"
#include "dolphin/dvd.h"
#include "stdio.h"
#include "ctype.h"

// Read states. DVD reads need 32-byte aligned addresses, offsets and sizes,
// so a read goes in parts: a short or misaligned start through the bounce
// buffer, the aligned middle straight into the destination, the rest
// through the bounce buffer 16K at a time.
enum {
    READ_SMALL = 1,    // the whole read fits in the bounce buffer
    READ_LEADIN = 2,   // up to the first aligned file offset
    READ_DIRECT = 3,   // the aligned middle, into the destination
    READ_BOUNCE = 4,   // the rest, through the bounce buffer
    READ_DONE = 5,
};

// The read in progress. size 0x8060
struct READSTATE {
    /* 0x00 */ int state;
    /* 0x04 */ int cur;  // destination of the next part
    /* 0x08 */ int offset;  // file offset of the next part
    /* 0x0C */ int size;
    /* 0x10 */ int dstdown;
    /* 0x14 */ int dstup;
    /* 0x18 */ int dstend;
    /* 0x1C */ int dstenddown;
    /* 0x20 */ int offdown;
    /* 0x24 */ int offup;
    /* 0x28 */ int offend;
    /* 0x2C */ int offenddown;
    /* 0x30 */ int misalign;  // offset - offdown
    /* 0x34 */ int lead;  // 32 - misalign
    /* 0x38 */ int result;
    // The request (readfile)
    /* 0x3C */ HANDLE_def* file;
    /* 0x40 */ void* buffer;
    /* 0x44 */ int fileoffset;
    /* 0x48 */ int filesize;
    /* 0x4C */ u8 unk_0x4C[0x14];
    /* 0x60 */ u8 buf[0x8000];
};

static OSThread ReadFileThread;
static u8 ReadFileThreadStack[0x2000];
OSMessageQueue ReadFileThreadMsgQ;
OSMessage ReadFileThreadMsgData[0x20];
static READSTATE gCurRead __attribute__((aligned(32)));

static void ReadFileThreadFunc(void* param);
static void AyncDVDCallback(s32 result, DVDFileInfo* info);

void initfiledev() {
    OSCreateThread(&ReadFileThread, (void* (*)(void*))ReadFileThreadFunc, NULL,
                   ReadFileThreadStack + sizeof(ReadFileThreadStack), sizeof(ReadFileThreadStack), 2, 1);
    OSInitMessageQueue(&ReadFileThreadMsgQ, ReadFileThreadMsgData, 0x20);
    OSResumeThread(&ReadFileThread);
}

void killfiledev() {
    OSSuspendThread(&ReadFileThread);
}

#pragma push
#pragma auto_inline off
static int MWIO_openfile(const char* name, unsigned int mode, _FILE** file) {
    switch (mode & 7) {
    case 0:
    case 1:
    case 6:
        break;
    default:
        REAL_abortmessage("openfile - ILLEGAL OPEN MODE (%d).\n", mode);
        *file = NULL;
        return 0;
    }
    *file = fopen(name, "a+");
    return *file != NULL;
}

#pragma pop

// Names starting "hd:" (any case, either slash) are host files; so is
// anything not opened for reading. Backslashes become slashes.
int openfile(const char* name, unsigned int mode, HANDLE_def* file) {
    const char* prefixes[3] = {"hd:\\", "hd:/", "hd:"};
    char path[0x104];
    int ok;
    int len = 0;
    u8 host = !(mode & 1);

    if (!host) {
        int i;
        for (i = 0; i < 3; i++) {
            const char* p = prefixes[i];
            const char* s = name;
            int n = 0;
            while (*p != 0) {
                if (*p != tolower(*s)) {
                    n = 0;
                    break;
                }
                n++;
                p++;
                s++;
            }
            len = n;
            if (n != 0) {
                host = 1;
                break;
            }
        }
    }

    if (host) {
        const char* src = name + len;
        char* dst = path;
        int i = 0;
        char c;
        while ((c = src[i]) != 0) {
            if (c == '\\') {
                *dst = '/';
            } else {
                *dst = c;
            }
            dst++;
            i++;
        }
        path[i] = src[i];
        ok = MWIO_openfile(path, mode, &file->u.fp);
        if (!ok) {
            fclose(file->u.fp);
        } else {
            file->type = FILEHANDLE_HOST;
        }
        return ok;
    } else {
        char* dst = path;
        int i = 0;
        char c;
        s32 entry;
        while ((c = *name) != 0) {
            if (c == '\\') {
                *dst = '/';
            } else {
                *dst = c;
            }
            dst++;
            i++;
            name++;
        }
        path[i] = c;
        entry = DVDConvertPathToEntrynum(path);
        if (entry == -1) {
            ok = 0;
        } else {
            ok = DVDFastOpen(entry, &file->u.dvd);
            if (!ok) {
                DVDClose(&file->u.dvd);
            }
        }
        if (ok) {
            file->type = FILEHANDLE_DVD;
        }
        return ok;
    }
}

int closefile(HANDLE_def* file) {
    int result = 0;
    if (file->type == FILEHANDLE_HOST) {
        result = fclose(file->u.fp);
    } else if (file->type == FILEHANDLE_DVD) {
        result = DVDClose(&file->u.dvd);
    }
    return (u8)(result == 0);
}

// Finishes the part just read and starts the next one.
static void AyncDVDRead(DVDFileInfo* info) {
    int n;

    if (gCurRead.state == READ_LEADIN) {
        MEM_copy(ADDR_TO_PTR(void, gCurRead.cur), gCurRead.buf + gCurRead.misalign, gCurRead.lead);
        gCurRead.cur += gCurRead.lead;
        gCurRead.offset += gCurRead.lead;
    } else if (gCurRead.state == READ_DIRECT) {
        DCInvalidateRange(ADDR_TO_PTR(void, gCurRead.cur), gCurRead.dstenddown - gCurRead.cur);
        n = gCurRead.dstenddown - gCurRead.cur;
        gCurRead.offset += n;
        gCurRead.cur += n;
    } else if (gCurRead.state == READ_BOUNCE) {
        n = 0x4000;
        if (gCurRead.dstend - gCurRead.cur < 0x4000) {
            n = gCurRead.dstend - gCurRead.cur;
        }
        MEM_copy(ADDR_TO_PTR(void, gCurRead.cur), gCurRead.buf, n);
        gCurRead.cur += n;
        gCurRead.offset += n;
    } else if (gCurRead.state == READ_SMALL) {
        MEM_copy(ADDR_TO_PTR(void, gCurRead.cur), gCurRead.buf + gCurRead.misalign, gCurRead.size);
        gCurRead.state = READ_SMALL;
        gCurRead.state = READ_DONE;
        AyncDVDCallback(0, NULL);
        return;
    }

    if (gCurRead.cur == gCurRead.dstup && gCurRead.cur < gCurRead.dstenddown) {
        gCurRead.state = READ_DIRECT;
        gCurRead.result = DVDReadAsyncPrio(info, ADDR_TO_PTR(void, gCurRead.cur), gCurRead.dstenddown - gCurRead.cur,
                                           gCurRead.offset, AyncDVDCallback, 2);
    } else if (gCurRead.dstend > gCurRead.cur) {
        int size = 0x4000;
        n = (gCurRead.dstend - gCurRead.cur + 31) & ~31;
        if (n < 0x4000) {
            size = n;
        }
        gCurRead.state = READ_BOUNCE;
        gCurRead.result += DVDReadAsyncPrio(info, gCurRead.buf, size, gCurRead.offset, AyncDVDCallback, 2);
    } else {
        gCurRead.state = READ_DONE;
        AyncDVDCallback(0, NULL);
    }
}

static int StartNonAlignedAyncRead(DVDFileInfo* info, void* dst, long offset, long size) {
    int n;

    if (size == 0) {
        gCurRead.state = READ_DONE;
        AyncDVDCallback(0, NULL);
        return 0;
    }

    gCurRead.size = size;
    gCurRead.offset = offset;
    gCurRead.cur = (int)PTR_TO_ADDR(dst);
    gCurRead.dstdown = (int)PTR_TO_ADDR(dst) & ~31;
    gCurRead.dstup = ((int)PTR_TO_ADDR(dst) + 31) & ~31;
    gCurRead.dstend = (int)PTR_TO_ADDR(dst) + size;
    gCurRead.dstenddown = ((int)PTR_TO_ADDR(dst) + size) & ~31;
    gCurRead.offdown = offset & ~31;
    gCurRead.offup = (offset + 31) & ~31;
    gCurRead.offend = offset + size;
    gCurRead.offenddown = (offset + size) & ~31;
    gCurRead.result = 0;
    gCurRead.misalign = offset - (offset & ~31);
    gCurRead.lead = 32 - gCurRead.misalign;

    if (size < 32) {
        gCurRead.state = READ_SMALL;
        gCurRead.result = DVDReadAsyncPrio(info, gCurRead.buf, (gCurRead.offend - gCurRead.offdown + 31) & ~31,
                                           gCurRead.offdown, AyncDVDCallback, 2);
    } else if (gCurRead.misalign != 0) {
        gCurRead.state = READ_LEADIN;
        gCurRead.result += DVDReadAsyncPrio(info, gCurRead.buf, (gCurRead.lead + 31) & ~31, gCurRead.offdown,
                                            AyncDVDCallback, 2);
    } else if ((int)PTR_TO_ADDR(dst) == gCurRead.dstup && (int)PTR_TO_ADDR(dst) < gCurRead.dstenddown) {
        gCurRead.state = READ_DIRECT;
        gCurRead.result = DVDReadAsyncPrio(info, dst, gCurRead.dstenddown - (int)PTR_TO_ADDR(dst), offset, AyncDVDCallback, 2);
    } else if (gCurRead.dstend > gCurRead.cur) {
        int len = 0x4000;
        n = (gCurRead.dstend - gCurRead.cur + 31) & ~31;
        if (n < 0x4000) {
            len = n;
        }
        gCurRead.state = READ_BOUNCE;
        gCurRead.result += DVDReadAsyncPrio(info, gCurRead.buf, len, gCurRead.offset, AyncDVDCallback, 2);
    }
    return gCurRead.size;
}

static void AyncDVDCallback(s32 result, DVDFileInfo* info) {
    OSSendMessage(&ReadFileThreadMsgQ, (OSMessage)1, OS_MESSAGE_BLOCK);
}

// Runs each readfile request, then waits for the drive to settle before
// completing the operation.
static void ReadFileThreadFunc(void* param) {
    OSMessage msg;

    for (;;) {
        HANDLE_def* file;
        int offset;
        int size;
        void* buffer;
        s32 status;

        OSReceiveMessage(&ReadFileThreadMsgQ, &msg, OS_MESSAGE_BLOCK);
        file = gCurRead.file;
        offset = gCurRead.fileoffset;
        size = gCurRead.filesize;
        buffer = gCurRead.buffer;
        if (file->type == FILEHANDLE_HOST) {
            fseek(file->u.fp, offset, SEEK_SET);
            fread(buffer, size, 1, file->u.fp);
        } else {
            StartNonAlignedAyncRead(&file->u.dvd, buffer, offset, size);
            OSReceiveMessage(&ReadFileThreadMsgQ, &msg, OS_MESSAGE_BLOCK);
            while (gCurRead.state != READ_DONE) {
                AyncDVDRead(&file->u.dvd);
                OSReceiveMessage(&ReadFileThreadMsgQ, &msg, OS_MESSAGE_BLOCK);
            }
        }
        for (;;) {
            status = DVDGetDriveStatus();
            if (status == 8 || (u32)status <= 1) {
                break;
            }
            THREAD_yield(100);
        }
        iFILE_CommandCompleteCallback(1);
    }
}

int readfile(HANDLE_def* file, void* buffer, int offset, int size) {
    gCurRead.fileoffset = offset;
    gCurRead.file = file;
    gCurRead.buffer = buffer;
    gCurRead.filesize = size;
    OSSendMessage(&ReadFileThreadMsgQ, NULL, OS_MESSAGE_BLOCK);
    return 1;
}

int writefile(HANDLE_def* file, void* buffer, int offset, int size) {
    if (file->type == FILEHANDLE_HOST) {
        fseek(file->u.fp, offset, SEEK_SET);
        fwrite(buffer, size, 1, file->u.fp);
        return 1;
    }
    return -1;
}

int getfilesize(HANDLE_def* file) {
    if (file->type == FILEHANDLE_HOST) {
        long pos = ftell(file->u.fp);
        long size;
        fseek(file->u.fp, 0, SEEK_END);
        size = ftell(file->u.fp);
        fseek(file->u.fp, pos, SEEK_SET);
        return size;
    }
    if (file->type == FILEHANDLE_DVD) {
        return file->u.dvd.length;
    }
    return 0;
}

void stopreadfile(HANDLE_def* file) {}

int deletefile(const char* name) {
    return -1;
}
