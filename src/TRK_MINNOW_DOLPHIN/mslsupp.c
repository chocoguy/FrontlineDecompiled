#include <TRK_MINNOW_DOLPHIN/trk.h>

// MSL console/file I/O hooks routed to the host debugger.

#define DSMSG_WriteFile 0xD0
#define DSMSG_ReadFile 0xD1
#define DSMSG_OpenFile 0xD2
#define DSMSG_CloseFile 0xD3
#define DSMSG_PositionFile 0xD4

// MSL return codes
#define __no_io_error 0
#define __io_error 1
#define __io_EOF 2

// MSL __file_modes (MWCC bitfields allocate from the top bit).
typedef struct __file_modes {
    unsigned int open_mode : 2;
    unsigned int io_mode : 3;
    unsigned int buffer_mode : 2;
    unsigned int file_kind : 3;
    unsigned int file_orientation : 2;
    unsigned int binary_io : 1;
} __file_modes;

u8 convertFileMode(__file_modes* mode);

static inline int __access_file(u32 handle, u8* buffer, u32* count, MessageCommandID cmd) {
    u32 countTemp;
    u8 result;

    if (GetTRKConnected() == FALSE) {
        return __io_error;
    }

    countTemp = *count;
    result = TRKAccessFile(cmd, handle, &countTemp, buffer);
    *count = countTemp;

    switch (result) {
    case 0:
        return __no_io_error;
    case 2:
        return __io_EOF;
    case 1:
    default:
        return __io_error;
    }
}

static inline int __close_handle(u32 handle) {
    u8 result;

    if (GetTRKConnected() == FALSE) {
        return __io_error;
    }

    result = TRKCloseFile(DSMSG_CloseFile, handle);
    switch (result) {
    case 0:
        return __no_io_error;
    case 2:
        return __io_EOF;
    case 1:
    default:
        return __io_error;
    }
}

int __read_console(u32 handle, u8* buffer, u32* count, void* ref_con) {
    if (GetUseSerialIO() == FALSE) {
        return __io_error;
    }
    return __access_file(0, buffer, count, DSMSG_ReadFile);
}

int __TRK_write_console(u32 handle, u8* buffer, u32* count, void* ref_con) {
    if (GetUseSerialIO() == FALSE) {
        return __io_error;
    }
    return __access_file(1, buffer, count, DSMSG_WriteFile);
}

int __close_console(u32 handle) {
    return __close_handle(handle);
}

int __read_file(u32 handle, u8* buffer, u32* count, void* ref_con) {
    return __access_file(handle, buffer, count, DSMSG_ReadFile);
}

int __write_file(u32 handle, u8* buffer, u32* count, void* ref_con) {
    return __access_file(handle, buffer, count, DSMSG_WriteFile);
}

int __open_file(const char* name, __file_modes mode, u32* handle) {
    u8 result;

    if (GetTRKConnected() == FALSE) {
        return __io_error;
    }

    result = TRKOpenFile(DSMSG_OpenFile, name, convertFileMode(&mode), handle);
    switch (result) {
    case 0:
        return __no_io_error;
    case 2:
        return __io_EOF;
    case 1:
    default:
        return __io_error;
    }
}

int __close_file(u32 handle) {
    return __close_handle(handle);
}

int __position_file(u32 handle, s32* position, int mode, void* ref_con) {
    u8 result;
    u8 dsMode;

    dsMode = 0;
    if (GetTRKConnected() == FALSE) {
        return __io_error;
    }

    if (mode == 0) {  // SEEK_SET
        dsMode = 0;
    } else if (mode == 1) {  // SEEK_CUR
        dsMode = 1;
    } else if (mode == 2) {  // SEEK_END
        dsMode = 2;
    }

    result = TRKPositionFile(DSMSG_PositionFile, handle, position, dsMode);
    switch (result) {
    case 0:
        return __no_io_error;
    case 2:
        return __io_EOF;
    case 1:
    default:
        return __io_error;
    }
}

#pragma auto_inline off
u8 convertFileMode(__file_modes* mode) {
    u8 modeOut;

    modeOut = 0;
    switch (mode->open_mode) {
    case 0:  // must exist
        modeOut |= 1;
        break;
    case 2:  // create or truncate
        modeOut |= 2;
        break;
    case 1:  // create if necessary
        modeOut |= 4;
        break;
    }

    switch (mode->io_mode) {
    case 1:  // read
        modeOut |= 1;
        break;
    case 2:  // write
        modeOut |= 2;
        break;
    case 6:  // append
        modeOut |= 4;
        break;
    case 3:  // read/write
        modeOut |= 0x12;
        break;
    case 7:  // read/append
        modeOut |= 7;
        break;
    }

    if (mode->binary_io == 1) {
        modeOut |= 8;
    }

    return modeOut;
}
#pragma auto_inline reset
