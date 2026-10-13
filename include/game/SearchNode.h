// CSearchNode: one node of an A* search, allocated from a fixed pool.
// Hand-maintained.
#ifndef _GAME_SEARCHNODE_H_
#define _GAME_SEARCHNODE_H_

#include "types.h"
#include "game/LinkedList.h"

struct CPathNode;

// size 0x1C
class CSearchNode : public CLinkedListElement {
public:
    // The base's first word doubles as the node's slot in a CSearchNodeQueue.
    int& HeapIndex() { return *(int*)&m_pNext; }

    /* 0x04 */ CSearchNode* m_pParent;  // also the free-list link while pooled
    /* 0x08 */ CPathNode* m_pPathNode;
    /* 0x0C */ f32 m_costFromStart;
    /* 0x10 */ f32 m_costToGoal;    // straight-line estimate
    /* 0x14 */ f32 m_totalCost;
    /* 0x18 */ bool m_flag;
    /* 0x19 */ u8 unk_0x19[0x3];

    CSearchNode();  // 0x800562A8
    ~CSearchNode();  // 0x80056210
    // Returns the search node that already owned pathNode, or NULL.
    CSearchNode* Setup(CSearchNode* parent, CPathNode* pathNode, const CPathNode* goal);  // 0x800560EC
    float GetCostValue(const CSearchNode* other);  // 0x8005608C
    void* operator new(unsigned long size);  // 0x8005607C
    void operator delete(void* ptr, unsigned long size);  // 0x8005606C
    static void Reset();  // 0x80055FC8
};

#endif
