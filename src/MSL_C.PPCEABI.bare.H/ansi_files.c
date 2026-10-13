#include "ansi_files.h"

static unsigned char stdin_buff[_MSL_CONSOLE_BUFSIZ];
static unsigned char stdout_buff[_MSL_CONSOLE_BUFSIZ];
static unsigned char stderr_buff[_MSL_CONSOLE_BUFSIZ];

// stdin, stdout, stderr and an empty slot; more FILEs are malloc'd and chained
// after the last one.
FILE __files[_STATIC_FILES] = {
    {0, {__must_exist, __read, _IOLBF, __console_file, __unoriented, 0}, {__neutral, 0, 0, 0}, 0, 0, 0, {0, 0},
     {0, 0}, 0, stdin_buff, sizeof(stdin_buff), stdin_buff, 0, 0, 0, 0, NULL, __read_console, __write_console,
     __close_console, NULL, &__files[1]},
    {1, {__must_exist, __write, _IOLBF, __console_file, __unoriented, 0}, {__neutral, 0, 0, 0}, 0, 0, 0, {0, 0},
     {0, 0}, 0, stdout_buff, sizeof(stdout_buff), stdout_buff, 0, 0, 0, 0, NULL, __read_console, __write_console,
     __close_console, NULL, &__files[2]},
    {2, {__must_exist, __write, _IONBF, __console_file, __unoriented, 0}, {__neutral, 0, 0, 0}, 0, 0, 0, {0, 0},
     {0, 0}, 0, stderr_buff, sizeof(stderr_buff), stderr_buff, 0, 0, 0, 0, NULL, __read_console, __write_console,
     __close_console, NULL, &__files[3]},
};

FILE* __find_unopened_file(void) {
    FILE* p = &__files[2];
    FILE* plast;

    p = p->next_file_struct;
    while (p != NULL) {
        if (p->mode.file_kind == __closed_file) {
            return p;
        }
        plast = p;
        p = p->next_file_struct;
    }

    p = (FILE*)malloc(sizeof(FILE));
    if (p != NULL) {
        memset(p, 0, sizeof(FILE));
        p->is_dynamically_allocated = 1;
        plast->next_file_struct = p;
        return p;
    }

    return NULL;
}

void __init_file(FILE* file, __file_modes mode, unsigned char* buff, size_t buff_size) {
    file->handle = 0;
    file->mode = mode;
    file->state.io_state = __neutral;
    file->state.free_buffer = 0;
    file->state.eof = 0;
    file->state.error = 0;
    file->position = 0;

    if (buff_size) {
        setvbuf(file, (char*)buff, _IOFBF, buff_size);
    } else {
        setvbuf(file, NULL, _IONBF, 0);
    }

    file->buffer_ptr = file->buffer;
    file->buffer_len = 0;
    if (file->mode.file_kind == __disk_file) {
        file->position_proc = __position_file;
        file->read_proc = __read_file;
        file->write_proc = __write_file;
        file->close_proc = __close_file;
    }

    file->idle_proc = NULL;
}

void __close_all(void) {
    FILE* p = &__files[0];
    FILE* plast;

    while (p) {
        if (p->mode.file_kind != __closed_file) {
            fclose(p);
        }

        plast = p;
        p = p->next_file_struct;
        if (plast->is_dynamically_allocated) {
            free(plast);
        } else {
            plast->mode.file_kind = __unavailable_file;
            if (p != NULL && p->is_dynamically_allocated) {
                plast->next_file_struct = NULL;
            }
        }
    }
}

int __flush_all(void) {
    int result = 0;
    FILE* file = &__files[0];

    while (file) {
        if (file->mode.file_kind) {
            if (fflush(file)) {
                result = EOF;
            }
        }
        file = file->next_file_struct;
    }

    return result;
}

int __flush_line_buffered_output_files(void) {
    int result = 0;
    FILE* file = &__files[0];

    while (file) {
        if (file->mode.file_kind && (file->mode.buffer_mode & _IOLBF) && file->state.io_state == __writing) {
            if (fflush(file)) {
                result = EOF;
            }
        }
        file = file->next_file_struct;
    }

    return result;
}
