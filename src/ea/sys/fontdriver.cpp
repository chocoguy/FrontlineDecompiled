#include "ea/sys/font.h"

FONTDRIVEREXT FONTcurrentdriverext;
FONTDRIVER* FONTcurrentdriver = &FONTnulldriver;

extern "C" void FONT_installdriver(FONTDRIVER* driver) {
    FONTcurrentdriver = driver;
}
