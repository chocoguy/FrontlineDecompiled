#include <string.h>

// Case-insensitive compare (ASCII letters only).
extern "C" int stricmp(const char* a, const char* b) {
    int diff;
    for (;;) {
        int cb = *b;
        int ca;
        if (cb >= 'A' && cb <= 'Z') {
            cb += 0x20;
        }
        ca = *a;
        if (ca >= 'A' && ca <= 'Z') {
            ca += 0x20;
        }
        diff = ca - cb;
        if (diff != 0 || *a == '\0') {
            break;
        }
        a++;
        b++;
    }
    return diff;
}
