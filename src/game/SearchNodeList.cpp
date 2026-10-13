#include "game/SearchNodeList.h"

#include "game/SearchNode.h"

CSearchNodeList::CSearchNodeList() {}

CSearchNodeList::~CSearchNodeList() {}

void CSearchNodeQueue::Insert(CSearchNode* node) {
    int i = m_count++;
    int parent = i >> 1;
    while (i > 1 && node->m_totalCost < Slot(parent)->m_totalCost) {
        Slot(i) = Slot(parent);
        Slot(i)->HeapIndex() = i;
        i = parent;
        parent >>= 1;
    }
    Slot(i) = node;
    node->HeapIndex() = i;
}

// Move `node` down from slot i to restore the heap below it.
static inline int SiftDown(CSearchNodeQueue* queue, CSearchNode* node, int i) {
    int child;
    while ((child = i * 2) < queue->m_count) {
        if (child + 1 < queue->m_count &&
            queue->Slot(child + 1)->m_totalCost < queue->Slot(child)->m_totalCost) {
            child++;
        }
        if (node->m_totalCost < queue->Slot(child)->m_totalCost) {
            break;
        }
        queue->Slot(i) = queue->Slot(child);
        queue->Slot(i)->HeapIndex() = i;
        i = child;
    }
    return i;
}

// Remove oldNode and insert newNode. The last element is only sifted down
// into oldNode's slot, never up (as in the original).
void CSearchNodeQueue::ReplaceElement(CSearchNode* oldNode, CSearchNode* newNode) {
    int i = oldNode->HeapIndex();
    CSearchNode* last = Slot(--m_count);
    i = SiftDown(this, last, i);
    Slot(i) = last;
    last->HeapIndex() = i;
    Insert(newNode);
    oldNode->HeapIndex() = 0;
}

CSearchNode* CSearchNodeQueue::PopFirst() {
    if (m_count <= 1) {
        return NULL;
    }
    CSearchNode* first = Slot(1);
    CSearchNode* last = Slot(--m_count);
    int i = SiftDown(this, last, 1);
    Slot(i) = last;
    last->HeapIndex() = i;
    first->HeapIndex() = 0;
    return first;
}

void CSearchNodeQueue::Clear() {
    for (int i = 1; i < m_count; i++) {
        Slot(i)->HeapIndex() = 0;
        delete Slot(i);
    }
    m_count = 1;
}
