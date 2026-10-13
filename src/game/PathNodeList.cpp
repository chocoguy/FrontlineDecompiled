// CPathNodeList: the level's AI path nodes. A grid of 5x5-unit cells maps
// each cell to the nodes in it and its eight neighbours, for finding the
// node closest to a position.
#include "game/PathNodeList.h"
#include "game/AIFilter.h"
#include "game/collision.h"
#include "game/endian.h"
#include "game/memory.h"
#include "game/sphere.h"
#include "game/vector.h"
#include "math.h"

#define PATHNODE_CELL_SIZE 5.0f
#define PATHNODE_MIN_MAP_SIZE 0x607

dwi::hash_map<unsigned int, CPathNodeList::PathNodes> CPathNodeList::s_pathNodeMap;
CPathNodeList* CPathNodeList::s_pnlCurrent;
CPathNode** CPathNodeList::s_pPathNodeMapEntries;

typedef dwi::hash_map<unsigned int, CPathNodeList::PathNodes>::entry PathNodeMapEntry;

// Cell key: 15-bit cell x in bits 1-15, 15-bit cell y in bits 16-30.
static inline u32 CellKey(int cx, int cy) {
    return ((cy & 0x7FFF) << 16) + ((cx & 0x7FFF) << 1);
}

static inline u32 GetCellKey(float x, float y) {
    int cx = (int)((x + 2.5f) / PATHNODE_CELL_SIZE) + 0x4000;
    int cy = (int)((y + 2.5f) / PATHNODE_CELL_SIZE) + 0x4000;
    return CellKey(cx, cy);
}

// The 3x3 cells around `key`, x-major.
static inline void GetNeighbourKeys(u32 key, u32* keys) {
    int cx = (key >> 1) & 0x7FFF;
    int cy = (key >> 16) & 0x7FFF;
    int n = 0;
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            keys[n++] = CellKey(cx + dx, cy + dy);
        }
    }
}

static inline float Distance(const CPathNode* node, const CAIFilterRealPosition& pos) {
    CAIFilterRealPosition p = node->GetPosition();
    float dx = p.field_0x0 - pos.field_0x0;
    float dy = p.field_0x4 - pos.field_0x4;
    float dz = p.field_0x8 - pos.field_0x8;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

CPathNodeList::CPathNodeList(void* base, BPDPathFindingNode* nodes, int numNodes) {
    m_nodes = (CPathNode*)((u8*)base + (u32)nodes);
    m_numNodes = numNodes;

    // fix up the records in place
    for (CPathNode* node = m_nodes; node < &m_nodes[m_numNodes]; node++) {
        EndianSwap(*(u32*)&node->m_pos[0]);
        EndianSwap(*(u32*)&node->m_pos[1]);
        EndianSwap(*(u32*)&node->m_pos[2]);
        EndianSwap(*(u32*)&node->m_pSearchNode);
        EndianSwap(*(u32*)&node->m_links);
        node->m_enabled = true;
        node->m_pSearchNode = NULL;
        node->m_links = (s16*)((u8*)base + (u32)node->m_links);
        for (int i = 0; i < node->m_numLinks; i++) {
            EndianSwap(node->m_links[i]);
        }
    }

    int size = m_numNodes * 2 + 1;
    if (size < PATHNODE_MIN_MAP_SIZE) {
        size = PATHNODE_MIN_MAP_SIZE;
    }
    s_pathNodeMap.reserve(size, "AI Path Node Map");

    // count the nodes listed under each cell
    int total = 0;
    for (int i = 0; i < m_numNodes; i++) {
        CPathNode* node = &m_nodes[i];
        u32 keys[9] = {0};
        GetNeighbourKeys(GetCellKey(node->m_pos[0], node->m_pos[1]), keys);
        for (int k = 0; k < 9; k++) {
            total++;
            PathNodeMapEntry* e = s_pathNodeMap.find(keys[k]);
            if (e == s_pathNodeMap.end()) {
                PathNodes cell;
                cell.m_count = 1;
                s_pathNodeMap.insert(keys[k], cell);
            } else {
                e->value.m_count++;
            }
        }
    }

    // fill in the lists
    CPathNode** entries = new ("AI Path Node Map Entries") CPathNode*[total];
    s_pPathNodeMapEntries = entries;
    for (int i = 0; i < m_numNodes; i++) {
        CPathNode* node = &m_nodes[i];
        u32 keys[9] = {0};
        GetNeighbourKeys(GetCellKey(node->m_pos[0], node->m_pos[1]), keys);
        for (int k = 0; k < 9; k++) {
            PathNodes& cell = s_pathNodeMap.find(keys[k])->value;
            if (cell.m_nodes == NULL) {
                cell.m_nodes = entries;
                for (int j = 0; j < cell.m_count; j++) {
                    cell.m_nodes[j] = NULL;
                }
                entries += cell.m_count;
            }
            int j = 0;
            while (cell.m_nodes[j] != NULL) {
                j++;
            }
            cell.m_nodes[j] = node;
        }
    }
    s_pnlCurrent = this;
}

CPathNode* CPathNodeList::GetPathNodeByID(int id) const {
    return &m_nodes[id];
}

// The reachable node nearest along `dir` from `pos` (distance^2 / forward
// distance), else the nearest node behind; within 5 units.
CPathNode* CPathNodeList::GetClosestNode(const CAIFilterRealPosition& pos,
                                         const CAIFilterRealVector3& dir) const {
    PathNodeMapEntry* e = s_pathNodeMap.find(GetCellKey(pos.field_0x0, pos.field_0x4));
    if (e == s_pathNodeMap.end()) {
        return NULL;
    }

    float bestAhead = 25.0f;
    float bestBehind = bestAhead;
    CPathNode* ahead = NULL;
    CPathNode* behind = NULL;
    int count = e->value.m_count;
    CPathNode** nodes = e->value.m_nodes;
    for (int i = 0; i < count; i++, nodes++) {
        CPathNode* node = *nodes;
        if (!node->m_enabled) {
            continue;
        }
        CAIFilterRealPosition p = node->GetPosition();
        float dx = p.field_0x0 - pos.field_0x0;
        float dy = p.field_0x4 - pos.field_0x4;
        float dz = p.field_0x8 - pos.field_0x8;
        float forward = dx * dir.field_0x0 + dy * dir.field_0x4 + dz * dir.field_0x8;
        if (forward > 0.01) {
            float metric = (dx * dx + dy * dy + dz * dz) / forward;
            if (metric < bestAhead &&
                !CAIFilterGlobal::CheckCollisionLineToEnvironment(p, pos, NULL)) {
                bestAhead = metric;
                ahead = node;
            }
        } else {
            float distSq = dx * dx + dy * dy + dz * dz;
            if (distSq < bestBehind &&
                !CAIFilterGlobal::CheckCollisionLineToEnvironment(p, pos, NULL)) {
                bestBehind = distSq;
                behind = node;
            }
        }
    }
    if (ahead != NULL) {
        return ahead;
    }
    return behind;
}

// The nearest enabled node within 5 units, optionally without a line of
// sight check.
CPathNode* CPathNodeList::GetClosestNode(const CAIFilterRealPosition& pos, bool noLineCheck) const {
    PathNodeMapEntry* e = s_pathNodeMap.find(GetCellKey(pos.field_0x0, pos.field_0x4));
    if (e == s_pathNodeMap.end()) {
        return NULL;
    }

    CPathNode* best = NULL;
    float bestDist = PATHNODE_CELL_SIZE;
    int count = e->value.m_count;
    CPathNode** nodes = e->value.m_nodes;
    for (int i = 0; i < count; i++, nodes++) {
        CPathNode* node = *nodes;
        if (!node->m_enabled) {
            continue;
        }
        CAIFilterRealPosition p = node->GetPosition();
        float dx = p.field_0x0 - pos.field_0x0;
        float dy = p.field_0x4 - pos.field_0x4;
        float dz = p.field_0x8 - pos.field_0x8;
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);
        if (dist < bestDist) {
            bool blocked;
            if (noLineCheck) {
                blocked = false;
            } else {
                blocked = CAIFilterGlobal::CheckCollisionLineToEnvironment(p, pos, NULL);
            }
            if (!blocked) {
                bestDist = dist;
                best = node;
            }
        }
    }
    return best;
}

// Starting at the node nearest `from`, walks to whichever linked node is
// furthest from `away` until it is at least `minDist` from it. NULL if the
// walk gets stuck closer than that.
CPathNode* CPathNodeList::GreedySearchAwayFromPoint(const CAIFilterRealPosition& from,
                                                    const CAIFilterRealPosition& away,
                                                    float minDist) const {
    CPathNode* node = GetClosestNode(from, false);
    if (node == NULL) {
        return NULL;
    }

    float best = Distance(node, away);
    bool moved = true;
    while (best < minDist && moved) {
        moved = false;
        s16* link = node->m_links;
        s16* end = &link[node->m_numLinks];
        for (; link < end; link++) {
            CPathNode* next = &m_nodes[(u16)*link];
            float dist = Distance(next, away);
            if (dist > best) {
                best = dist;
                node = next;
                moved = true;
            }
        }
    }
    if (best >= minDist) {
        return node;
    }
    return NULL;
}

// Re-enables the nodes of every cell DisableCollidingNodes touched.
void CPathNodeList::EnableAll() {
    PathNodeMapEntry* e = s_pathNodeMap.m_table;
    if (!(e->key != (u32)-1 && e->key != (u32)-2)) {
        do {
            e++;
        } while (e->key == (u32)-1);
    }
    while (e != s_pathNodeMap.end()) {
        if (!e->value.m_allEnabled) {
            for (int i = 0; i < e->value.m_count; i++) {
                e->value.m_nodes[i]->m_enabled = true;
            }
        }
        do {
            e++;
        } while (e->key == (u32)-1);
    }
}

// Disables the nodes of `cell` whose 1-unit sphere is not clear of `volume`.
static inline bool DisableNodes(const CPathNodeList::PathNodes& cell, const IVolume* volume) {
    CVolSphere sphere;
    sphere.SetRadius(1.0f);
    bool disabled = false;
    CPathNode** nodes = cell.m_nodes;
    for (int i = 0; i < cell.m_count; i++, nodes++) {
        CCollision collision;
        CPathNode* node = *nodes;
        sphere.SetCenter(CVector3(node->m_pos[0], node->m_pos[1], node->m_pos[2]));
        if (sphere.TestCollision(*volume, collision, false) != 1) {
            node->m_enabled = false;
            disabled = true;
        }
    }
    return disabled;
}

// Disables the nodes near `volume` (the cells of its two extent corners).
void CPathNodeList::DisableCollidingNodes(const IVolume* volume) {
    CVector3 min;
    CVector3 max;
    volume->GetExtents(min, max);

    PathNodes cell;
    u32 key = GetCellKey(min.x, min.y);
    PathNodeMapEntry* e = s_pathNodeMap.find(key);
    if (e != s_pathNodeMap.end()) {
        cell = e->value;
        if (DisableNodes(cell, volume)) {
            cell.m_allEnabled = false;
            s_pathNodeMap.erase(e);
            s_pathNodeMap.insert(key, cell);
        }
    }

    key = GetCellKey(max.x, max.y);
    e = s_pathNodeMap.find(key);
    if (e != s_pathNodeMap.end() && e->value.m_nodes != cell.m_nodes) {
        cell = e->value;
        if (DisableNodes(cell, volume)) {
            cell.m_allEnabled = false;
            s_pathNodeMap.erase(e);
            s_pathNodeMap.insert(key, cell);
        }
    }
}
