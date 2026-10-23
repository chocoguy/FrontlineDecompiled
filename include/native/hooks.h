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

#ifdef __cplusplus
}
#endif

#endif
