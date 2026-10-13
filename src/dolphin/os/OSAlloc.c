#include <dolphin/os.h>

#define ALIGNMENT 32
#define MINOBJSIZE 64u
#define HEADERSIZE 32

#define RoundUp(x, align) (((x) + (align) - 1) & (-(align)))
#define RoundDown(x, align) ((x) & (-(align)))
#define RoundUpPtr(x, align) ((void*)RoundUp((u32)(x), (align)))
#define RoundDownPtr(x, align) ((void*)RoundDown((u32)(x), (align)))

// Header in front of every block, free or allocated.
typedef struct Cell {
    struct Cell* prev;
    struct Cell* next;
    long size;  // including the header
} Cell;

typedef struct HeapDesc {
    long size;  // negative when the slot is unused
    Cell* free;  // sorted by address
    Cell* allocated;
} HeapDesc;

volatile OSHeapHandle __OSCurrHeap = -1;

static HeapDesc* HeapArray;
static int NumHeaps;
static void* ArenaStart;
static void* ArenaEnd;

static Cell* DLAddFront(Cell* list, Cell* cell) {
    cell->next = list;
    cell->prev = NULL;
    if (list) {
        list->prev = cell;
    }
    return cell;
}

static Cell* DLExtract(Cell* list, Cell* cell) {
    if (cell->next) {
        cell->next->prev = cell->prev;
    }
    if (cell->prev == NULL) {
        return cell->next;
    } else {
        cell->prev->next = cell->next;
        return list;
    }
}

// Inserts cell into the address-ordered list, coalescing with its neighbours.
static Cell* DLInsert(Cell* list, Cell* cell) {
    Cell* prev;
    Cell* next;

    for (next = list, prev = NULL; next; prev = next, next = next->next) {
        if (cell <= next) {
            break;
        }
    }

    cell->next = next;
    cell->prev = prev;
    if (next) {
        next->prev = cell;
        if ((char*)cell + cell->size == (char*)next) {
            cell->size += next->size;
            cell->next = next = next->next;
            if (next) {
                next->prev = cell;
            }
        }
    }

    if (prev) {
        prev->next = cell;
        if ((char*)prev + prev->size == (char*)cell) {
            prev->size += cell->size;
            prev->next = next;
            if (next) {
                next->prev = prev;
            }
        }
        return list;
    } else {
        return cell;
    }
}

void* OSAllocFromHeap(OSHeapHandle heap, u32 size) {
    HeapDesc* hd;
    Cell* cell;
    Cell* newCell;
    long leftoverSize;

    hd = &HeapArray[heap];

    size += HEADERSIZE;
    size = RoundUp(size, ALIGNMENT);

    // First fit.
    for (cell = hd->free; cell != NULL; cell = cell->next) {
        if ((long)size <= cell->size) {
            break;
        }
    }

    if (cell == NULL) {
        return NULL;
    }

    leftoverSize = cell->size - (long)size;
    if (leftoverSize < MINOBJSIZE) {
        hd->free = DLExtract(hd->free, cell);
    } else {
        cell->size = (long)size;

        newCell = (Cell*)((char*)cell + size);
        newCell->size = leftoverSize;
        newCell->prev = cell->prev;
        newCell->next = cell->next;
        if (newCell->next != NULL) {
            newCell->next->prev = newCell;
        }
        if (newCell->prev != NULL) {
            newCell->prev->next = newCell;
        } else {
            hd->free = newCell;
        }
    }

    hd->allocated = DLAddFront(hd->allocated, cell);

    return (void*)((char*)cell + HEADERSIZE);
}

void OSFreeToHeap(OSHeapHandle heap, void* ptr) {
    HeapDesc* hd;
    Cell* cell;

    cell = (Cell*)((char*)ptr - HEADERSIZE);
    hd = &HeapArray[heap];

    hd->allocated = DLExtract(hd->allocated, cell);
    hd->free = DLInsert(hd->free, cell);
}

OSHeapHandle OSSetCurrentHeap(OSHeapHandle heap) {
    OSHeapHandle prev;

    prev = __OSCurrHeap;
    __OSCurrHeap = heap;
    return prev;
}

void* OSInitAlloc(void* arenaStart, void* arenaEnd, int maxHeaps) {
    u32 arraySize;
    OSHeapHandle i;

    arraySize = sizeof(HeapDesc) * maxHeaps;
    HeapArray = arenaStart;
    NumHeaps = maxHeaps;

    for (i = 0; i < NumHeaps; i++) {
        HeapDesc* hd = &HeapArray[i];

        hd->size = -1;
        hd->free = hd->allocated = NULL;
    }

    __OSCurrHeap = -1;

    arenaStart = (void*)((char*)HeapArray + arraySize);
    arenaStart = RoundUpPtr(arenaStart, ALIGNMENT);

    ArenaStart = arenaStart;
    ArenaEnd = RoundDownPtr(arenaEnd, ALIGNMENT);

    return arenaStart;
}

OSHeapHandle OSCreateHeap(void* start, void* end) {
    OSHeapHandle heap;
    HeapDesc* hd;
    Cell* cell;

    start = RoundUpPtr(start, ALIGNMENT);
    end = RoundDownPtr(end, ALIGNMENT);

    for (heap = 0; heap < NumHeaps; heap++) {
        hd = &HeapArray[heap];
        if (hd->size < 0) {
            hd->size = (char*)end - (char*)start;

            cell = (Cell*)start;
            cell->prev = NULL;
            cell->next = NULL;
            cell->size = hd->size;

            hd->free = cell;
            hd->allocated = NULL;
            return heap;
        }
    }

    return -1;
}
