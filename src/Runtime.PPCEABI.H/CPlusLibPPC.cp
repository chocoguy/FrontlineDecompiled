typedef unsigned long size_t;

extern "C" void* __copy(char* to, char* from, size_t size) {
    char* p;

    if (to && size) {
        p = to;
        do {
            *p = *from;
            ++p;
            ++from;
            --size;
        } while (size);
    }

    return to;
}
