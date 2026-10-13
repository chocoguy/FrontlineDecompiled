// CCoverPoint: a cover position from the level data. Its flags say which
// ways an AI can peek from it (left, right, over); AIs claim those peek
// positions while using the cover.
#include "game/CoverPoint.h"
#include "game/AIFilter.h"
#include "game/AIObject.h"
#include "game/Moh2.h"
#include "game/bsobject.h"
#include "game/endian.h"
#include "game/matrix.h"
#include "math.h"

// Cover only counts against threats within 30 degrees of its facing.
static float cosCoverPointEffectiveAngle = cos(0.5235987901687622);

struct CoverFlagMap {
    u32 flag;
    COVERPOINT_TYPE type;
};

// The peek directions the data flags allow.
static inline u32 GetPeekTypes(const CoverPointData_t* data) {
    u32 flags = data->flags;
    EndianSwap(flags);
    const CoverFlagMap map[8] = {
        {CPF_LEFT_0, COVERPOINT_LEFT},  {CPF_RIGHT_0, COVERPOINT_RIGHT},
        {CPF_OVER_0, COVERPOINT_OVER},  {CPF_OVER_1, COVERPOINT_OVER},
        {CPF_LEFT_1, COVERPOINT_LEFT},  {CPF_RIGHT_1, COVERPOINT_RIGHT},
        {CPF_LEFT_2, COVERPOINT_LEFT},  {CPF_RIGHT_2, COVERPOINT_RIGHT},
    };
    u32 types = 0;
    for (int i = 0; i < 8; i++) {
        if (flags & map[i].flag) {
            types |= map[i].type;
        }
    }
    types &= 0xFF;
    return types & (COVERPOINT_LEFT | COVERPOINT_RIGHT | COVERPOINT_OVER);
}

// Row 1 of the cover's orientation: the direction out of cover.
static inline void GetFacing(const CoverPointData_t* data, CAIFilterRealVector3& dir) {
    CMatrix m;
    data->orientation.GetMatrix(m);
    dir.field_0x0 = m.m[1][0];
    dir.field_0x4 = m.m[1][1];
    dir.field_0x8 = m.m[1][2];
}

// A free peek direction, preferring right, then over.
COVERPOINT_TYPE CCoverPoint::GetAvailablePeekPosition() const {
    u32 types = GetPeekTypes(m_data);
    int avail = ~(m_occupied & 0xFF) & types;
    if (avail > 1) {
        avail &= 1 - avail;
    }
    return (COVERPOINT_TYPE)avail;
}

// The spot to stand at for peeking `type`: pulled back from the cover and
// shifted sideways for left/right.
void CCoverPoint::GetModifiedPosition(CAIFilterRealVector3& out, COVERPOINT_TYPE type) const {
    out.field_0x0 = m_data->position[0];
    out.field_0x4 = m_data->position[1];
    out.field_0x8 = m_data->position[2];
    CAIFilterRealVector3 v;
    GetFacing(m_data, v);

    switch (type) {
    case COVERPOINT_LEFT: {
        v.field_0x0 *= 0.275f;
        v.field_0x4 *= 0.275f;
        v.field_0x8 *= 0.275f;
        out.field_0x0 -= v.field_0x0;
        out.field_0x4 -= v.field_0x4;
        out.field_0x8 -= v.field_0x8;
        v.field_0x0 *= 0.9090909f;
        v.field_0x4 *= 0.9090909f;
        v.field_0x8 *= 0.9090909f;
        out.field_0x0 += v.field_0x4;
        out.field_0x4 += -v.field_0x0;
        out.field_0x8 += v.field_0x8;
        break;
    }
    case COVERPOINT_RIGHT: {
        v.field_0x0 *= 0.275f;
        v.field_0x4 *= 0.275f;
        v.field_0x8 *= 0.275f;
        out.field_0x0 -= v.field_0x0;
        out.field_0x4 -= v.field_0x4;
        out.field_0x8 -= v.field_0x8;
        v.field_0x0 *= 0.9090909f;
        v.field_0x4 *= 0.9090909f;
        v.field_0x8 *= 0.9090909f;
        out.field_0x0 += -v.field_0x4;
        out.field_0x4 += v.field_0x0;
        out.field_0x8 += v.field_0x8;
        break;
    }
    case COVERPOINT_OVER:
        v.field_0x0 *= 1.0f;
        v.field_0x4 *= 1.0f;
        v.field_0x8 *= 1.0f;
        out.field_0x0 -= v.field_0x0;
        out.field_0x4 -= v.field_0x4;
        out.field_0x8 -= v.field_0x8;
        break;
    }
}

// Claims peek direction(s) `type`; fails if the cover does not allow them.
bool CCoverPoint::FlagAsOccupied(COVERPOINT_TYPE type) {
    u32 types = GetPeekTypes(m_data);
    if (type & ~types) {
        DebugMsg("Coverpoint flag error! cptTypes = %x cptPosition = %x\n", types, type);
        return false;
    }
    m_occupied |= type;
    return true;
}

void CCoverPoint::FlagAsUnoccupied(COVERPOINT_TYPE type) {
    m_occupied &= ~type;
}

// Returns 1 if from->to is clear, 2 if clear after stepping out of cover
// (`out` = the stepped-out start), 0 if blocked.
int CCoverPoint::CheckLineCollision(const CAIFilterRealPosition& from,
                                    const CAIFilterRealPosition& to,
                                    CAIFilterRealPosition* out) const {
    if (!CAIFilterGlobal::CheckCollisionLineToEnvironment(from, to, NULL)) {
        return 1;
    }

    u32 types = GetPeekTypes(m_data);
    CAIFilterRealVector3 v;
    CAIFilterRealPosition p;
    if (!(types & (COVERPOINT_LEFT | COVERPOINT_RIGHT))) {
        // try 1.5 units to either side
        GetFacing(m_data, v);
        CAIFilterRealVector3 side;
        side.field_0x0 = -v.field_0x4 * 1.5f;
        side.field_0x4 = v.field_0x0 * 1.5f;
        side.field_0x8 = v.field_0x8 * 1.5f;
        p.field_0x0 = from.field_0x0 + side.field_0x0;
        p.field_0x4 = from.field_0x4 + side.field_0x4;
        p.field_0x8 = from.field_0x8 + side.field_0x8;
        if (!CAIFilterGlobal::CheckCollisionLineToEnvironment(p, to, NULL)) {
            out->field_0x0 = p.field_0x0;
            out->field_0x4 = p.field_0x4;
            out->field_0x8 = p.field_0x8;
            return 2;
        }

        GetFacing(m_data, v);
        side.field_0x0 = v.field_0x4 * 1.5f;
        side.field_0x4 = -v.field_0x0 * 1.5f;
        side.field_0x8 = v.field_0x8 * 1.5f;
        p.field_0x0 = from.field_0x0 + side.field_0x0;
        p.field_0x4 = from.field_0x4 + side.field_0x4;
        p.field_0x8 = from.field_0x8 + side.field_0x8;
        if (!CAIFilterGlobal::CheckCollisionLineToEnvironment(p, to, NULL)) {
            out->field_0x0 = p.field_0x0;
            out->field_0x4 = p.field_0x4;
            out->field_0x8 = p.field_0x8;
            return 2;
        }
        return 0;
    }

    // step 0.8 units out on the peek side
    GetFacing(m_data, v);
    if (types & COVERPOINT_LEFT) {
        float x = v.field_0x0;
        v.field_0x0 = -v.field_0x4;
        v.field_0x4 = x;
    } else if (types & COVERPOINT_RIGHT) {
        float x = v.field_0x0;
        v.field_0x0 = v.field_0x4;
        v.field_0x4 = -x;
    }
    v.field_0x0 *= 0.8f;
    v.field_0x4 *= 0.8f;
    v.field_0x8 *= 0.8f;
    p.field_0x0 = from.field_0x0 + v.field_0x0;
    p.field_0x4 = from.field_0x4 + v.field_0x4;
    p.field_0x8 = from.field_0x8 + v.field_0x8;
    if (!CAIFilterGlobal::CheckCollisionLineToEnvironment(p, to, NULL)) {
        out->field_0x0 = p.field_0x0;
        out->field_0x4 = p.field_0x4;
        out->field_0x8 = p.field_0x8;
        return 2;
    }
    return 0;
}

// Unit vector from `from` to `to` (left as is when zero length).
static inline void Direction(const CAIFilterRealPosition& from, const CAIFilterRealPosition& to,
                             float& x, float& y, float& z) {
    x = to.field_0x0 - from.field_0x0;
    y = to.field_0x4 - from.field_0x4;
    z = to.field_0x8 - from.field_0x8;
    float len = sqrtf(x * x + y * y + z * z);
    if (len != 0.0f) {
        float inv = 1.0f / len;
        x *= inv;
        y *= inv;
        z *= inv;
    }
}

// Whether the cover shields `user` from a threat at `threat`: the threat must
// be over 6 units away and in front of the cover, and no other AI (not on
// the user's team) may be nearer the threat or near the user.
bool CCoverPoint::ProvidesCover(const CAIFilterRealPosition& threat,
                                const CAIFilterRealPosition* user,
                                const CAIObject* userObj) const {
    if (threat.GetDistanceSquaredXYReal(*user) < 36.0f) {
        return false;
    }

    CAIFilterRealVector3 facing;
    GetFacing(m_data, facing);
    float dx, dy, dz;
    Direction(threat, *user, dx, dy, dz);
    if (dx * facing.field_0x0 + dy * facing.field_0x4 + dz * facing.field_0x8 <
        cosCoverPointEffectiveAngle) {
        return false;
    }
    if (userObj == NULL) {
        return true;
    }

    float userDist = threat.GetDistanceSquaredXYReal(userObj->m_position);
    for (CAIObject* obj = CAIObject::GetGlobalListStart(); obj != NULL; obj = obj->m_pNext) {
        if (obj == userObj) {
            continue;
        }
        if (obj->field_0x14 != 0 && obj->field_0x14 == userObj->field_0x14) {
            continue;
        }
        if (userObj->m_position.GetDistanceSquaredXYZReal(obj->m_position) < 36.0f) {
            return false;
        }
        float dist = threat.GetDistanceSquaredXYReal(obj->m_position);
        if (dist < 36.0f || dist < userDist) {
            return false;
        }
        Direction(threat, obj->m_position, dx, dy, dz);
        if (dx * facing.field_0x0 + dy * facing.field_0x4 + dz * facing.field_0x8 < 0.0f &&
            dist < 324.0f) {
            return false;
        }
    }
    return true;
}

// Clears the data flags and tells every AI using this cover.
void CCoverPoint::Invalidate() {
    // the original clears the eight flag bits of the first byte one by one
    ((u8*)&m_data->flags)[0] = 0;
    for (CAIObject* obj = CAIObject::GetGlobalListStart(); obj != NULL; obj = obj->m_pNext) {
        if (obj->m_pCoverPoint == this) {
            BSObjectTriggerEvent(((CAIFilterSoldierObject*)obj->m_pFilter)->m_pBSObject, 0x3E,
                                 NULL, NULL, true);
        }
    }
}

CAIFilterRealPosition CCoverPoint::GetPeekLocation() {
    CAIFilterRealVector3 dir;
    GetFacing(m_data, dir);
    u32 types = GetPeekTypes(m_data);
    if (types & COVERPOINT_LEFT) {
        float x = dir.field_0x0;
        dir.field_0x0 = -dir.field_0x4;
        dir.field_0x4 = x;
    } else if (types & COVERPOINT_RIGHT) {
        float x = dir.field_0x0;
        dir.field_0x0 = dir.field_0x4;
        dir.field_0x4 = -x;
    } else if (types & COVERPOINT_OVER) {
        dir.field_0x0 = 0.0f;
        dir.field_0x4 = 0.0f;
        dir.field_0x8 = 1.0f;
    }
    dir.field_0x0 *= 0.8f;
    dir.field_0x4 *= 0.8f;
    dir.field_0x8 *= 0.8f;

    CAIFilterRealVector3 base;
    GetModifiedPosition(base, (COVERPOINT_TYPE)types);
    CAIFilterRealPosition result;
    result.field_0x0 = base.field_0x0 + dir.field_0x0;
    result.field_0x4 = base.field_0x4 + dir.field_0x4;
    result.field_0x8 = (float)((base.field_0x8 + dir.field_0x8) + 1.0);
    return result;
}
