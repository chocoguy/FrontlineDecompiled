#include "ea/sys/font.h"

static void NULL_draw(FONT*, const FONTCHAR*, float, float) {}

FONTDRIVER FONTnulldriver = {NULL_draw, NULL, NULL, NULL, NULL};
