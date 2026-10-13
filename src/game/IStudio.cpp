// IStudio: the game's UI runtime instance.
#include "game/IStudio.h"
#include "game/GameMessage.h"
#include "game/IStudioFuncs.h"
#include "game/Pause.h"
#include "game/TibPolygon.h"
#include "game/TibText.h"
#include "game/UIStudio.h"
#include "game/input.h"
#include "game/matstack.h"
#include "game/memory.h"
#include "ea/real.h"
#include "string.h"

// Input events 0x79.. are digital UI buttons 0..15, 0x89..0x8C analog 16..19.
#define ISTUDIO_FIRST_BUTTON_EVENT 0x79
#define ISTUDIO_NUM_BUTTONS 16
#define ISTUDIO_FIRST_ANALOG_EVENT 0x89

IStudio::IStudio() {
    m_ui = NULL;
    m_param0 = 0;
    m_param1 = 0;
    m_param2 = 0;
    m_param3 = 0;
    m_param4 = 0;
}

IStudio::~IStudio() {}

void IStudio::ISirenStartup(unsigned long p0, unsigned long p1, unsigned long p2,
                            unsigned long p3, unsigned long p4) {
    m_param0 = p0;
    m_param1 = p1;
    m_param2 = p2;
    m_param3 = p3;
    m_param4 = p4;
    m_ui = (UISInfo_t*)DWI_alloc("ISiren memory", UISGetMemSize(p0, p1, p2, p0, p3, p3), 0x400);
    UISInit(m_ui, p0, p1, p2, p0, p3, p3, p4);
    UISRegisterResourceFncs(m_ui, ResLoadScreen, ResUnloadScreen);
    UISRegisterTransformFncs(m_ui, iSirenTransformfnc);
    UISRegisterMessageFnc(m_ui, GameMessage);
    UISRegisterPluginFnc(m_ui, 0, TibPolyProcessFnc);
    UISRegisterPluginFnc(m_ui, 7, TibTextProcessFnc);
    UISRegisterPluginFnc(m_ui, 1, DummyPluginProcessFnc);
    UISRegisterPluginFnc(m_ui, 2, DummyPluginProcessFnc);
    UISRegisterPluginFnc(m_ui, 3, DummyPluginProcessFnc);
    UISRegisterPluginFnc(m_ui, 4, DummyPluginProcessFnc);
    UISRegisterPluginFnc(m_ui, 5, DummyPluginProcessFnc);
    UISRegisterPluginFnc(m_ui, 6, DummyPluginProcessFnc);
}

void IStudio::iSirenLoadScreen(unsigned short group, unsigned short screen) {
    UISLoadScreen(m_ui, group, screen, 0, NULL);
}

void IStudio::iSirenActivateScreen(unsigned short group, unsigned short screen) {
    UISSetScreenActive(m_ui, group, screen);
}

void IStudio::iSirenDrawProcessFnc(unsigned long arg) {
    g_matStack->Ident();
    UISDrawObjects(m_ui, arg);
}

void IStudio::iSirenUpdate(unsigned long frames) {
    iSirenEventProcessFnc();
    UISIdleProcess(m_ui, frames);
}

void IStudio::iSirenShutDown() {
    UISShutdown(m_ui);
    memset(m_ui, 0, UISGetMemSize(m_param0, m_param1, m_param2, m_param0, m_param3, m_param3));
    MEM_free(m_ui);
    m_param0 = 0;
    m_param1 = 0;
    m_param2 = 0;
    m_param3 = 0;
    m_param4 = 0;
}

void IStudio::iSirenEventProcessFnc() {
#define BUTTON(i)                                                          \
    if (g_inputMgr.GetValue(port, ISTUDIO_FIRST_BUTTON_EVENT + (i)) != 0) { \
        value = 1.0f;                                                      \
        UISProcessEvent(m_ui, port + 1, (i), 1, &value, 0);                \
    }
#define ANALOG(i)                                                                  \
    if ((value = g_inputMgr.GetValue(port, ISTUDIO_FIRST_ANALOG_EVENT + (i))) != 0.0f) { \
        UISProcessEvent(m_ui, port + 1, ISTUDIO_NUM_BUTTONS + (i), 1, &value, 0);  \
    }
    float value;
    for (int port = 0; port < 4; port++) {
        if (g_PauseScreenLockedController == -1 || port == g_PauseScreenLockedController) {
            BUTTON(0) BUTTON(1) BUTTON(2) BUTTON(3) BUTTON(4) BUTTON(5) BUTTON(6) BUTTON(7)
            BUTTON(8) BUTTON(9) BUTTON(10) BUTTON(11) BUTTON(12) BUTTON(13) BUTTON(14) BUTTON(15)
            ANALOG(0) ANALOG(1) ANALOG(2) ANALOG(3)
        }
    }
#undef BUTTON
#undef ANALOG
}
