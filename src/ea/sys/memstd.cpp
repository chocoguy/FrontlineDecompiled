#include "ea/sys/mem.h"

extern "C" int MEM_size(void* ptr) {
    return MEM_header(ptr)->size;
}
