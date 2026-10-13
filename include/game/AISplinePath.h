// Spline paths: the level's poly paths turned into chains of cubic segments
// (a natural cubic spline through the points), and the traversal state of
// an AI walking one. Hand-maintained.
#ifndef _GAME_AISPLINEPATH_H_
#define _GAME_AISPLINEPATH_H_

#include "types.h"
#include "game_types.h"

class CAIFilterObject;
class CAIFilterRealPosition;
class CAIFilterRealVector3;
class CAISplinePath;
class CAISplinePathManager;
class CAISplinePathSegment;
class CAISplinePathTraversal;
class CVector3;

// A path point as stored in the level data (x, y, z).
struct _aifilter_simple_v3 {
    /* 0x0 */ f32 x;
    /* 0x4 */ f32 y;
    /* 0x8 */ f32 z;
};

// A 16-byte vector as the spline code copies them.
struct AISplineVec {
    /* 0x0 */ f32 x;
    /* 0x4 */ f32 y;
    /* 0x8 */ f32 z;
    /* 0xC */ f32 w;
};

// All the level's spline paths, each generated forwards (index i) and
// backwards (index i + m_numPaths / 2).
// size 0x18 (operator new)
class CAISplinePathManager {
public:
    /* 0x0 */ u32 m_numPaths;
    /* 0x4 */ CAISplinePath* m_paths;
    // Scratch buffers used while generating (AllocateGenerateBuffers).
    /* 0x8 */ AISplineVec* m_points;
    /* 0xC */ f32* m_gamma;
    /* 0x10 */ AISplineVec* m_delta;
    /* 0x14 */ AISplineVec* m_tangents;
    void GenerateSplinePath(unsigned long, unsigned long, unsigned long, const _aifilter_simple_v3*);  // 0x8004FA94
    void FreeGenerateBuffers();  // 0x8004FB34
    void AllocateGenerateBuffers();  // 0x8004FB90
    CAISplinePathManager(unsigned long);  // 0x8004FC18
};

// CAISplinePathTraversal::m_state
enum {
    SPLINE_IDLE = 0,
    SPLINE_WALK = 1,          // on the spline
    SPLINE_ASTAR = 2,         // A* walk to the spline's start first
    SPLINE_WALK_ON = 3,       // on the spline, past the first check
    SPLINE_ASTAR_ON = 4,
    SPLINE_END = 5,           // heading for the final point
    SPLINE_ASTAR_END = 6,
    SPLINE_DONE = 7,
};

// size 0x24
class CAISplinePathTraversal {
public:
    /* 0x0 */ u32 m_state;
    /* 0x4 */ f32 m_param;         // position on the current segment, 0..1
    /* 0x8 */ f32 m_lastDistSq;    // to m_point at the last check
    /* 0xC */ u32 m_segmentIndex;
    /* 0x10 */ CAISplinePath* m_path;
    /* 0x14 */ CAISplinePathSegment* m_segment;
    /* 0x18 */ f32 m_point[3];     // the point to walk to
    void GetFinalPointForward(CAIFilterRealPosition*);  // 0x8004FCBC
    void GetNextPointForward(const CAIFilterRealPosition&, CAIFilterRealPosition*, CAIFilterObject*);  // 0x8004FDD4
    void ContinueTraversalForwardAStar(CAIFilterObject*);  // 0x8005039C
    void PrepareForTraversalForward(CAISplinePath*, bool, CAIFilterObject*);  // 0x8005041C
    void SetupNextSegmentForward(const CAIFilterRealPosition&);  // 0x800505B0
    void MakeModifiedNextPoint();  // 0x80050760
    ~CAISplinePathTraversal();  // 0x80050AA8
    CAISplinePathTraversal();  // 0x80050AE4
};

// size 0xC
class CAISplinePath {
public:
    /* 0x0 */ CAISplinePathSegment* m_segments;  // linked through m_pNext too
    /* 0x4 */ u32 m_lastSegment;  // the last segment is the end point
    /* 0x8 */ u32 m_id;  // poly path id (+ 0x1000000 when reversed)
    void GenerateTestSplinePath(const CAISplinePathManager&, unsigned long, const _aifilter_simple_v3*, bool);  // 0x80050AF4
    ~CAISplinePath();  // 0x80051674
    CAISplinePath();  // 0x800516B0
};

// P(t) = ((a t + b) t + c) t + d for t in 0..1. size 0x48
class CAISplinePathSegment {
public:
    /* 0x0 */ CAISplinePathSegment* m_pNext;
    /* 0x4 */ f32 m_step;  // parameter step of about 0.25 units of length
    /* 0x8 */ AISplineVec m_a;
    /* 0x18 */ AISplineVec m_b;
    /* 0x28 */ AISplineVec m_c;
    /* 0x38 */ AISplineVec m_d;
    float GetClosestParameter(float, const CAIFilterRealVector3&);  // 0x800516B4
    void ExpandDerivative(float, CVector3&);  // 0x80051AD4
    void Expand(float, CVector3&);  // 0x80051B90
    ~CAISplinePathSegment();  // 0x80051C90
    CAISplinePathSegment();  // 0x80051CCC
};

#endif
