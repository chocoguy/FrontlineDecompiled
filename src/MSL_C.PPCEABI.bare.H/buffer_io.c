#include "ansi_files.h"

int setvbuf(FILE* file, char* buff, int mode, size_t size) {
    int kind = file->mode.file_kind;

    if (mode == _IONBF) {
        fflush(file);
    }

    if (file->state.io_state != __neutral || kind == __closed_file) {
        return -1;
    }

    if (mode != _IONBF && mode != _IOLBF && mode != _IOFBF) {
        return -1;
    }

    if (file->buffer && file->state.free_buffer) {
        free(file->buffer);
    }

    file->mode.buffer_mode = mode;
    file->state.free_buffer = 0;
    file->buffer = &file->char_buffer;
    file->buffer_ptr = &file->char_buffer;
    file->buffer_size = 1;
    file->buffer_len = 0;
    file->buffer_alignment = 0;

    if (mode == _IONBF || size < 1) {
        *(file->buffer_ptr) = 0;
        return 0;
    }

    if (!buff) {
        if (!(buff = (char*)malloc(size))) {
            return -1;
        }
        file->state.free_buffer = 1;
    }

    file->buffer = (unsigned char*)buff;
    file->buffer_ptr = file->buffer;
    file->buffer_size = size;
    file->buffer_alignment = 0;

    return 0;
}

int __flush_buffer(FILE* file, size_t* bytes_flushed) {
    size_t buffer_len;
    int ioresult;

    buffer_len = file->buffer_ptr - file->buffer;

    if (buffer_len) {
        file->buffer_len = buffer_len;

        ioresult = (*file->write_proc)(file->handle, file->buffer, &file->buffer_len, file->idle_proc);

        if (bytes_flushed) {
            *bytes_flushed = file->buffer_len;
        }

        if (ioresult) {
            return ioresult;
        }

        file->position += file->buffer_len;
    }

    __prep_buffer(file);
    return __no_io_error;
}

int __load_buffer(FILE* file, size_t* bytes_loaded, int alignment) {
    int ioresult;

    __prep_buffer(file);

    if (alignment == 1) {
        file->buffer_len = file->buffer_size;
    }

    ioresult = (*file->read_proc)(file->handle, file->buffer, &file->buffer_len, file->idle_proc);

    if (ioresult == __io_EOF) {
        file->buffer_len = 0;
    }

    if (bytes_loaded) {
        *bytes_loaded = file->buffer_len;
    }

    if (ioresult) {
        return ioresult;
    }

    file->position += file->buffer_len;
    return __no_io_error;
}

void __prep_buffer(FILE* file) {
    file->buffer_ptr = file->buffer;
    file->buffer_len = file->buffer_size;
    file->buffer_len -= file->position & file->buffer_alignment;
    file->buffer_pos = file->position;
}
