// Binary double -> decimal digit string conversion used by printf.
#include <string.h>

#define SIGDIGLEN 36
#define DBL_MANT_DIG 53
#define CHAR_BIT 8

#define FP_NAN 1
#define FP_INFINITE 2
#define FP_ZERO 3
#define FP_NORMAL 4
#define FP_SUBNORMAL 5

double frexp(double x, int* exp);
double ldexp(double x, int exp);
double modf(double x, double* iptr);

typedef struct decimal {
    /* 0x0 */ char sign;
    /* 0x2 */ short exp;
    struct {
        /* 0x4 */ unsigned char length;
        /* 0x5 */ unsigned char text[SIGDIGLEN];
        /* 0x29 */ unsigned char unused;
    } sig;
} decimal;

typedef struct decform {
    /* 0x0 */ char style;
    /* 0x2 */ short digits;
} decform;

#define __HI(x) (*(unsigned long*)&x)
#define __LO(x) (*(1 + (unsigned long*)&x))

static inline int __fpclassifyd(double x) {
    switch (__HI(x) & 0x7FF00000) {
    case 0x7FF00000:
        if ((__HI(x) & 0x000FFFFF) || __LO(x)) {
            return FP_NAN;
        } else {
            return FP_INFINITE;
        }
        break;
    case 0:
        if ((__HI(x) & 0x000FFFFF) || __LO(x)) {
            return FP_SUBNORMAL;
        } else {
            return FP_ZERO;
        }
        break;
    }
    return FP_NORMAL;
}

#define fpclassify(x) __fpclassifyd(x)
#define isfinite(x) (fpclassify(x) > FP_INFINITE)
#define signbit(x) (__HI(x) >> 31)

#pragma auto_inline off
static int __count_trailing_zerol(unsigned long x) {
    int result = 0;
    int bits_not_checked = sizeof(unsigned long) * CHAR_BIT;
    int n = sizeof(unsigned long) * CHAR_BIT / 2;
    unsigned long mask = (unsigned long)(-1) >> n;
    int sh = n;

    while (bits_not_checked) {
        if ((x & mask) == 0) {
            x >>= sh;
            result += sh;
            bits_not_checked -= sh;
        } else if (mask == 1) {
            break;
        }

        if (n > 1) {
            n /= 2;
        }

        if (mask > 1) {
            mask >>= n;
            sh -= n;
        }
    }

    return result;
}
#pragma auto_inline reset

static inline int __count_trailing_zero(double x) {
    unsigned long* l = (unsigned long*)&x;

    if (l[1] != 0) {
        return __count_trailing_zerol(l[1]);
    }

    return (int)(sizeof(unsigned long) * CHAR_BIT + __count_trailing_zerol(l[0] | 0x00100000));
}

static inline int __must_round(const decimal* d, int digits) {
    const unsigned char* i = d->sig.text + digits;

    if (*i > 5) {
        return 1;
    }

    if (*i < 5) {
        return -1;
    }

    {
        const unsigned char* e = d->sig.text + d->sig.length;
        for (i++; i < e; i++) {
            if (*i != 0) {
                return 1;
            }
        }
    }

    if (d->sig.text[digits - 1] & 1) {
        return 1;
    }

    return -1;
}

static inline void __dorounddecup(decimal* d, int digits) {
    unsigned char* b = d->sig.text;
    unsigned char* i = b + digits - 1;

    while (1) {
        if (*i < 9) {
            (*i)++;
            break;
        }
        if (i == b) {
            *i = 1;
            d->exp++;
            break;
        }
        *i-- = 0;
    }
}

static inline void __rounddec(decimal* d, int digits) {
    if (digits > 0 && digits < d->sig.length) {
        int unkBool = __must_round(d, digits);
        d->sig.length = digits;

        if (unkBool >= 0) {
            __dorounddecup(d, digits);
        }
    }
}

#pragma auto_inline off
static void __ull2dec(decimal* result, unsigned long long val) {
    result->sign = 0;

    if (val == 0) {
        result->exp = 0;
        result->sig.length = 1;
        result->sig.text[0] = 0;
        return;
    }

    result->sig.length = 0;

    for (; val != 0; val /= 10) {
        result->sig.text[result->sig.length++] = val % 10;
    }

    {
        unsigned char* i = result->sig.text;
        unsigned char* j = result->sig.text + result->sig.length;

        for (; i < --j; ++i) {
            unsigned char t = *i;
            *i = *j;
            *j = t;
        }
    }

    result->exp = result->sig.length - 1;
}
#pragma auto_inline reset

static void __timesdec(decimal* result, const decimal* x, const decimal* y) {
    unsigned long accumulator = 0;
    unsigned char mantissa[SIGDIGLEN * 2];
    int i = x->sig.length + y->sig.length - 1;
    unsigned char* ip = mantissa + i + 1;
    unsigned char* ep = ip;

    result->sign = 0;

    for (; i > 0; i--) {
        int k = y->sig.length - 1;
        int j = i - k - 1;
        int l;
        int t;
        const unsigned char* jp;
        const unsigned char* kp;

        if (j < 0) {
            j = 0;
            k = i - 1;
        }

        jp = x->sig.text + j;
        kp = y->sig.text + k;
        l = k + 1;
        t = x->sig.length - j;

        if (l > t) {
            l = t;
        }

        for (; l > 0; l--, jp++, kp--) {
            accumulator += *jp * *kp;
        }

        *--ip = accumulator % 10;
        accumulator /= 10;
    }

    result->exp = (short)(x->exp + y->exp);

    if (accumulator) {
        *--ip = accumulator;
        result->exp++;
    }

    for (i = 0; i < SIGDIGLEN && ip < ep; i++, ip++) {
        result->sig.text[i] = *ip;
    }
    result->sig.length = i;

    if (ip < ep && *ip >= 5) {
        if (*ip == 5) {
            unsigned char* jp = ip + 1;
            for (; jp < ep; jp++) {
                if (*jp != 0) {
                    goto round;
                }
            }
            if ((*(ip - 1) & 1) == 0) {
                return;
            }
        }
    round:
        __dorounddecup(result, result->sig.length);
    }
}

#pragma auto_inline off
static void __str2dec(decimal* d, const char* s, short exp) {
    int i;

    d->exp = exp;
    d->sign = 0;

    for (i = 0; i < SIGDIGLEN && *s;) {
        d->sig.text[i++] = *s++ - '0';
    }
    d->sig.length = i;

    // (The original compares the next character itself, not its digit value.)
    if (*s != 0 && *s >= 5) {
        const char* p = s + 1;

        for (; *p != 0; p++) {
            if (*p != '0') {
                goto round;
            }
        }

        if ((d->sig.text[i - 1] & 1) == 0) {
            return;
        }

    round:
        __dorounddecup(d, d->sig.length);
    }
}
#pragma auto_inline reset

// result = 2^exp
static void __two_exp(decimal* result, short exp) {
    switch (exp) {
    case -64:
        __str2dec(result, "542101086242752217003726400434970855712890625", -20);
        return;
    case -53:
        __str2dec(result, "11102230246251565404236316680908203125", -16);
        return;
    case -32:
        __str2dec(result, "23283064365386962890625", -10);
        return;
    case -16:
        __str2dec(result, "152587890625", -5);
        return;
    case -8:
        __str2dec(result, "390625", -3);
        return;
    case -7:
        __str2dec(result, "78125", -3);
        return;
    case -6:
        __str2dec(result, "15625", -2);
        return;
    case -5:
        __str2dec(result, "3125", -2);
        return;
    case -4:
        __str2dec(result, "625", -2);
        return;
    case -3:
        __str2dec(result, "125", -1);
        return;
    case -2:
        __str2dec(result, "25", -1);
        return;
    case -1:
        __str2dec(result, "5", -1);
        return;
    case 0:
        __str2dec(result, "1", 0);
        return;
    case 1:
        __str2dec(result, "2", 0);
        return;
    case 2:
        __str2dec(result, "4", 0);
        return;
    case 3:
        __str2dec(result, "8", 0);
        return;
    case 4:
        __str2dec(result, "16", 1);
        return;
    case 5:
        __str2dec(result, "32", 1);
        return;
    case 6:
        __str2dec(result, "64", 1);
        return;
    case 7:
        __str2dec(result, "128", 2);
        return;
    case 8:
        __str2dec(result, "256", 2);
        return;
    }

    {
        decimal x2, temp;

        __two_exp(&x2, exp / 2);
        __timesdec(result, &x2, &x2);

        if (exp & 1) {
            temp = *result;
            if (exp > 0) {
                decimal n2;
                __str2dec(&n2, "2", 0);
                __timesdec(result, &temp, &n2);
            } else {
                decimal n5;
                __str2dec(&n5, "5", -1);
                __timesdec(result, &temp, &n5);
            }
        }
    }
}

#pragma auto_inline off
static void __num2dec_internal(decimal* d, double x) {
    char sign = (char)(signbit(x) != 0);

    if (x == 0.0) {
        d->sign = sign;
        d->exp = 0;
        d->sig.length = 1;
        d->sig.text[0] = 0;
        return;
    }

    if (!isfinite(x)) {
        d->sign = sign;
        d->exp = 0;
        d->sig.length = 1;
        d->sig.text[0] = (fpclassify(x) == FP_NAN) ? 'N' : 'I';
        return;
    }

    if (sign != 0) {
        x = -x;
    }

    {
        int exp;
        double frac = frexp(x, &exp);
        short num_bits_extract = DBL_MANT_DIG - __count_trailing_zero(frac);
        double integer;
        decimal int_d, pow2_d;

        __two_exp(&pow2_d, exp - num_bits_extract);
        modf(ldexp(frac, num_bits_extract), &integer);
        __ull2dec(&int_d, (unsigned long long)integer);
        __timesdec(d, &int_d, &pow2_d);
        d->sign = sign;
    }
}
#pragma auto_inline reset

void __num2dec(const decform* form, double x, decimal* d) {
    short digits = form->digits;
    int i;

    __num2dec_internal(d, x);

    if (d->sig.text[0] > 9) {
        return;
    }

    if (digits > SIGDIGLEN) {
        digits = SIGDIGLEN;
    }

    __rounddec(d, digits);

    while (d->sig.length < digits) {
        d->sig.text[d->sig.length++] = 0;
    }

    d->exp -= d->sig.length - 1;

    for (i = 0; i < d->sig.length; i++) {
        d->sig.text[i] += '0';
    }
}
