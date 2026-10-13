#include "ea/snd/smix.h"

// The mixer in use (SNDSYS_linkmaincpumixer).
void (*MIXinitfn)(MIXOPTS* opts);
void (*MIXrestorefn)();
void (*MIXaudioslicefn)(f32** dest, int frames);
void (*MIXplayinitfn)(int voice, int format, int mode, int a, int b, u8* stretch, int c, int d, int e, int f, int g,
                      int h, int i);
void (*MIXplayfn)(int voice);
void (*MIXstopfn)(int voice);
void (*MIXsetpitchfn)(int voice, int pitch);
