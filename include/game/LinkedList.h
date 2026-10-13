// Intrusive singly linked list. Hand-maintained.
#ifndef _GAME_LINKEDLIST_H_
#define _GAME_LINKEDLIST_H_

#include "types.h"

// size 0x4; derive from this to put an object in a CLinkedList
class CLinkedListElement {
public:
    /* 0x0 */ CLinkedListElement* m_pNext;
    CLinkedListElement();  // 0x8005372C
    ~CLinkedListElement();  // 0x800536F0
};

// size 0xC
class CLinkedList {
public:
    /* 0x0 */ CLinkedListElement* m_pHead;
    /* 0x4 */ CLinkedListElement* m_pTail;
    /* 0x8 */ int m_count;
    CLinkedList();  // 0x800536DC
    ~CLinkedList();  // 0x800536A0
    void AddElementAtBeginning(CLinkedListElement* elem);  // 0x80053674
    void AddElementAtEnd(CLinkedListElement* elem);  // 0x80053640
    void RemoveElement(CLinkedListElement* elem);  // 0x800535A0
    bool IsEmpty();  // 0x80053590
};

#endif
