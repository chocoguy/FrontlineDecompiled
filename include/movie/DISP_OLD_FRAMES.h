#ifndef _MOVIE_DISP_OLD_FRAMES_H_
#define _MOVIE_DISP_OLD_FRAMES_H_

#include "types.h"

// Play's frame time history (stats only): times in ms per frame for the
// last 16 frames, copied to the old set every 16. size 0x84
class DISP_OLD_FRAMES {
public:
    /* 0x00 */ int m_index;
    /* 0x04 */ f32 m_old[16];
    /* 0x44 */ f32 m_new[16];

    DISP_OLD_FRAMES() {
        m_index = 0;
        for (int i = 0; i < 16; i++) {
            m_old[i] = 0.0f;
            m_new[i] = 0.0f;
        }
    }
    ~DISP_OLD_FRAMES() {}  // 0x80103E58 (weak)
};

#endif
