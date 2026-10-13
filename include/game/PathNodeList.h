#ifndef _GAME_PATHNODELIST_H_
#define _GAME_PATHNODELIST_H_

#include "types.h"
#include "game_types.h"
#include "dwi/hash_map.h"
#include "game/PathNode.h"

struct BPDPathFindingNode;
class CAIFilterRealPosition;
class CAIFilterRealVector3;
class IVolume;

// The level's path nodes, with a 5x5-unit grid (s_pathNodeMap) of the nodes
// within reach of each cell for nearest-node lookups (size 0x8).
class CPathNodeList {
public:
    // The nodes listed under one grid cell.
    struct PathNodes {
        /* 0x0 */ s32 m_count;
        /* 0x4 */ CPathNode** m_nodes;
        /* 0x8 */ bool m_allEnabled;  // false once DisableCollidingNodes disabled some

        PathNodes() : m_count(0), m_nodes(NULL), m_allEnabled(true) {}
    };

    /* 0x0 */ CPathNode* m_nodes;
    /* 0x4 */ s32 m_numNodes;

    void DisableCollidingNodes(const IVolume*);  // 0x80054810
    void EnableAll();  // 0x80055028
    CPathNode* GreedySearchAwayFromPoint(const CAIFilterRealPosition&, const CAIFilterRealPosition&, float) const;  // 0x800550D8
    CPathNode* GetClosestNode(const CAIFilterRealPosition&, bool) const;  // 0x800552E8
    CPathNode* GetClosestNode(const CAIFilterRealPosition&, const CAIFilterRealVector3&) const;  // 0x8005552C
    CPathNode* GetPathNodeByID(int) const;  // 0x80055790
    CPathNodeList(void*, BPDPathFindingNode*, int);  // 0x800557A0

    static dwi::hash_map<unsigned int, PathNodes> s_pathNodeMap;  // 0x801AAC04
    static CPathNodeList* s_pnlCurrent;  // 0x8034E908
    static CPathNode** s_pPathNodeMapEntries;  // 0x8034E90C
};

#endif
