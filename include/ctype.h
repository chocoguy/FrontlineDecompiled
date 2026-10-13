// Minimal MSL C ctype.h, added as decompiled code needs it.
#ifndef _MSL_CTYPE_H_
#define _MSL_CTYPE_H_

#ifdef __cplusplus
extern "C" {
#endif

// MSL character class bits
#define __control_char 0x01
#define __motion_char 0x02
#define __space_char 0x04
#define __punctuation 0x08
#define __digit 0x10
#define __hex_digit 0x20
#define __lower_case 0x40
#define __upper_case 0x80

extern unsigned char __ctype_map[256];
extern unsigned char __lower_map[256];

#define isspace(c) ((int)(__ctype_map[(unsigned char)(c)] & (__motion_char | __space_char)))
#define isdigit(c) ((int)(__ctype_map[(unsigned char)(c)] & __digit))
#define tolower(c) ((c) == -1 ? -1 : (int)__lower_map[(unsigned char)(c)])

#ifdef __cplusplus
}
#endif

#endif
