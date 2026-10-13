// The controller configuration screen. A layout picks, for each of 13
// actions, one of up to 15 bindings (configArray maps it to a button or
// axis). The screen draws the controller's hot spots with each action's
// name at its binding; up/down picks an action, left/right its binding,
// A/Start assigns it (unassigning whatever had that button), Y applies and
// B cancels. Building a layout writes the player's input keymap.
#include "game/ConfigGC.h"
#include "game/ShellMenu.h"
#include "game/pumhandler.h"
#include "game/input.h"
#include "game/font.h"
#include "game/sprite.h"
#include "game/Screen.h"
#include "game/toilet.h"
#include "game/color.h"
#include "game/Moh2.h"
#include "ea/sys/font.h"
#include "string.h"

// Binding names by row type (buttonType).
const char* axisText[CONFIG_NUM_CHOICES] = {
    "Left X Axis", "Left Y Axis", "Right X Axis", "Right Y Axis", "L / R Axis", "Unassigned", "NO_TEXT", "NO_TEXT",
    "NO_TEXT",     "NO_TEXT",     "NO_TEXT",      "NO_TEXT",      "NO_TEXT",    "NO_TEXT",    "NO_TEXT",
};
const char* digitalText[CONFIG_NUM_CHOICES] = {
    "Y", "X", "A", "B", "L", "R", "Z", "Unassigned", "DPad Up", "DPad Down", "NO_TEXT", "NO_TEXT", "NO_TEXT", "NO_TEXT", "NO_TEXT",
};
const char* analogText[CONFIG_NUM_CHOICES] = {
    "L",       "R",       "Unassigned", "NO_TEXT", "NO_TEXT", "NO_TEXT", "NO_TEXT", "NO_TEXT",
    "NO_TEXT", "NO_TEXT", "NO_TEXT",    "NO_TEXT", "NO_TEXT", "NO_TEXT", "NO_TEXT",
};
s8 defaultConfigText[14] = {1, 3, 2, 0, 0, 1, 4, 5, 6, 9, 8, 3, 2, 0};
s8 currConfigText[14] = {1, 3, 2, 0, 0, 1, 4, 5, 6, 9, 8, 3, 2, 0};    // the assigned layout
s8 visualConfigText[14] = {1, 3, 2, 0, 0, 1, 4, 5, 6, 9, 8, 3, 2, 0};  // as shown (being picked)
// Row types: 2 axis (axisText; 5 unassigned), 0 button (digitalText; 7
// unassigned), 1 analog button (analogText).
s8 buttonType[14] = {2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
// Binding -> button or axis code by row; -1 can't be picked.
s8 configArray[14][CONFIG_NUM_CHOICES] = {
    {0x00, 0x01, 0x02, 0x03, 0x09, 0x7B, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0x00, 0x01, 0x02, 0x03, 0x09, 0x7B, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0x00, 0x01, 0x02, 0x03, 0x09, 0x7B, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0x00, 0x01, 0x02, 0x03, 0x09, 0x7B, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {0x0B, 0x0A, 0x08, 0x09, 0x06, 0x05, 0x04, 0x7B, 0x03, 0x02, -1, -1, -1, -1, -1},
    {0x0B, 0x0A, 0x08, 0x09, 0x06, 0x05, 0x04, 0x7B, 0x03, 0x02, -1, -1, -1, -1, -1},
    {0x0B, 0x0A, 0x08, 0x09, 0x06, 0x05, 0x04, 0x7B, -1, -1, -1, -1, -1, -1, -1},
    {0x0B, 0x0A, 0x08, 0x09, 0x06, 0x05, 0x04, 0x7B, 0x03, 0x02, -1, -1, -1, -1, -1},
    {0x0B, 0x0A, 0x08, 0x09, 0x06, 0x05, 0x04, 0x7B, 0x03, 0x02, -1, -1, -1, -1, -1},
    {0x0B, 0x0A, 0x08, 0x09, 0x06, 0x05, 0x04, 0x7B, 0x03, 0x02, -1, -1, -1, -1, -1},
    {0x0B, 0x0A, 0x08, 0x09, 0x06, 0x05, 0x04, 0x7B, 0x03, 0x02, -1, -1, -1, -1, -1},
    {0x0B, 0x0A, 0x08, 0x09, 0x06, 0x05, 0x04, 0x7B, 0x03, 0x02, -1, -1, -1, -1, -1},
    {0x0B, 0x0A, 0x08, 0x09, 0x06, 0x05, 0x04, 0x7B, 0x03, 0x02, -1, -1, -1, -1, -1},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
};
// The controller picture's hot spots.
s16 hotSpotXLoc[18] = {150, 206, 150, 94, 150, 150, 500, 555, 500, 444, 538, 538, 538, 505, 208, 430, 538, 320};
s16 hotSpotYLoc[18] = {160, 190, 220, 190, 280, 305, 310, 340, 370, 340, 180, 213, 246, 280, 100, 100, 148, 302};
// Binding -> hot spot (CONFIG_NONE: none): an axis's two ends, or a button.
s8 AxisPlusHotBoxLoc[CONFIG_NUM_CHOICES] = {1, 0, 7, 6, 14, 17, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B};
s8 AxisMinusHotBoxLoc[CONFIG_NUM_CHOICES] = {3, 2, 9, 8, 15, 17, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B};
s8 ButtonHotBoxLoc[CONFIG_NUM_CHOICES] = {10, 11, 12, 13, 14, 15, 16, 17, 4, 5, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B};

CDualConfig g_ControllerConfig;
// Action names. The first loop writes 15 entries (one past the table, into
// MovementText, which is filled afterwards); sized here to hold them.
char movementText[15][0x18];
char MovementText[15][0x18];  // the names drawn at the bindings

// The input event each action row binds.
static const u16 s_actionEvents[CONFIG_NUM_ACTIONS] = {7, 0xA, 9, 8, 0xB, 0xC, 0x10, 0xE, 0xF, 0x13, 0x14, 0x15, 0xD};

// The first non-zero value of input event `id` over the four ports.
static float AnyPortValue(unsigned long id) {
    float value;
    for (u32 port = 0;;) {
        value = (float)g_inputMgr.GetValue(port, id);
        if (value != 0.0f) {
            break;
        }
        port++;
        if ((int)port >= 4) {
            break;
        }
    }
    return value;
}

// Writes `layout`'s bindings into `player`'s keymap after the fixed first
// 0x13 entries, then the fixed tail. `visual` gets the layout. When
// `entries` is given the bindings go there instead (ApplyChanges).
static int BuildKeymap(const s8* layout, int player, KEYMAPENTRY* entries) {
    u8 buttons[CONFIG_NUM_ACTIONS];
    for (int i = 0; i < CONFIG_NUM_ACTIONS; i++) {
        buttons[i] = CONFIG_NONE;
    }
    for (int i = 0; i < CONFIG_NUM_ACTIONS; i++) {
        visualConfigText[i] = layout[i];
        buttons[i] = configArray[i][layout[i]];
    }
    KEYMAP* keymap = (KEYMAP*)((u8*)g_config_keymap + player * 0x104);
    if (entries == NULL) {
        entries = keymap->entries;
    }
    int n = 0x13;
    for (int i = 0; i < CONFIG_NUM_ACTIONS; i++) {
        if (buttons[i] != CONFIG_NONE) {
            entries[n].event = s_actionEvents[i];
            entries[n].source = buttons[i];
            n++;
        }
    }
    keymap->entries[n].event = 0x16;
    keymap->entries[n].source = 0;
    keymap->entries[n + 1].event = 0x17;
    keymap->entries[n + 1].source = 1;
    keymap->entries[n + 2].event = 0x11;
    keymap->entries[n + 2].source = buttons[1];
    keymap->entries[n + 3].event = 0x12;
    keymap->entries[n + 3].source = buttons[2];
    keymap->entries[n + 4].event = 0xFFFF;
    keymap->entries[n + 4].source = 0xFFFF;
    return n;
}

// Layout `config`: a preset, or one of the shell's custom layouts.
static void GetLayout(CDualConfig* self, int config, char* layout) {
    if (config > 5) {
        g_Shell.RetrieveShellControllerButtons(layout, config - 6);
    } else {
        self->CopyDefaultConfig(config, layout);
    }
}

void CDualConfig::SetupButtonsForMultiplayerGame() {
    char layout[16];
    GetLayout(this, m_config, layout);
    BuildKeymap((s8*)layout, m_player, NULL);
}

// Sets currConfigText to preset `config` (other values keep it) and copies it.
void CDualConfig::CopyDefaultConfig(int config, char* layout) {
    s8* c = currConfigText;
    switch (config) {
    case 1:
        c[0] = 1; c[1] = 3; c[2] = 2; c[3] = 0; c[4] = 0; c[5] = 1; c[6] = 4;
        c[7] = 5; c[8] = 6; c[9] = 9; c[10] = 8; c[11] = 3; c[12] = 2;
        break;
    case 2:
        c[0] = 1; c[1] = 3; c[2] = 0; c[3] = 4; c[4] = 0; c[5] = 9; c[6] = 6;
        c[7] = 2; c[8] = 3; c[9] = 8; c[10] = 7; c[11] = 1; c[12] = 3;
        break;
    case 3:
        c[0] = 1; c[1] = 3; c[2] = 2; c[3] = 0; c[4] = 0; c[5] = 1; c[6] = 4;
        c[7] = 5; c[8] = 6; c[9] = 9; c[10] = 8; c[11] = 2; c[12] = 3;
        break;
    case 4:
        c[0] = 1; c[1] = 3; c[2] = 2; c[3] = 0; c[4] = 0; c[5] = 1; c[6] = 4;
        c[7] = 5; c[8] = 2; c[9] = 9; c[10] = 8; c[11] = 6; c[12] = 3;
        break;
    case 5:
        c[0] = 1; c[1] = 3; c[2] = 0; c[3] = 2; c[4] = 0; c[5] = 1; c[6] = 4;
        c[7] = 5; c[8] = 6; c[9] = 9; c[10] = 8; c[11] = 3; c[12] = 2;
        break;
    }
    memcpy(layout, currConfigText, sizeof(currConfigText));
}

void CDualConfig::GetDefaultCustomControllerValues(char* layout) {
    layout[0] = 1;
    layout[1] = 3;
    layout[2] = 2;
    layout[3] = 0;
    layout[4] = 0;
    layout[5] = 1;
    layout[6] = 4;
    layout[7] = 5;
    layout[8] = 6;
    layout[9] = 9;
    layout[10] = 8;
    layout[11] = 3;
    layout[12] = 2;
}

// Copies the action names out of the shell's message table.
void CDualConfig::InitializeButtonMovementArray() {
    char unused[0x50];
    CUserMessageTable* messages;
    for (int i = 0; i <= 14; i++) {
        memset(unused, 0, sizeof(unused));
        messages = m_messages;
        strcpy(movementText[i], i + 0x2E5 < messages->m_count ? messages->m_entries[i + 0x2E5].m_string : NULL);
    }
    static const int ids[14] = {0x2E5, 0x2E6, 0x2E7, 0x2E8, 0x2E9, 0x2EA, 0x2EB,
                                0x2EC, 0x2ED, 0x2F7, 0x2F8, 0x2F0, 0x2F1, 0x2FA};
    for (int i = 0; i < 14; i++) {
        messages = m_messages;
        strcpy(MovementText[i], ids[i] < messages->m_count ? messages->m_entries[ids[i]].m_string : NULL);
    }
    m_messages = NULL;
}

void CDualConfig::RunNewConfigRendererer() {
    _spritepolyvert verts[4] = {
        {0.0f, 0.0f, 308.0f, 212.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
        {1.0f, 0.0f, 332.0f, 212.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
        {1.0f, 1.0f, 332.0f, 236.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
        {0.0f, 1.0f, 308.0f, 236.0f, 0.0f, {0x80, 0x80, 0x80, 0x80}},
    };
    for (int i = 0; i < 17; i++) {
        CSprite* sprite = m_hotbox;
        float x = hotSpotXLoc[i];
        float y = hotSpotYLoc[i];
        float halfH = 0.5f * (float)sprite->m_texture.m_height;
        float halfW = 0.5f * (float)(s16)sprite->m_texture.m_width;
        verts[0].x = x - halfW;
        verts[1].x = halfW + x;
        verts[2].x = halfW + x;
        verts[3].x = x - halfW;
        verts[0].y = y - halfH;
        verts[1].y = y - halfH;
        verts[2].y = halfH + y;
        verts[3].y = halfH + y;
        sprite->DrawPoly(verts, 4, CSprite::POLY_TRIANGLE_FAN);
    }
    DisplayControlsForNewConfigRendererer();
    HandleAnalogButtonPress();
    HandleButtonPress();
    if (m_selected > 12) {
        m_selected = 0;
    } else if (m_selected < 0) {
        m_selected = 12;
    }
    if (visualConfigText[m_selected] > 14) {
        visualConfigText[m_selected] = 0;
    } else if (visualConfigText[m_selected] < 0) {
        visualConfigText[m_selected] = 14;
    }
    UnassignPreviousAssignedButton();
    if (m_applied) {
        ApplyChanges();
    }
}

// Draws action `row`'s name at the hot spot(s) of `binding`.
static void DrawBinding(CFont* font, int row, s8 binding, float xOffset) {
    float x, y, w, h;
    char* text = MovementText[row];
    switch (buttonType[row]) {
    case 0:
    case 1: {
        FONT_getrecta(font->m_font, text, &x, &y, &w, &h);
        int spot = ButtonHotBoxLoc[binding];
        if (binding != 7) {
            font->DrawText(-(0.5f * w - hotSpotXLoc[spot]) + xOffset, -(0.5f * h - hotSpotYLoc[spot]) + -5.0f, text);
        }
        break;
    }
    case 2: {
        FONT_getrecta(font->m_font, text, &x, &y, &w, &h);
        int spot = AxisPlusHotBoxLoc[binding];
        if (binding != 5) {
            font->DrawText(-(0.5f * w - hotSpotXLoc[spot]) + xOffset, -(0.5f * h - hotSpotYLoc[spot]) + -5.0f, text);
        }
        spot = AxisMinusHotBoxLoc[binding];
        if (binding != 5) {
            font->DrawText(-(0.5f * w - hotSpotXLoc[spot]) + xOffset, -(0.5f * h - hotSpotYLoc[spot]) + -5.0f, text);
        }
        break;
    }
    }
}

void CDualConfig::DisplayControlsForNewConfigRendererer() {
    int row = m_selected - 1;
    if (row < 0) {
        row = 12;
    }
    for (int i = 0; i < CONFIG_NUM_ACTIONS; i++) {
        if (row > 12) {
            row = 0;
        }
        m_rows[i] = row;
        row++;
    }
    s8 unassigned = WillBeUnassigned();

    // The selected action's name (the lines above and below are skipped).
    int y = -9;
    for (int i = 0; i < 3; i++, y += 20) {
        if (i != 0 && i != 2) {
            s8 r = m_rows[i];
            if (r == m_selected) {
                m_menuFont->SetColor(CColor(0, 0, 0, 0x80));
            }
            char* text = movementText[r];
            float x, ty, w, h;
            FONT_getrecta(m_menuFont->m_font, text, &x, &ty, &w, &h);
            x = -(w * 0.5f - 320.0f);
            m_menuFont->DrawText(x, 0.5f + (312.0f * g_screen.m_heightScale + y), text);
            if (r == m_selected) {
                m_menuFont->SetColor(CColor(0x40, 0x40, 0x40, 0x40));
            }
        }
    }

    // Each action's name at its binding, except one that the selection
    // would take over.
    for (s8 i = 0; i < CONFIG_NUM_ACTIONS; i++) {
        int sel = m_selected;
        s8 selBinding = visualConfigText[sel];
        bool selIsTriggerAxis = selBinding == 4 && buttonType[sel] == 2;
        if (selIsTriggerAxis) {
            // The L/R axis takes the L and R buttons and the other L/R axis.
            bool hidden;
            if (i == sel) {
                hidden = false;
            } else if (selBinding == 4) {
                s8 b = visualConfigText[i];
                s8 t = buttonType[i];
                if (b == 4 && t == 2) {
                    hidden = true;
                } else if (b == 4 && t == 1) {
                    hidden = true;
                } else if (b == 4 && t == 0) {
                    hidden = true;
                } else if (b == 5 && t == 1) {
                    hidden = true;
                } else if (b == 5 && t == 0) {
                    hidden = true;
                } else {
                    hidden = false;
                }
            } else {
                hidden = false;
            }
            if (hidden) {
                continue;
            }
        }
        // L or R takes the L/R axis.
        bool hidden;
        if (buttonType[sel] == 2) {
            hidden = false;
        } else if ((selBinding == 4 || selBinding == 5) && buttonType[i] == 2 && visualConfigText[i] == 4) {
            hidden = true;
        } else {
            hidden = false;
        }
        if (!hidden && unassigned != i) {
            if (i == sel) {
                m_bindingFont->SetColor(CColor(0x6E, 0x6E, 0x6E, 0x40));
            }
            DrawBinding(m_bindingFont, i, visualConfigText[i], 0.0f);
            if (i == m_selected) {
                m_bindingFont->SetColor(CColor(0x28, 0x28, 0x28, 0x28));
            }
        }
    }

    // While picking, the selection's assigned binding too.
    int sel = m_selected;
    if (visualConfigText[sel] != currConfigText[sel]) {
        float x, y2, w, h;
        char* text = MovementText[(s8)sel];
        s8 binding = currConfigText[(s8)sel];
        switch (buttonType[sel]) {
        case 0:
        case 1: {
            FONT_getrecta(m_bindingFont->m_font, text, &x, &y2, &w, &h);
            int spot = ButtonHotBoxLoc[binding];
            if (binding != 7) {
                m_bindingFont->DrawText(-(0.5f * w - hotSpotXLoc[spot]), -(0.5f * h - hotSpotYLoc[spot]) + -5.0f, text);
            }
            break;
        }
        case 2: {
            FONT_getrecta(m_bindingFont->m_font, text, &x, &y2, &w, &h);
            int spot = AxisPlusHotBoxLoc[binding];
            if (binding != 5) {
                m_bindingFont->DrawText(-(0.5f * w - hotSpotXLoc[spot]), -(0.5f * h - hotSpotYLoc[spot]) + -5.0f, text);
            }
            binding = currConfigText[(s8)sel];
            spot = AxisMinusHotBoxLoc[binding];
            if (binding != 5) {
                m_bindingFont->DrawText(-(0.5f * w - hotSpotXLoc[spot]), -(0.5f * h - hotSpotYLoc[spot]) + -5.0f, text);
            }
            break;
        }
        }
    }
}

void CDualConfig::OnEnter() {
    char layout[16];
    GetLayout(this, m_config, layout);
    for (int i = 0; i < CONFIG_NUM_ACTIONS; i++) {
        currConfigText[i] = layout[i];
        visualConfigText[i] = layout[i];
    }
    m_savedConfig = g_Shell.Get_currentControllerSetting();
    if (m_savedConfig == 2) {
        currConfigText[8] = 7;
        visualConfigText[8] = 7;
    }
    currConfigText[13] = 0;
    visualConfigText[13] = 0;
    m_selected = 0;
    m_cancelled = false;
}

// Sets currConfigText to preset `config` and applies it.
void CDualConfig::SetUpDefaultConfig(int config) {
    s8* c = currConfigText;
    switch (config) {
    case 1:
        c[0] = 1; c[1] = 3; c[2] = 2; c[3] = 0; c[4] = 0; c[5] = 1; c[6] = 4;
        c[7] = 5; c[8] = 6; c[9] = 9; c[10] = 8; c[11] = 3; c[12] = 2;
        break;
    case 2:
        c[0] = 1; c[1] = 3; c[2] = 0; c[3] = 4; c[4] = 0; c[5] = 9; c[6] = 6;
        c[7] = 2; c[8] = 3; c[9] = 8; c[10] = 7; c[11] = 1; c[12] = 3;
        break;
    case 3:
        c[0] = 1; c[1] = 3; c[2] = 2; c[3] = 0; c[4] = 0; c[5] = 1; c[6] = 4;
        c[7] = 5; c[8] = 6; c[9] = 9; c[10] = 8; c[11] = 2; c[12] = 3;
        break;
    case 4:
        c[0] = 1; c[1] = 3; c[2] = 2; c[3] = 0; c[4] = 0; c[5] = 1; c[6] = 4;
        c[7] = 5; c[8] = 2; c[9] = 9; c[10] = 8; c[11] = 6; c[12] = 3;
        break;
    case 5:
        c[0] = 1; c[1] = 3; c[2] = 0; c[3] = 2; c[4] = 0; c[5] = 1; c[6] = 4;
        c[7] = 5; c[8] = 6; c[9] = 9; c[10] = 8; c[11] = 3; c[12] = 2;
        break;
    }
    ApplyChanges();
}

bool CDualConfig::GetAppliedState() {
    return m_applied;
}

void CDualConfig::SetAppliedState(bool applied) {
    m_applied = applied;
}

// The shell's current layout into the player's keymap (and the keymap
// to the debug log).
void CDualConfig::SetupButtonsForGame() {
    char layout[16];
    GetLayout(this, g_Shell.Get_currentControllerSetting(), layout);
    BuildKeymap((s8*)layout, m_player, NULL);
    for (int i = 0; i < 0x40; i++) {
        KEYMAP* keymap = (KEYMAP*)((u8*)g_config_keymap + m_player * 0x104);
        DebugMsg("--------------------------------------------------\n");
        DebugMsg("customConfigButton[%d].events.eventId = %d\n", i, keymap->entries[i].event);
        DebugMsg("customConfigButton[%d].events.inputBinding = %d\n", i, keymap->entries[i].source);
    }
}

// currConfigText (saved to the shell for a custom layout) into the keymap.
// As the original: the bindings always go to player 0's keymap while the
// fixed tail goes to m_player's.
void CDualConfig::ApplyChanges() {
    if (m_config > 5) {
        g_Shell.CopyControllerButtons((char*)currConfigText, m_config - 6);
    }
    BuildKeymap(currConfigText, m_player, ((KEYMAP*)g_config_keymap)->entries);
}

// The action whose binding the selection's pick would take, or -1.
s8 CDualConfig::WillBeUnassigned() {
    s8 result = -1;
    for (s8 i = 0; i < CONFIG_NUM_ACTIONS; i++) {
        int sel = m_selected;
        if (i == sel) {
            continue;
        }
        s8 t = buttonType[i];
        s8 pick = visualConfigText[sel];
        if ((t != 0 || pick != 7) && (t != 1 || pick != 7) && (t != 2 || pick != 5) && currConfigText[i] == pick) {
            s8 selType = buttonType[sel];
            if (t == 0 && t == selType) {
                result = i;
            } else if (t == 2 && t == selType) {
                result = i;
            } else if (t == 1 && t == selType) {
                result = i;
            } else if (t == 1 && selType == 0) {
                result = i;
            } else if (t == 0 && selType == 1) {
                result = i;
            }
        }
    }
    return result;
}

// Unassigns the actions sharing the selection's binding, and the L/R axis
// against the L and R buttons.
void CDualConfig::UnassignPreviousAssignedButton() {
    for (s8 i = 0; i < CONFIG_NUM_ACTIONS; i++) {
        int sel = m_selected;
        if (i != sel && currConfigText[i] == currConfigText[sel]) {
            s8 t = buttonType[i];
            if (t == 0 && t == buttonType[sel]) {
                visualConfigText[i] = 7;
                currConfigText[i] = 7;
            } else if (t == 2 && t == buttonType[sel]) {
                visualConfigText[i] = 5;
                currConfigText[i] = 5;
            } else if (t == 1 && t == buttonType[sel]) {
                visualConfigText[i] = 7;
                currConfigText[i] = 7;
            } else if (t == 1 && buttonType[sel] == 0) {
                visualConfigText[i] = 7;
                currConfigText[i] = 7;
            } else if (t == 0 && buttonType[sel] == 1) {
                visualConfigText[i] = 7;
                currConfigText[i] = 7;
            }
        }
    }
    int sel = m_selected;
    s8 type = buttonType[sel];
    if (type == 2) {
        if (currConfigText[sel] == 4) {
            // The L/R axis: unassign the L (4) and R (5) buttons.
            for (int b = 4; b < 6; b++) {
                for (int i = 0; i < CONFIG_NUM_ACTIONS; i++) {
                    if (buttonType[i] != 2 && currConfigText[i] == b) {
                        visualConfigText[i] = 7;
                        currConfigText[i] = 7;
                    }
                }
            }
        }
    } else if (type >= 0 && type < 2) {
        if (currConfigText[sel] == 4 || currConfigText[sel] == 5) {
            for (int i = 0; i < CONFIG_NUM_ACTIONS; i++) {
                if (buttonType[i] == 2 && currConfigText[i] == 4) {
                    visualConfigText[i] = 5;
                    currConfigText[i] = 5;
                }
            }
        }
    }
}

// Steps the selection's binding back, skipping ones the row can't take.
// As the original: the skip doesn't wrap (it can read the row before).
static void PrevBinding(CDualConfig* self) {
    s8 binding = visualConfigText[self->m_selected] - 1;
    if (binding < 0) {
        binding = 14;
    }
    visualConfigText[self->m_selected] = binding;
    if (self->m_selected != 13) {
        while (configArray[self->m_selected][binding] == -1) {
            binding--;
            visualConfigText[self->m_selected]--;
        }
    }
}

static void NextBinding(CDualConfig* self) {
    s8 binding = visualConfigText[self->m_selected] + 1;
    if (binding > 14) {
        binding = 0;
    }
    visualConfigText[self->m_selected] = binding;
    if (self->m_selected != 13) {
        while (configArray[self->m_selected][binding] == -1) {
            binding++;
            visualConfigText[self->m_selected]++;
        }
    }
}

void CDualConfig::HandleButtonPress() {
    if (AnyPortValue(0x7B) != 0.0f) {  // up
        visualConfigText[m_selected] = currConfigText[m_selected];
        m_selected--;
        return;
    }
    if (AnyPortValue(0x7C) != 0.0f) {  // down
        visualConfigText[m_selected] = currConfigText[m_selected];
        m_selected++;
        return;
    }
    if (AnyPortValue(0x7D) != 0.0f) {  // left
        PrevBinding(this);
    } else if (AnyPortValue(0x7E) != 0.0f) {  // right
        NextBinding(this);
    } else {
        float assign = AnyPortValue(0x80);
        float assign2 = AnyPortValue(0x87);
        if (assign != 0.0f || assign2 != 0.0f) {
            currConfigText[m_selected] = visualConfigText[m_selected];
            return;
        }
        if (AnyPortValue(0x81) != 0.0f) {  // apply
            m_applied = true;
            g_Shell.Set_currentControllerSetting(m_config);
            return;
        }
        if (AnyPortValue(0x7F) != 0.0f) {  // cancel: back to the layout on entry
            m_cancelled = true;
            m_applied = true;
            g_Shell.Set_currentControllerSetting(m_savedConfig);
            m_config = m_savedConfig;
            if (m_savedConfig >= 6) {
                char layout[16];
                g_Shell.RetrieveShellControllerButtons(layout, m_savedConfig - 6);
                for (int i = 0; i < CONFIG_NUM_ACTIONS; i++) {
                    currConfigText[i] = layout[i];
                    visualConfigText[i] = layout[i];
                }
                currConfigText[13] = 0;
                visualConfigText[13] = 0;
                return;
            }
            SetUpDefaultConfig(m_savedConfig);
            return;
        }
        if (AnyPortValue(0x82) != 0.0f) {  // defaults
            SetUpDefaultConfig(1);
            currConfigText[13] = 0;
            visualConfigText[13] = 0;
        }
    }
}

// The stick moves like the D-pad once it has travelled far enough.
void CDualConfig::HandleAnalogButtonPress() {
    float value = AnyPortValue(0x8A);
    m_vertAccum += value;
    if (value < -48.0f && value < -10.0f) {
        if (m_vertAccum < -775.0f) {
            visualConfigText[m_selected] = currConfigText[m_selected];
            m_selected--;
            m_vertAccum = 0.0f;
            g_Shell.PlayShellSound(0);
        }
    } else if (value > 48.0f && value > 10.0f) {
        if (m_vertAccum > 775.0f) {
            visualConfigText[m_selected] = currConfigText[m_selected];
            m_selected++;
            m_vertAccum = 0.0f;
            g_Shell.PlayShellSound(0);
        }
    } else {
        m_vertAccum = 0.0f;
    }

    value = AnyPortValue(0x89);
    m_horizAccum += value;
    if (value < -48.0f && value < -10.0f) {
        if (m_horizAccum < -775.0f) {
            PrevBinding(this);
            m_horizAccum = 0.0f;
            g_Shell.PlayShellSound(0);
        }
    } else if (value > 48.0f && value > 10.0f) {
        if (m_horizAccum > 775.0f) {
            NextBinding(this);
            m_horizAccum = 0.0f;
            g_Shell.PlayShellSound(0);
        }
    } else {
        m_horizAccum = 0.0f;
    }
}

void CDualConfig::Init() {
    char* fontData = (char*)TLT_LoadFileNormal("data/objFont.gfn", NULL, 0);
    m_bindingFont = new CFont(fontData, (ERenderPriority)0x3E);
    m_bindingFont->SetColor(CColor(0x40, 0x40, 0x40, 0x40));
    m_bindingFont->SetHeight(12.0f);
    m_bindingFont->SetScale(1.0f, 1.0f);
    CScreen* screen = CScreen::GetCurrent();
    if (screen != NULL) {
        m_bindingFont->SetDepth(screen->GetDepthBufferMax());
    }
    fontData = (char*)TLT_LoadFileNormal("data/objFont.gfn", NULL, 0);
    m_menuFont = new CFont(fontData, (ERenderPriority)0x3E);
    m_menuFont->SetColor(CColor(0x40, 0x40, 0x40, 0x40));
    m_menuFont->SetHeight(12.0f);
    if (screen != NULL) {
        m_menuFont->SetDepth(screen->GetDepthBufferMax());
    }
    m_hotbox = new CSprite("data\\MOHFL_exports\\hotbox.gsh", CSprite::LOAD_SHELL, (ERenderPriority)0x34);

    char layout[16];
    GetLayout(this, m_config, layout);
    for (int i = 0; i < CONFIG_NUM_ACTIONS; i++) {
        currConfigText[i] = layout[i];
        visualConfigText[i] = layout[i];
    }
    currConfigText[13] = 0;
    visualConfigText[13] = 0;
    InitializeButtonMovementArray();
    m_savedConfig = 1;
}

CDualConfig::~CDualConfig() {
    delete m_hotbox;
    delete m_menuFont;
    delete m_bindingFont;
}
