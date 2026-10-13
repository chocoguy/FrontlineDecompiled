// Minimal MSL stdlib.h declarations, added as decompiled code needs them.
#ifndef _MSL_STDLIB_H_
#define _MSL_STDLIB_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long size_t;

void* bsearch(const void* key, const void* base, size_t count, size_t size,
              int (*compare)(const void*, const void*));
void qsort(void* base, size_t count, size_t size, int (*compare)(const void*, const void*));
int abs(int x);
int atoi(const char* s);
double atof(const char* s);
int rand(void);
void srand(unsigned int seed);
void exit(int status);

#ifdef __cplusplus
}
#endif

#endif
