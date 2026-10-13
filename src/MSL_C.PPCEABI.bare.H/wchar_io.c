#include "ansi_files.h"

// Sets (mode > 0 wide, < 0 byte) or reports the stream's orientation.
int fwide(FILE* file, int mode) {
    int orientation;
    int result;

    if (file == NULL || file->mode.file_kind == __closed_file) {
        return 0;
    }

    orientation = file->mode.file_orientation;
    switch (orientation) {
    case __unoriented:
        if (mode > 0) {
            file->mode.file_orientation = __wide_oriented;
        } else if (mode < 0) {
            file->mode.file_orientation = __char_oriented;
        }
        result = mode;
        break;
    case __wide_oriented:
        result = 1;
        break;
    case __char_oriented:
        result = -1;
        break;
    }

    return result;
}
