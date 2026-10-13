#include "ansi_files.h"

int InitializeUART(unsigned long baudRate);
int WriteUARTN(const void* buf, unsigned long len);
int __TRK_write_console(__file_handle handle, unsigned char* buffer, size_t* count, __idle_proc idle_proc);

// stdout/stderr: the serial UART (when present), echoed to MetroTRK.
int __write_console(__file_handle handle, unsigned char* buffer, size_t* count, __idle_proc idle_proc) {
    static int initialized = 0;
    int result = 0;

    if (initialized == 0) {
        result = InitializeUART(57600);
        if (result == 0) {
            initialized = 1;
        }
    }

    if (result != 0) {
        return __io_error;
    }

    if (WriteUARTN(buffer, *count) != 0) {
        *count = 0;
        return __io_error;
    }

    __TRK_write_console(handle, buffer, count, idle_proc);
    return __no_io_error;
}
