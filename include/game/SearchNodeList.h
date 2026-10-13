// Containers for the A* path search. Hand-maintained.
#ifndef _GAME_SEARCHNODELIST_H_
#define _GAME_SEARCHNODELIST_H_

#include "types.h"
#include "game/LinkedList.h"

class CSearchNode;

// Binary min-heap of search nodes keyed on m_totalCost. The heap is 1-based:
// slot i is the word at offset 4*i, so slot 0 is m_count, which is one past
// the last used slot (1 when empty). Each node records its slot in
// m_heapIndex.
class CSearchNodeQueue {
public:
    /* 0x0 */ int m_count;
    /* 0x4 */ CSearchNode* m_heap[1];  // actual capacity set by the owner

    CSearchNode*& Slot(int i) { return ((CSearchNode**)this)[i]; }

    void Insert(CSearchNode* node);  // 0x80056644
    void ReplaceElement(CSearchNode* oldNode, CSearchNode* newNode);  // 0x8005654C
    CSearchNode* PopFirst();  // 0x80056494
    void Clear();  // 0x80056414
};

class CSearchNodeList : public CLinkedList {
public:
    CSearchNodeList();  // 0x800566F8
    ~CSearchNodeList();  // 0x800566A4
};

#endif
