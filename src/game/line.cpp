#include "game/line.h"

// Functions are written in reverse address order: the game was compiled with
// deferred inlining, which emits them last to first.

#define DEGENERATE_EPSILON 1e-12f
#define PARALLEL_EPSILON 0.00001f

void CLine3::GetProjection(CVector3 point, CVector3& out, float& t) const {
    if (!(GetLengthSq() < DEGENERATE_EPSILON)) {
        float dx = m_dir.x;
        float dy = m_dir.y;
        float dz = m_dir.z;
        t = (dx * (point.x - m_start.x) + dy * (point.y - m_start.y) + dz * (point.z - m_start.z)) /
            GetLengthSq();
        out.Scale(m_dir, t);
        out.Add(m_start);
    } else {
        out.Copy(m_start);
        t = 0.5f;
    }
}

// Magic Software's segment-segment distance (Eberly), with this segment as
// segment 0 and `seg` as segment 1.
bool CLine3::GetSquaredDistanceBetweenSegments(const CLine3& seg, float& s, float& t,
                                               float* sqrDist) const {
    float diffX = m_start.x - seg.m_start.x;
    float diffY = m_start.y - seg.m_start.y;
    float diffZ = m_start.z - seg.m_start.z;
    float a00 = GetLengthSq();
    float a01 = -(m_dir.x * seg.m_dir.x + m_dir.y * seg.m_dir.y + m_dir.z * seg.m_dir.z);
    float a11 = seg.GetLengthSq();
    float b0 = m_dir.x * diffX + m_dir.y * diffY + m_dir.z * diffZ;
    float c = diffX * diffX + diffY * diffY + diffZ * diffZ;
    float det = fabs(a00 * a11 - a01 * a01);
    float b1 = -(seg.m_dir.x * diffX + seg.m_dir.y * diffY + seg.m_dir.z * diffZ);
    float dist;
    float tmp;

    bool parallel = det < PARALLEL_EPSILON;
    if (!parallel) {
        s = a01 * b1 - a11 * b0;
        t = a01 * b0 - a00 * b1;

        if (s >= 0.0f) {
            if (s <= det) {
                if (t >= 0.0f) {
                    if (t <= det) {  // region 0 (interior)
                        float invDet = 1.0f / det;
                        s *= invDet;
                        t *= invDet;
                        dist = s * (a00 * s + a01 * t + 2.0f * b0) +
                               t * (a01 * s + a11 * t + 2.0f * b1) + c;
                    } else {  // region 3 (side)
                        t = 1.0f;
                        tmp = a01 + b0;
                        if (tmp >= 0.0f) {
                            s = 0.0f;
                            dist = a11 + 2.0f * b1 + c;
                        } else if (-tmp >= a00) {
                            s = 1.0f;
                            dist = a00 + a11 + c + 2.0f * (b1 + tmp);
                        } else {
                            s = -tmp / a00;
                            dist = tmp * s + a11 + 2.0f * b1 + c;
                        }
                    }
                } else {  // region 7 (side)
                    t = 0.0f;
                    if (b0 >= 0.0f) {
                        s = 0.0f;
                        dist = c;
                    } else if (-b0 >= a00) {
                        s = 1.0f;
                        dist = a00 + 2.0f * b0 + c;
                    } else {
                        s = -b0 / a00;
                        dist = b0 * s + c;
                    }
                }
            } else {
                if (t >= 0.0) {
                    if (t <= det) {  // region 1 (side)
                        s = 1.0f;
                        tmp = a01 + b1;
                        if (tmp >= 0.0f) {
                            t = 0.0f;
                            dist = a00 + 2.0f * b0 + c;
                        } else if (-tmp >= a11) {
                            t = 1.0f;
                            dist = a00 + a11 + c + 2.0f * (b0 + tmp);
                        } else {
                            t = -tmp / a11;
                            dist = tmp * t + a00 + 2.0f * b0 + c;
                        }
                    } else {  // region 2 (corner)
                        tmp = a01 + b0;
                        if (-tmp <= a00) {
                            t = 1.0f;
                            if (tmp >= 0.0f) {
                                s = 0.0f;
                                dist = a11 + 2.0f * b1 + c;
                            } else {
                                s = -tmp / a00;
                                dist = tmp * s + a11 + 2.0f * b1 + c;
                            }
                        } else {
                            s = 1.0f;
                            tmp = a01 + b1;
                            if (tmp >= 0.0f) {
                                t = 0.0f;
                                dist = a00 + 2.0f * b0 + c;
                            } else if (-tmp >= a11) {
                                t = 1.0f;
                                dist = a00 + a11 + c + 2.0f * (b0 + tmp);
                            } else {
                                t = -tmp / a11;
                                dist = tmp * t + a00 + 2.0f * b0 + c;
                            }
                        }
                    }
                } else {  // region 8 (corner)
                    if (-b0 < a00) {
                        t = 0.0f;
                        if (b0 >= 0.0f) {
                            s = 0.0f;
                            dist = c;
                        } else {
                            s = -b0 / a00;
                            dist = b0 * s + c;
                        }
                    } else {
                        s = 1.0f;
                        tmp = a01 + b1;
                        if (tmp >= 0.0f) {
                            t = 0.0f;
                            dist = a00 + 2.0f * b0 + c;
                        } else if (-tmp >= a11) {
                            t = 1.0f;
                            dist = a00 + a11 + c + 2.0f * (b0 + tmp);
                        } else {
                            t = -tmp / a11;
                            dist = tmp * t + a00 + 2.0f * b0 + c;
                        }
                    }
                }
            }
        } else {
            if (t >= 0.0f) {
                if (t <= det) {  // region 5 (side)
                    s = 0.0f;
                    if (b1 >= 0.0f) {
                        t = 0.0f;
                        dist = c;
                    } else if (-b1 >= a11) {
                        t = 1.0f;
                        dist = a11 + 2.0f * b1 + c;
                    } else {
                        t = -b1 / a11;
                        dist = b1 * t + c;
                    }
                } else {  // region 4 (corner)
                    tmp = a01 + b0;
                    if (tmp < 0.0f) {
                        t = 1.0f;
                        if (-tmp >= a00) {
                            s = 1.0f;
                            dist = a00 + a11 + c + 2.0f * (b1 + tmp);
                        } else {
                            s = -tmp / a00;
                            dist = tmp * s + a11 + 2.0f * b1 + c;
                        }
                    } else {
                        s = 0.0f;
                        if (b1 >= 0.0f) {
                            t = 0.0f;
                            dist = c;
                        } else if (-b1 >= a11) {
                            t = 1.0f;
                            dist = a11 + 2.0f * b1 + c;
                        } else {
                            t = -b1 / a11;
                            dist = b1 * t + c;
                        }
                    }
                }
            } else {  // region 6 (corner)
                if (b0 < 0.0f) {
                    t = 0.0f;
                    if (-b0 >= a00) {
                        s = 1.0f;
                        dist = a00 + 2.0f * b0 + c;
                    } else {
                        s = -b0 / a00;
                        dist = b0 * s + c;
                    }
                } else {
                    s = 0.0f;
                    if (b1 >= 0.0f) {
                        t = 0.0f;
                        dist = c;
                    } else if (-b1 >= a11) {
                        t = 1.0f;
                        dist = a11 + 2.0f * b1 + c;
                    } else {
                        t = -b1 / a11;
                        dist = b1 * t + c;
                    }
                }
            }
        }
    } else if (a01 > 0.0f) {
        // Parallel, directions at an obtuse angle.
        if (b0 >= 0.0f) {
            s = 0.0f;
            t = 0.0f;
            dist = c;
        } else if (-b0 <= a00) {
            s = -b0 / a00;
            t = 0.0f;
            dist = b0 * s + c;
        } else {
            s = 1.0f;
            tmp = a00 + b0;
            if (-tmp >= a01) {
                t = 1.0f;
                dist = a00 + a11 + c + 2.0f * (a01 + b0 + b1);
            } else {
                t = -tmp / a01;
                dist = a00 + 2.0f * b0 + c + t * (a11 * t + 2.0f * (a01 + b1));
            }
        }
    } else {
        // Parallel, directions at an acute angle.
        if (-b0 >= a00) {
            s = 1.0f;
            t = 0.0f;
            dist = a00 + 2.0f * b0 + c;
        } else if (b0 <= 0.0f) {
            s = -b0 / a00;
            t = 0.0f;
            dist = b0 * s + c;
        } else {
            s = 0.0f;
            if (b0 >= -a01) {
                t = 1.0f;
                dist = a11 + 2.0f * b1 + c;
            } else {
                t = -b0 / a01;
                dist = c + t * (2.0f * b1 + a11 * t);
            }
        }
    }

    if (sqrDist != NULL) {
        *sqrDist = fabs(dist);
    }
    return parallel;
}

bool CLine3::IsDegenerate() const {
    return GetLengthSq() < DEGENERATE_EPSILON;
}
