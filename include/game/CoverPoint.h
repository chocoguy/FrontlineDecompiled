#ifndef _GAME_COVERPOINT_H_
#define _GAME_COVERPOINT_H_

#include "types.h"
#include "game_types.h"
#include "game/quaternion.h"

class CAIFilterRealPosition;
class CAIFilterRealVector3;
class CAIObject;

// Cover point record from the level data (little-endian).
struct CoverPointData_t {
    /* 0x00 */ u8 unk_0x0[0x10];
    /* 0x10 */ f32 position[3];
    /* 0x1C */ CQuaternion orientation;  // row 1 of its matrix faces out of cover
    /* 0x2C */ u8 unk_0x2C[0x44];
    /* 0x70 */ u32 flags;  // CPF_* bits, little-endian
};

// CoverPointData_t::flags: which way the cover can be peeked from.
#define CPF_LEFT_0 0x01
#define CPF_RIGHT_0 0x02
#define CPF_OVER_0 0x04
#define CPF_OVER_1 0x08
#define CPF_LEFT_1 0x10
#define CPF_RIGHT_1 0x20
#define CPF_LEFT_2 0x40
#define CPF_RIGHT_2 0x80

// size 0x8
class CCoverPoint {
public:
    /* 0x0 */ u32 m_occupied;  // COVERPOINT_TYPE bits taken by AIs
    /* 0x4 */ CoverPointData_t* m_data;

    CAIFilterRealPosition GetPeekLocation();  // 0x800523CC
    void Invalidate();  // 0x800526B0
    bool ProvidesCover(const CAIFilterRealPosition&, const CAIFilterRealPosition*, const CAIObject*) const;  // 0x80052788
    int CheckLineCollision(const CAIFilterRealPosition&, const CAIFilterRealPosition&, CAIFilterRealPosition*) const;  // 0x80052ABC
    void FlagAsUnoccupied(COVERPOINT_TYPE);  // 0x80052F5C
    bool FlagAsOccupied(COVERPOINT_TYPE);  // 0x80052F6C
    void GetModifiedPosition(CAIFilterRealVector3&, COVERPOINT_TYPE) const;  // 0x8005314C
    COVERPOINT_TYPE GetAvailablePeekPosition() const;  // 0x800533A8
};

#endif
