// IStudio: owns one instance of the UI runtime ("iSiren") and feeds it
// controller input. Hand-maintained.
#ifndef _GAME_ISTUDIO_H_
#define _GAME_ISTUDIO_H_

#include "types.h"

struct UISInfo_t;

// size 0x18
class IStudio {
public:
    /* 0x00 */ UISInfo_t* m_ui;
    // UISInit / UISGetMemSize parameters.
    /* 0x04 */ u32 m_param0;
    /* 0x08 */ u32 m_param1;
    /* 0x0C */ u32 m_param2;
    /* 0x10 */ u32 m_param3;
    /* 0x14 */ u32 m_param4;

    void iSirenEventProcessFnc();  // 0x8008B01C
    void iSirenShutDown();  // 0x8008B5E0
    void iSirenUpdate(unsigned long);  // 0x8008B660
    void iSirenDrawProcessFnc(unsigned long);  // 0x8008B6A4
    void iSirenActivateScreen(unsigned short group, unsigned short screen);  // 0x8008B6EC
    void iSirenLoadScreen(unsigned short group, unsigned short screen);  // 0x8008B710
    void ISirenStartup(unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);  // 0x8008B73C
    ~IStudio();  // 0x8008B8B8
    IStudio();  // 0x8008B8F4
};

#endif
