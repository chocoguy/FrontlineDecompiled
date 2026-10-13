#include "ea/snd/sndi.h"

// Smallest prime of at least ms * rate / 1000 samples (minimum 3).
int SNDI_findprime(int ms, int rate) {
    int n = ms * rate / 1000;
    if (n < 3) {
        n = 3;
    }
    for (;;) {
        int root;
        int i = 0;
        for (;;) {
            int prev = i++;
            if (i * prev == n) {
                root = i - 1;
                break;
            }
            if (i * prev > n) {
                root = i - 2;
                break;
            }
        }
        int limit = root + 1;
        for (int d = 2; d <= limit; d++) {
            if (n % d == 0) {
                n++;
                break;
            }
            if (d == limit) {
                return n;
            }
        }
    }
}
