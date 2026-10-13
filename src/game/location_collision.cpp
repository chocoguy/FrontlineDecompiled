// Location collision: body part lookup for character collision shapes.
#include "game/location_collision.h"
#include "string.h"

// Hash table bucket sizes (dwi::prime_list, from a shared container header).
static const u32 dwi_prime_list[27] = {
    0x00000035, 0x00000061, 0x000000C1, 0x00000185, 0x00000301, 0x00000607, 0x00000C07, 0x00001807,
    0x00003001, 0x00006011, 0x0000C005, 0x0001800D, 0x00030005, 0x00060019, 0x000C0001, 0x00180005,
    0x0030000B, 0x0060000D, 0x00C00005, 0x01800013, 0x03000005, 0x06000017, 0x0C000013, 0x18000005,
    0x30000059, 0x60000005, 0xC0000001,
};

struct LocationDef {
    /* 0x00 */ char name[0x20];
    /* 0x20 */ s32 location;
    /* 0x24 */ s32 region;
    /* 0x28 */ u8 valid;

    LocationDef(const char* shapeName, s32 loc, s32 reg) {
        location = loc;
        region = reg;
        valid = 1;
        strncpy(name, shapeName, 0x1F);
    }
};

static LocationDef g_ClsnMap[] = {
    LocationDef("co_HeadBShape", 0, 5),     LocationDef("co_HeadFShape", 1, 5),
    LocationDef("co_NeckShape", 2, 5),      LocationDef("co_handRShape", 6, 1),
    LocationDef("co_loarm_RShape", 7, 2),   LocationDef("co_uparm_RShape", 8, 2),
    LocationDef("co_handLShape", 3, 1),     LocationDef("co_loarm_LShape", 4, 2),
    LocationDef("co_uparm_LShape", 5, 2),   LocationDef("co_ChestShape", 9, 5),
    LocationDef("co_Back1Shape", 10, 5),    LocationDef("co_AbdFShape", 12, 5),
    LocationDef("co_Back2Shape", 11, 5),    LocationDef("co_ankle_RShape", 16, 1),
    LocationDef("co_calf_LShape", 14, 2),   LocationDef("co_calf_RShape", 17, 2),
    LocationDef("co_thigh_RShape", 18, 3),  LocationDef("co_ankle_LShape", 13, 1),
    LocationDef("co_thigh_LShape", 15, 3),  LocationDef("co_HipBShape", 19, 4),
    LocationDef("co_HipFShape", 20, 4),
};

CObjLocationCollisionMap::CObjLocationCollisionMap(const CObjLocationCollisionData& data) {
    m_numShapes = data.numShapes;
    for (int i = 0; i < m_numShapes; i++) {
        char name[0x20];
        strncpy(name, data.shapes[i].name, 0x1F);
        // The original's end bound is the table's size in bytes, not entries,
        // so an unknown name reads past the table (as in the original).
        LocationDef* def = g_ClsnMap;
        while (def != g_ClsnMap + sizeof(g_ClsnMap)) {
            if (stricmp(def->name, name) == 0) {
                break;
            }
            def++;
        }
        m_info[i].location = def->location;
        m_info[i].region = def->region;
        m_info[i].valid = def->valid;
    }
}
