#include "ea/real.h"
#include "string.h"
#include "ctype.h"

// BIG archive headers: "BIGF" and "BIG<n>" (n = bytes per offset field)
// have a 16-byte header, then entries of big-endian offset and size followed
// by the name; 0xC0FB has a 6-byte header and 3-byte fields. A header may
// end with an 8-byte debug tag ("L234" then a word).
// The fields are big-endian: native loads on the console.
#ifdef FRONTLINE_NATIVE
#define BIG_U16(p) __builtin_bswap16(*(const u16*)(p))
#define BIG_U32(p) __builtin_bswap32(*(const u32*)(p))
#else
#define BIG_U16(p) (*(const u16*)(p))
#define BIG_U32(p) (*(const u32*)(p))
#endif

enum {
    BIGTYPE_NONE = 0,
    BIGTYPE_C0FB = 1,
    BIGTYPE_BIGF = 2,
    BIGTYPE_BIGN = 3,
};

static inline int bigtype(const void* header) {
    int type = BIGTYPE_NONE;
    if (BIG_U16(header) == 0xC0FB) {
        type = BIGTYPE_C0FB;
    } else if (BIG_U32(header) == 0x42494746) {  // "BIGF"
        type = BIGTYPE_BIGF;
    } else if ((BIG_U32(header) & ~0xFF) == 0x42494700) {  // "BIG?"
        type = BIGTYPE_BIGN;
    }
    return type;
}

static inline int bigheadersize(const void* header) {
    int size = 0;
    switch (bigtype(header)) {
    case BIGTYPE_C0FB:
        size = BIG_U16((const u16*)header + 1) + 4;
        break;
    case BIGTYPE_BIGF:
    case BIGTYPE_BIGN:
        size = BIG_U32((const u32*)header + 3);
        break;
    }
    return size;
}

// Reads a big-endian field of n bytes (the last 4 when longer).
static inline u32 bigfield(const u8* p, int n) {
    if (n <= 4) {
        if (n == 1) {
            return *p;
        } else if (n == 2) {
            return BIG_U16(p);
        } else if (n == 3) {
            return BIG_U32(p) >> 8;
        } else if (n == 4) {
            return BIG_U32(p);
        }
        return 0;
    }
    return BIG_U32(p + n - 4);
}

extern "C" int BIG_typeofheader(const void* header) {
    return bigtype(header);
}

extern "C" int BIG_sizeofheader(const void* header) {
    return bigheadersize(header);
}

// The debug tag at the end of the header: returns its size (8) or 0.
static int BIG_debuginfo(const void* header, int* version, unsigned int* info) {
    int result = 0;
    const char* p = (const char*)header + bigheadersize(header) - 8;
    if (((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) && p[1] >= '0' && p[1] <= '9' &&
        p[2] >= '0' && p[2] <= '9' && p[3] >= '0' && p[3] <= '9') {
        result = 8;
        if (version != NULL) {
            *version = (p[1] - '0') * 100 + (p[2] - '0') * 10 + (p[3] - '0');
        }
        if (info != NULL) {
            *info = BIG_U32(p + 4);
        }
    }
    return result;
}

extern "C" int BIG_locateentryz(const void* big, const char* name, int index, int* offset, int* size) {
    const u8* header = (const u8*)big;
    int debug = BIG_debuginfo(big, NULL, NULL);
    const u8* p = header + 0x10;
    int offsize = 4;
    int sizesize = 4;
    const u8* end = header + bigheadersize(big) - debug;
    const char* entry = NULL;

    switch (bigtype(big)) {
    case BIGTYPE_C0FB:
        p = header + 6;
        offsize = 3;
        sizesize = 3;
        break;
    case BIGTYPE_BIGN:
        offsize = header[3] - '0';
        sizesize = offsize;
        if (offsize > 4) {
            sizesize = 4;
        }
        break;
    }

    if (name != NULL) {
        while (p < end) {
            const char* a = name;
            const char* b;
            int d;
            entry = (const char*)p + offsize + sizesize;
            b = entry;
            d = tolower(*b) - tolower(*a);
            while (d == 0 && *b != 0) {
                a++;
                b++;
                d = tolower(*b) - tolower(*a);
            }
            if (d == 0) {
                if (offset != NULL) {
                    *offset = bigfield(p, offsize);
                }
                if (size != NULL) {
                    *size = bigfield(p + offsize, sizesize);
                }
                return (int)PTR_TO_ADDR(entry);
            }
            p = (const u8*)entry + strlen(entry) + 1;
        }
    } else {
        int i = 0;
        while (p < end) {
            entry = (const char*)p + offsize + sizesize;
            if (i == index) {
                if (offset != NULL) {
                    *offset = bigfield(p, offsize);
                }
                if (size != NULL) {
                    *size = bigfield(p + offsize, sizesize);
                }
                return (int)PTR_TO_ADDR(entry);
            }
            p = (const u8*)entry + strlen(entry) + 1;
            i++;
        }
    }

    if (offset != NULL) {
        *offset = 0;
    }
    if (size != NULL) {
        *size = 0;
    }
    return 0;
}

extern "C" void* BIG_locate(const void* big, const char* name) {
    int offset = 0;
    void* result = NULL;
    if (name != NULL) {
        BIG_locateentryz(big, name, 0, &offset, NULL);
    }
    if (offset != 0) {
        result = (u8*)big + offset;
    }
    return result;
}

extern "C" int BIG_count(const void* big) {
    int count = 0;
    switch (bigtype(big)) {
    case BIGTYPE_C0FB:
        count = BIG_U16((const u16*)big + 2);
        break;
    case BIGTYPE_BIGF:
    case BIGTYPE_BIGN:
        count = BIG_U32((const u32*)big + 2);
        break;
    }
    return count;
}
