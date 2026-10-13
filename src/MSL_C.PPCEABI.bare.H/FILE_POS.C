#include "ansi_files.h"

int fseek(FILE* file, long offset, int whence) {
    return _fseek(file, (fpos_t)offset, whence);
}

int _fseek(FILE* file, fpos_t offset, int whence) {
    fpos_t position;
    __pos_proc pos_proc;

    if (file->mode.file_kind != __disk_file || file->state.error) {
        errno = EFPOS;
        return -1;
    }

    if (file->state.io_state == __writing) {
        if (__flush_buffer(file, NULL) != __no_io_error) {
            set_error(file);
            errno = EFPOS;
            return -1;
        }
    }

    if (whence == SEEK_CUR) {
        whence = SEEK_SET;
        position = ftell(file);
        offset += position;
    }

    if (whence != SEEK_END && file->mode.io_mode != __read_write &&
        (file->state.io_state == __reading || file->state.io_state == __rereading)) {
        if (offset >= file->position || offset < file->buffer_pos) {
            file->state.io_state = __neutral;
        } else {
            file->buffer_ptr = file->buffer + (offset - file->buffer_pos);
            file->buffer_len = file->position - offset;
            file->state.io_state = __reading;
        }
    } else {
        file->state.io_state = __neutral;
    }

    if (file->state.io_state == __neutral) {
        if ((pos_proc = file->position_proc) != NULL && (*pos_proc)(file->handle, &offset, whence, file->idle_proc)) {
            set_error(file);
            errno = EFPOS;
            return -1;
        }

        file->state.eof = 0;
        file->position = offset;
        file->buffer_len = 0;
    }

    return 0;
}

long ftell(FILE* file) {
    int charsInUndoBuffer = 0;
    unsigned long position;
    unsigned char kind = file->mode.file_kind;

    if (!(kind == __disk_file || kind == __console_file) || file->state.error) {
        errno = EFPOS;
        return -1;
    }

    if (file->state.io_state == __neutral) {
        return file->position;
    }

    position = file->buffer_pos + (file->buffer_ptr - file->buffer);

    if (file->state.io_state >= __rereading) {
        charsInUndoBuffer = file->state.io_state - __reading;
        position -= charsInUndoBuffer;
    }

    return position;
}
