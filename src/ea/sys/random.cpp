#include "ea/sys/random.h"

// 192-bit lagged-add generator: seed[5] += seed[4] ... carried up the chain, then the whole
// number is incremented.
u32 seed[6] = {0xF22D0E56, 0x883126E9, 0xC624DD2F, 0x0702C49C, 0x9E353F7D, 0x6FDF3B64};

extern "C" u32 random(void) {
    u32 a;
    u32 b;
    u32 sum;
    u8 carry;

    a = seed[4];
    b = seed[5];
    carry = 0;
    sum = b + a;
    if (sum < b || sum < a) {
        carry = 1;
    }
    seed[4] = sum;

    a = seed[3];
    b = a + carry + sum;
    carry = (b < a);
    seed[3] = b;

    a = seed[2];
    sum = a + carry + b;
    carry = (sum < a);
    seed[2] = sum;

    a = seed[1];
    b = a + carry + sum;
    carry = (b < a);
    seed[1] = b;

    a = seed[0] + carry + b;
    seed[5]++;
    seed[0] = a;
    if (seed[5] == 0) {
        if (++seed[4] == 0) {
            if (++seed[3] == 0) {
                if (++seed[2] == 0) {
                    if (++seed[1] == 0) {
                        seed[0] = ++a;
                    }
                }
            }
        }
    }
    return a;
}
