// Points the movie player's memory hooks at the REAL allocator.
#include "movie/rcmp2real.h"
#include "ea/real.h"

void RCMP::RCMP_SYSTEM::SetREALDefaults() {
    m_alloc = MEM_allocalign;
    m_free = (void (*)(void*))MEM_free;
    m_allocParam = 0;
}
