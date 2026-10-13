// Minimal MSL C stdio.h declarations, added as decompiled code needs them.
#ifndef _MSL_STDIO_H_
#define _MSL_STDIO_H_

#include "stdarg.h"

#ifdef __cplusplus
extern "C" {
#endif

int sprintf(char* buf, const char* fmt, ...);
int printf(const char* fmt, ...);
int vsprintf(char* buf, const char* fmt, va_list args);
int vprintf(const char* fmt, va_list args);

#ifndef _SIZE_T_DEFINED
#define _SIZE_T_DEFINED
typedef unsigned long size_t;
#endif

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef struct _FILE FILE;
FILE* fopen(const char* name, const char* mode);
int fclose(FILE* file);
size_t fread(void* buf, size_t size, size_t count, FILE* file);
size_t fwrite(const void* buf, size_t size, size_t count, FILE* file);
int fseek(FILE* file, long offset, int origin);
long ftell(FILE* file);

#ifdef __cplusplus
}
#endif

#endif
