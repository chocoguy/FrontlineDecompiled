// CDualConfig: the controller configuration screen (custom button layouts)
// and building the input keymap from a layout. Hand-maintained.
#ifndef _GAME_CONFIGGC_H_
#define _GAME_CONFIGGC_H_

#include "types.h"
#include "game_types.h"

class CFont;
class CSprite;
class CUserMessageTable;

#define CONFIG_NUM_ACTIONS 13  // rows of a layout
#define CONFIG_NUM_CHOICES 15  // bindings a row can pick from
#define CONFIG_NONE 0x7B       // configArray: no button for this binding

// Layouts 1..5 are the presets; 6 and up are the shell's custom layouts.
// size 0x44
class CDualConfig {
public:
    CDualConfig() {
        field_0x10 = 1;
        m_selected = 0;
        field_0x18 = 0;
        m_applied = false;
        m_bindingFont = NULL;
        m_active = false;
        m_cancelled = false;
        m_hotbox = NULL;
        m_horizAccum = 0.0f;
        m_vertAccum = 0.0f;
        m_player = 0;
        m_config = 1;
    }

    /* 0x00 */ CSprite* m_hotbox;      // the controller's hot spots
    /* 0x04 */ CFont* m_bindingFont;   // binding names at the hot spots
    /* 0x08 */ CFont* m_menuFont;      // the selected action's name
    /* 0x0C */ bool m_applied;         // Y/B pressed: apply (and leave)
    /* 0x0D */ bool m_active;          // the screen is up
    /* 0x0E */ bool m_cancelled;
    /* 0x10 */ int field_0x10;
    /* 0x14 */ int m_selected;         // action row
    /* 0x18 */ int field_0x18;
    /* 0x1C */ s8 m_rows[CONFIG_NUM_ACTIONS];  // rows from the one above the selection
    /* 0x2C */ f32 m_horizAccum;       // stick travel toward the next choice
    /* 0x30 */ f32 m_vertAccum;        // stick travel toward the next row
    /* 0x34 */ int m_savedConfig;      // the layout on entry (restored on cancel)
    /* 0x38 */ int m_player;           // whose keymap the layout goes to
    /* 0x3C */ CUserMessageTable* m_messages;  // the shell's (for the action names)
    /* 0x40 */ int m_config;           // layout being edited

    void SetupButtonsForMultiplayerGame();  // 0x800F77AC
    void CopyDefaultConfig(int, char*);  // 0x800F7B58
    void GetDefaultCustomControllerValues(char*);  // 0x800F7DA4
    void InitializeButtonMovementArray();  // 0x800F7E00
    void RunNewConfigRendererer();  // 0x800F8148
    void DisplayControlsForNewConfigRendererer();  // 0x800F83B0
    void OnEnter();  // 0x800F93AC
    void SetUpDefaultConfig(int);  // 0x800F9540
    bool GetAppliedState();  // 0x800F977C
    void SetAppliedState(bool);  // 0x800F9784
    void SetupButtonsForGame();  // 0x800F978C
    void ApplyChanges();  // 0x800F9BBC
    s8 WillBeUnassigned();  // 0x800F9F30
    void UnassignPreviousAssignedButton();  // 0x800FA0B4
    void HandleButtonPress();  // 0x800FA30C
    void HandleAnalogButtonPress();  // 0x800FA98C
    void Init();  // 0x800FAD00
    ~CDualConfig();  // 0x800FAFC0
};
SIZE_ASSERT(CDualConfig, 0x44);

extern CDualConfig g_ControllerConfig;  // 0x803142BC
#endif
