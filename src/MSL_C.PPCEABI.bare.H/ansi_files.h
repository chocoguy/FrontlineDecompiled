// MSL C internal stdio declarations (the FILE layout and the helpers shared
// by the stdio units).
#ifndef _MSL_ANSI_FILES_H_
#define _MSL_ANSI_FILES_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long __file_handle;
typedef unsigned long fpos_t;
#ifndef _WCHAR_T_DEFINED
#define _WCHAR_T_DEFINED
typedef unsigned short wchar_t;
#endif

#define EOF -1
#define _IONBF 0
#define _IOLBF 1
#define _IOFBF 2
#define BUFSIZ 4096
#define _MSL_CONSOLE_BUFSIZ 256

// file_kind
#define __closed_file 0
#define __disk_file 1
#define __console_file 2
#define __unavailable_file 3

// open_mode
#define __must_exist 0
#define __create_if_necessary 1
#define __create_or_truncate 2

// io_mode
#define __read 1
#define __write 2
#define __read_write 3
#define __append 4

// io_state
#define __neutral 0
#define __writing 1
#define __reading 2
#define __rereading 3

// io results
#define __no_io_error 0
#define __io_error 1
#define __io_EOF 2

// file_orientation
#define __unoriented 0
#define __char_oriented 1
#define __wide_oriented 2

typedef struct __file_modes {
    unsigned int open_mode : 2;
    unsigned int io_mode : 3;
    unsigned int buffer_mode : 2;
    unsigned int file_kind : 3;
    unsigned int file_orientation : 2;
    unsigned int binary_io : 1;
} __file_modes;

typedef struct __file_state {
    unsigned int io_state : 3;
    unsigned int free_buffer : 1;
    unsigned char eof;
    unsigned char error;
} __file_state;

typedef void (*__idle_proc)(void);
typedef int (*__pos_proc)(__file_handle file, fpos_t* position, int mode, __idle_proc idle_proc);
typedef int (*__io_proc)(__file_handle file, unsigned char* buff, size_t* count, __idle_proc idle_proc);
typedef int (*__close_proc)(__file_handle file);

// size 0x50
struct _FILE {
    /* 0x00 */ __file_handle handle;
    /* 0x04 */ __file_modes mode;
    /* 0x08 */ __file_state state;
    /* 0x0C */ unsigned char is_dynamically_allocated;
    /* 0x0D */ unsigned char char_buffer;
    /* 0x0E */ unsigned char char_buffer_overflow;
    /* 0x0F */ unsigned char ungetc_buffer[2];
    /* 0x12 */ wchar_t ungetwc_buffer[2];
    /* 0x18 */ unsigned long position;
    /* 0x1C */ unsigned char* buffer;
    /* 0x20 */ unsigned long buffer_size;
    /* 0x24 */ unsigned char* buffer_ptr;
    /* 0x28 */ unsigned long buffer_len;
    /* 0x2C */ unsigned long buffer_alignment;
    /* 0x30 */ unsigned long saved_buffer_len;
    /* 0x34 */ unsigned long buffer_pos;
    /* 0x38 */ __pos_proc position_proc;
    /* 0x3C */ __io_proc read_proc;
    /* 0x40 */ __io_proc write_proc;
    /* 0x44 */ __close_proc close_proc;
    /* 0x48 */ __idle_proc idle_proc;
    /* 0x4C */ struct _FILE* next_file_struct;
};

#define _STATIC_FILES 4
extern FILE __files[_STATIC_FILES];

#define stdin (&__files[0])
#define stdout (&__files[1])
#define stderr (&__files[2])

#define set_error(file)          \
    do {                         \
        (file)->state.error = 1; \
        (file)->buffer_len = 0;  \
    } while (0)

// ansi_files.c
FILE* __find_unopened_file(void);
void __init_file(FILE* file, __file_modes mode, unsigned char* buff, size_t buff_size);
void __close_all(void);
int __flush_all(void);
int __flush_line_buffered_output_files(void);

// buffer_io.c
void __prep_buffer(FILE* file);
int __load_buffer(FILE* file, size_t* bytes_loaded, int alignment);
int __flush_buffer(FILE* file, size_t* bytes_flushed);
int setvbuf(FILE* file, char* buff, int mode, size_t size);

// file_io.c
int __get_file_modes(const char* mode, __file_modes* modes);
FILE* freopen(const char* name, const char* mode, FILE* file);
int fflush(FILE* file);

// FILE_POS.C
int _fseek(FILE* file, fpos_t offset, int mode);

// direct_io.c
size_t __fread(void* ptr, size_t memb_size, size_t num_memb, FILE* file);
size_t __fwrite(const void* ptr, size_t memb_size, size_t num_memb, FILE* file);

// misc_io.c
void __stdio_atexit(void);
void clearerr(FILE* file);

// wchar_io.c
int fwide(FILE* file, int mode);

// mem.c
void* __memrchr(const void* src, int val, size_t n);
void* memchr(const void* src, int val, size_t n);

// abort_exit.c
extern void (*__stdio_exit)(void);
extern void (*__console_exit)(void);

// mslsupp.c (MetroTRK) / uart_console_io.c: console and file system hooks
int __read_console(__file_handle handle, unsigned char* buffer, size_t* count, __idle_proc idle_proc);
int __write_console(__file_handle handle, unsigned char* buffer, size_t* count, __idle_proc idle_proc);
int __close_console(__file_handle handle);
int __open_file(const char* name, __file_modes mode, __file_handle* handle);
int __position_file(__file_handle handle, fpos_t* position, int mode, __idle_proc idle_proc);
int __read_file(__file_handle handle, unsigned char* buffer, size_t* count, __idle_proc idle_proc);
int __write_file(__file_handle handle, unsigned char* buffer, size_t* count, __idle_proc idle_proc);
int __close_file(__file_handle handle);

// errno.c
extern int errno;
#define EDOM 33
#define ERANGE 34
#define EFPOS 40

void free(void* ptr);
void* malloc(size_t size);

#ifdef __cplusplus
}
#endif

#endif
