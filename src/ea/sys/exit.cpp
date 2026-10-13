#include "ea/sys/exit.h"
#include <stdlib.h>

typedef void (*EXITFUNC)(void);

static EXITFUNC exitfunctions[64] = {0};

// Run the exit functions, newest slot first.
extern "C" void REAL_restore(void) {
    int i;
    for (i = 63; i >= 0; i--) {
        if (exitfunctions[i] != NULL) {
            exitfunctions[i]();
        }
        exitfunctions[i] = NULL;
    }
}

extern "C" void REAL_exit(void) {
    int i;
    for (i = 63; i >= 0; i--) {
        if (exitfunctions[i] != NULL) {
            exitfunctions[i]();
        }
        exitfunctions[i] = NULL;
    }
    exit(0);
}

extern "C" void REAL_addexit(void (*func)(void)) {
    int i;
    for (i = 0; i < 64; i++) {
        if (exitfunctions[i] == func) {
            return;
        }
    }
    for (i = 0; i < 64; i++) {
        if (exitfunctions[i] == NULL) {
            exitfunctions[i] = func;
            return;
        }
    }
}

extern "C" void REAL_removeexit(void (*func)(void)) {
    int i;
    for (i = 0; i < 64; i++) {
        if (exitfunctions[i] == func) {
            exitfunctions[i] = NULL;
            return;
        }
    }
}
