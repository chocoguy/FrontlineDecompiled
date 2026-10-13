#include "ea/sys/systask.h"
#include "ea/sys/systemvars.h"
#include "ea/real.h"

// Library tasks run from SYNCTASK_run: every `period` libticks (0 from SYNCTASK_add's -1).
struct SYNCTASK {
    /* 0x0 */ SYNCTASKFUNC func;
    /* 0x4 */ int period;
    /* 0x8 */ int next;  // libticks when due
    /* 0xC */ int busy;
};

static SYNCTASK systemtasksubs[16];

extern "C" void SYNCTASK_init(void) {
    MEM_fill(systemtasksubs, 0, sizeof(systemtasksubs));
}

// Add or update a task. Calls made from inside another add (re-entered) take a later free
// slot, one per level.
extern "C" void SYNCTASK_add(SYNCTASKFUNC func, int period, int delay) {
    static int reentry = 0;
    int slot = -1;
    int skip;
    int i;

    if (period == -1) {
        period = 0;
    } else if (period == 0) {
        period = 1;
    }
    skip = reentry++;
    for (i = 0; i < 16; i++) {
        if (systemtasksubs[i].func == func) {
            slot = i;
        } else if (systemtasksubs[i].func == NULL && slot == -1) {
            if (skip != 0) {
                skip--;
            } else {
                slot = i;
            }
        }
    }
    if (slot != -1) {
        systemtasksubs[slot].func = func;
        systemtasksubs[slot].period = period;
        systemtasksubs[slot].next = libticks + delay;
        systemtasksubs[slot].busy = 0;
    }
    reentry--;
}

extern "C" void SYNCTASK_del(void (*func)(void)) {
    int i;
    for (i = 0; i < 16 && (void*)systemtasksubs[i].func != (void*)func; i++) {
    }
    if (i < 16 && (void*)systemtasksubs[i].func == (void*)func) {
        systemtasksubs[i].func = NULL;
    }
}

// Run every due task; returns the OR of their results.
extern "C" int SYNCTASK_run(int arg) {
    static int lastsystemtask = 0;
    int result = 0;
    int i;

    lastsystemtask = libticks;
    for (i = 0; i < 16; i++) {
        SYNCTASK* task = &systemtasksubs[i];
        if (task->func != NULL && libticks >= task->next && task->busy == 0) {
            task->busy = 1;
            result |= task->func(arg, libticks - task->next);
            task->next = libticks + task->period;
            task->busy = 0;
        }
    }
    return result;
}
