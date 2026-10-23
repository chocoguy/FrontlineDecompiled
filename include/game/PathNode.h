// CPathNode: a node of the level's AI path-finding graph. The level data
// holds BPDPathFindingNode records (little-endian, links as offsets);
// CPathNodeList fixes them up in place and uses them as CPathNodes.
// Hand-maintained.
#ifndef _GAME_PATHNODE_H_
#define _GAME_PATHNODE_H_

#include "types.h"
#include "game/AIFilter.h"

class CSearchNode;

// size 0x18
struct CPathNode {
    /* 0x00 */ f32 m_pos[3];
    /* 0x0C */ PTR32(CSearchNode) m_pSearchNode;  // the search node visiting this node, if any
    /* 0x10 */ u8 m_numLinks;
#ifdef FRONTLINE_NATIVE
    // clang allocates bitfields from the low bit, CodeWarrior from the high
    // bit: declared in reverse so the file's bits line up.
    /* 0x11 */ u8 m_flags : 7;
    /* 0x11 */ u8 m_enabled : 1;  // cleared while a dynamic obstacle blocks the node
#else
    /* 0x11 */ u8 m_enabled : 1;  // cleared while a dynamic obstacle blocks the node
    /* 0x11 */ u8 m_flags : 7;
#endif
    /* 0x12 */ u8 unk_0x12[0x2];
    /* 0x14 */ PTR32(s16) m_links;  // indices of the linked nodes

    CAIFilterRealPosition GetPosition() const {
        CAIFilterRealPosition p;
        p.field_0x0 = m_pos[0];
        p.field_0x4 = m_pos[1];
        p.field_0x8 = m_pos[2];
        return p;
    }
};
FILE_SIZE_ASSERT(CPathNode, 0x18);

#endif
