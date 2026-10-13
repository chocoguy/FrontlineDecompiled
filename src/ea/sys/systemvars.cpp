#include "ea/sys/systemvars.h"

extern "C" void SYSTEM_abortmessage(const char* fmt, ...);

TIMERSUB vbltmrsub[8] = {0};
TIMERSUB tmrsub[8] = {0};
void (*REALabortmessage)(const char* fmt, ...) = SYSTEM_abortmessage;
int vblticks;
s32 TIMERhz;
int ticks;
int libticks;
