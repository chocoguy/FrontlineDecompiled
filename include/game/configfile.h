// Skeleton for game/configfile (from tools/gen_headers.py, now hand-maintained).
// INI-style text config: "[Section]" headers followed by "key = value" lines.
#ifndef _GAME_CONFIGFILE_H_
#define _GAME_CONFIGFILE_H_

#include "types.h"
#include "game_types.h"

#define CONFIG_MAX_SECTIONS 64

class CConfigSection {
public:
    CConfigSection();          // 0x8001C5C0

    /* 0x0 */ char* name;
    /* 0x4 */ char* start;     // first character after "[name]"
    /* 0x8 */ char* end;       // last character of the section
};

// size 0x30C
class CConfigFile {
public:
    CConfigFile(const char* path);                             // 0x8001C43C
    ~CConfigFile();                                            // 0x8001C3E0
    bool SetSection(const char* name);                         // 0x8001C350
    bool GetBool(const char* key, bool defaultValue);          // 0x8001C23C
    bool GetString(const char* key, char* out, int size);      // 0x8001C138

private:
    inline char* FindValue(const char* key);

    /* 0x000 */ CConfigSection m_sections[CONFIG_MAX_SECTIONS];
    /* 0x300 */ int m_numSections;
    /* 0x304 */ int m_currentSection;
    /* 0x308 */ char* m_data;
};

extern char* eolString;

#endif
