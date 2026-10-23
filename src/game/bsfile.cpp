// BS script class files. The data is little-endian and stored as offsets;
// loading swaps it in place and turns the offsets into pointers.
#include "game/bsfile.h"
#include "game/bstypes.h"
#include "game/bsutil.h"
#include "game/endian.h"
#include "game/toilet.h"
#include "stdio.h"
#include "string.h"

// The class loaded right after the .sin header.
#define BSFILE_CLASS_OFFSET 0x64
// Where the variable-size data starts (after the class).
#define BSFILE_DATA_OFFSET 0xC8

BSClass_struct* g_pcClassList;

static inline BSClass_struct* FindClass(const char* name) {
    BSClass_struct* c = g_pcClassList;
    while (c != NULL) {
        if (stricmp(name, c->m_name) == 0) {
            return c;
        }
        c = c->m_next;
    }
    return NULL;
}

static inline int GetClass(const char* name, BSClass_struct** out) {
    *out = FindClass(name);
    if (*out != NULL) {
        if ((*out)->m_loaded) {
            return 0;
        }
        return BSFileReadClass(name, true, false, out);
    }
    BSFileReadClass(name, true, true, out);
    (*out)->m_next = g_pcClassList;
    g_pcClassList = *out;
    return 0;
}

int BSFileReadClass(const char* name, bool, bool, BSClass_struct** out) {
    char path[100];
    char message[256];
    int size;

    // <name>.sin: the class, its strings and its threads' layout.
    sprintf(path, "%s.sin", name);
    u8* sin = (u8*)TLT_LoadFileFromLevelBigFile(path, &size);
    u32 offset = BSFILE_DATA_OFFSET;
    EndianSwap(*(s32*)(sin + 0x20));
    *out = (BSClass_struct*)(sin + BSFILE_CLASS_OFFSET);
    BSClass_struct* cls = *out;
    EndianSwap(cls->field_0x48);
    EndianSwap(cls->m_memoryRequired);
    EndianSwap(*(s16*)&cls->m_numThreads);
    EndianSwap(cls->m_numStrings);
    EndianSwap(cls->m_strings);
    if ((*out)->m_release != -1) {
        sprintf(message,
                "ERROR:  script %s is a debugging version.  Incompatible with nondebug engine\n",
                path);
    }

    (*out)->m_loaded = 1;
    (*out)->m_next = NULL;
    (*out)->m_threads = NULL;
    (*out)->m_sinFile = sin;

    if ((*out)->m_numStrings > 0) {
        (*out)->m_strings = (PTR32(char)*)(sin + (u32)PTR_TO_ADDR((*out)->m_strings));
        offset = (*out)->m_numStrings * 4 + BSFILE_DATA_OFFSET;
        for (int i = 0; i < (*out)->m_numStrings; i++) {
            EndianSwap((*out)->m_strings[i]);
            (*out)->m_strings[i] = (char*)(sin + (u32)PTR_TO_ADDR((*out)->m_strings[i]));
            u32 len = strlen((*out)->m_strings[i]) + 1;
            if (len & 3) {
                len += 4 - (len & 3);
            }
            offset += len;
        }
    } else {
        (*out)->m_strings = NULL;
    }

    (*out)->m_threads = (BSCode_struct*)(sin + offset);
    offset += (*out)->m_numThreads * sizeof(BSCode_struct);
    for (int t = 0; t < (*out)->m_numThreads; t++) {
        BSCode_struct* code = &(*out)->m_threads[t];
        EndianSwap(code->m_code);
        EndianSwap(code->field_0x4);
        EndianSwap(*(s16*)&code->m_numHandlers);
        EndianSwap(*(s16*)&code->m_numLocals);
        EndianSwap(*(s16*)&code->field_0x16);
        code->m_code = (u32*)(sin + offset);
        offset += (code->m_numInherited + 1) * 4;
        code->m_handlers = (PTR32(BSHandler_struct)*)(sin + offset);
        offset += code->m_numHandlers * 4;

        for (u32 h = 0; h < code->m_numHandlers; h++) {
            EndianSwap(code->m_handlers[h]);
            if (code->m_handlers[h] != BS_HANDLER_NONE &&
                code->m_handlers[h] != BS_HANDLER_INHERIT) {
                code->m_handlers[h] = (BSHandler_struct*)(sin + offset);
                offset += sizeof(BSHandler_struct);
            }
        }

        for (u32 h = 0; h < code->m_numHandlers; h++) {
            BSHandler_struct* handler = code->m_handlers[h];
            if (handler == BS_HANDLER_NONE || handler == BS_HANDLER_INHERIT) {
                continue;
            }
            EndianSwap(handler->m_entry);
            EndianSwap(handler->m_parent);
            EndianSwap(handler->m_events);
            EndianSwap(handler->m_messages);
            handler = code->m_handlers[h];
            if (handler->m_numEvents != 0) {
                handler->m_events = (BSCode_EventHandlerEntry_struct*)(sin + offset);
                handler = code->m_handlers[h];
                u8 n = handler->m_numEvents;
                BSCode_EventHandlerEntry_struct* e = handler->m_events;
                offset += n * sizeof(BSCode_EventHandlerEntry_struct);
                for (int i = 0; i < n; i++, e++) {
                    EndianSwap(e->m_address);
                    EndianSwap(e->m_event);
                }
            } else {
                handler->m_events = NULL;
            }
        }

        for (u32 h = 0; h < code->m_numHandlers; h++) {
            BSHandler_struct* handler = code->m_handlers[h];
            if (handler == BS_HANDLER_NONE || handler == BS_HANDLER_INHERIT) {
                continue;
            }
            if (handler->m_numMessages != 0) {
                handler->m_messages = (BSCode_MessageHandlerEntry_struct*)(sin + offset);
                handler = code->m_handlers[h];
                u8 n = handler->m_numMessages;
                BSCode_MessageHandlerEntry_struct* m = handler->m_messages;
                offset += n * sizeof(BSCode_MessageHandlerEntry_struct);
                for (int i = 0; i < n; i++, m++) {
                    EndianSwap(m->m_address);
                    EndianSwap(m->m_filterArg);
                    EndianSwap(*(s16*)&m->m_message);
                }
            } else {
                handler->m_messages = NULL;
            }
        }
    }

    // <name>.cbs: the code, and the base class whose handlers and inherited
    // code words this class reuses.
    sprintf(path, "%s.cbs", name);
    u8* cbs = (u8*)TLT_LoadFileFromLevelBigFile(path, &size);
    EndianSwap(*(s32*)(cbs + 0x20));
    BSClass_struct* base = NULL;
    const char* baseName = (const char*)(cbs + 0x24);
    if (baseName[0] != '\0') {
        int err = GetClass(baseName, &base);
        if (err != 0) {
            return err;
        }
        for (int t = 0; t < (*out)->m_numThreads; t++) {
            BSCode_struct* code = &(*out)->m_threads[t];
            if (t >= base->m_numThreads) {
                continue;
            }
            BSCode_struct* baseCode = &base->m_threads[t];
            for (int h = 0; h < code->m_numHandlers; h++) {
                if (code->m_handlers[h] == BS_HANDLER_INHERIT) {
                    code->m_handlers[h] = baseCode->m_handlers[h];
                } else if (code->m_handlers[h] == BS_HANDLER_NONE) {
                    code->m_handlers[h] = NULL;
                }
            }
        }
    }

    (*out)->m_cbsFile = cbs;
    u32 cbsOffset = 0x68;
    s32 count = *(s32*)(cbs + 0x64);
    EndianSwap(count);
    for (int t = 0; t < count; t++) {
        u32 n = *(u32*)(cbs + cbsOffset);
        cbsOffset += 4;
        EndianSwap(n);
        BSCode_struct* code = &(*out)->m_threads[t];
        u8 inherited = code->m_numInherited;
        if (n != 0) {
            code->m_code[inherited] = (u32)PTR_TO_ADDR((cbs + cbsOffset));
            cbsOffset += n * 4;
            u32* words = ADDR_TO_PTR(u32, (*out)->m_threads[t].m_code[inherited]);
            for (int i = 0; i < (int)n; i++) {
                EndianSwap(words[i]);
            }
        } else {
            code->m_code[inherited] = 0;
        }
        if (inherited != 0) {
            if (base == NULL) {
                return 5;
            }
            for (u32 i = 0; i < inherited; i++) {
                (*out)->m_threads[t].m_code[i] = base->m_threads[t].m_code[i];
            }
        }
    }

    (*out)->m_memoryRequired = BSUtilComputeClassMemoryRequirements(*out);
    return 0;
}

int BSFileGetClass(const char* name, BSClass_struct** out) {
    return GetClass(name, out);
}

void BSFileDeleteClass(BSClass_struct* cls) {
    TLT_CloseFile(cls->m_cbsFile);
    TLT_CloseFile(cls->m_sinFile);
}

int BSInitFile() {
    return 0;
}

int BSFileGetMemoryRequirements() {
    return 0;
}
