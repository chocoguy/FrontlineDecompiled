// Location collision: maps a character's collision shapes (by name) to the
// body part they belong to, for hit locations. Hand-maintained.
#ifndef _GAME_LOCATION_COLLISION_H_
#define _GAME_LOCATION_COLLISION_H_

#include "types.h"

#define LOCATION_COLLISION_MAX_SHAPES 0x18

// One collision shape in a character's data file: a box in its bone's
// space (laid out as a CVolBox, vtable word included).
struct CObjLocationCollisionBox {
    /* 0x00 */ u8 box[0x50];
    /* 0x50 */ char name[0x20];
    /* 0x70 */ s32 bone;  // index into the owner's bone matrices
    /* 0x74 */ u8 unk_0x74[0xC];
};

struct CObjLocationCollisionData {
    /* 0x000 */ CObjLocationCollisionBox shapes[LOCATION_COLLISION_MAX_SHAPES];
    /* 0xC00 */ s32 numShapes;
    /* 0xC04 */ s32 field_0xC04;
};

// size 0x124
class CObjLocationCollisionMap {
public:
    class LocationInfo {
    public:
        /* 0x0 */ s32 location;  // body part (head, neck, hand R, ...)
        /* 0x4 */ s32 region;    // 1 = extremity ... 5 = head/torso
        /* 0x8 */ u8 valid;

        LocationInfo() {}
    };

    /* 0x000 */ LocationInfo m_info[LOCATION_COLLISION_MAX_SHAPES];
    /* 0x120 */ s32 m_numShapes;

    CObjLocationCollisionMap(const CObjLocationCollisionData&);  // 0x80072EA0
};

#endif
