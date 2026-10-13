#include "ansi_files.h"

size_t fread(void* ptr, size_t memb_size, size_t num_memb, FILE* file) {
    return __fread(ptr, memb_size, num_memb, file);
}

size_t __fread(void* ptr, size_t memb_size, size_t num_memb, FILE* file) {
    unsigned char* read_ptr;
    size_t num_bytes, bytes_to_go, bytes_read;
    int ioresult;
    int always_buffer;

    if (fwide(file, 0) == 0) {
        fwide(file, -1);
    }

    bytes_to_go = memb_size * num_memb;

    if (!bytes_to_go || file->state.error || file->mode.file_kind == __closed_file) {
        return 0;
    }

    always_buffer = 1;
    if (file->mode.binary_io && file->mode.buffer_mode != _IOFBF) {
        always_buffer = 0;
    }

    if (file->state.io_state == __neutral) {
        if (file->mode.io_mode & __read) {
            file->state.io_state = __reading;
            file->buffer_len = 0;
        }
    }

    if (file->state.io_state < __reading) {
        set_error(file);
        return 0;
    }

    if (file->mode.buffer_mode & _IOLBF) {
        if (__flush_line_buffered_output_files()) {
            set_error(file);
            return 0;
        }
    }

    read_ptr = (unsigned char*)ptr;
    bytes_read = 0;

    if (bytes_to_go && file->state.io_state >= __rereading) {
        do {
            if (fwide(file, 0) == 1) {
                *(wchar_t*)read_ptr = file->ungetwc_buffer[file->state.io_state - __rereading];
                num_bytes = sizeof(wchar_t);
            } else {
                *read_ptr = file->ungetc_buffer[file->state.io_state - __rereading];
                num_bytes = 1;
            }
            read_ptr += num_bytes;
            bytes_to_go -= num_bytes;
            bytes_read += num_bytes;
            file->state.io_state--;
        } while (bytes_to_go && file->state.io_state >= __rereading);

        if (file->state.io_state == __reading) {
            file->buffer_len = file->saved_buffer_len;
        }
    }

    if (bytes_to_go && (file->buffer_len || always_buffer)) {
        do {
            if (!file->buffer_len) {
                ioresult = __load_buffer(file, NULL, 0);
                if (ioresult) {
                    if (ioresult == __io_error) {
                        set_error(file);
                    } else {
                        file->state.io_state = __neutral;
                        file->state.eof = 1;
                        file->buffer_len = 0;
                    }
                    bytes_to_go = 0;
                    break;
                }
            }

            num_bytes = file->buffer_len;
            if (num_bytes > bytes_to_go) {
                num_bytes = bytes_to_go;
            }

            memcpy(read_ptr, file->buffer_ptr, num_bytes);
            read_ptr += num_bytes;
            bytes_read += num_bytes;
            bytes_to_go -= num_bytes;
            file->buffer_ptr += num_bytes;
            file->buffer_len -= num_bytes;
        } while (bytes_to_go && always_buffer);
    }

    if (bytes_to_go && !always_buffer) {
        unsigned char* save_buffer = file->buffer;
        size_t save_size = file->buffer_size;

        file->buffer = read_ptr;
        file->buffer_size = bytes_to_go;

        ioresult = __load_buffer(file, &num_bytes, 1);
        if (ioresult) {
            if (ioresult == __io_error) {
                set_error(file);
            } else {
                file->state.io_state = __neutral;
                file->state.eof = 1;
                file->buffer_len = 0;
            }
        }

        bytes_read += num_bytes;

        file->buffer = save_buffer;
        file->buffer_size = save_size;

        __prep_buffer(file);
        file->buffer_len = 0;
    }

    return bytes_read / memb_size;
}

size_t fwrite(const void* ptr, size_t memb_size, size_t num_memb, FILE* file) {
    return __fwrite(ptr, memb_size, num_memb, file);
}

size_t __fwrite(const void* ptr, size_t memb_size, size_t num_memb, FILE* file) {
    unsigned char* write_ptr;
    size_t num_bytes, bytes_to_go, bytes_written;
    int ioresult;
    int always_buffer;

    if (fwide(file, 0) == 0) {
        fwide(file, -1);
    }

    bytes_to_go = memb_size * num_memb;

    if (!bytes_to_go || file->state.error || file->mode.file_kind == __closed_file) {
        return 0;
    }

    if (file->mode.file_kind == __console_file) {
        __stdio_atexit();
    }

    always_buffer = !file->mode.binary_io || file->mode.buffer_mode == _IOFBF || file->mode.buffer_mode == _IOLBF;

    if (file->state.io_state == __neutral) {
        if (file->mode.io_mode & __write) {
            if (file->mode.io_mode & __append) {
                if (fseek(file, 0, SEEK_END)) {
                    return 0;
                }
            }
            file->state.io_state = __writing;
            __prep_buffer(file);
        }
    }

    if (file->state.io_state != __writing) {
        set_error(file);
        return 0;
    }

    write_ptr = (unsigned char*)ptr;
    bytes_written = 0;

    if (bytes_to_go && (file->buffer_ptr != file->buffer || always_buffer)) {
        file->buffer_len = file->buffer_size - (file->buffer_ptr - file->buffer);

        do {
            unsigned char* newline = NULL;

            num_bytes = file->buffer_len;
            if (num_bytes > bytes_to_go) {
                num_bytes = bytes_to_go;
            }

            if (file->mode.buffer_mode == _IOLBF && num_bytes) {
                if ((newline = (unsigned char*)__memrchr(write_ptr, '\n', num_bytes)) != NULL) {
                    num_bytes = newline + 1 - write_ptr;
                }
            }

            if (num_bytes) {
                memcpy(file->buffer_ptr, write_ptr, num_bytes);
                write_ptr += num_bytes;
                bytes_written += num_bytes;
                bytes_to_go -= num_bytes;
                file->buffer_ptr += num_bytes;
                file->buffer_len -= num_bytes;
            }

            if (!file->buffer_len || newline != NULL || (file->mode.buffer_mode == _IONBF)) {
                ioresult = __flush_buffer(file, NULL);
                if (ioresult) {
                    set_error(file);
                    bytes_to_go = 0;
                    break;
                }
            }
        } while (bytes_to_go && always_buffer);
    }

    if (bytes_to_go && !always_buffer) {
        unsigned char* save_buffer = file->buffer;
        size_t save_size = file->buffer_size;

        file->buffer = write_ptr;
        file->buffer_size = bytes_to_go;
        file->buffer_ptr = write_ptr + bytes_to_go;

        if (__flush_buffer(file, &num_bytes) != __no_io_error) {
            set_error(file);
        }

        bytes_written += num_bytes;

        file->buffer = save_buffer;
        file->buffer_size = save_size;

        __prep_buffer(file);
        file->buffer_len = 0;
    }

    if (file->mode.buffer_mode != _IOFBF) {
        file->buffer_len = 0;
    }

    return ((bytes_written + memb_size - 1) / memb_size);
}
