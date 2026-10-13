#include "game/LinkedList.h"

CLinkedListElement::CLinkedListElement() {
    m_pNext = NULL;
}

CLinkedListElement::~CLinkedListElement() {}

CLinkedList::CLinkedList() {
    m_pHead = NULL;
    m_pTail = NULL;
    m_count = 0;
}

CLinkedList::~CLinkedList() {}

// Note: does not clear elem->m_pNext when the list is empty.
void CLinkedList::AddElementAtBeginning(CLinkedListElement* elem) {
    if (m_pHead != NULL) {
        elem->m_pNext = m_pHead;
    } else {
        m_pTail = elem;
    }
    m_pHead = elem;
    m_count++;
}

void CLinkedList::AddElementAtEnd(CLinkedListElement* elem) {
    CLinkedListElement* tail = m_pTail;
    if (tail != NULL) {
        elem->m_pNext = tail->m_pNext;
        tail->m_pNext = elem;
    } else {
        m_pHead = elem;
    }
    m_pTail = elem;
    m_count++;
}

void CLinkedList::RemoveElement(CLinkedListElement* elem) {
    if (elem == m_pHead) {
        if (elem->m_pNext != NULL) {
            m_pHead = elem->m_pNext;
            elem->m_pNext = NULL;
        } else {
            m_pHead = NULL;
            m_pTail = NULL;
        }
        m_count--;
        return;
    }
    for (CLinkedListElement* prev = m_pHead; prev != NULL; prev = prev->m_pNext) {
        if (prev->m_pNext == elem) {
            if (elem->m_pNext != NULL) {
                prev->m_pNext = elem->m_pNext;
                elem->m_pNext = NULL;
            } else {
                prev->m_pNext = NULL;
                m_pTail = prev;
            }
            m_count--;
            return;
        }
    }
}

bool CLinkedList::IsEmpty() {
    return m_pHead == NULL;
}
