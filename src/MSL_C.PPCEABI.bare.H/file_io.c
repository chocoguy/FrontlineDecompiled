#include "ansi_files.h"

int fclose(FILE* file) {
    int flush_result, close_result;

    if (file == NULL) {
        return EOF;
    }
    if (file->mode.file_kind == __closed_file) {
        return 0;
    }

    flush_result = fflush(file);
    close_result = (*file->close_proc)(file->handle);

    file->mode.file_kind = __closed_file;
    file->handle = 0;

    if (file->state.free_buffer) {
        free(file->buffer);
    }

    return ((flush_result || close_result) ? EOF : 0);
}

#pragma auto_inline off
int fflush(FILE* file) {
    fpos_t position;

    if (!file) {
        return __flush_all();
    }

    if (file->state.error || file->mode.file_kind == __closed_file) {
        return EOF;
    }

    if (file->mode.io_mode == __read) {
        return 0;
    }

    if (file->state.io_state >= __rereading) {
        file->state.io_state = __reading;
    }

    if (file->state.io_state == __reading) {
        file->buffer_len = 0;
    }

    if (file->state.io_state != __writing) {
        file->state.io_state = __neutral;
        return 0;
    }

    if (file->mode.file_kind != __disk_file) {
        position = 0;
    } else {
        position = ftell(file);
    }

    if (__flush_buffer(file, NULL)) {
        set_error(file);
        return EOF;
    }

    file->state.io_state = __neutral;
    file->position = position;
    file->buffer_len = 0;
    return 0;
}
#pragma auto_inline reset

FILE* fopen(const char* name, const char* mode) {
    return freopen(name, mode, __find_unopened_file());
}

#pragma auto_inline off
FILE* freopen(const char* name, const char* mode, FILE* file) {
    __file_modes modes;

    __stdio_atexit();

    if (!file) {
        return NULL;
    }

    fclose(file);
    clearerr(file);

    if (!__get_file_modes(mode, &modes)) {
        return NULL;
    }

    __init_file(file, modes, 0, BUFSIZ);

    if (__open_file(name, modes, &file->handle)) {
        file->mode.file_kind = __closed_file;
        if (file->state.free_buffer) {
            free(file->buffer);
        }
        return NULL;
    }

    if (modes.io_mode & __append) {
        fseek(file, 0, SEEK_END);
    }

    return file;
}
#pragma auto_inline reset

// Parses an fopen mode string ("r", "wb", "a+", ...).
int __get_file_modes(const char* mode, __file_modes* modes) {
    const char* mode_ptr = mode;
    unsigned long mode_str;
    unsigned char open_mode, io_mode;

    modes->file_kind = __disk_file;
    modes->file_orientation = __unoriented;
    modes->binary_io = 0;

    mode_str = *mode_ptr++;

    switch (mode_str) {
    case 'r':
        open_mode = __must_exist;
        break;
    case 'w':
        open_mode = __create_or_truncate;
        break;
    case 'a':
        open_mode = __create_if_necessary;
        break;
    default:
        return 0;
    }

    modes->open_mode = open_mode;

    switch (*mode_ptr++) {
    case 'b':
        modes->binary_io = 1;
        if (*mode_ptr == '+') {
            mode_str = (mode_str << 8) | '+';
        }
        break;
    case '+':
        mode_str = (mode_str << 8) | '+';
        if (*mode_ptr == 'b') {
            modes->binary_io = 1;
        }
        break;
    }

    switch (mode_str) {
    case 'r':
        io_mode = __read;
        break;
    case 'w':
        io_mode = __write;
        break;
    case 'a':
        io_mode = __write | __append;
        break;
    case 'r+':
        io_mode = __read_write;
        break;
    case 'w+':
        io_mode = __read_write;
        break;
    case 'a+':
        io_mode = __read_write | __append;
        break;
    }

    modes->io_mode = io_mode;
    return 1;
}
