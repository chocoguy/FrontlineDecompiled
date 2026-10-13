#include "ea/sys/mutex2.h"
#include "ea/real.h"
#include "dolphin/os.h"

struct MUTEX {
    /* 0x00 */ u32 unk0;
    /* 0x04 */ OSMutex mutex;
};

extern "C" int MUTEX_create(void* mutex) {
    OSInitMutex(&((MUTEX*)mutex)->mutex);
    return 1;
}

extern "C" void MUTEX_destroy(void* mutex) {
    MEM_fill(mutex, 0xDEADBEEF, 4);
}

extern "C" void MUTEX_lock(void* mutex) {
    OSLockMutex(&((MUTEX*)mutex)->mutex);
}

extern "C" void MUTEX_unlock(void* mutex) {
    OSUnlockMutex(&((MUTEX*)mutex)->mutex);
}
