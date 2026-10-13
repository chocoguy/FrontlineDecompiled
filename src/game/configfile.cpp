#include "game/configfile.h"

#include <ctype.h>
#include <string.h>
#include "game/Moh2.h"
#include "game/toilet.h"

char* eolString = "\r\n";

// Value of `key` in the current section, after "=" and leading whitespace.
inline char* CConfigFile::FindValue(const char* key) {
    char* line;
    char* lineEnd;
    char* value;

    if (m_currentSection < 0) {
        return NULL;
    }
    line = strstr(m_sections[m_currentSection].start, key);
    if (line != NULL) {
        lineEnd = strstr(line, eolString);
        if (lineEnd == NULL) {
            lineEnd = m_sections[m_currentSection].end;
        }
        value = strchr(line, '=');
        if (value != NULL && value < lineEnd) {
            value++;
            while (isspace(*value)) {
                value++;
            }
            return value;
        }
    }
    return NULL;
}

bool CConfigFile::GetString(const char* key, char* out, int size) {
    char* value = FindValue(key);
    char* lineEnd;

    if (value != NULL) {
        memcpy(out, value, size);
        lineEnd = strstr(out, eolString);
        if (lineEnd != NULL) {
            *lineEnd = '\0';
        }
        return true;
    }
    return false;
}

bool CConfigFile::GetBool(const char* key, bool defaultValue) {
    char* value = FindValue(key);
    char buf[5];

    if (value != NULL) {
        strncpy(buf, value, 4);
        buf[4] = '\0';
        return stricmp(buf, "true") == 0;
    }
    return defaultValue;
}

bool CConfigFile::SetSection(const char* name) {
    for (int i = 0; i < m_numSections; i++) {
        if (stricmp(m_sections[i].name, name) == 0) {
            m_currentSection = i;
            return true;
        }
    }
    m_currentSection = -1;
    return false;
}

CConfigFile::~CConfigFile() {
    if (m_data != NULL) {
        TLT_CloseFile(m_data);
    }
}

CConfigFile::CConfigFile(const char* path) {
    int size;
    char* p;
    char* name;
    char* close;

    m_numSections = 0;
    m_currentSection = -1;
    m_data = NULL;
    m_data = (char*)TLT_LoadFileNormal(path, &size, 0);
    if (m_data == NULL || size <= 0) {
        return;
    }
    m_data[size - 1] = '\n';

    p = strchr(m_data, '[');
    while (p != NULL) {
        name = p + 1;
        close = strchr(name, ']');
        if (close == NULL) {
            DebugMsg("CConfigFile::CConfigFile() -- Section name %s doesn't terminate ( ])", name);
            return;
        }
        m_sections[m_numSections].name = name;
        *close = '\0';
        m_sections[m_numSections].start = close + 1;
        p = strchr(close + 1, '[');
        m_sections[m_numSections].end = p;
        if (p != NULL) {
            m_sections[m_numSections].end--;
        } else {
            m_sections[m_numSections].end = &m_data[size];
        }
        m_numSections++;
    }
}

CConfigSection::CConfigSection() {
    name = NULL;
    end = NULL;
}
