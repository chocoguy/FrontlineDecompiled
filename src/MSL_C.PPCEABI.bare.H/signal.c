#include <stdlib.h>

#define SIGABRT 1
#define __signal_max 6

typedef void (*__signal_func_ptr)(int);

#define SIG_DFL ((__signal_func_ptr)0)
#define SIG_IGN ((__signal_func_ptr)1)

static __signal_func_ptr signal_funcs[__signal_max];

int raise(int sig) {
    __signal_func_ptr signal_func;

    if (sig < 1 || sig > __signal_max) {
        return -1;
    }

    signal_func = signal_funcs[sig - 1];

    if (signal_func != SIG_IGN) {
        signal_funcs[sig - 1] = SIG_DFL;
    }

    if (signal_func == SIG_IGN || (signal_func == SIG_DFL && sig == SIGABRT)) {
        return 0;
    }

    if (signal_func == SIG_DFL) {
        exit(0);
    }

    (*signal_func)(sig);
    return 0;
}
