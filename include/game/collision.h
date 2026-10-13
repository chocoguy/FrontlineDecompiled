// CCollision: collision query state and contact list. Hand-maintained.
#ifndef _GAME_COLLISION_H_
#define _GAME_COLLISION_H_

#include "types.h"
#include "game_types.h"

class CCollision;
class SClsnContact;

class ISceneNode;

// Flags in CCollision::m_flags.
enum {
    CLSN_FLAG_VOLUME_TYPE = 0x30,  // ISceneNode::EVolumeType index (>> 4)
    CLSN_FLAG_SINGLE_TEST = 0x40,  // Test(): one test, no time-of-contact search
    CLSN_FLAG_NEAREST = 0x80,      // line tests: shorten to the nearest hit (m_fraction)
};

// A collision query between two scene nodes (or volumes) and its contacts.
// size 0x20 (CCSGVolume keeps arrays of them)
class CCollision {
public:
    /* 0x0 */ ISceneNode* m_pNode0;
    /* 0x4 */ ISceneNode* m_pNode1;
    /* 0x8 */ f32 m_time;      // time step being tested
    /* 0xC */ f32 m_fraction;  // nearest hit along a line (1 = none)
    /* 0x10 */ u8 m_flags;     // CLSN_FLAG_*
    /* 0x11 */ u8 unk_0x11[0x3];
    /* 0x14 */ s32 m_result;  // worst CLSN_* result seen
    /* 0x18 */ SClsnContact* m_pContacts;  // NULL when nothing was hit
    /* 0x1C */ int m_numContacts;
    void ClearContacts();  // 0x800C74C8
    // Finds the time of first contact between the two nodes by bisecting
    // m_time; `fraction` gets the contact time over the starting step.
    int Test(float& fraction);  // 0x800C7510
    void SwapOrder();  // 0x800C79C4
    void AddContact(const SClsnContact&);  // 0x800C79D8
    void operator=(const CCollision&);  // 0x800C7A58
    ~CCollision();  // 0x800C7B80
    CCollision(const CCollision&);  // 0x800C7C04
    CCollision();  // 0x800C7CF4
};

extern const float CONTACT_EPSILON;  // 0x80350BA8
extern const float CONTACT_EPSILON_SQRD;  // 0x80350BAC

// Collision test results, by separation distance.
enum {
    CLSN_NONE = 1,         // apart by more than CONTACT_EPSILON
    CLSN_TOUCHING = 2,     // within CONTACT_EPSILON
    CLSN_PENETRATING = 3,  // overlapping by more than CONTACT_EPSILON
};

inline int ClassifyContact(float distance) {
    if (distance > CONTACT_EPSILON) {
        return CLSN_NONE;
    } else if (distance < -CONTACT_EPSILON) {
        return CLSN_PENETRATING;
    }
    return CLSN_TOUCHING;
}

#endif
