#include <ctype.h>
#include <stdlib.h>

#define EOF -1
#define ERANGE 34
#define ULONG_MAX 0xFFFFFFFFUL
#define LONG_MAX 0x7FFFFFFFL
#define LONG_MIN (-LONG_MAX - 1)
#define INT_MAX 0x7FFFFFFF

extern int errno;
extern unsigned char __upper_map[256];

#define isalpha(c) ((int)(__ctype_map[(unsigned char)(c)] & (__lower_case | __upper_case)))
#define toupper(c) ((c) == -1 ? -1 : (int)__upper_map[(unsigned char)(c)])

typedef struct {
    char* NextChar;
    int NullCharDetected;
} __InStrCtrl;

int __StringRead(void* isc, int ch, int Action);

#define __GetAChar 0
#define __UngetAChar 1

enum scan_states {
    start = 0x01,
    check_for_zero = 0x02,
    leading_zero = 0x04,
    need_digit = 0x08,
    digit_loop = 0x10,
    finished = 0x20,
    failure = 0x40
};

#define final_state(scan_state) (scan_state & (finished | failure))
#define success(scan_state) (scan_state & (leading_zero | digit_loop | finished))
#define fetch() (count++, (*ReadProc)(ReadProcArg, 0, __GetAChar))
#define unfetch(c) (*ReadProc)(ReadProcArg, c, __UngetAChar)

// Scanner shared by strtoul/strtol/atoi and scanf: reads digits in `base`
// (0 = by prefix) through ReadProc.
unsigned long __strtoul(int base, int max_width, int (*ReadProc)(void*, int, int), void* ReadProcArg,
                        int* chars_scanned, int* negative, int* overflow) {
    int scan_state = start;
    int count = 0;
    int spaces = 0;
    unsigned long value = 0;
    unsigned long value_max = 0;
    int c;

    *negative = *overflow = 0;

    if (base < 0 || base == 1 || base > 36 || max_width < 1) {
        scan_state = failure;
    } else {
        c = fetch();
    }

    if (base != 0) {
        value_max = ULONG_MAX / base;
    }

    while (count <= max_width && c != EOF && !final_state(scan_state)) {
        switch (scan_state) {
        case start:
            if (isspace(c)) {
                c = fetch();
                count--;
                spaces++;
                break;
            }

            if (c == '+') {
                c = fetch();
            } else if (c == '-') {
                c = fetch();
                *negative = 1;
            }

            scan_state = check_for_zero;
            break;

        case check_for_zero:
            if (base == 0 || base == 16) {
                if (c == '0') {
                    scan_state = leading_zero;
                    c = fetch();
                    break;
                }
            }

            scan_state = need_digit;
            break;

        case leading_zero:
            if (c == 'X' || c == 'x') {
                base = 16;
                scan_state = need_digit;
                c = fetch();
                break;
            }

            if (base == 0) {
                base = 8;
            }

            scan_state = digit_loop;
            break;

        case need_digit:
        case digit_loop:
            if (base == 0) {
                base = 10;
            }

            if (!value_max) {
                value_max = ULONG_MAX / base;
            }

            if (isdigit(c)) {
                if ((c -= '0') >= base) {
                    if (scan_state == digit_loop) {
                        scan_state = finished;
                    } else {
                        scan_state = failure;
                    }

                    c += '0';
                    break;
                }
            } else if (!isalpha(c) || (toupper(c) - 'A' + 10) >= base) {
                if (scan_state == digit_loop) {
                    scan_state = finished;
                } else {
                    scan_state = failure;
                }

                break;
            } else {
                c = toupper(c) - 'A' + 10;
            }

            if (value > value_max) {
                *overflow = 1;
            }

            value *= base;

            if (c > (ULONG_MAX - value)) {
                *overflow = 1;
            }

            value += c;

            scan_state = digit_loop;
            c = fetch();
            break;
        }
    }

    if (!success(scan_state)) {
        count = 0;
        value = 0;
        *chars_scanned = 0;
    } else {
        count--;
        *chars_scanned = count + spaces;
    }

    unfetch(c);
    return value;
}

static inline long strtol(const char* str, char** end, int base) {
    unsigned long uvalue;
    long svalue;
    int count, negative, overflow;
    __InStrCtrl isc;

    isc.NextChar = (char*)str;
    isc.NullCharDetected = 0;

    uvalue = __strtoul(base, INT_MAX, &__StringRead, (void*)&isc, &count, &negative, &overflow);

    if (end) {
        *end = (char*)str + count;
    }

    if (overflow || (!negative && uvalue > LONG_MAX) || (negative && uvalue > -LONG_MIN)) {
        svalue = (negative ? -LONG_MIN : LONG_MAX);
        errno = ERANGE;
    } else {
        svalue = (negative ? (long)-uvalue : (long)uvalue);
    }

    return svalue;
}

static inline long atol(const char* str) {
    return strtol(str, NULL, 10);
}

int atoi(const char* str) {
    return atol(str);
}
