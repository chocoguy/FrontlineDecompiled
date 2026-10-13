#include "ea/sys/mem.h"
#include "dolphin/os.h"

// Drop every class, giving OS heap blocks back.
extern "C" void MEM_restore(void) {
    int i;
    int j;

    // A class registered under several ids is removed once.
    for (i = 0; i < 64; i++) {
        for (j = 0; j < 64; j++) {
            if (i != j && memclass[i] == memclass[j] && memclass[j] != NULL) {
                memclass[j] = NULL;
            }
        }
    }
    for (i = 62; i >= 0; i--) {
        if (memclass[i] != NULL) {
            void* block = memclass[i]->osBlock;
            MEMCLASS_remove(i);
            if (block != NULL) {
                OSFreeToHeap(__OSCurrHeap, block);
            }
            memclass[i] = NULL;
        }
    }
}
