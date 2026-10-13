// The game's movie playback: RCMP_PlayMovie streams a movie with its
// interleaved audio and shows it until it ends or (when skippable) a button
// is pressed, then prints playback stats.
#include "movie/rcmp_main.h"
#include "movie/avplayer.h"
#include "movie/DISP_OLD_FRAMES.h"
#include "game/Screen.h"
#include "game/input.h"
#include "dolphin/os.h"
#include "dolphin/gx.h"
#include "dolphin/vi.h"
#include "ea/real.h"
#include "ea/sys/printstr.h"
#include "ea/sys/timer.h"

FRAMEWORK g_fw;
OPTION_PARSING_PLAYER* CurentOPPlayer;

inline OPTION_PARSING_PLAYER::~OPTION_PARSING_PLAYER() {
    if (m_player != NULL) {
        delete m_player;
        m_player = NULL;
    }
}

void OPTION_PARSING_PLAYER::Play(bool skippable) {
    DISP_OLD_FRAMES times;
    int minRate = 99999999;
    u8 finished = 0;
    s64 total = 0;
    int frames = 0;
    u8 redraw = 0;
    int startTick;
    int memory;
    RCMP::FRAME* frame;
    unsigned int numFrames = 2;
    int startTime = 0x21;

    m_skippable = skippable;
    if (m_software) {
        numFrames = 1;
        startTime = 0x10;
    }
    frame = m_player->GetFirstFrame(numFrames, startTime);
    m_player->Pause();
    if (frame != NULL) {
        if (m_software) {
            GXRenderModeObj* rmode = g_screen.m_pRenderMode;
            void* xfb = g_screen.m_pDrawBuffer;
            m_driver = RCMP::GCSW_VD_create(m_player->m_decoder, frame, rmode, xfb);
            static int FirstTime = 1;
            if (FirstTime) {
                MEM_fill(xfb, 0, rmode->xfbHeight * rmode->fbWidth * 2);
                DCStoreRange(xfb, rmode->xfbHeight * rmode->fbWidth * 2);
                VISetBlack(FALSE);
                VIFlush();
                FirstTime = 0;
            }
        } else {
            m_driver = RCMP::MOHFL_VD_create(m_player->m_decoder, frame);
        }
        m_frameWidth = frame->m_shape->width;
        m_frameHeight = frame->m_shape->height;
        m_screenWidth = g_screen.GetWidth();
        m_screenHeight = g_screen.GetHeight();
        m_x = 0.5f * -(m_frameWidth * m_scaleX - m_screenWidth);
        m_y = 0.5f * -(m_frameHeight * m_scaleY - m_screenHeight);
        m_driver->SetPosition(m_x, m_y, 0.0f);
    }
    memory = MEM_totalunused(0);
    m_player->UnPause();
    startTick = TIMER_gettick();

    for (;;) {
        int start = OSGetTick() * 12;  // in CPU cycles
        if (!m_software) {
            g_screen.Wait();
        }
        if (redraw) {
            m_driver->ReDraw();
        } else {
            m_driver->Draw(frame);
        }
        if (!m_software) {
            GXDrawDone();
            g_screen.Flip();
        }
        SYNCTASK_run(0);
        THREAD_yield(0);

        if (frame == NULL) {
            ReadInput();
        } else if (m_noSkip) {
            m_driver->CompleteDraw();
            frame = m_player->GetFrame(1.0f + m_player->m_currentFrame);
            redraw = 0;
            ReadInput();
        } else if (m_waitRightTime) {
            while (!m_player->IsTimeForDecode() && !m_quit) {
                ReadInput();
                SYNCTASK_run(0);
                THREAD_yield(0);
            }
            m_driver->CompleteDraw();
            frame = m_player->GetFrame(m_player->m_frameTime);
            redraw = 0;
        } else {
            if (m_player->IsTimeForDecode()) {
                m_driver->CompleteDraw();
                frame = m_player->GetFrame(m_player->m_frameTime);
                redraw = 0;
            } else {
                redraw = 1;
            }
            ReadInput();
        }
        if (frame == NULL) {
            redraw = 1;
            finished = m_player->IsAudioFinished();
        }
        if (finished || m_quit) {
            break;
        }

        // frame stats
        u32 cpuClock = OS_CORE_CLOCK;
        frames++;
        int cycles = OSGetTick() * 12 - start;
        total += cycles;
        int rate = (f32)cpuClock / (f32)cycles;
        if (minRate > rate) {
            minRate = rate;
        }
        times.m_new[times.m_index] = (f32)cycles / (0.001f * (f32)cpuClock);
        if (++times.m_index >= 16) {
            times.m_index = 0;
            MEM_copy(times.m_old, times.m_new, sizeof(times.m_old));
        }
    }

    int endTick = TIMER_gettick();
    f32 cpuTime = (f32)total / (f32)OS_CORE_CLOCK;
    PRINT_string("Frames %d\n", frames);
    PRINT_string("Min FrameRate %d\n", minRate);
    PRINT_string("Ave FrameRate %.2f\n", (f32)OS_CORE_CLOCK / (f32)(total / frames));
    PRINT_string("CPU Time %.2f\n", cpuTime);
    PRINT_string("TIMER Time %.2f\n", (f32)(endTick - startTick) / (f32)TIMERhz);
    PRINT_string("Memory Available Durring Moive %.2fk %dBytes\n", memory * (1.0f / 1024.0f), memory);
    m_skippable = 1;
    if (m_driver != NULL) {
        delete m_driver;
        m_driver = NULL;
    }
}

// Any of the menu/face buttons (when skippable) ends the movie.
#pragma push
#pragma auto_inline off
void OPTION_PARSING_PLAYER::ReadInput() {
    g_inputMgr.Update(0.0f);
    if (m_skippable) {
        if (g_inputMgr.GetValueAnyPlayer(0x80)) {
            m_quit = 1;
        }
        if (g_inputMgr.GetValueAnyPlayer(0x79)) {
            m_quit = 1;
        }
        if (g_inputMgr.GetValueAnyPlayer(0x7F)) {
            m_quit = 1;
        }
        if (g_inputMgr.GetValueAnyPlayer(0x82)) {
            m_quit = 1;
        }
        if (g_inputMgr.GetValueAnyPlayer(0x81)) {
            m_quit = 1;
        }
        if (g_inputMgr.GetValueAnyPlayer(0x85)) {
            m_quit = 1;
        }
        if (g_inputMgr.GetValueAnyPlayer(0x84)) {
            m_quit = 1;
        }
        if (g_inputMgr.GetValueAnyPlayer(0x86)) {
            m_quit = 1;
        }
        if (g_inputMgr.GetValueAnyPlayer(0x7B)) {
            m_quit = 1;
        }
        if (g_inputMgr.GetValueAnyPlayer(0x7C)) {
            m_quit = 1;
        }
        if (g_inputMgr.GetValueAnyPlayer(0x7D)) {
            m_quit = 1;
        }
        if (g_inputMgr.GetValueAnyPlayer(0x7E)) {
            m_quit = 1;
        }
    }
}

#pragma pop

// Plays movie `name` (interleaved audio, streamed). The player object is
// written to after it is deleted (as in the original).
void RCMP_PlayMovie(char* name, bool skippable) {
    bool found;
    OPTION_PARSING_PLAYER* player = new OPTION_PARSING_PLAYER;
    CurentOPPlayer = player;
    if (!FILE_exists(name)) {
        PRINT_string("Couldn't find movie file %s\n", name);
        found = false;
    } else {
        player->m_player = new RCMP::AV_PLAYER(name, 0x100000, NULL, 0, RCMP::AV_PLAYER::LOAD_STREAM,
                                               RCMP::AV_PLAYER::SOUND_ON);
        found = true;
    }
    if (found) {
        CurentOPPlayer->m_playing = 1;
        CurentOPPlayer->Play(skippable);
        delete CurentOPPlayer;
        CurentOPPlayer->m_playing = 0;
    }
}

void RCMP_Shutdown() {
    g_fw.Restore();
}

void RCMP_Initialize() {
    g_fw.Init();
    RCMP::rcmp_sys.SetREALDefaults();
}
