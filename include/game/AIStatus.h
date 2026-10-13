// CAIStatus: a set of AI status variables and matching of condition lists
// against them. Hand-maintained.
#ifndef _GAME_AISTATUS_H_
#define _GAME_AISTATUS_H_

#include "types.h"
#include "game_types.h"

class CAIObject;

// One condition: (value of `var`) `op` `value`.
struct aistatus_match {
    /* 0x0 */ int var;    // aistatus_var
    /* 0x4 */ int value;
    /* 0x8 */ int op;     // 0 ==, 1 !=, 2 <, 3 >
};

inline void SetStatusMatch(aistatus_match* m, int var, int value, int op) {
    m->var = var;
    m->value = value;
    m->op = op;
}

// Status variables (aistatus_var). The stored ones live in CAIStatus::m_vars;
// the computed ones describe s_paioAttacker relative to s_paioTarget.
enum {
    AISTATUS_HEALTH = 0,      // 1..15 (0 = dead)
    AISTATUS_VAR1 = 1,        // CAIObject::field_0x15
    AISTATUS_VAR2 = 2,        // CAIObject::field_0x214
    AISTATUS_THREAT = 3,      // own threat level (4 for soldiers, tanks and the player)
    AISTATUS_VAR4 = 4,
    AISTATUS_TEAM = 5,
    AISTATUS_VISIBILITY = 6,  // computed (always 0)
    AISTATUS_DISTANCE = 7,    // computed: XY distance / 10, up to 15
    AISTATUS_ATTACKERS = 8,   // how many AIs target this one, up to 15
    AISTATUS_THREAT_VALUE = 9,  // computed
    AISTATUS_LOS = 10,        // computed: 15 if in line of sight
    AISTATUS_EXP_DISTANCE = 11,  // computed: sqrt-scaled distance 0..15
    AISTATUS_IS_ENEMY = 12,   // computed: 15 if on an enemy team
    AISTATUS_DEAD = 13,         // 15 once dead (scripts set it)
    AISTATUS_NUM_VARS = 14
};

// The variables are stored as an int array starting at the object.
class CAIStatus {
public:
    /* 0x00 */ int m_vars[AISTATUS_NUM_VARS];

    CAIStatus();  // 0x80051E88
    ~CAIStatus();  // 0x80051E4C
    // True when every condition in the list holds.
    bool MatchStatusList(aistatus_match* list, int count);  // 0x80051CD0
    int GetVarValue(aistatus_var) const;  // 0x8004960C

    // Pair whose relation the computed variables (6, 7, 9-12) describe.
    static CAIObject* s_paioAttacker;  // 0x8034E8F0
    static CAIObject* s_paioTarget;  // 0x8034E8F4
};

#endif
