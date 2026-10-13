// The decoder's variable-length code tables and readers (MSSG getvlc.c
// and getvlc.h): macroblock types, motion codes, coded block patterns,
// address increments and DC differences. A bad code sets MPDfault_flag.
#include "movie/mpeg2dec.h"

static VLCtab PMBtab0[8] = {
    {-1, 0}, {8, 3}, {2, 2}, {2, 2}, {10, 1}, {10, 1}, {10, 1}, {10, 1},
};

static VLCtab PMBtab1[8] = {
    {-1, 0}, {17, 6}, {18, 5}, {18, 5}, {26, 5}, {26, 5}, {1, 5}, {1, 5},
};

static VLCtab BMBtab0[16] = {
    {-1, 0}, {-1, 0}, {8, 4}, {10, 4}, {4, 3}, {4, 3}, {6, 3}, {6, 3},
    {12, 2}, {12, 2}, {12, 2}, {12, 2}, {14, 2}, {14, 2}, {14, 2}, {14, 2},
};

static VLCtab BMBtab1[8] = {
    {-1, 0}, {17, 6}, {22, 6}, {26, 6}, {30, 5}, {30, 5}, {1, 5}, {1, 5},
};

static VLCtab spIMBtab[16] = {
    {-1, 0}, {64, 4}, {17, 4}, {1, 4}, {82, 2}, {82, 2}, {82, 2}, {82, 2},
    {66, 1}, {66, 1}, {66, 1}, {66, 1}, {66, 1}, {66, 1}, {66, 1}, {66, 1},
};

static VLCtab spPMBtab0[16] = {
    {-1, 0}, {-1, 0}, {8, 4}, {40, 4}, {26, 3}, {26, 3}, {42, 3}, {42, 3},
    {10, 2}, {10, 2}, {10, 2}, {10, 2}, {58, 2}, {58, 2}, {58, 2}, {58, 2},
};

static VLCtab spPMBtab1[16] = {
    {-1, 0}, {-1, 0}, {82, 7}, {64, 7}, {2, 7}, {66, 7}, {17, 7}, {1, 7},
    {18, 6}, {18, 6}, {50, 6}, {50, 6}, {32, 6}, {32, 6}, {34, 6}, {34, 6},
};

static VLCtab spBMBtab0[14] = {
    {8, 4}, {10, 4}, {4, 3}, {4, 3}, {6, 3}, {6, 3}, {12, 2}, {12, 2},
    {12, 2}, {12, 2}, {14, 2}, {14, 2}, {14, 2}, {14, 2},
};

static VLCtab spBMBtab1[12] = {
    {26, 7}, {22, 7}, {1, 7}, {30, 7}, {40, 6}, {40, 6}, {42, 6}, {42, 6},
    {36, 6}, {36, 6}, {38, 6}, {38, 6},
};

static VLCtab spBMBtab2[8] = {
    {17, 8}, {17, 8}, {58, 8}, {58, 8}, {54, 9}, {82, 9}, {64, 9}, {66, 9},
};

static VLCtab SNRMBtab[8] = {
    {-1, 0}, {0, 3}, {18, 2}, {18, 2}, {2, 1}, {2, 1}, {2, 1}, {2, 1},
};

static VLCtab MVtab0[8] = {
    {-1, 0}, {3, 3}, {2, 2}, {2, 2}, {1, 1}, {1, 1}, {1, 1}, {1, 1},
};

static VLCtab MVtab1[8] = {
    {-1, 0}, {-1, 0}, {-1, 0}, {7, 6}, {6, 6}, {5, 6}, {4, 5}, {4, 5},
};

static VLCtab MVtab2[12] = {
    {16, 9}, {15, 9}, {14, 9}, {13, 9}, {12, 9}, {11, 9}, {10, 8}, {10, 8},
    {9, 8}, {9, 8}, {8, 8}, {8, 8},
};

static VLCtab CBPtab0[32] = {
    {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0},
    {62, 5}, {2, 5}, {61, 5}, {1, 5}, {56, 5}, {52, 5}, {44, 5}, {28, 5},
    {40, 5}, {20, 5}, {48, 5}, {12, 5}, {32, 4}, {32, 4}, {16, 4}, {16, 4},
    {8, 4}, {8, 4}, {4, 4}, {4, 4}, {60, 3}, {60, 3}, {60, 3}, {60, 3},
};

static VLCtab CBPtab1[64] = {
    {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {58, 8}, {54, 8}, {46, 8}, {30, 8},
    {57, 8}, {53, 8}, {45, 8}, {29, 8}, {38, 8}, {26, 8}, {37, 8}, {25, 8},
    {43, 8}, {23, 8}, {51, 8}, {15, 8}, {42, 8}, {22, 8}, {50, 8}, {14, 8},
    {41, 8}, {21, 8}, {49, 8}, {13, 8}, {35, 8}, {19, 8}, {11, 8}, {7, 8},
    {34, 7}, {34, 7}, {18, 7}, {18, 7}, {10, 7}, {10, 7}, {6, 7}, {6, 7},
    {33, 7}, {33, 7}, {17, 7}, {17, 7}, {9, 7}, {9, 7}, {5, 7}, {5, 7},
    {63, 6}, {63, 6}, {63, 6}, {63, 6}, {3, 6}, {3, 6}, {3, 6}, {3, 6},
    {36, 6}, {36, 6}, {36, 6}, {36, 6}, {24, 6}, {24, 6}, {24, 6}, {24, 6},
};

static VLCtab CBPtab2[8] = {
    {-1, 0}, {0, 9}, {39, 9}, {27, 9}, {59, 9}, {55, 9}, {47, 9}, {31, 9},
};

static VLCtab MBAtab1[16] = {
    {-1, 0}, {-1, 0}, {7, 5}, {6, 5}, {5, 4}, {5, 4}, {4, 4}, {4, 4},
    {3, 3}, {3, 3}, {3, 3}, {3, 3}, {2, 3}, {2, 3}, {2, 3}, {2, 3},
};

static VLCtab MBAtab2[104] = {
    {33, 11}, {32, 11}, {31, 11}, {30, 11}, {29, 11}, {28, 11}, {27, 11}, {26, 11},
    {25, 11}, {24, 11}, {23, 11}, {22, 11}, {21, 10}, {21, 10}, {20, 10}, {20, 10},
    {19, 10}, {19, 10}, {18, 10}, {18, 10}, {17, 10}, {17, 10}, {16, 10}, {16, 10},
    {15, 8}, {15, 8}, {15, 8}, {15, 8}, {15, 8}, {15, 8}, {15, 8}, {15, 8},
    {14, 8}, {14, 8}, {14, 8}, {14, 8}, {14, 8}, {14, 8}, {14, 8}, {14, 8},
    {13, 8}, {13, 8}, {13, 8}, {13, 8}, {13, 8}, {13, 8}, {13, 8}, {13, 8},
    {12, 8}, {12, 8}, {12, 8}, {12, 8}, {12, 8}, {12, 8}, {12, 8}, {12, 8},
    {11, 8}, {11, 8}, {11, 8}, {11, 8}, {11, 8}, {11, 8}, {11, 8}, {11, 8},
    {10, 8}, {10, 8}, {10, 8}, {10, 8}, {10, 8}, {10, 8}, {10, 8}, {10, 8},
    {9, 7}, {9, 7}, {9, 7}, {9, 7}, {9, 7}, {9, 7}, {9, 7}, {9, 7},
    {9, 7}, {9, 7}, {9, 7}, {9, 7}, {9, 7}, {9, 7}, {9, 7}, {9, 7},
    {8, 7}, {8, 7}, {8, 7}, {8, 7}, {8, 7}, {8, 7}, {8, 7}, {8, 7},
    {8, 7}, {8, 7}, {8, 7}, {8, 7}, {8, 7}, {8, 7}, {8, 7}, {8, 7},
};

static VLCtab DClumtab0[32] = {
    {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2},
    {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2},
    {0, 3}, {0, 3}, {0, 3}, {0, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3},
    {4, 3}, {4, 3}, {4, 3}, {4, 3}, {5, 4}, {5, 4}, {6, 5}, {-1, 0},
};

static VLCtab DClumtab1[16] = {
    {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6}, {7, 6},
    {8, 7}, {8, 7}, {8, 7}, {8, 7}, {9, 8}, {9, 8}, {10, 9}, {11, 9},
};

static VLCtab DCchromtab0[32] = {
    {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2},
    {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2},
    {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2},
    {3, 3}, {3, 3}, {3, 3}, {3, 3}, {4, 4}, {4, 4}, {5, 5}, {-1, 0},
};

static VLCtab DCchromtab1[32] = {
    {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6},
    {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6},
    {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7}, {7, 7},
    {8, 8}, {8, 8}, {8, 8}, {8, 8}, {9, 9}, {9, 9}, {10, 10}, {11, 10},
};

DCTtab DCTtabfirst[12] = {
    {0, 2, 4}, {2, 1, 4}, {1, 1, 3}, {1, 1, 3}, {0, 1, 1}, {0, 1, 1}, {0, 1, 1}, {0, 1, 1},
    {0, 1, 1}, {0, 1, 1}, {0, 1, 1}, {0, 1, 1},
};

DCTtab DCTtabnext[12] = {
    {0, 2, 4}, {2, 1, 4}, {1, 1, 3}, {1, 1, 3}, {64, 0, 2}, {64, 0, 2}, {64, 0, 2}, {64, 0, 2},
    {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2},
};

DCTtab DCTtab0[60] = {
    {65, 0, 6}, {65, 0, 6}, {65, 0, 6}, {65, 0, 6}, {2, 2, 7}, {2, 2, 7}, {9, 1, 7}, {9, 1, 7},
    {0, 4, 7}, {0, 4, 7}, {8, 1, 7}, {8, 1, 7}, {7, 1, 6}, {7, 1, 6}, {7, 1, 6}, {7, 1, 6},
    {6, 1, 6}, {6, 1, 6}, {6, 1, 6}, {6, 1, 6}, {1, 2, 6}, {1, 2, 6}, {1, 2, 6}, {1, 2, 6},
    {5, 1, 6}, {5, 1, 6}, {5, 1, 6}, {5, 1, 6}, {13, 1, 8}, {0, 6, 8}, {12, 1, 8}, {11, 1, 8},
    {3, 2, 8}, {1, 3, 8}, {0, 5, 8}, {10, 1, 8}, {0, 3, 5}, {0, 3, 5}, {0, 3, 5}, {0, 3, 5},
    {0, 3, 5}, {0, 3, 5}, {0, 3, 5}, {0, 3, 5}, {4, 1, 5}, {4, 1, 5}, {4, 1, 5}, {4, 1, 5},
    {4, 1, 5}, {4, 1, 5}, {4, 1, 5}, {4, 1, 5}, {3, 1, 5}, {3, 1, 5}, {3, 1, 5}, {3, 1, 5},
    {3, 1, 5}, {3, 1, 5}, {3, 1, 5}, {3, 1, 5},
};

DCTtab DCTtab0a[252] = {
    {65, 0, 6}, {65, 0, 6}, {65, 0, 6}, {65, 0, 6}, {7, 1, 7}, {7, 1, 7}, {8, 1, 7}, {8, 1, 7},
    {6, 1, 7}, {6, 1, 7}, {2, 2, 7}, {2, 2, 7}, {0, 7, 6}, {0, 7, 6}, {0, 7, 6}, {0, 7, 6},
    {0, 6, 6}, {0, 6, 6}, {0, 6, 6}, {0, 6, 6}, {4, 1, 6}, {4, 1, 6}, {4, 1, 6}, {4, 1, 6},
    {5, 1, 6}, {5, 1, 6}, {5, 1, 6}, {5, 1, 6}, {1, 5, 8}, {11, 1, 8}, {0, 11, 8}, {0, 10, 8},
    {13, 1, 8}, {12, 1, 8}, {3, 2, 8}, {1, 4, 8}, {2, 1, 5}, {2, 1, 5}, {2, 1, 5}, {2, 1, 5},
    {2, 1, 5}, {2, 1, 5}, {2, 1, 5}, {2, 1, 5}, {1, 2, 5}, {1, 2, 5}, {1, 2, 5}, {1, 2, 5},
    {1, 2, 5}, {1, 2, 5}, {1, 2, 5}, {1, 2, 5}, {3, 1, 5}, {3, 1, 5}, {3, 1, 5}, {3, 1, 5},
    {3, 1, 5}, {3, 1, 5}, {3, 1, 5}, {3, 1, 5}, {1, 1, 3}, {1, 1, 3}, {1, 1, 3}, {1, 1, 3},
    {1, 1, 3}, {1, 1, 3}, {1, 1, 3}, {1, 1, 3}, {1, 1, 3}, {1, 1, 3}, {1, 1, 3}, {1, 1, 3},
    {1, 1, 3}, {1, 1, 3}, {1, 1, 3}, {1, 1, 3}, {1, 1, 3}, {1, 1, 3}, {1, 1, 3}, {1, 1, 3},
    {1, 1, 3}, {1, 1, 3}, {1, 1, 3}, {1, 1, 3}, {1, 1, 3}, {1, 1, 3}, {1, 1, 3}, {1, 1, 3},
    {1, 1, 3}, {1, 1, 3}, {1, 1, 3}, {1, 1, 3}, {64, 0, 4}, {64, 0, 4}, {64, 0, 4}, {64, 0, 4},
    {64, 0, 4}, {64, 0, 4}, {64, 0, 4}, {64, 0, 4}, {64, 0, 4}, {64, 0, 4}, {64, 0, 4}, {64, 0, 4},
    {64, 0, 4}, {64, 0, 4}, {64, 0, 4}, {64, 0, 4}, {0, 3, 4}, {0, 3, 4}, {0, 3, 4}, {0, 3, 4},
    {0, 3, 4}, {0, 3, 4}, {0, 3, 4}, {0, 3, 4}, {0, 3, 4}, {0, 3, 4}, {0, 3, 4}, {0, 3, 4},
    {0, 3, 4}, {0, 3, 4}, {0, 3, 4}, {0, 3, 4}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2},
    {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2},
    {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2},
    {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2},
    {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2},
    {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2},
    {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2},
    {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2},
    {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 1, 2}, {0, 2, 3}, {0, 2, 3}, {0, 2, 3}, {0, 2, 3},
    {0, 2, 3}, {0, 2, 3}, {0, 2, 3}, {0, 2, 3}, {0, 2, 3}, {0, 2, 3}, {0, 2, 3}, {0, 2, 3},
    {0, 2, 3}, {0, 2, 3}, {0, 2, 3}, {0, 2, 3}, {0, 2, 3}, {0, 2, 3}, {0, 2, 3}, {0, 2, 3},
    {0, 2, 3}, {0, 2, 3}, {0, 2, 3}, {0, 2, 3}, {0, 2, 3}, {0, 2, 3}, {0, 2, 3}, {0, 2, 3},
    {0, 2, 3}, {0, 2, 3}, {0, 2, 3}, {0, 2, 3}, {0, 4, 5}, {0, 4, 5}, {0, 4, 5}, {0, 4, 5},
    {0, 4, 5}, {0, 4, 5}, {0, 4, 5}, {0, 4, 5}, {0, 5, 5}, {0, 5, 5}, {0, 5, 5}, {0, 5, 5},
    {0, 5, 5}, {0, 5, 5}, {0, 5, 5}, {0, 5, 5}, {9, 1, 7}, {9, 1, 7}, {1, 3, 7}, {1, 3, 7},
    {10, 1, 7}, {10, 1, 7}, {0, 8, 7}, {0, 8, 7}, {0, 9, 7}, {0, 9, 7}, {0, 12, 8}, {0, 13, 8},
    {2, 3, 8}, {4, 2, 8}, {0, 14, 8}, {0, 15, 8},
};

DCTtab DCTtab1[8] = {
    {16, 1, 10}, {5, 2, 10}, {0, 7, 10}, {2, 3, 10}, {1, 4, 10}, {15, 1, 10}, {14, 1, 10}, {4, 2, 10},
};

DCTtab DCTtab1a[8] = {
    {5, 2, 9}, {5, 2, 9}, {14, 1, 9}, {14, 1, 9}, {2, 4, 10}, {16, 1, 10}, {15, 1, 9}, {15, 1, 9},
};

DCTtab DCTtab2[16] = {
    {0, 11, 12}, {8, 2, 12}, {4, 3, 12}, {0, 10, 12}, {2, 4, 12}, {7, 2, 12}, {21, 1, 12}, {20, 1, 12},
    {0, 9, 12}, {19, 1, 12}, {18, 1, 12}, {1, 5, 12}, {3, 3, 12}, {0, 8, 12}, {6, 2, 12}, {17, 1, 12},
};

DCTtab DCTtab3[16] = {
    {10, 2, 13}, {9, 2, 13}, {5, 3, 13}, {3, 4, 13}, {2, 5, 13}, {1, 7, 13}, {1, 6, 13}, {0, 15, 13},
    {0, 14, 13}, {0, 13, 13}, {0, 12, 13}, {26, 1, 13}, {25, 1, 13}, {24, 1, 13}, {23, 1, 13}, {22, 1, 13},
};

DCTtab DCTtab4[16] = {
    {0, 31, 14}, {0, 30, 14}, {0, 29, 14}, {0, 28, 14}, {0, 27, 14}, {0, 26, 14}, {0, 25, 14}, {0, 24, 14},
    {0, 23, 14}, {0, 22, 14}, {0, 21, 14}, {0, 20, 14}, {0, 19, 14}, {0, 18, 14}, {0, 17, 14}, {0, 16, 14},
};

DCTtab DCTtab5[16] = {
    {0, 40, 15}, {0, 39, 15}, {0, 38, 15}, {0, 37, 15}, {0, 36, 15}, {0, 35, 15}, {0, 34, 15}, {0, 33, 15},
    {0, 32, 15}, {1, 14, 15}, {1, 13, 15}, {1, 12, 15}, {1, 11, 15}, {1, 10, 15}, {1, 9, 15}, {1, 8, 15},
};

DCTtab DCTtab6[16] = {
    {1, 18, 16}, {1, 17, 16}, {1, 16, 16}, {1, 15, 16}, {6, 3, 16}, {16, 2, 16}, {15, 2, 16}, {14, 2, 16},
    {13, 2, 16}, {12, 2, 16}, {11, 2, 16}, {31, 1, 16}, {30, 1, 16}, {29, 1, 16}, {28, 1, 16}, {27, 1, 16},
};


// Macroblock types (MSSG getvlc.c), inlined into Get_macroblock_type.
static inline int Get_I_macroblock_type() {
    if (Get_Bits1()) {
        return 1;
    }
    if (!Get_Bits1()) {
        MPDfault_flag = 1;
    }
    return 17;
}

static inline int Get_P_macroblock_type() {
    int code;
    if ((code = Show_Bits(6)) >= 8) {
        code >>= 3;
        Flush_Buffer(PMBtab0[code].len);
        return PMBtab0[code].val;
    }
    if (code == 0) {
        MPDfault_flag = 1;
        return 0;
    }
    Flush_Buffer(PMBtab1[code].len);
    return PMBtab1[code].val;
}

static inline int Get_B_macroblock_type() {
    int code;
    if ((code = Show_Bits(6)) >= 8) {
        code >>= 2;
        Flush_Buffer(BMBtab0[code].len);
        return BMBtab0[code].val;
    }
    if (code == 0) {
        MPDfault_flag = 1;
        return 0;
    }
    Flush_Buffer(BMBtab1[code].len);
    return BMBtab1[code].val;
}

static inline int Get_D_macroblock_type() {
    if (!Get_Bits1()) {
        MPDfault_flag = 1;
    }
    return 1;
}

static inline int Get_I_Spatial_macroblock_type() {
    int code;
    if ((code = Show_Bits(4)) == 0) {
        MPDfault_flag = 1;
        return 0;
    }
    Flush_Buffer(spIMBtab[code].len);
    return spIMBtab[code].val;
}

static inline int Get_P_Spatial_macroblock_type() {
    int code;
    if ((code = Show_Bits(7)) < 2) {
        MPDfault_flag = 1;
        return 0;
    }
    if (code >= 16) {
        code >>= 3;
        Flush_Buffer(spPMBtab0[code].len);
        return spPMBtab0[code].val;
    }
    Flush_Buffer(spPMBtab1[code].len);
    return spPMBtab1[code].val;
}

static inline int Get_B_Spatial_macroblock_type() {
    int code;
    VLCtab* p;
    code = Show_Bits(9);
    if (code >= 64) {
        p = &spBMBtab0[(code >> 5) - 2];
    } else if (code >= 16) {
        p = &spBMBtab1[(code >> 2) - 4];
    } else if (code >= 8) {
        p = &spBMBtab2[code - 8];
    } else {
        MPDfault_flag = 1;
        return 0;
    }
    Flush_Buffer(p->len);
    return p->val;
}

static inline int Get_SNR_macroblock_type() {
    int code;
    code = Show_Bits(3);
    if (code == 0) {
        MPDfault_flag = 1;
        return 0;
    }
    Flush_Buffer(SNRMBtab[code].len);
    return SNRMBtab[code].val;
}

int Get_macroblock_type() {
    int macroblock_type = 0;
    if (MPDld->scalable_mode == SC_SNR) {
        macroblock_type = Get_SNR_macroblock_type();
    } else {
        switch (MPDpicture_coding_type) {
        case I_TYPE:
            macroblock_type = MPDld->pict_scal ? Get_I_Spatial_macroblock_type() : Get_I_macroblock_type();
            break;
        case P_TYPE:
            macroblock_type = MPDld->pict_scal ? Get_P_Spatial_macroblock_type() : Get_P_macroblock_type();
            break;
        case B_TYPE:
            macroblock_type = MPDld->pict_scal ? Get_B_Spatial_macroblock_type() : Get_B_macroblock_type();
            break;
        case D_TYPE:
            macroblock_type = Get_D_macroblock_type();
            break;
        }
    }
    return macroblock_type;
}

int Get_motion_code() {
    int code;
    if (Get_Bits1()) {
        return 0;
    }
    if ((code = Show_Bits(9)) >= 64) {
        code >>= 6;
        Flush_Buffer(MVtab0[code].len);
        return Get_Bits1() ? -MVtab0[code].val : MVtab0[code].val;
    }
    if (code >= 24) {
        code >>= 3;
        Flush_Buffer(MVtab1[code].len);
        return Get_Bits1() ? -MVtab1[code].val : MVtab1[code].val;
    }
    if ((code -= 12) < 0) {
        MPDfault_flag = 1;
        return 0;
    }
    Flush_Buffer(MVtab2[code].len);
    return Get_Bits1() ? -MVtab2[code].val : MVtab2[code].val;
}

// The dual-prime differential motion vector.
int Get_dmvector() {
    if (Get_Bits(1)) {
        return Get_Bits(1) ? -1 : 1;
    }
    return 0;
}

int Get_coded_block_pattern() {
    int code;
    if ((code = Show_Bits(9)) >= 128) {
        code >>= 4;
        Flush_Buffer(CBPtab0[code].len);
        return CBPtab0[code].val;
    }
    if (code >= 8) {
        code >>= 1;
        Flush_Buffer(CBPtab1[code].len);
        return CBPtab1[code].val;
    }
    if (code < 1) {
        MPDfault_flag = 1;
        return 0;
    }
    Flush_Buffer(CBPtab2[code].len);
    return CBPtab2[code].val;
}

int Get_macroblock_address_increment() {
    int code, val;
    val = 0;
    while ((code = Show_Bits(11)) < 24) {
        if (code != 15) {  // not macroblock_stuffing
            if (code == 8) {  // macroblock_escape
                val += 33;
            } else {
                MPDfault_flag = 1;
                return 1;
            }
        }
        Flush_Buffer(11);
    }
    if (code >= 1024) {
        Flush_Buffer(1);
        return val + 1;
    }
    if (code >= 128) {
        code >>= 6;
        Flush_Buffer(MBAtab1[code].len);
        return val + MBAtab1[code].val;
    }
    code -= 24;
    Flush_Buffer(MBAtab2[code].len);
    return val + MBAtab2[code].val;
}

int Get_Luma_DC_dct_diff() {
    int code, size, dct_diff;
    code = Show_Bits(5);
    if (code < 31) {
        size = DClumtab0[code].val;
        Flush_Buffer(DClumtab0[code].len);
    } else {
        code = Show_Bits(9) - 0x1f0;
        size = DClumtab1[code].val;
        Flush_Buffer(DClumtab1[code].len);
    }
    if (size == 0) {
        dct_diff = 0;
    } else {
        dct_diff = Get_Bits(size);
        if ((dct_diff & (1 << (size - 1))) == 0) {
            dct_diff -= (1 << size) - 1;
        }
    }
    return dct_diff;
}

int Get_Chroma_DC_dct_diff() {
    int code, size, dct_diff;
    code = Show_Bits(5);
    if (code < 31) {
        size = DCchromtab0[code].val;
        Flush_Buffer(DCchromtab0[code].len);
    } else {
        code = Show_Bits(10) - 0x3e0;
        size = DCchromtab1[code].val;
        Flush_Buffer(DCchromtab1[code].len);
    }
    if (size == 0) {
        dct_diff = 0;
    } else {
        dct_diff = Get_Bits(size);
        if ((dct_diff & (1 << (size - 1))) == 0) {
            dct_diff -= (1 << size) - 1;
        }
    }
    return dct_diff;
}
