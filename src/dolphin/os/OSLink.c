#include <dolphin/os.h>

typedef struct OSModuleInfo OSModuleInfo;

typedef struct OSModuleQueue {
    OSModuleInfo* head;
    OSModuleInfo* tail;
} OSModuleQueue;

#define __OSModuleList (*(OSModuleQueue*)0x800030C8)
#define __OSStringTable (*(const char**)0x800030D0)

void __OSModuleInit(void) {
    __OSModuleList.head = __OSModuleList.tail = NULL;
    __OSStringTable = NULL;
}
