#include "ea/snd/sndi.h"

// Same 192-bit generator as ea/sys/random.cpp.
u32 SNDIrandseedorig[6] = {0xF22D0E56, 0x883126E9, 0xC624DD2F, 0x0702C49C, 0x9E353F7D, 0x6FDF3B64};
u32 SNDIrandseed[6];

void SNDI_randomseed(unsigned int seed) {
    SNDIrandseed[0] = seed + SNDIrandseedorig[0];
    SNDIrandseed[1] = seed + SNDIrandseedorig[1];
    SNDIrandseed[2] = seed + SNDIrandseedorig[2];
    SNDIrandseed[3] = seed + SNDIrandseedorig[3];
    SNDIrandseed[4] = seed + SNDIrandseedorig[4];
    SNDIrandseed[5] = seed + SNDIrandseedorig[5];
}

unsigned int iSNDrandom() {
    u32 a;
    u32 b;
    u32 sum;
    u8 carry;

    a = SNDIrandseed[4];
    b = SNDIrandseed[5];
    carry = 0;
    sum = b + a;
    if (sum < b || sum < a) {
        carry = 1;
    }
    SNDIrandseed[4] = sum;

    a = SNDIrandseed[3];
    b = a + carry + sum;
    carry = (b < a);
    SNDIrandseed[3] = b;

    a = SNDIrandseed[2];
    sum = a + carry + b;
    carry = (sum < a);
    SNDIrandseed[2] = sum;

    a = SNDIrandseed[1];
    b = a + carry + sum;
    carry = (b < a);
    SNDIrandseed[1] = b;

    a = SNDIrandseed[0] + carry + b;
    SNDIrandseed[5]++;
    SNDIrandseed[0] = a;
    if (SNDIrandseed[5] == 0) {
        if (++SNDIrandseed[4] == 0) {
            if (++SNDIrandseed[3] == 0) {
                if (++SNDIrandseed[2] == 0) {
                    if (++SNDIrandseed[1] == 0) {
                        SNDIrandseed[0] = ++a;
                    }
                }
            }
        }
    }
    return a;
}
