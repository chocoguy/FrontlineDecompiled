// Pop-up messages and subtitles: the user-message string table, the
// subtitle queue, and PopUpMessageHandler (one per player view), which
// keeps four messages for each placement and draws them. Hand-maintained.
#ifndef _GAME_PUMHANDLER_H_
#define _GAME_PUMHANDLER_H_

#include "types.h"
#include "game_types.h"
#include "game/popup.h"
#include "game/color.h"

class CFont;
class CSprite;

// One string of the table (size 8).
struct UMEntry {
    /* 0x0 */ char* m_string;
    /* 0x4 */ u8 m_length;
    /* 0x5 */ u8 m_gap;  // bytes after the terminator to the next string
    /* 0x6 */ u8 unk_0x6[0x2];
};

// The user messages: the text file and its index file (a little-endian
// count, then a length and gap byte per string).
// size 0x10
class CUserMessageTable {
public:
    /* 0x0 */ UMEntry* m_entries;
    /* 0x4 */ void* m_text;
    /* 0x8 */ void* m_index;
    /* 0xC */ s32 m_count;
    void Unload();  // 0x800B8544
    void Load(char*, char*);  // 0x800B85BC
    ~CUserMessageTable();  // 0x800B8770
    CUserMessageTable();  // 0x800B87F4
};

extern CUserMessageTable umTable;  // 0x801EF3EC

// String `index` of the user-message table, or NULL past its end.
inline char* GetUMString(int index) {
    return index < umTable.m_count ? umTable.m_entries[index].m_string : NULL;
}

// A subtitle line waiting to show (size 0x3C).
struct QueuedSubtitle {
    QueuedSubtitle() : m_delay(0.0f), m_time(0.0f), m_color(0, 0, 0, 0x80) {}
    /* 0x00 */ f32 m_delay;  // frames before it shows
    /* 0x04 */ f32 m_time;  // seconds on screen
    /* 0x08 */ CColor m_color;
    /* 0x0C */ char m_text[0x30];
};

// Message placements (sections of the handler).
enum {
    PUM_PLACE_MIDDLE = 0,  // middle of the screen, new lines push down
    PUM_PLACE_SUBTITLE = 1,  // subtitles at the bottom, push up
    PUM_PLACE_LOWER = 2,
    PUM_PLACE_FIXED = 3,  // at the given position, never pushed
    PUM_PLACE_TOP = 4,  // top of the screen, push down
    PUM_NUM_PLACES = 5,
};

#define PUM_SLOTS 4

// size 0x60C (operator new)
class PopUpMessageHandler {
public:
    /* 0x000 */ PopUpMessage m_messages[PUM_NUM_PLACES][PUM_SLOTS];
    /* 0x5F0 */ u32 unk_0x5F0;
    /* 0x5F4 */ CFont* m_pFont;
    /* 0x5F8 */ CFont* m_pSubtitleFont;
    /* 0x5FC */ CSprite* m_pBack;  // behind the top messages
    /* 0x600 */ CSprite* m_pSubtitleBack;
    /* 0x604 */ f32 m_dt;  // frames this update (set by the main loop)
    /* 0x608 */ u8 m_show;  // the shell's message display option
    /* 0x609 */ u8 unk_0x609[0x3];
    void AddPopUpMessage(char*, unsigned char, unsigned short, unsigned short, unsigned char, unsigned char, unsigned char, unsigned char, unsigned char, unsigned char, unsigned char);  // 0x800B9420
    void PopUpMessageHandler_Loop();  // 0x800B9A00
    ~PopUpMessageHandler();  // 0x800BA304
    PopUpMessageHandler(char*, char*);  // 0x800BA3D0
};
OFFSET_ASSERT(PopUpMessageHandler, m_pFont, 0x5F4);
SIZE_ASSERT(PopUpMessageHandler, 0x60C);

// Free functions
void DisplaySubtitle(const char*, float, CColor);  // 0x800B880C
void UpdateSubtitles(float);  // 0x800B913C
void PromptUserMessage(char*, PopUpMessageHandler*, PopUpMessage*);  // 0x800B92C0
int PromptUserMessage(unsigned int, PopUpMessageHandler*);  // 0x800B938C

extern PopUpMessageHandler* g_pPopUpMessageHandler[4];  // 0x8019756C (bsbifunc.cpp)
#endif
