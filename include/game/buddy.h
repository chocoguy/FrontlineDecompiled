// Buddies: friendly soldiers that follow the player's progress through a
// level along numbered path groups. Attached buddies have a slot; extra
// ones wait in a ready list until an attached one dies. Hand-maintained.
#ifndef _GAME_BUDDY_H_
#define _GAME_BUDDY_H_

#include "types.h"
#include "game_types.h"

class CSoldierObject;

// size 0x14
struct BuddyInfo {
    /* 0x00 */ CSoldierObject* m_soldier;
    /* 0x04 */ int m_checkpoint;  // the last checkpoint it was sent to
    /* 0x08 */ int m_pathBase;    // its path group; it walks path m_pathBase + checkpoint
    /* 0x0C */ int m_moving;      // has orders it has not finished
    /* 0x10 */ bool m_inUse;
};

void BIFunc_SetBuddyPathGroup(int**, void*);  // 0x80056728
void BIFunc_UpdateBuddyCheckpoints(int**, void*);  // 0x800567E8
void BIFunc_UpdateBuddyProgress(int**, void*);  // 0x80056BD4
void BIFunc_BuddyDied(int**, void*);  // 0x80057164
void BIFunc_GetInitialBuddyOrders(int**, void*);  // 0x80057640
void BIFunc_AddBuddy(int**, void*);  // 0x80057B30
void AddBuddy(BuddyInfo, bool);  // 0x80057EAC
void BIFunc_CreateBuddyDataStructures(int**, void*);  // 0x8005823C
void DestroyBuddyDataStructures();  // 0x80058CCC

#endif
