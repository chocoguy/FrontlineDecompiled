// The file device under FILESYS: host files ("hd:" names, MSL stdio) or
// DVD files. Reads run on their own thread and report through
// iFILE_CommandCompleteCallback. Hand-maintained.
#ifndef _EA_SYS_FILEDEV_H_
#define _EA_SYS_FILEDEV_H_

#include "types.h"

struct HANDLE_def;

void initfiledev();  // 0x801478A0
void killfiledev();  // 0x8014790C
int openfile(const char* name, unsigned int mode, HANDLE_def* file);  // 0x801479B8
int closefile(HANDLE_def* file);  // 0x80147BBC
int readfile(HANDLE_def* file, void* buffer, int offset, int size);  // 0x8014817C
int writefile(HANDLE_def* file, void* buffer, int offset, int size);  // 0x801481CC
int getfilesize(HANDLE_def* file);  // 0x80148248
void stopreadfile(HANDLE_def* file);  // 0x801482E0
int deletefile(const char* name);  // 0x801482E4

#endif
