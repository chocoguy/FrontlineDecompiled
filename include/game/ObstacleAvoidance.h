#ifndef _GAME_OBSTACLEAVOIDANCE_H_
#define _GAME_OBSTACLEAVOIDANCE_H_

#include "types.h"
#include "game_types.h"
#include "game/AIFilter.h"

class CAIObject;

// One probe line of the avoidance fan, swept at three heights (size 0x20).
class CObstacleGridEntry {
public:
    /* 0x00 */ f32 m_dist[3];        // distance to the hit, per height pass
    /* 0x0C */ bool m_blocked[3];    // m_dist closer than the probe range
    /* 0x10 */ CAIFilterRealPosition m_end;  // end point of the probe line

    CObstacleGridEntry();
    ~CObstacleGridEntry();

    void SetDist(int pass, float dist, float range) {
        m_dist[pass] = dist;
        m_blocked[pass] = dist < range;
    }
    int GetBlockedMask() const {
        int mask = 0;
        if (m_blocked[2]) mask = 1;
        if (m_blocked[1]) mask += 2;
        if (m_blocked[0]) mask += 4;
        return mask;
    }
};

#define OBSTACLE_NUM_LINES 13
#define OBSTACLE_CENTER_LINE 5

enum {
    OBSTACLE_STATE_NONE = 0,
    OBSTACLE_STATE_STEP_DONE = 3,  // reached the step point; owner checks downwards
    OBSTACLE_STATE_STEP = 4,       // walking to m_stepPosition (low obstacle)
};

// Probes a fan of lines in front of an AI, one line per update, and picks a
// free direction when the way to the objective is blocked (size 0x3A0).
class CObstacleAvoidance {
public:
    /* 0x000 */ u16 m_line;            // probe line checked next
    /* 0x002 */ s16 m_centerHold[3];   // center line keeps a hit for a few sweeps
    /* 0x008 */ s16 m_balance;         // hits minus misses over one sweep
    /* 0x00C */ f32 m_range;           // probe length
    /* 0x010 */ f32 m_scale;           // m_range / 112.5
    /* 0x014 */ u8 unk_0x14[0x4];
    /* 0x018 */ CObstacleGridEntry m_grid[OBSTACLE_NUM_LINES];  // last complete sweep
    /* 0x1B8 */ CObstacleGridEntry m_work[OBSTACLE_NUM_LINES];  // sweep in progress
    /* 0x358 */ f32 m_upClearance;
    /* 0x35C */ f32 m_downClearance;
    /* 0x360 */ f32 m_height;          // probe height of the current pass
    /* 0x364 */ s32 m_pass;            // 0..2
    /* 0x368 */ CAIFilterRealPosition m_target;
    /* 0x378 */ CAIFilterRealPosition m_stepPosition;
    /* 0x388 */ CAIObject* m_pOwner;
    /* 0x38C */ f32 field_0x38C;
    /* 0x390 */ f32 field_0x390;
    /* 0x394 */ f32 m_stepHeight;      // 0 = cannot step over low obstacles
    /* 0x398 */ s32 m_state;
    /* 0x39C */ s32 m_side;            // 1 = prefer left, 2 = prefer right

    void DetermineNewObjectivePosition(CAIFilterRealPosition&);  // 0x80053738
    void RunEnvironmentCollisionChecks();  // 0x80053C34
    void PrepareForAvoidance(CAIFilterRealPosition&);  // 0x80054304
    ~CObstacleAvoidance();  // 0x80054434
    CObstacleAvoidance();  // 0x800544B0
};

#endif
