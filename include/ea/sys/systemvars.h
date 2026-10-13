// EA system globals (systemvars.cpp).
#ifndef _EA_SYS_SYSTEMVARS_H_
#define _EA_SYS_SYSTEMVARS_H_

#include "types.h"

typedef void (*TIMERSUB)(void);

extern TIMERSUB vbltmrsub[8];  // run by the vertical blank timer
extern TIMERSUB tmrsub[8];     // run by the TIMER alarm
extern void (*REALabortmessage)(const char* fmt, ...);  // SYSTEM_abortmessage
extern int vblticks;
extern "C" s32 TIMERhz;
extern int ticks;     // TIMER alarm ticks
extern int libticks;  // same, for library tasks (SYNCTASK)

#endif
