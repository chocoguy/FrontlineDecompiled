// Variadic arguments for MWCC on PowerPC (EABI va_list layout).
#ifndef _STDARG_H_
#define _STDARG_H_

typedef struct __va_list_struct {
    char gpr;
    char fpr;
    char reserved[2];
    char* input_arg_area;
    char* reg_save_area;
} __va_list[1];
typedef __va_list va_list;

#ifdef __cplusplus
extern "C" {
#endif
void* __va_arg(void* ap, int type);
#ifdef __cplusplus
}
#endif

#define va_start(ap, fmt) ((void)fmt, __builtin_va_info(&ap))
#define va_arg(ap, t) (*((t*)__va_arg(ap, _var_arg_typeof(t))))
#define va_end(ap) ((void)0)

#endif
