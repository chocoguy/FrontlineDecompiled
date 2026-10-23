#include "game/pumhandler.h"
#include "game/popup.h"
#include "game/font.h"
#include "game/sprite.h"
#include "game/Screen.h"
#include "game/viewport.h"
#include "game/scene.h"
#include "game/player.h"
#include "game/Pause.h"
#include "game/ShellMenu.h"
#include "game/toilet.h"
#include "game/memory.h"
#include "game/Moh2.h"
#include "ea/sys/font.h"
#include "dwi/list.h"

#include <string.h>

void DebugMsg(const char*, ...);

CUserMessageTable umTable;
dwi::list<QueuedSubtitle> g_subtitle_queue;
bool g_subtitle_queue_initialized;

// Messages per placement (two in multiplayer).
static int POPUPS_IN_SECTION = PUM_SLOTS;
static float MP_PUM_TEXT_SCALE_X_2P = 1.0f;
static float MP_PUM_TEXT_SCALE_Y_2P = 1.0f;
static float MP_PUM_TEXT_SCALE_X_4P = 0.9f;
static float MP_PUM_TEXT_SCALE_Y_4P = 0.9f;

// The background quad: grey, half transparent.
#define PUM_BACK_VERT(u, v) {u, v, 0.0f, 0.0f, 0.0f, {0x80, 0x80, 0x80, 0x40}}

void CUserMessageTable::Unload() {
    if (m_entries != NULL) {
        delete[] m_entries;
        m_entries = NULL;
    }
    if (m_text != NULL) {
        TLT_CloseFile(m_text);
        m_text = NULL;
    }
    if (m_index != NULL) {
        TLT_CloseFile(m_index);
        m_index = NULL;
    }
    m_count = 0;
}

// Loads the text file `text` and its index `index`; each string is
// terminated in place.
void CUserMessageTable::Load(char* text, char* index) {
    if (m_text != NULL || m_index != NULL || m_entries != NULL) {
        Unload();
    }
    m_text = TLT_LoadFileNormal(text, NULL, 0);
    m_index = TLT_LoadFileNormal(index, NULL, 0);
    char* str = (char*)m_text;
    u8* idx = (u8*)m_index;
    if (str == NULL || idx == NULL) {
        DebugMsg("Can't find User message files: %s and %s!!!\n", text, index);
        return;
    }
    m_count = (u16)(idx[0] | (idx[1] << 8));
    if (m_entries != NULL) {
        delete[] m_entries;
    }
    m_entries = (UMEntry*)new u8[m_count * sizeof(UMEntry)];
    int at = 2;
    for (u16 i = 0; i < m_count; i++, at += 2) {
        u8 gap = idx[at];
        u8 length = idx[at + 1];
        m_entries[i].m_gap = gap;
        m_entries[i].m_string = str;
        char* end = str + length;
        str = end + (gap + 1);
        m_entries[i].m_length = length;
        *end = 0;
    }
}

CUserMessageTable::~CUserMessageTable() {
    if (m_entries != NULL) {
        delete[] m_entries;
    }
    if (m_text != NULL) {
        TLT_CloseFile(m_text);
    }
    if (m_index != NULL) {
        TLT_CloseFile(m_index);
    }
    m_count = 0;
}

CUserMessageTable::CUserMessageTable() {
    m_entries = NULL;
    m_index = NULL;
    m_text = NULL;
    m_count = 0;
}

static inline void QueueSubtitle(const char* text, int length, float time, const CColor& color, bool whole) {
    QueuedSubtitle s;
    memset(s.m_text, 0, sizeof(s.m_text));
    if (whole) {
        strcpy(s.m_text, text);
    } else {
        strncpy(s.m_text, text, length);
    }
    s.m_time = time;
    s.m_color = color;
    s.m_delay = 0.0f;
    g_subtitle_queue.push_back(s);
}

// Replaces the queued subtitles with `text`, broken into lines of about
// 25 characters (at spaces, up to 29). Each line shows for `time` seconds
// (at least 1.5 per line); four lines show at once, the next four after.
void DisplaySubtitle(const char* text, float time, CColor color) {
    int length = strlen(text);
    s8 lines = 0;
    if (!g_subtitle_queue_initialized) {
        g_subtitle_queue.reserve(0x14, "Subtitle Queue");
        g_subtitle_queue_initialized = 1;
    }
    g_subtitle_queue.clear();
    while (length > 0x19) {
        const char* end = text;
        while (end - text <= 0x19) {
            const char* space = strchr(end, ' ');
            if (space != NULL) {
                space++;
                if (space - text > 0x1D) {
                    break;
                }
                end = space;
            } else {
                if (length <= 0x1D) {
                    end = text + length;
                }
                break;
            }
        }
        QueueSubtitle(text, end - text, time, color, false);
        while (*end == ' ') {
            end++;
        }
        text = end;
        length = strlen(end);
        lines++;
    }
    if (length > 0) {
        QueueSubtitle(text, 0, time, color, true);
        lines++;
    }
    float least = 1.5f * lines;
    if (time < least) {
        time = least;
    }
    if (lines >= 4) {
        time /= (float)(lines / 4);
    }
    int i = 0;
    for (dwi::list<QueuedSubtitle>::node* n = g_subtitle_queue.begin(); n != g_subtitle_queue.end(); n = n->next) {
        n->value.m_time = time;
        n->value.m_delay = ((float)(i++ / 4) * time) / 0.016683351f;
    }
}

// Shows the queued subtitles whose delay is up (`dt` frames).
void UpdateSubtitles(float dt) {
    if (!g_subtitle_queue_initialized) {
        g_subtitle_queue.reserve(0x14, "Subtitle queue");
        g_subtitle_queue_initialized = 1;
    }
    dwi::list<QueuedSubtitle>::node* n = g_subtitle_queue.begin();
    while (n != g_subtitle_queue.end()) {
        if (n->value.m_delay <= dt) {
            CColor c = n->value.m_color;
            g_pPopUpMessageHandler[0]->AddPopUpMessage(n->value.m_text, PUM_PLACE_SUBTITLE,
                                                       140.0f * g_screen.m_widthScale,
                                                       402.0f * g_screen.m_heightScale, 1, n->value.m_time, 0x16,
                                                       c.r, c.g, c.b, c.a);
            dwi::list<QueuedSubtitle>::node* next = n->next;
            g_subtitle_queue.erase(n);
            n = next;
        } else {
            n->value.m_delay -= dt;
            n = n->next;
        }
    }
}

void PromptUserMessage(char* text, PopUpMessageHandler* handler, PopUpMessage* message) {
    u8 b = message->Get_b();
    u8 g = message->Get_g();
    u8 r = message->Get_r();
    int time = message->Get_onScreenTime();
    u8 property = message->Get_property();
    u16 y = message->Get_screenLocY();
    u16 x = message->Get_screenLocX();
    handler->AddPopUpMessage(text, message->Get_placement(), x, y, property, time, 0x16, r, g, b, 0x23);
}

// User message `index` at the top of the screen for five seconds.
int PromptUserMessage(unsigned int index, PopUpMessageHandler* handler) {
    char* text = GetUMString(index);
    if (text == NULL) {
        return 0;
    }
    handler->AddPopUpMessage(text, PUM_PLACE_TOP, 0x140, 0xF0, 1, 5, 0x16, 0x3F, 0x3F, 0x3F, 0x80);
    return 1;
}

// The scrolling form of a settled property.
static inline u8 ScrollingProperty(u8 property) {
    switch (property) {
    case 1:
        return 4;
    case 2:
        return 5;
    case 3:
        return 6;
    case 7:
        return 8;
    }
    return property;
}

// Shows `text` in a free slot of placement `placement` (else replacing
// the oldest one, the one farthest along; fixed messages need a free
// slot). The other messages there move along by a line.
void PopUpMessageHandler::AddPopUpMessage(char* text, unsigned char placement, unsigned short x, unsigned short y,
                                          unsigned char property, unsigned char time, unsigned char size,
                                          unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    static int lastUsedSlotOnTop = -1;
    static char lastString[0x32];
    PopUpMessage* section = m_messages[placement];
    s16 slot;
    s16 i;
    PopUpMessage* m = section;
    for (i = 0; i < POPUPS_IN_SECTION; i++, m++) {
        if (m->Available()) {
            slot = i;
            goto found;
        }
    }
    if (placement == PUM_PLACE_FIXED) {
        slot = -1;
    } else {
        u16 oldest = 0;
        u16 top = section[0].Get_top();
        switch (placement) {
        case PUM_PLACE_MIDDLE:
        case PUM_PLACE_TOP:
            m = section + 1;
            for (i = 1; i < POPUPS_IN_SECTION; i++, m++) {
                u16 t = m->Get_top();
                if (t > top) {
                    oldest = i;
                    top = t;
                }
            }
            break;
        case PUM_PLACE_SUBTITLE:
        case PUM_PLACE_LOWER:
            m = section + 1;
            for (i = 1; i < POPUPS_IN_SECTION; i++, m++) {
                u16 t = m->Get_top();
                if (t < top) {
                    oldest = i;
                    top = t;
                }
            }
            break;
        }
        section[oldest].Reset();
        slot = oldest;
    }
found:
    if (placement == PUM_PLACE_TOP) {
        lastUsedSlotOnTop = slot;
        if (strlen(text) > 0x31) {
            text[0x31] = 0;
        }
        strncpy(lastString, text, 0x32);
    }
    if (slot == -1) {
        return;
    }
    m = section;
    for (s16 j = 0; j < POPUPS_IN_SECTION; j++, m++) {
        u8 p = m->Get_property();
        if (!m->Available()) {
            switch (p) {
            case 1:
                m->Set_property(4);
                break;
            case 2:
                m->Set_property(5);
                break;
            case 3:
                m->Set_property(6);
                break;
            case 7:
                m->Set_property(8);
                break;
            }
            m->MoveTop();
        }
    }
    m = &section[slot];
    m->SetString(text);
    m->Set_placement(placement);
    m->Set_rgba(r, g, b, a);
    switch (placement) {
    case PUM_PLACE_MIDDLE:
        m->Set_screenLocs(0x140, 144.0f * g_screen.m_heightScale);
        m->Set_top(144.0f * g_screen.m_heightScale);
        m->Set_bottom(120.0f * g_screen.m_heightScale);
        m->Set_placement(PUM_PLACE_MIDDLE);
        break;
    case PUM_PLACE_SUBTITLE:
        m->Set_screenLocs(140.0f * g_screen.m_widthScale, 402.0f * g_screen.m_heightScale);
        m->Set_top(402.0f * g_screen.m_heightScale);
        m->Set_bottom(426.0f * g_screen.m_heightScale);
        m->Set_placement(PUM_PLACE_SUBTITLE);
        break;
    case PUM_PLACE_LOWER:
        m->Set_screenLocs(0x80, 296.0f * g_screen.m_heightScale);
        m->Set_top(296.0f * g_screen.m_heightScale);
        m->Set_bottom(320.0f * g_screen.m_heightScale);
        m->Set_placement(PUM_PLACE_LOWER);
        break;
    case PUM_PLACE_FIXED:
        m->Set_screenLocs(x, y);
        break;
    case PUM_PLACE_TOP:
        m->Set_screenLocs(0x140, 12.0f * g_screen.m_heightScale);
        m->Set_top(12.0f * g_screen.m_heightScale);
        m->Set_bottom(88.0f * g_screen.m_heightScale);
        m->Set_placement(PUM_PLACE_TOP);
        break;
    default:
        m->Set_screenLocs(0x140, 12.0f * g_screen.m_heightScale);
        break;
    }
    m->Set_property(ScrollingProperty(property));
    m->Set_onScreenTime((float)time);
    m->Set_textSize(size);
    m->ChangeAvailability(0);
}

static inline int CountShowing(PopUpMessage* section) {
    int n = 0;
    for (int i = 0; i < POPUPS_IN_SECTION; i++, section++) {
        if (!section->Available()) {
            n++;
        }
    }
    return n;
}

// Draws the backgrounds and the messages of this player's view, scrolls
// the ones moving into place and counts down their time.
void PopUpMessageHandler::PopUpMessageHandler_Loop() {
    float left = 0.0f;
    float right = (float)g_screen.GetWidth();
    float subTop = 322.0f * g_screen.m_heightScale;
    float subBottom = 448.0f * g_screen.m_heightScale;
    CScreen* screen = CScreen::GetCurrent();
    int screenW = screen->GetWidth();
    int screenH = screen->GetHeight();
    float dt = 0.016683351f * m_dt;
    float vpWidth = g_viewport.m_width;
    float vpHeight = g_viewport.m_height;
    if (g_bInMultiplayerMode) {
        float x0 = (float)screenW * g_viewport.m_left;
        left = 10.0f * vpWidth + x0;
        right = (right - 10.0f) * vpWidth + x0;
        float y0 = (float)screenH * g_viewport.m_top;
        subTop = subTop * vpHeight + y0;
        subBottom = subBottom * vpHeight + y0;
    }
    switch (g_NumPlayers) {
    case 1:
        m_pFont->SetScale(1.0f, 1.0f);
        break;
    case 2:
        m_pFont->SetScale(MP_PUM_TEXT_SCALE_X_2P, MP_PUM_TEXT_SCALE_Y_2P);
        break;
    case 3:
    case 4:
        m_pFont->SetScale(MP_PUM_TEXT_SCALE_X_4P, MP_PUM_TEXT_SCALE_Y_4P);
        break;
    }

    if (m_show) {
        // Behind the top messages, down to the last one.
        int n = CountShowing(m_messages[PUM_PLACE_TOP]);
        if (n > 0) {
            int unused = (4 - n) * 0x16;
            _spritepolyvert v[4] = {
                PUM_BACK_VERT(0.0f, 0.0f),
                PUM_BACK_VERT(1.0f, 0.0f),
                PUM_BACK_VERT(1.0f, 1.0f),
                PUM_BACK_VERT(0.0f, 1.0f),
            };
            float bottom = 110.0f * g_screen.m_heightScale;
            v[0].x = left;
            v[1].x = right;
            v[2].x = right;
            v[2].y = bottom;
            v[3].x = left;
            v[3].y = bottom;
            v[0].y += g_viewport.m_top * (float)g_screen.GetHeight();
            v[1].y += g_viewport.m_top * (float)g_screen.GetHeight();
            v[2].y += g_viewport.m_top * (float)g_screen.GetHeight() - (float)unused;
            v[3].y += g_viewport.m_top * (float)g_screen.GetHeight() - (float)unused;
            m_pBack->DrawPoly(v, 4, (CSprite::EPolyPrimType)5);
        }
        // Behind the subtitles (single player, HUD not hidden).
        if (CountShowing(m_messages[PUM_PLACE_SUBTITLE]) > 0 && !g_scene.GetPlayer(0)->m_ui.m_letterbox &&
            !g_bInMultiplayerMode) {
            _spritepolyvert v[4] = {
                PUM_BACK_VERT(0.0f, 0.0f),
                PUM_BACK_VERT(1.0f, 0.0f),
                PUM_BACK_VERT(1.0f, 1.0f),
                PUM_BACK_VERT(0.0f, 1.0f),
            };
            v[0].x = left;
            v[0].y = subTop;
            v[1].x = right;
            v[1].y = subTop;
            v[2].x = right;
            v[2].y = subBottom;
            v[3].x = left;
            v[3].y = subBottom;
            m_pSubtitleBack->DrawPoly(v, 4, (CSprite::EPolyPrimType)5);
        }
    }

    // The newest top message (the highest) is drawn brighter.
    int newest = -1;
    {
        s16 highest = 0x929;
        PopUpMessage* m = m_messages[PUM_PLACE_TOP];
        for (s16 i = 0; i < POPUPS_IN_SECTION; i++, m++) {
            if (!m->Available()) {
                s16 top = m->Get_top();
                if (top < highest) {
                    newest = i;
                    highest = top;
                }
            }
        }
    }
    s16 bright = newest;

    for (int place = 0; place < PUM_NUM_PLACES; place++) {
        PopUpMessage* m = m_messages[place];
        for (int i = 0; i < POPUPS_IN_SECTION; i++, m++) {
            if (m->Available()) {
                continue;
            }
            switch (m->Get_property()) {
            case 4:
                m->Scroll();
                break;
            case 1:
                if (!m_show) {
                    break;
                }
                if (place == PUM_PLACE_SUBTITLE) {
                    float x = (float)m->Get_screenLocX();
                    m_pSubtitleFont->SetColor(CColor(0xFF, 0xFF, 0xFF, m->Get_a()));
                    m_pSubtitleFont->DrawText(x, (float)m->Get_screenLocY(), m->GetString());
                    break;
                }
                if (g_scene.GetPlayer(0)->m_ui.m_letterbox) {
                    break;
                }
                if (i != bright || g_bInMultiplayerMode) {
                    if (m->Get_r() == 0x3C) {
                        m_pFont->SetColor(CColor(0x3C, 0x3C, 0x28, 0x80));
                    } else {
                        m_pFont->SetColor(CColor(0x23, 0x23, 0x23, 0x80));
                    }
                } else {
                    if (m->Get_r() == 0x3C) {
                        m_pFont->SetColor(CColor(0x3C, 0x3C, 0x28, 0x80));
                    } else {
                        m_pFont->SetColor(CColor(0x37, 0x37, 0x37, 0x80));
                    }
                }
                {
                    float rx, ry, rw, rh;
                    FONT_getrecta(m_pFont->m_font, m->GetString(), &rx, &ry, &rw, &rh);
                    float sx, sy;
                    m_pFont->GetScale(sx, sy);
                    if (g_bInMultiplayerMode) {
                        static int MP_PUM_LEFT_JUSTIFY = 0x1E;
                        float x = vpWidth * (float)MP_PUM_LEFT_JUSTIFY + (float)screenW * g_viewport.m_left;
                        float y = sy * (float)m->Get_screenLocY() + (float)screenH * g_viewport.m_top;
                        y += 8.0f;
                        m_pFont->DrawText(x, y, m->GetString());
                    } else {
                        m_pFont->DrawTextCentered((float)m->Get_screenLocY(), m->GetString());
                    }
                }
                break;
            }
            if (!g_Pause.bIsGamePaused()) {
                if (m->Get_onScreenTime() < 0.0f) {
                    m->Reset();
                }
                m->UpdateDisplayTime(dt);
            }
        }
    }
}

PopUpMessageHandler::~PopUpMessageHandler() {
    if (m_pSubtitleBack != NULL) {
        delete m_pSubtitleBack;
    }
    if (m_pBack != NULL) {
        delete m_pBack;
    }
    if (m_pFont != NULL) {
        delete m_pFont;
    }
    m_pFont = NULL;
}

// Loads the user messages (`text`, `index`), the fonts named by the level
// (the multiplayer font in multiplayer) and the message backgrounds.
PopUpMessageHandler::PopUpMessageHandler(char* text, char* index) {
    umTable.Load(text, index);
    LevelFileContentsStruct_* level = TLT_FindNextResourceByType((TLTResourceID)0xB, NULL);
    void* font;
    if (g_bInMultiplayerMode) {
        font = TLT_LoadFileFromLevelBigFile(ADDR_TO_PTR(char, level->fields[3].value), NULL);
    } else {
        font = TLT_LoadFileFromLevelBigFile(ADDR_TO_PTR(char, level->fields[44].value), NULL);
    }
    void* subtitleFont = TLT_LoadFileFromLevelBigFile(ADDR_TO_PTR(char, level->fields[2].value), NULL);
    if (g_bInMultiplayerMode) {
        POPUPS_IN_SECTION = 2;
    }
    m_pFont = new CFont((const char*)font, (ERenderPriority)0x6F);
    m_pFont->SetColor(CColor(0x3F, 0x3F, 0x3F, 0x80));
    CScreen* screen = CScreen::GetCurrent();
    if (screen != NULL) {
        m_pFont->SetDepth(screen->GetDepthBufferMax());
    }
    m_pSubtitleFont = new CFont((const char*)subtitleFont, (ERenderPriority)0x70);
    m_pSubtitleFont->SetColor(CColor(0x3F, 0x3F, 0x3F, 0x3F));
    m_pSubtitleFont->SetScale(1.0f, 1.0f);
    if (screen != NULL) {
        m_pSubtitleFont->SetDepth(screen->GetDepthBufferMax());
    }
    if (m_pBack == NULL) {
        void* mem = DWI_alloc(NULL, sizeof(CSprite), 0x400);
        CSprite* sprite = NULL;
        if (mem != NULL) {
            sprite = ::new (mem) CSprite("data\\bitmaps\\popupBack.gsh", (CSprite::ESpriteLoadType)1, (ERenderPriority)0x6E);
        }
        m_pBack = sprite;
    }
    if (m_pSubtitleBack == NULL) {
        void* mem = DWI_alloc(NULL, sizeof(CSprite), 0x400);
        CSprite* sprite = NULL;
        if (mem != NULL) {
            sprite = ::new (mem) CSprite("data\\bitmaps\\popupSubtitleBack.gsh", (CSprite::ESpriteLoadType)1,
                                         (ERenderPriority)0xA);
        }
        m_pSubtitleBack = sprite;
    }
    m_show = g_Shell.field_0x19A6;
}
