// MemoryCardGC: the GameCube memory card wrapper used by the shell (save
// games and the preferences file).
#include "game/MemoryCardGC.h"
#include "game/Moh2.h"
#include "dolphin/card.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

const char* GAME_CODENAME = "GMFE";
const char* COMPANY_CODENAME = "69";
const char* COMPANY_DEBUGNAME = "CD";
const char* GAME_DEBUGNAME = "DEBG";

static bool SysCardInit;

#define SAVE_PREFIX "MOHFL"
#define SAVE_NAME_FORMAT "MOHFLx%03d"
#define SAVE_COMMENT "Save: "
#define SAVE_NAME_LENGTH 10

// A file's first sector: comment (two 32-byte lines), then the icon (C8
// texture and palette).
enum {
    HEADER_SIZE = 0x640,
    HEADER_ICON = 0x40,
    HEADER_ICON_CLUT = 0x440,
};

// The file icon (32x32 C8, RGB5A3 palette).
// 0x200 bytes
static const u8 ICON_CLUT_DATA[0x200] __attribute__((aligned(4))) = {
#include "game/MemoryCardIconClut.inc"
};
// 0x400 bytes
static const u8 ICON_TEXTURE_DATA[0x400] __attribute__((aligned(4))) = {
#include "game/MemoryCardIcon.inc"
};

// Ours: game "GMFE" by "69" (or the debug codes).
static inline bool IsOurFile(const CARDStat& stat) {
    return (strncmp((const char*)stat.gameName, GAME_CODENAME, 4) == 0 ||
            strncmp((const char*)stat.gameName, GAME_DEBUGNAME, 4) == 0) &&
           (strncmp((const char*)stat.company, COMPANY_CODENAME, 2) == 0 ||
            strncmp((const char*)stat.company, COMPANY_DEBUGNAME, 2) == 0);
}

// Header sector settings: icon at 0x40, no banner, comment at 0.
static inline void SetIconStatus(CARDStat& stat) {
    CARDSetIconSpeed(&stat, 0, CARD_STAT_SPEED_SLOW);
    CARDSetBannerFormat(&stat, CARD_STAT_BANNER_NONE);
    CARDSetCommentAddress(&stat, 0);
    CARDSetIconAddress(&stat, HEADER_ICON);
    CARDSetIconAnim(&stat, CARD_STAT_ANIM_LOOP);
    CARDSetIconFormat(&stat, 0, CARD_STAT_ICON_C8);
    CARDSetIconSpeed(&stat, 1, CARD_STAT_SPEED_END);
}

static inline void Unmount(CMemoryCard* card, int port) {
    CARDUnmount(port);
    card->m_mounted[port] = 0;
}

// Waits for the card's async operation, servicing disc errors meanwhile.
static inline s32 WaitForCard(int port) {
    s32 result;
    while ((result = CARDGetResultCode(port)) == CARD_RESULT_BUSY) {
        MohDVDErrorTask(0, 0);
    }
    return result;
}

// Closes the open file and unmounts.
static inline bool CloseFile(CMemoryCard* card) {
    if (card->m_mounted[card->m_port]) {
        s32 result = CARDClose(&card->m_file);
        Unmount(card, card->m_port);
        if (result != 0) {
            DebugMsg("File failed to close with error %d\n", result);
            return false;
        }
        DebugMsg("File closed successfuly\n");
    }
    return true;
}

// Whether there is a card in `port` (unmounts when there is none or it
// is unusable).
static inline bool Probe(CMemoryCard* card, int port) {
    s32 result;
    do {
        result = CARDProbe(port);
    } while (result == CARD_RESULT_BUSY);
    if (result == 0) {
        if (card->m_mounted[port]) {
            CARDUnmount(port);
        }
        card->m_mounted[port] = 0;
        return false;
    }
    do {
        result = CARDProbeEx(port, NULL, NULL);
    } while (result == CARD_RESULT_BUSY);
    if (result < 0) {
        if (card->m_mounted[port]) {
            CARDUnmount(port);
        }
        card->m_mounted[port] = 0;
    }
    return true;
}

int CMemoryCard::GetPort() {
    return m_port;
}

int CMemoryCard::GetSaveGameOverheadSize(bool) {
    return 2;
}

// Leaves the card mounted when the file is found.
bool CMemoryCard::ArePrefsPresent() {
    Unmount(this, m_port);
    if (MountPortIfRequired(m_port) != 1) {
        return false;
    }
    char name[0x20];
    sprintf(name, "%sprefs", SAVE_PREFIX);
    for (s32 i = 0; i < CARD_MAX_FILE; i++) {
        CARDFileInfo file;
        if (CARDFastOpen(m_port, i, &file) == 0) {
            CARDStat stat;
            if (CARDGetStatus(m_port, file.fileNo, &stat) == 0 && strstr(stat.fileName, name) != NULL &&
                IsOurFile(stat)) {
                CARDClose(&file);
                return true;
            }
            CARDClose(&file);
        }
    }
    return false;
}

bool CMemoryCard::LoadPrefs(unsigned char* data, unsigned int size) {
    u8 buffer[0x2000] __attribute__((aligned(32)));
    int mounted = MountPortIfRequired(m_port);
    if (mounted != 1) {
        DebugMsg("Memory card isn't ready\n", mounted);
        return false;
    }
    char name[0x20];
    sprintf(name, "%sprefs", SAVE_PREFIX);
    s32 result = CARDOpen(m_port, name, &m_file);
    if (result != 0) {
        DebugMsg("No pref file found: %d\n", result);
        return false;
    }
    u32 sectorSize;
    result = CARDGetSectorSize(m_port, &sectorSize);
    if (result < 0) {
        DebugMsg("CARDGetSectorSize() failed. (%d)\n", result);
        return false;
    }
    result = CARDReadAsync(&m_file, buffer, sectorSize, sectorSize, NULL);
    if (result < 0) {
        DebugMsg("Data read failed with error code %d\n", result);
        CloseFile(this);
        return false;
    }
    do {
        result = CARDGetResultCode(m_port);
    } while (result == CARD_RESULT_BUSY);
    if (result < 0) {
        DebugMsg("Data write failed with error code %d\n", result);
        CloseFile(this);
        return false;
    }
    CloseFile(this);
    memcpy(data, buffer, size);
    return true;
}

bool CMemoryCard::SavePrefs(unsigned char* data, unsigned int size) {
    u8 buffer[0x2000] __attribute__((aligned(32)));
    if (MountPortIfRequired(m_port) != 1) {
        return false;
    }
    char name[0x20];
    sprintf(name, "%sprefs", SAVE_PREFIX);
    s32 result = CARDOpen(m_port, name, &m_file);
    if (result != 0) {
        DebugMsg("No pref file found\n", result);
        return false;
    }
    u32 sectorSize;
    result = CARDGetSectorSize(m_port, &sectorSize);
    if (result < 0) {
        DebugMsg("CARDGetSectorSize() failed. (%d)\n", result);
        return false;
    }
    memcpy(buffer, data, size);
    result = CARDWriteAsync(&m_file, buffer, sectorSize, sectorSize, NULL);
    if (result < 0) {
        DebugMsg("Data write failed with error code %d\n", result);
        CloseFile(this);
        return false;
    }
    do {
        result = CARDGetResultCode(m_port);
    } while (result == CARD_RESULT_BUSY);
    if (result < 0) {
        DebugMsg("Data write failed with error code %d\n", result);
        CloseFile(this);
        return false;
    }
    CloseFile(this);
    return true;
}

// Writes `stat.length` bytes from a sector-sized buffer (as the original;
// the second sector gets whatever follows).
bool CMemoryCard::CreatePrefs() {
    static u8 HeaderC8[HEADER_SIZE] = "Medal Of Honor Frontline\n\0\0\0\0\0\0\0Preference File\n";
    u8 buffer[0x2000] __attribute__((aligned(32)));
    if (MountPortIfRequired(m_port) != 1) {
        return false;
    }
    char name[0x20];
    sprintf(name, "%sprefs", SAVE_PREFIX);
    u32 sectorSize;
    s32 result = CARDGetSectorSize(m_port, &sectorSize);
    if (result < 0) {
        DebugMsg("CARDGetSectorSize() failed on error code %d\n", result);
        return false;
    }
    result = CARDCreateAsync(m_port, name, sectorSize * 2, &m_file, NULL);
    if (result < 0) {
        DebugMsg("Failed prefs file create with error code %d\n", result);
        return false;
    }
    while (CARDGetResultCode(m_port) == CARD_RESULT_BUSY) {
    }
    CARDStat stat;
    result = CARDGetStatus(m_port, m_file.fileNo, &stat);
    if (result < 0) {
        DebugMsg("CARDGetStatus() failed on error code %d\n", result);
        return false;
    }
    CARDSetAttributes(m_port, m_file.fileNo, CARD_ATTR_PUBLIC | CARD_ATTR_NO_COPY | CARD_ATTR_NO_MOVE);
    memcpy(HeaderC8 + HEADER_ICON, ICON_TEXTURE_DATA, sizeof(ICON_TEXTURE_DATA));
    memcpy(HeaderC8 + HEADER_ICON_CLUT, ICON_CLUT_DATA, sizeof(ICON_CLUT_DATA));
    SetIconStatus(stat);
    memcpy(buffer, HeaderC8, HEADER_SIZE);
    result = CARDWrite(&m_file, buffer, stat.length, 0);
    if (result < 0) {
        DebugMsg("Failed to CARDWrite in CreateNewSaveGame.\n", result);
    }
    strncpy((char*)stat.company, COMPANY_CODENAME, 2);
    strncpy((char*)stat.gameName, GAME_CODENAME, 4);
    result = CARDSetStatus(m_port, m_file.fileNo, &stat);
    if (result < 0) {
        DebugMsg("Failed to set CARDStat in CreateNewSaveGame\n", result);
    }
    return true;
}

// Writes the open save's data sector.
bool CMemoryCard::Write(unsigned char* data, unsigned int size) {
    u8 buffer[0x2000] __attribute__((aligned(32)));
    if (MountPortIfRequired(m_port) != 1) {
        return false;
    }
    u32 sectorSize;
    s32 result = CARDGetSectorSize(m_port, &sectorSize);
    if (result < 0) {
        DebugMsg("CARDGetSectorSize() failed. (%d)\n", result);
        return false;
    }
    memcpy(buffer, data, size);
    result = CARDWriteAsync(&m_file, buffer, sectorSize, sectorSize, NULL);
    if (result < 0) {
        DebugMsg("Data write failed with error code %d\n", result);
        return false;
    }
    do {
        result = CARDGetResultCode(m_port);
    } while (result == CARD_RESULT_BUSY);
    if (result < 0) {
        DebugMsg("Data write failed with error code %d\n", result);
        return false;
    }
    return true;
}

// Reads the open save's data sector.
bool CMemoryCard::Read(unsigned char* data, unsigned int size) {
    u8 buffer[0x2000] __attribute__((aligned(32)));
    if (MountPortIfRequired(m_port) != 1) {
        return false;
    }
    s32 result = CARDGetSectorSize(m_port, &m_sectorSize);
    if (result < 0) {
        DebugMsg("CARDGetSectorSize() failed. (%d)\n", result);
        return false;
    }
    result = CARDReadAsync(&m_file, buffer, m_sectorSize, m_sectorSize, NULL);
    if (result < 0) {
        DebugMsg("Data read failed with error code %d\n", result);
        return false;
    }
    do {
        result = CARDGetResultCode(m_port);
    } while (result == CARD_RESULT_BUSY);
    if (result < 0) {
        DebugMsg("Data read failed with error code %d\n", result);
        return false;
    }
    memcpy(data, buffer, size);
    return true;
}

bool CMemoryCard::Close() {
    if (!m_mounted[m_port]) {
        return true;
    }
    return CloseFile(this);
}

// Opens save `slot` and keeps its comment line (for the corrupt-file
// dialog).
bool CMemoryCard::Open(unsigned int slot, bool) {
    u8 buffer[0x2000] __attribute__((aligned(32)));
    if (MountPortIfRequired(m_port) != 1) {
        return false;
    }
    char name[0x20];
    sprintf(name, SAVE_NAME_FORMAT, slot);
    if (CARDOpen(m_port, name, &m_file) != 0) {
        return false;
    }
    CARDStat stat;
    s32 result = CARDGetStatus(m_port, m_file.fileNo, &stat);
    if (result < 0) {
        DebugMsg("Failed to set CARDStat in CreateNewSaveGame\n", result);
    }
    result = CARDReadAsync(&m_file, buffer, m_sectorSize, 0, NULL);
    if (result < 0) {
        DebugMsg("Data read failed with error code %d\n", result);
        return false;
    }
    do {
        result = CARDGetResultCode(m_port);
    } while (result == CARD_RESULT_BUSY);
    if (result < 0) {
        DebugMsg("Data read failed with error code %d\n", result);
        return false;
    }
    strncpy(m_corruptFileName, (char*)buffer + 0x20, 0x20);
    return true;
}

// Deletes the preferences file on the current port (unmounting `port`).
bool CMemoryCard::DeletePrefFile(int port) {
    if (MountPortIfRequired(port) != 1) {
        return false;
    }
    char name[0x20];
    sprintf(name, "%sprefs", SAVE_PREFIX);
    s32 result = CARDDelete(m_port, name);
    Unmount(this, port);
    if (result != 0) {
        DebugMsg("Failed to delete file\n");
        return false;
    }
    return true;
}

bool CMemoryCard::DeleteSaveGame(unsigned int slot) {
    if (MountPortIfRequired(m_port) != 1) {
        return false;
    }
    char name[0x20];
    sprintf(name, SAVE_NAME_FORMAT, slot);
    s32 result = CARDDelete(m_port, name);
    Unmount(this, m_port);
    if (result != 0) {
        DebugMsg("Failed to delete file\n");
        return false;
    }
    return true;
}

bool CMemoryCard::IsSaveGamePresent(int slot) {
    if (MountPortIfRequired(m_port) != 1) {
        return false;
    }
    char name[0x20];
    sprintf(name, SAVE_NAME_FORMAT, slot);
    if (CARDOpen(m_port, name, &m_file) != 0) {
        return false;
    }
    CloseFile(this);
    return true;
}

// The first save number not in use.
static inline u16 FirstFreeSave(CMemoryCard* card) {
    s16 i = 0;
    while (card->m_saveUsed[i] && i < CARD_MAX_FILE) {
        i++;
    }
    return i;
}

int CMemoryCard::GetNextSaveGame(unsigned int* slot, unsigned int* sectorSize, char* name) {
    u8 buffer[0x2000] __attribute__((aligned(32)));
    if (MountPortIfRequired(m_port) != 1) {
        return 0;
    }
    // Next of our save files.
    while (!m_isSaveFile[m_listIndex]) {
        if (m_listIndex >= CARD_MAX_FILE - 1) {
            u16 free = FirstFreeSave(this);
            *slot = free;
            m_freeSave = free;
            return 0;
        }
        m_listIndex++;
    }
    if (m_listIndex >= CARD_MAX_FILE) {
        *slot = m_listIndex;
        m_freeSave = m_listIndex;
        return 0;
    }
    if (CARDFastOpen(m_port, m_listIndex, &m_file) != 0) {
        return 2;
    }
    *sectorSize = m_sectorSize;
    CARDStat stat;
    s32 result = CARDGetStatus(m_port, m_file.fileNo, &stat);
    if (result < 0) {
        DebugMsg("CARDGetStatus() failed on error code %d\n", result);
        return 2;
    }
    result = CARDReadAsync(&m_file, buffer, m_sectorSize, 0, NULL);
    if (result < 0) {
        DebugMsg("Data read failed with error code %d\n", result);
        return 2;
    }
    do {
        result = CARDGetResultCode(m_port);
    } while (result == CARD_RESULT_BUSY);
    if (result < 0) {
        DebugMsg("Data read failed with error code %d\n", result);
        return 2;
    }

    // The second comment line: "Save: <name>\n".
    char comment[0x20];
    strncpy(comment, (char*)buffer + 0x20, 0x20);
    bool corrupt = false;
    u32 length = 0;
    for (char* p = comment; *p != '\0' && *p != '\n'; p++, length++) {
        if (length > strlen(SAVE_COMMENT) + SAVE_NAME_LENGTH) {
            corrupt = true;
            strncpy(m_corruptFileName, comment, 0x20);
            sprintf(name, "corrupt");
            break;
        }
    }
    if (length == 0) {
        return 2;
    }
    // (This overwrites "corrupt", as the original.)
    strncpy(name, comment + strlen(SAVE_COMMENT), SAVE_NAME_LENGTH);
    name[SAVE_NAME_LENGTH] = '\0';
    *slot = atoi(stat.fileName + strlen(SAVE_PREFIX) + 1);
    if (*slot > CARD_MAX_FILE) {
        return 2;
    }
    m_saveUsed[*slot] = 1;
    CloseFile(this);
    m_listIndex++;
    return corrupt ? 2 : 1;
}

void CMemoryCard::RewindSaveGameList() {
    m_listIndex = 0;
    GetNumSaveGames();
}

bool CMemoryCard::Format() {
    CARDMountAsync(m_port, m_workArea, NULL, NULL);
    WaitForCard(m_port);
    s32 result = CARDFormatAsync(m_port, NULL);
    if (result < 0) {
        DebugMsg("Format failed with error %d\n", result);
        Unmount(this, m_port);
        return false;
    }
    result = WaitForCard(m_port);
    if (result < 0) {
        DebugMsg("Format failed with error %d\n", result);
        Unmount(this, m_port);
        return false;
    }
    Unmount(this, m_port);
    return true;
}

// Creates the file for the first free save number; writes its header
// (comment with `name`, icon).
bool CMemoryCard::CreateNewSaveGame(unsigned int* slot, char* name) {
    static u8 HeaderC8[HEADER_SIZE] = "Medal Of Honor Frontline\n\0\0\0\0\0\0\0Save: \n";
    static u8 CARDData[0x2000] __attribute__((aligned(32)));
    if (MountPortIfRequired(m_port) != 1) {
        return false;
    }
    m_freeSave = FirstFreeSave(this);
    *slot = m_freeSave;
    char fileName[0x24];
    sprintf(fileName, SAVE_NAME_FORMAT, *slot);
    u32 sectorSize;
    s32 result = CARDGetSectorSize(m_port, &sectorSize);
    if (result < 0) {
        DebugMsg("CARDGetSectorSize() failed on error code %d\n", result);
        return false;
    }
    DebugMsg("Sector size is %d bytes.\n", sectorSize);
    CARDFileInfo file;
    result = CARDCreateAsync(m_port, fileName, sectorSize * 2, &file, NULL);
    if (result < 0) {
        if (result == CARD_RESULT_EXIST) {
            DebugMsg("Failed to create file because file already exists!\n");
            if (CARDDelete(m_port, fileName) != 0) {
                DebugMsg("Failed to delete file\n");
                return false;
            }
            CARDCreateAsync(m_port, fileName, sectorSize * 2, &file, NULL);
            do {
                result = CARDGetResultCode(m_port);
            } while (result == CARD_RESULT_BUSY);
        }
        if (result < 0) {
            DebugMsg("Failed file create with error code %d\n", result);
            return false;
        }
    }
    WaitForCard(m_port);
    CARDStat stat;
    result = CARDGetStatus(m_port, file.fileNo, &stat);
    if (result < 0) {
        DebugMsg("CARDGetStatus() failed on error code %d\n", result);
        return false;
    }
    CARDSetAttributes(m_port, file.fileNo, CARD_ATTR_PUBLIC | CARD_ATTR_NO_COPY | CARD_ATTR_NO_MOVE);
    strncpy((char*)HeaderC8 + 0x20 + strlen(SAVE_COMMENT), name, SAVE_NAME_LENGTH);
    SetIconStatus(stat);
    strncpy((char*)stat.company, COMPANY_CODENAME, 2);
    strncpy((char*)stat.gameName, GAME_CODENAME, 4);
    if (CARDSetStatus(m_port, file.fileNo, &stat) < 0) {
        DebugMsg("Failed to set CARDStat in CreateNewSaveGame\n");
        return false;
    }
    memcpy(HeaderC8 + HEADER_ICON, ICON_TEXTURE_DATA, sizeof(ICON_TEXTURE_DATA));
    memcpy(HeaderC8 + HEADER_ICON_CLUT, ICON_CLUT_DATA, sizeof(ICON_CLUT_DATA));
    memcpy(CARDData, HeaderC8, HEADER_SIZE);
    // Writes the file's whole length from the sector buffer (as the original).
    CARDWriteAsync(&file, CARDData, stat.length, 0, NULL);
    if (WaitForCard(m_port) < 0) {
        DebugMsg("Failed to CARDWrite in CreateNewSaveGame.\n");
    }
    DebugMsg("gameName %.4s\n", stat.gameName);
    DebugMsg("company %.2s\n", stat.company);
    DebugMsg("length %d\n", stat.length);
    DebugMsg("fileName %.32s\n", stat.fileName);
    DebugMsg("time %d\n", stat.time);
    return true;
}

// Counts our save files (not the preferences) and marks them in
// m_isSaveFile; unmounts.
int CMemoryCard::GetNumSaveGames() {
    for (int i = 0; i < CARD_MAX_FILE; i++) {
        m_isSaveFile[i] = 0;
        m_saveUsed[i] = 0;
    }
    if (MountPortIfRequired(m_port) != 1) {
        return 0;
    }
    int count = 0;
    for (s32 i = 0; i < CARD_MAX_FILE; i++) {
        CARDFileInfo file;
        if (CARDFastOpen(m_port, i, &file) == 0) {
            CARDStat stat;
            if (CARDGetStatus(m_port, file.fileNo, &stat) == 0 && strstr(stat.fileName, SAVE_PREFIX) != NULL &&
                IsOurFile(stat) && strstr(stat.fileName, "prefs") == NULL) {
                m_isSaveFile[i] = 1;
                count++;
            }
            CARDClose(&file);
        }
    }
    Unmount(this, m_port);
    return count;
}

int CMemoryCard::GetCheckCardStatus() {
    CARDCheckAsync(m_port, NULL);
    return WaitForCard(m_port);
}

u32 CMemoryCard::GetFilesFree(int port) {
    if (MountPortIfRequired(port) != 1) {
        return 0;
    }
    s32 bytes;
    s32 files;
    s32 result = CARDFreeBlocks(port, &bytes, &files);
    if (result < 0) {
        DebugMsg("Free Blocks failed. (%d)\n", result);
        Unmount(this, port);
        return 0;
    }
    return files;
}

// Free blocks.
int CMemoryCard::GetAvailableMemory() {
    if (MountPortIfRequired(m_port) != 1) {
        return 0;
    }
    u32 sectorSize;
    s32 result = CARDGetSectorSize(m_port, &sectorSize);
    if (result < 0) {
        DebugMsg("CARDGetSectorSize() failed. (%d)\n", result);
        Unmount(this, m_port);
        return 0;
    }
    DebugMsg("Sector size %u bytes.\n", sectorSize);
    m_sectorSize = sectorSize;
    s32 bytes;
    s32 files;
    result = CARDFreeBlocks(m_port, &bytes, &files);
    if (result < 0) {
        DebugMsg("Free Blocks failed. (%d)\n", result);
        Unmount(this, m_port);
        return 0;
    }
    DebugMsg("%d bytes (%d blocks) free.\n%d files free.\n", bytes, (u32)bytes / sectorSize, files);
    return (u32)bytes / sectorSize;
}

int CMemoryCard::IsFormated() {
    if (m_mounted[m_port]) {
        Unmount(this, m_port);
    }
    DebugMsg("Attempting to mount a card into slot %d\n", m_port);
    int mounted = MountPortIfRequired(m_port);
    if (mounted != 1) {
        return mounted;
    }
    s32 result = CARDGetSectorSize(m_port, &m_sectorSize);
    if (result < 0) {
        DebugMsg("CARDGetSectorSize() failed on error code %d\n", result);
        return 0;
    }
    return 1;
}

// Blocks on the card, minus the 5 system blocks.
int CMemoryCard::GetMaxBlocksOnCard(int port) {
    s32 memSize = 0;
    s32 sectorSize = 0x2000;
    while (CARDProbeEx(port, &memSize, &sectorSize) == CARD_RESULT_BUSY) {
    }
    return (memSize << 17) / sectorSize - 5;
}

int CMemoryCard::MountPortIfRequired(int port) {
    if (!Probe(this, port)) {
        return 0;
    }
    if (m_mounted[port]) {
        return 1;
    }
    s32 result = CARDMountAsync(port, m_workArea, NULL, NULL);
    if (result < 0) {
        if (result == CARD_RESULT_IOERROR || result == CARD_RESULT_FATAL_ERROR) {
            return 3;
        }
        DebugMsg("Mount failed. Error code %d returned\n", result);
        return 0;
    }
    do {
        result = CARDGetResultCode(port);
    } while (result == CARD_RESULT_BUSY);
    switch (result) {
    case CARD_RESULT_READY:
    case CARD_RESULT_BROKEN:
        break;
    case CARD_RESULT_IOERROR:
    case CARD_RESULT_FATAL_ERROR:
        DebugMsg("Card mount failure. Error code %d returned\n", result);
        return 3;
    case CARD_RESULT_ENCODING:
        DebugMsg("Card mount failure. Error code %d returned\n", result);
        Unmount(this, port);
        return 0;
    default:
        DebugMsg("Card mount failure. Error code %d returned\n", result);
        return 0;
    }
    // Mounted (possibly broken): check it.
    CARDCheckAsync(port, NULL);
    result = WaitForCard(port);
    if (result != CARD_RESULT_READY) {
        DebugMsg("CARDCheck failed. Error code %d returned\n", result);
        Unmount(this, port);
        if (result == CARD_RESULT_BROKEN) {
            return 2;
        }
        if (result == CARD_RESULT_IOERROR || result == CARD_RESULT_FATAL_ERROR) {
            return 3;
        }
        return 0;
    }
    m_mounted[port] = 1;
    return 1;
}

// An 8 KB-sector card (or none usable).
bool CMemoryCard::Is8KCard(int port) {
    s32 memSize;
    s32 sectorSize;
    s32 result;
    do {
        result = CARDProbeEx(port, &memSize, &sectorSize);
    } while (result == CARD_RESULT_BUSY);
    if (result < 0) {
        if (m_mounted[port]) {
            CARDUnmount(port);
        }
        m_mounted[port] = 0;
        return true;
    }
    return sectorSize == 0x2000;
}

bool CMemoryCard::IsAnyDeviceInPort(int port) {
    s32 result;
    do {
        result = CARDProbe(port);
    } while (result == CARD_RESULT_BUSY);
    if (result == 0) {
        if (m_mounted[port]) {
            CARDUnmount(port);
        }
        m_mounted[port] = 0;
        return false;
    }
    return true;
}

// False for a card from another region.
bool CMemoryCard::IsEncodingCorrect(int port) {
    if (m_mounted[port]) {
        Unmount(this, port);
    }
    s32 result = CARDMountAsync(port, m_workArea, NULL, NULL);
    if (result < 0) {
        DebugMsg("Mount failed. Error code %d returned\n", result);
        return true;
    }
    do {
        result = CARDGetResultCode(port);
    } while (result == CARD_RESULT_BUSY);
    Unmount(this, port);
    return result != CARD_RESULT_ENCODING;
}

// Selects `port`; false when there is no usable card.
bool CMemoryCard::OpenSlot(int port) {
    m_port = port;
    s32 result;
    do {
        result = CARDProbe(port);
    } while (result == CARD_RESULT_BUSY);
    if (result == 0) {
        if (m_mounted[port]) {
            CARDUnmount(port);
        }
        m_mounted[port] = 0;
        return false;
    }
    do {
        result = CARDProbeEx(port, NULL, NULL);
    } while (result == CARD_RESULT_BUSY);
    if (result < 0) {
        if (m_mounted[port]) {
            CARDUnmount(port);
        }
        m_mounted[port] = 0;
    }
    return result != CARD_RESULT_WRONGDEVICE;
}

bool CMemoryCard::QuerySlot(int port) {
    return Probe(this, port);
}

void CMemoryCard::Initialize() {}

CMemoryCard::~CMemoryCard() {}

CMemoryCard::CMemoryCard() {
    if (!SysCardInit) {
        CARDInit();
        SysCardInit = true;
    }
    m_mounted[0] = 0;
    m_mounted[1] = 0;
    memset(m_corruptFileName, 0, sizeof(m_corruptFileName));
}
