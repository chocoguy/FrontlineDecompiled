// Native build only: what the platform layer provides to the shared game
// code. Implemented in FrontlineRewritten's platform/.
#ifndef _NATIVE_HOOKS_H_
#define _NATIVE_HOOKS_H_

#ifdef __cplusplus
extern "C" {
#endif

// True on a game thread holding the baton (docs/architecture/0002): only those
// may touch the game's heaps.
int NativeOnGameThread(void);

// Frees game-heap memory released from another thread, at the next safe
// point on a game thread.
void NativeFreeLater(void* p);

// SPIN_WAIT() in types.h: sleeps until an interrupt is raised, then delivers
// it. The baton stays with the caller, as the CPU would on the console.
void NativeSpinWait(void);

// The way to play, chosen on the app's setup screen for the whole run:
// Classic keeps the original game; Rewritten turns on deliberate changes
// (ROADMAP, "Two modes"). Classic is the default path.
enum {
    NATIVE_MODE_CLASSIC,
    NATIVE_MODE_REWRITTEN,
};
int NativeGameMode(void);

// The width of the picture the game renders in a level, in the console's
// pixels: 640 (4:3), or wider for widescreen in Rewritten (a multiple of 16:
// 768 for 16:10, 848 for 16:9). Its 2D stays a 640 x 448 screen in the middle.
int NativeRenderWidth(void);

// Rewritten's high frame rate: the display shows up to 120 frames a second
// by drawing frames between the game's from its recorded GX draws, with the
// camera moved on. The game itself still runs at 59.94. 0 in Classic.
int NativeHighFrameRate(void);

// Rewritten's draw distance: times the level's own, for its far plane and fog
// alike (so the fog looks the same, only farther out). 1 in Classic.
float NativeDrawDistance(void);

// The normal (unzoomed) view's half-angle in degrees: 35 in Classic (70
// degrees across at 4:3), or Rewritten's field of view from the app's
// Settings, which can change at any time (FRONTLINE_FOV=<degrees across>).
float NativeBaseFov(void);

// Rewritten's movies: marks a GXTexObj's texels as rows (the decoder's
// planes as they are) rather than the console's blocks, filtered linearly,
// so the movie can be drawn without the swizzle that needs point sampling.
void NativeLinearTexture(void* texObj);

// What the frames drawn between the game's need to know about this one
// (platform/src/gx/frame_motion.h). Cheap and harmless with the high frame
// rate off.
// - The camera a scene was drawn with (CScene::Draw), once a viewport: world
//   to eye as GX takes it, and the projection's x and y scales.
void NativeFrameView(const float worldToEye[3][4], float scaleX, float scaleY);
// - The player's view this frame (single player, Gameloop_Loop): how it moves
//   (NATIVE_VIEW_*), the zoom scale AddMouseLook gives the mouse, and its
//   pitch.
enum {
    NATIVE_VIEW_ATTACHED = 1,  // carried by something (a landing craft, a cutscene)
    NATIVE_VIEW_ON_GUN = 2,    // on a mounted gun, which turns with the view
    NATIVE_VIEW_LOOK = 4,      // the mouse turns the view
};
void NativePlayerView(int flags, float lookScale, float pitch);
// - What the GX draws from now on are, which says how they move (the render
//   bins tell; orthographic draws are always fixed to the screen).
enum {
    NATIVE_DRAW_WORLD,
    NATIVE_DRAW_SKY,        // the sky box: through the camera's rotation only
    NATIVE_DRAW_VIEWMODEL,  // the weapon in hand: fixed to the camera
};
void NativeDrawClass(int cls);
// - The culling frustum's half-width (or, `vertical`, half-height) tangent
//   `t`, widened by what those frames may turn the camera (t itself with the
//   high frame rate off).
float NativeCullTan(float t, int vertical);

// Keyboard and mouse: what the player does, as game actions rather than
// GameCube buttons, so the bindings don't depend on the controller layout
// picked in the game's options. CInputManager turns them into input events.
enum {
    // Playing (in a level, not paused).
    NATIVE_ACTION_FORWARD = 1 << 0,
    NATIVE_ACTION_BACK = 1 << 1,
    NATIVE_ACTION_LEFT = 1 << 2,
    NATIVE_ACTION_RIGHT = 1 << 3,
    NATIVE_ACTION_WALK = 1 << 4,  // half speed, as a stick pushed halfway
    NATIVE_ACTION_JUMP = 1 << 5,
    NATIVE_ACTION_CROUCH = 1 << 6,
    NATIVE_ACTION_AIM = 1 << 7,
    NATIVE_ACTION_FIRE = 1 << 8,
    NATIVE_ACTION_RELOAD = 1 << 9,
    NATIVE_ACTION_NEXT_WEAPON = 1 << 10,
    NATIVE_ACTION_PREV_WEAPON = 1 << 11,
    NATIVE_ACTION_MELEE = 1 << 12,
    NATIVE_ACTION_USE = 1 << 13,
    NATIVE_ACTION_CENTER_VIEW = 1 << 14,
    NATIVE_ACTION_HINT = 1 << 15,
    NATIVE_ACTION_SCORES = 1 << 16,  // multiplayer frag counts
    NATIVE_ACTION_PAUSE = 1 << 17,
    // Menus (the shell, the pause menu, prompts): the console's buttons.
    NATIVE_ACTION_MENU_UP = 1 << 18,
    NATIVE_ACTION_MENU_DOWN = 1 << 19,
    NATIVE_ACTION_MENU_LEFT = 1 << 20,
    NATIVE_ACTION_MENU_RIGHT = 1 << 21,
    NATIVE_ACTION_MENU_A = 1 << 22,
    NATIVE_ACTION_MENU_B = 1 << 23,
    NATIVE_ACTION_MENU_X = 1 << 24,
    NATIVE_ACTION_MENU_Y = 1 << 25,
    NATIVE_ACTION_MENU_Z = 1 << 26,
    NATIVE_ACTION_MENU_L = 1 << 27,
    NATIVE_ACTION_MENU_R = 1 << 28,
    NATIVE_ACTION_MENU_START = 1 << 29,
};

typedef struct {
    unsigned int held;     // NATIVE_ACTION_* bits held now
    unsigned int pressed;  // pressed since the last take, even if already let go
    float lookRight;       // mouse movement since the last take, in radians
    float lookUp;
    int wheel;  // scroll wheel notches since the last take, up positive
} NativeInput;

// Takes the keyboard and mouse input for controller `port` since the last
// take (once per frame, from CInputManager::Update). `playing` tells the app
// whether the mouse is looking around now, so it can capture the pointer.
void NativeTakeInput(int port, int playing, NativeInput* input);

// On-screen controls (prompts and menu button icons) show the keyboard's
// keys or the controller's own text and icons, by which was used last.
enum {
    NATIVE_DEVICE_KEYBOARD,
    NATIVE_DEVICE_CONTROLLER,
};
int NativePromptDevice(void);

// Counts changes of NativePromptDevice, so callers redo work only then.
unsigned int NativePromptDeviceChanges(void);

// What the keyboard and mouse bind to the actions prompts name, as players
// read it: "F", "Right Mouse Button", "Esc".
enum {
    NATIVE_LABEL_USE,
    NATIVE_LABEL_CROUCH,
    NATIVE_LABEL_AIM,
    NATIVE_LABEL_PAUSE,
    NATIVE_LABEL_HINT,
    NATIVE_LABEL_SCOPE_ZOOM,
    NATIVE_LABEL_MENU_A,
    NATIVE_LABEL_MENU_B,
    NATIVE_LABEL_MENU_X,
    NATIVE_LABEL_MENU_Y,
    NATIVE_NUM_LABELS,
};
const char* NativeKeyLabel(int label);

// The keyboard's version of a message that names a controller button
// ("Press ACTION to Open Door" -> "Press F to Open Door"): writes it to `out`
// (at most `size` - 1 characters) and returns 1, or returns 0 if `text`
// names no button.
int NativeKeyboardPrompt(const char* text, char* out, int size);

// Menu button icons for the keyboard, in place of the controller's.
enum {
    NATIVE_ICON_KEY,         // a key with a label on it
    NATIVE_ICON_ARROW_KEYS,  // the four arrow keys, dim, in a plus (the D-pad)
    NATIVE_ICON_ARROW_UP,    // one arrow key, lit (laid over the plus)
    NATIVE_ICON_ARROW_DOWN,
    NATIVE_ICON_ARROW_LEFT,
    NATIVE_ICON_ARROW_RIGHT,
};

// Draws `icon` (with `label` for a key) into `rgba`: width x height pixels,
// rows from the top, alpha not premultiplied.
void NativeRenderKeyIcon(int icon, const char* label, int width, int height, unsigned char* rgba);

#ifdef __cplusplus
}
#endif

#endif
