// CLine3: a line segment. Hand-maintained.
#ifndef _GAME_LINE_H_
#define _GAME_LINE_H_

#include "types.h"
#include "game_types.h"
#include "game/vector.h"

class CLine3;
class CVector3;

// A segment: start, end and the direction end - start (size 0x3C).
class CLine3 {
public:
    /* 0x00 */ CVector3 m_start;
    /* 0x10 */ CVector3 m_end;
    /* 0x20 */ CVector3 m_dir;
    // Cached |m_dir| and |m_dir|^2, computed on demand.
    /* 0x30 */ mutable f32 m_length;
    /* 0x34 */ mutable f32 m_lengthSq;
    /* 0x38 */ mutable u8 m_lengthValid;
    /* 0x39 */ mutable u8 m_lengthSqValid;
    /* 0x3A */ u8 unk_0x3A[0x2];
    CLine3() {}
    CLine3& operator=(const CLine3& other) {
        if (&other != this) {
            m_start.Copy(other.m_start);
            m_end.Copy(other.m_end);
            m_dir.Copy(other.m_dir);
            m_length = other.m_length;
            m_lengthSq = other.m_lengthSq;
            m_lengthValid = other.m_lengthValid;
            m_lengthSqValid = other.m_lengthSqValid;
        }
        return *this;
    }
    // Start and end points.
    void SetSE(CVector3 start, CVector3 end) {
        m_start = start;
        m_end = end;
        m_dir.x = m_end.x - m_start.x;
        m_dir.y = m_end.y - m_start.y;
        m_dir.z = m_end.z - m_start.z;
        m_lengthValid = 0;
        m_lengthSqValid = 0;
    }
    ~CLine3();  // 0x8001C5D0 (weak; defined in debug_picker)
    // Start point and direction (weak in the original).
    void SetSD(CVector3 start, CVector3 dir) {
        m_start = start;
        m_dir = dir;
        m_end.x = start.x + dir.x;
        m_end.y = start.y + dir.y;
        m_end.z = start.z + dir.z;
        m_lengthValid = 0;
        m_lengthSqValid = 0;
    }
    float GetLengthSq() const {
        if (!m_lengthSqValid) {
            m_lengthSq = m_dir.x * m_dir.x + m_dir.y * m_dir.y + m_dir.z * m_dir.z;
            m_lengthSqValid = 1;
        }
        return m_lengthSq;
    }
    float GetLength() const {
        if (!m_lengthValid) {
            m_length = sqrtf(GetLengthSq());
            m_lengthValid = 1;
        }
        return m_length;
    }
    // True when the segment has (almost) no length.
    bool IsDegenerate() const;  // 0x800922F4
    // Closest points between the segments (Eberly): parameters along this
    // line and `other`, and the squared distance. Returns true when the
    // segments are (almost) parallel.
    bool GetSquaredDistanceBetweenSegments(const CLine3& other, float& s, float& t, float* sqrDist) const;  // 0x80092344
    // Closest point on the infinite line to `point` and its parameter t
    // (the midpoint, t = 0.5, for a degenerate segment).
    void GetProjection(CVector3 point, CVector3& out, float& t) const;  // 0x80092B28
    CLine3(CVector3, CVector3);  // 0x800A4E04
};

#endif
