#include "game/SearchNode.h"

#include "game/AIFilter.h"
#include "game/PathNode.h"

#define SEARCH_NODE_POOL_SIZE 0x1000

static CSearchNode s_rsnAStarSearchNodePool[SEARCH_NODE_POOL_SIZE];
static CSearchNode* s_rsnAStarSearchNodeFreeList;

static inline void BuildFreeList() {
    CSearchNode* prev = NULL;
    for (int i = 0; i < SEARCH_NODE_POOL_SIZE; i++) {
        CSearchNode* node = &s_rsnAStarSearchNodePool[i];
        node->m_pParent = prev;
        node->m_pPathNode = NULL;
        node->m_flag = false;
        prev = node;
    }
    s_rsnAStarSearchNodeFreeList = prev;
}

// Runs at static init after the pool is constructed.
static int s_poolInit = (BuildFreeList(), 0);

CSearchNode::CSearchNode() {
    HeapIndex() = 0;
    m_pPathNode = NULL;
}

CSearchNode::~CSearchNode() {
    if (m_pPathNode->m_pSearchNode == this) {
        m_pPathNode->m_pSearchNode = NULL;
    }
    if (m_flag) {
        m_flag = !m_flag;
    }
    HeapIndex() = 0;
    m_pPathNode = NULL;
}

CSearchNode* CSearchNode::Setup(CSearchNode* parent, CPathNode* pathNode, const CPathNode* goal) {
    CSearchNode* prevOwner = pathNode->m_pSearchNode;
    m_pParent = parent;
    m_pPathNode = pathNode;
    if (prevOwner == NULL) {
        pathNode->m_pSearchNode = this;
    }
    if (parent != NULL) {
        CAIFilterRealPosition pos = m_pPathNode->GetPosition();
        CAIFilterRealPosition parentPos = parent->m_pPathNode->GetPosition();
        m_costFromStart = parentPos.GetDistanceXYZReal(pos);
        m_costFromStart = m_costFromStart + parent->m_costFromStart;
    } else {
        m_costFromStart = 0.0f;
    }
    CAIFilterRealPosition pos = m_pPathNode->GetPosition();
    CAIFilterRealPosition goalPos = goal->GetPosition();
    m_costToGoal = pos.GetDistanceXYZReal(goalPos);
    m_totalCost = m_costFromStart + m_costToGoal;
    return prevOwner;
}

float CSearchNode::GetCostValue(const CSearchNode* other) {
    CAIFilterRealPosition pos = m_pPathNode->GetPosition();
    CAIFilterRealPosition otherPos = other->m_pPathNode->GetPosition();
    return otherPos.GetDistanceXYZReal(pos);
}

void* CSearchNode::operator new(unsigned long size) {
    CSearchNode* node = s_rsnAStarSearchNodeFreeList;
    s_rsnAStarSearchNodeFreeList = node->m_pParent;
    return node;
}

void CSearchNode::operator delete(void* ptr, unsigned long size) {
    ((CSearchNode*)ptr)->m_pParent = s_rsnAStarSearchNodeFreeList;
    s_rsnAStarSearchNodeFreeList = (CSearchNode*)ptr;
}

void CSearchNode::Reset() {
    BuildFreeList();
}
