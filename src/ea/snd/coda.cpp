#include "ea/snd/eaxadecf.h"

namespace SND {

void* (*CODANew)(unsigned long size);
void (*CODADelete)(void* ptr);

void CODASetNew(void* (*fn)(unsigned long)) {
    CODANew = fn;
}

void CODASetDelete(void (*fn)(void*)) {
    CODADelete = fn;
}

}  // namespace SND
