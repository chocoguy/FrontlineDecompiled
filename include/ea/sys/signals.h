// EA signals: a one-message queue to wait on.
#ifndef _EA_SYS_SIGNALS_H_
#define _EA_SYS_SIGNALS_H_

#include "dolphin/os.h"

struct SIGNAL {
    /* 0x00 */ u32 unk0;
    /* 0x04 */ OSMessageQueue queue;
    /* 0x24 */ OSMessage message;
};

extern "C" {
int SIGNAL_create(SIGNAL* signal);
BOOL SIGNAL_set(SIGNAL* signal);
BOOL SIGNAL_wait(SIGNAL* signal);
}

#endif
