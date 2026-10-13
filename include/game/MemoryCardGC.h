// MemoryCardGC: the GameCube memory card wrapper. Hand-maintained.
#ifndef _GAME_MEMORYCARDGC_H_
#define _GAME_MEMORYCARDGC_H_

#include "types.h"
#include "game_types.h"
#include "dolphin/card.h"

class CMemoryCard;

// The GameCube memory card: one save game per file ("MOHFLx%03d"), plus
// the preferences file ("MOHFLprefs"). Each file is two sectors: the
// header (comment, icon) and the data. Calls mount the card as needed.
// size 0x14160 (operator new)
class CMemoryCard {
public:
    /* 0x0 */ u8 unk_0x0[0x4];
    /* 0x4 */ s32 m_port;
    /* 0x8 */ CARDFileInfo m_file;
    /* 0x1C */ u32 m_sectorSize;
    /* 0x20 */ u16 m_listIndex;  // file GetNextSaveGame looks at next
    /* 0x22 */ u16 m_freeSave;  // first unused save number
    /* 0x24 */ u8 m_isSaveFile[CARD_MAX_FILE];  // by file number
    /* 0xA3 */ u8 m_saveUsed[CARD_MAX_FILE];  // by save number
    /* 0x122 */ u8 m_mounted[2];  // by port
    /* 0x124 */ u8 unk_0x124[0x1C];
    /* 0x140 */ u8 m_workArea[0x14000];  // CARDMount's, shared by both ports
    /* 0x14140 */ char m_corruptFileName[0x20];
    int GetPort();  // 0x800EE7A4
    int GetSaveGameOverheadSize(bool);  // 0x800EE7AC
    bool ArePrefsPresent();  // 0x800EE7B4
    bool LoadPrefs(unsigned char*, unsigned int);  // 0x800EE914
    bool SavePrefs(unsigned char*, unsigned int);  // 0x800EEB8C
    bool CreatePrefs();  // 0x800EEE0C
    bool Write(unsigned char*, unsigned int);  // 0x800EF040
    bool Read(unsigned char*, unsigned int);  // 0x800EF15C
    bool Close();  // 0x800EF278
    bool Open(unsigned int slot, bool write);  // 0x800EF324
    bool DeletePrefFile(int);  // 0x800EF468
    bool DeleteSaveGame(unsigned int);  // 0x800EF514
    bool IsSaveGamePresent(int);  // 0x800EF5C4
    // Lists the saves one per call: 1 found (slot and name set), 2 corrupt or
    // unreadable, 0 at the end (slot set to the first free save number).
    int GetNextSaveGame(unsigned int* slot, unsigned int* sectorSize, char* name);  // 0x800EF6B8
    void RewindSaveGameList();  // 0x800EF9C4
    bool Format();  // 0x800EF9EC
    bool CreateNewSaveGame(unsigned int* slot, char* name);  // 0x800EFB18
    int GetNumSaveGames();  // 0x800EFEF4
    int GetCheckCardStatus();  // 0x800F00C0
    u32 GetFilesFree(int);  // 0x800F0118
    int GetAvailableMemory();  // 0x800F01AC
    // 0 unformatted, 1 ok, 2/3 broken or foreign encoding.
    int IsFormated();  // 0x800F02C8
    int GetMaxBlocksOnCard(int);  // 0x800F0378
    // 1 mounted, 0 failed, 2 broken (needs formatting), 3 I/O error.
    int MountPortIfRequired(int);  // 0x800F03DC
    bool Is8KCard(int);  // 0x800F0644
    bool IsAnyDeviceInPort(int);  // 0x800F06DC
    bool IsEncodingCorrect(int);  // 0x800F0760
    bool OpenSlot(int);  // 0x800F083C
    bool QuerySlot(int);  // 0x800F0928
    void Initialize();  // 0x800F09F4
    ~CMemoryCard();  // 0x800F09F8
    CMemoryCard();  // 0x800F0A34
};

SIZE_ASSERT(CMemoryCard, 0x14160);

extern const char* GAME_CODENAME;  // 0x8034E308 "GMFE"
extern const char* COMPANY_CODENAME;  // 0x8034E30C "69"
extern const char* COMPANY_DEBUGNAME;  // 0x8034E310
extern const char* GAME_DEBUGNAME;  // 0x8034E314

#endif
