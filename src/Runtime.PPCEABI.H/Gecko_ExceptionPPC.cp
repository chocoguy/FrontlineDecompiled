// CodeWarrior C++ exception handling for PowerPC ("Gecko" variant: frames may
// save FPRs together with their paired-single halves, 16 bytes each).

typedef unsigned long size_t;

void operator delete(void* ptr) throw();

namespace std {

class exception {
public:
    exception() {}
    virtual ~exception() {}
    virtual const char* what() const { return "exception"; }
};

class bad_exception : public exception {
public:
    bad_exception() {}
    virtual ~bad_exception() {}
    virtual const char* what() const { return "bad_exception"; }
};

void terminate();
void unexpected();

}  // namespace std

using namespace std;

// Linker-generated exception table index ranges (see __init_cpp_exceptions).
typedef struct __eti_init_info {
    void* eti_start;
    void* eti_end;
    void* code_start;
    unsigned long code_size;
} __eti_init_info;

extern "C" char __throw_catch_compare(const char* throwtype, const char* catchtype, long* offset_result);

// Thrown object bookkeeping kept in a catch block's frame. size 0x18
typedef struct CatchInfo {
    /* 0x00 */ void* location;
    /* 0x04 */ void* typeinfo;
    /* 0x08 */ void* dtor;
    /* 0x0C */ void* sublocation;
    /* 0x10 */ long pointercopy;
    /* 0x14 */ void* stacktop;
} CatchInfo;

// Exception table index entry (extabindex). size 0xC
typedef struct ExceptionTableIndex {
    /* 0x0 */ unsigned long functionoffset;
    /* 0x4 */ unsigned long eti_field;
    /* 0x8 */ unsigned long exceptionoffset;
} ExceptionTableIndex;

#define ETI_GetFunctionSize(field) ((field) & 0x7FFFFFFF)
#define ETI_GetDirectStore(field) ((field) >> 31)

typedef struct ExceptionRangeSmall {
    /* 0x0 */ unsigned short start;
    /* 0x2 */ unsigned short end;
    /* 0x4 */ unsigned short action;
} ExceptionRangeSmall;

typedef struct ExceptionTableSmall {
    /* 0x0 */ unsigned short et_field;
    /* 0x2 */ ExceptionRangeSmall ranges[1];
} ExceptionTableSmall;

typedef struct ExceptionRangeLarge {
    /* 0x0 */ unsigned long start;
    /* 0x4 */ unsigned short size;
    /* 0x6 */ unsigned short action;
} ExceptionRangeLarge;

typedef struct ExceptionTableLarge {
    /* 0x0 */ unsigned short et_field;
    /* 0x2 */ unsigned short et_field2;
    /* 0x4 */ ExceptionRangeLarge ranges[1];
} ExceptionTableLarge;

#define ET_IsLargeTable(field) ((field) >> 3 & 1)
#define ET_GetSavedGPRs(field) ((field) >> 11)
#define ET_GetSavedFPRs(field) ((field) >> 6 & 0x1F)
#define ET_HasElfVector(field) ((field) >> 1 & 1)
#define ET_GetHasFramePtr(field) ((field) >> 4 & 1)

typedef unsigned char exaction_type;

#define EXACTION_ENDBIT 0x80
#define EXACTION_MASK 0x7F

#define EXACTION_ENDOFLIST 0
#define EXACTION_BRANCH 1
#define EXACTION_DESTROYLOCAL 2
#define EXACTION_DESTROYLOCALCOND 3
#define EXACTION_DESTROYLOCALPOINTER 4
#define EXACTION_DESTROYLOCALARRAY 5
#define EXACTION_DESTROYBASE 6
#define EXACTION_DESTROYMEMBER 7
#define EXACTION_DESTROYMEMBERCOND 8
#define EXACTION_DESTROYMEMBERARRAY 9
#define EXACTION_DELETEPOINTER 10
#define EXACTION_DELETEPOINTERCOND 11
#define EXACTION_CATCHBLOCK 12
#define EXACTION_ACTIVECATCHBLOCK 13
#define EXACTION_TERMINATE 14
#define EXACTION_SPECIFICATION 15
#define EXACTION_CATCHBLOCK_32 16

typedef struct ex_branch {
    char action;
    char unused;
    unsigned short target;
} ex_branch;

typedef struct ex_destroylocal {
    char action;
    char unused;
    short local;
    void* dtor;
} ex_destroylocal;

typedef struct ex_destroylocalcond {
    char action;
    char dlc_field;
    short cond;
    short local;
    void* dtor;
} ex_destroylocalcond;

#define ex_destroylocalcond_GetRegCond(field) ((field) >> 7)

typedef struct ex_destroylocalpointer {
    char action;
    char dlp_field;
    short pointer;
    void* dtor;
} ex_destroylocalpointer;

#define ex_destroylocalpointer_GetRegPointer(field) ((field) >> 7)

typedef struct ex_destroylocalarray {
    char action;
    char dla_field;
    short localarray;
    unsigned short elements;
    unsigned short element_size;
    void* dtor;
} ex_destroylocalarray;

typedef struct ex_destroymember {
    char action;
    char dm_field;
    short objectptr;
    long offset;
    void* dtor;
} ex_destroymember;

#define ex_destroymember_GetRegPointer(field) ((field) >> 7)

typedef struct ex_destroymembercond {
    char action;
    char dmc_field;
    short cond;
    short objectptr;
    long offset;
    void* dtor;
} ex_destroymembercond;

#define ex_destroymembercond_GetRegCond(field) ((field) >> 7)
#define ex_destroymembercond_GetRegPointer(field) ((field) >> 6 & 1)

typedef struct ex_destroymemberarray {
    char action;
    char dma_field;
    short objectptr;
    long offset;
    long elements;
    long element_size;
    void* dtor;
} ex_destroymemberarray;

#define ex_destroymemberarray_GetRegPointer(field) ((field) >> 7)

typedef struct ex_deletepointer {
    char action;
    char dp_field;
    short objectptr;
    void* deletefunc;
} ex_deletepointer;

#define ex_deletepointer_GetRegPointer(field) ((field) >> 7)

typedef struct ex_deletepointercond {
    char action;
    char dpc_field;
    short cond;
    short objectptr;
    void* deletefunc;
} ex_deletepointercond;

#define ex_deletepointercond_GetRegCond(field) ((field) >> 7)
#define ex_deletepointercond_GetRegPointer(field) ((field) >> 6 & 1)

typedef struct ex_catchblock {
    char action;
    char unused;
    char* catch_type;
    unsigned short catch_pcoffset;
    short cinfo_ref;
} ex_catchblock;

typedef struct ex_activecatchblock {
    char action;
    char unused;
    short cinfo_ref;
} ex_activecatchblock;

typedef struct ex_specification {
    char action;
    char unused;
    unsigned short specs;
    unsigned long pcoffset;
    unsigned long cinfo_ref;
    char* spec[1];
} ex_specification;

typedef struct ex_catchblock_32 {
    char action;
    char unused;
    char* catch_type;
    unsigned long catch_pcoffset;
    unsigned long cinfo_ref;
} ex_catchblock_32;

typedef char local_cond_type;

#define DTORCALL_COMPLETE(dtor, objptr) (((void (*)(void*, short))dtor)(objptr, -1))
#define DTORCALL_PARTIAL(dtor, objptr) (((void (*)(void*, short))dtor)(objptr, 0))

typedef union MWE_GeckoVector64 {
    double d;
    float f[2];
} MWE_GeckoVector64;

typedef struct GeckoFPRContext {
    double d;
    MWE_GeckoVector64 v;
} GeckoFPRContext;

// Registers and throw state captured by __throw. size 0x2A4
typedef struct ThrowContext {
    /* 0x000 */ GeckoFPRContext FPR[32];
    /* 0x200 */ long GPR[32];
    /* 0x280 */ long CR;
    /* 0x284 */ char* SP;
    /* 0x288 */ char* FP;
    /* 0x28C */ char* throwSP;
    /* 0x290 */ char* returnaddr;
    /* 0x294 */ char* throwtype;
    /* 0x298 */ void* location;
    /* 0x29C */ void* dtor;
    /* 0x2A0 */ CatchInfo* catchinfo;
} ThrowContext;

typedef struct MWExceptionInfo {
    /* 0x00 */ ExceptionTableSmall* exception_record;
    /* 0x04 */ char* current_function;
    /* 0x08 */ char* action_pointer;
    /* 0x0C */ char* code_section;
    /* 0x10 */ char* data_section;
    /* 0x14 */ char* TOC;
} MWExceptionInfo;

typedef struct FragmentInfo {
    ExceptionTableIndex* exception_start;
    ExceptionTableIndex* exception_end;
    char* code_start;
    char* code_end;
    char* data_start;
    char* data_end;
    char* TOC;
    int active;
} FragmentInfo;

typedef struct ProcessInfo {
    __eti_init_info* exception_info;
    char* TOC;
    int active;
} ProcessInfo;

typedef struct ActionIterator {
    /* 0x00 */ MWExceptionInfo info;
    /* 0x18 */ char* current_SP;
    /* 0x1C */ char* current_FP;
    /* 0x20 */ long current_R31;
} ActionIterator;

#define MAXFRAGMENTS 1
static ProcessInfo fragmentinfo[MAXFRAGMENTS];

typedef void (*DeleteFunc)(void*);

#define RETURN_ADDRESS 4

static void ExPPC_FindExceptionRecord(char* returnaddr, MWExceptionInfo* info);
static exaction_type ExPPC_NextAction(ActionIterator* iter);
static char* ExPPC_PopStackFrame(ThrowContext* context, MWExceptionInfo* info);
static void ExPPC_UnwindStack(ThrowContext* context, MWExceptionInfo* info, void* catcher);
static void ExPPC_ThrowHandler(ThrowContext* context);
extern "C" void __throw(char* throwtype, void* location, void* dtor);
extern "C" void __end__catch(CatchInfo* catchinfo);
extern "C" void __unexpected(CatchInfo* catchinfo);
extern "C" int __register_fragment(struct __eti_init_info* info, char* TOC);
extern "C" void __unregister_fragment(int fragmentID);

// Restores the registers saved in context and jumps to newPC (a catch
// block or unexpected handler) with r2 = newRTOC.
static asm void ExPPC_LongJump(register ThrowContext* context, register void* newRTOC, register void* newPC) {
    nofralloc
    mr r8, r5
    mr r2, r4
    lwz r0, 0x280(r3)
    mtcrf 255, r0
    lmw r13, 0x234(r3)
    addi r7, r3, 0xe8
    psq_lx f14, r0, r7, 0, 0
    lfd f14, 0xe0(r3)
    addi r7, r3, 0xf8
    psq_lx f15, r0, r7, 0, 0
    lfd f15, 0xf0(r3)
    addi r7, r3, 0x108
    psq_lx f16, r0, r7, 0, 0
    lfd f16, 0x100(r3)
    addi r7, r3, 0x118
    psq_lx f17, r0, r7, 0, 0
    lfd f17, 0x110(r3)
    addi r7, r3, 0x128
    psq_lx f18, r0, r7, 0, 0
    lfd f18, 0x120(r3)
    addi r7, r3, 0x138
    psq_lx f19, r0, r7, 0, 0
    lfd f19, 0x130(r3)
    addi r7, r3, 0x148
    psq_lx f20, r0, r7, 0, 0
    lfd f20, 0x140(r3)
    addi r7, r3, 0x158
    psq_lx f21, r0, r7, 0, 0
    lfd f21, 0x150(r3)
    addi r7, r3, 0x168
    psq_lx f22, r0, r7, 0, 0
    lfd f22, 0x160(r3)
    addi r7, r3, 0x178
    psq_lx f23, r0, r7, 0, 0
    lfd f23, 0x170(r3)
    addi r7, r3, 0x188
    psq_lx f24, r0, r7, 0, 0
    lfd f24, 0x180(r3)
    addi r7, r3, 0x198
    psq_lx f25, r0, r7, 0, 0
    lfd f25, 0x190(r3)
    addi r7, r3, 0x1a8
    psq_lx f26, r0, r7, 0, 0
    lfd f26, 0x1a0(r3)
    addi r7, r3, 0x1b8
    psq_lx f27, r0, r7, 0, 0
    lfd f27, 0x1b0(r3)
    addi r7, r3, 0x1c8
    psq_lx f28, r0, r7, 0, 0
    lfd f28, 0x1c0(r3)
    addi r7, r3, 0x1d8
    psq_lx f29, r0, r7, 0, 0
    lfd f29, 0x1d0(r3)
    addi r7, r3, 0x1e8
    psq_lx f30, r0, r7, 0, 0
    lfd f30, 0x1e0(r3)
    addi r7, r3, 0x1f8
    psq_lx f31, r0, r7, 0, 0
    lfd f31, 0x1f0(r3)
    mtlr r8
    lwz r1, 0x28c(r3)
    lwz r3, 0x284(r3)
    lwz r3, 0x0(r3)
    stw r3, 0x0(r1)
    blr
}

// throw: captures the caller's non-volatile registers and dispatches.
extern "C" asm void __throw(char* throwtype, void* location, void* dtor) {
    nofralloc
    stwu r1, -0x2c0(r1)
    mflr r0
    stw r0, 0x2c4(r1)
    stw r3, 0x8(r1)
    stw r4, 0xc(r1)
    stw r5, 0x10(r1)
    stmw r13, 0x24c(r1)
    stfd f14, 0xf8(r1)
    addi r3, r1, 0x100
    psq_stx f14, r0, r3, 0, 0
    stfd f15, 0x108(r1)
    addi r3, r1, 0x110
    psq_stx f15, r0, r3, 0, 0
    stfd f16, 0x118(r1)
    addi r3, r1, 0x120
    psq_stx f16, r0, r3, 0, 0
    stfd f17, 0x128(r1)
    addi r3, r1, 0x130
    psq_stx f17, r0, r3, 0, 0
    stfd f18, 0x138(r1)
    addi r3, r1, 0x140
    psq_stx f18, r0, r3, 0, 0
    stfd f19, 0x148(r1)
    addi r3, r1, 0x150
    psq_stx f19, r0, r3, 0, 0
    stfd f20, 0x158(r1)
    addi r3, r1, 0x160
    psq_stx f20, r0, r3, 0, 0
    stfd f21, 0x168(r1)
    addi r3, r1, 0x170
    psq_stx f21, r0, r3, 0, 0
    stfd f22, 0x178(r1)
    addi r3, r1, 0x180
    psq_stx f22, r0, r3, 0, 0
    stfd f23, 0x188(r1)
    addi r3, r1, 0x190
    psq_stx f23, r0, r3, 0, 0
    stfd f24, 0x198(r1)
    addi r3, r1, 0x1a0
    psq_stx f24, r0, r3, 0, 0
    stfd f25, 0x1a8(r1)
    addi r3, r1, 0x1b0
    psq_stx f25, r0, r3, 0, 0
    stfd f26, 0x1b8(r1)
    addi r3, r1, 0x1c0
    psq_stx f26, r0, r3, 0, 0
    stfd f27, 0x1c8(r1)
    addi r3, r1, 0x1d0
    psq_stx f27, r0, r3, 0, 0
    stfd f28, 0x1d8(r1)
    addi r3, r1, 0x1e0
    psq_stx f28, r0, r3, 0, 0
    stfd f29, 0x1e8(r1)
    addi r3, r1, 0x1f0
    psq_stx f29, r0, r3, 0, 0
    stfd f30, 0x1f8(r1)
    addi r3, r1, 0x200
    psq_stx f30, r0, r3, 0, 0
    stfd f31, 0x208(r1)
    addi r3, r1, 0x210
    psq_stx f31, r0, r3, 0, 0
    mfcr r3
    stw r3, 0x298(r1)
    lwz r3, 0x0(r1)
    lwz r4, 0x4(r3)
    stw r3, 0x29c(r1)
    stw r3, 0x2a4(r1)
    stw r4, 0x2a8(r1)
    lwz r3, 0x8(r1)
    stw r3, 0x2ac(r1)
    lwz r3, 0xc(r1)
    stw r3, 0x2b0(r1)
    lwz r3, 0x10(r1)
    stw r3, 0x2b4(r1)
    addi r3, r1, 0x18
    bl ExPPC_ThrowHandler
    nop
    lwz r0, 0x2c4(r1)
    mtlr r0
    addi r1, r1, 0x2c0
    blr
}

// Leaving a catch block destroys the caught object.
extern "C" void __end__catch(CatchInfo* catchinfo) {
    if (catchinfo->location && catchinfo->dtor) {
        DTORCALL_COMPLETE(catchinfo->dtor, catchinfo->location);
    }
}

static inline exaction_type ExPPC_CurrentAction(const ActionIterator* iter) {
    if (iter->info.action_pointer == 0) {
        return EXACTION_ENDOFLIST;
    }

    return ((ex_destroylocal*)iter->info.action_pointer)->action & EXACTION_MASK;
}

static inline int ExPPC_IsInSpecification(char* extype, ex_specification* spec) {
    long i, offset;

    for (i = 0; i < spec->specs; i++) {
        if (__throw_catch_compare(extype, spec->spec[i], &offset)) {
            return 1;
        }
    }

    return 0;
}

static inline void ExPPC_HandleUnexpected(ThrowContext* context, MWExceptionInfo* info, ex_specification* unexp) {
    CatchInfo* catchinfo;

    ExPPC_UnwindStack(context, info, unexp);

    catchinfo = (CatchInfo*)(context->FP + unexp->cinfo_ref);
    catchinfo->location = context->location;
    catchinfo->typeinfo = context->throwtype;
    catchinfo->dtor = context->dtor;
    catchinfo->stacktop = unexp;

    ExPPC_LongJump(context, info->TOC, info->current_function + unexp->pcoffset);
}

static void ExPPC_ThrowHandler(ThrowContext* context) {
    ActionIterator iter;
    MWExceptionInfo info;
    exaction_type action;
    CatchInfo* catchinfo;
    long catchoffset;

    ExPPC_FindExceptionRecord(context->returnaddr, &info);

    if (info.exception_record == 0) {
        terminate();
    }

    context->FP = (ET_GetHasFramePtr(info.exception_record->et_field)) ? (char*)context->GPR[31] : context->SP;

    if (context->throwtype == 0) {
        // Rethrow: find the active catch block and its exception.
        iter.info = info;
        iter.current_SP = context->SP;
        iter.current_FP = context->FP;
        iter.current_R31 = context->GPR[31];

        for (action = ExPPC_CurrentAction(&iter);; action = ExPPC_NextAction(&iter)) {
            switch (action) {
            case EXACTION_ACTIVECATCHBLOCK:
                break;

            case EXACTION_ENDOFLIST:
            case EXACTION_DESTROYLOCAL:
            case EXACTION_DESTROYLOCALCOND:
            case EXACTION_DESTROYLOCALPOINTER:
            case EXACTION_DESTROYLOCALARRAY:
            case EXACTION_DESTROYBASE:
            case EXACTION_DESTROYMEMBER:
            case EXACTION_DESTROYMEMBERCOND:
            case EXACTION_DESTROYMEMBERARRAY:
            case EXACTION_DELETEPOINTER:
            case EXACTION_DELETEPOINTERCOND:
            case EXACTION_CATCHBLOCK:
            case EXACTION_CATCHBLOCK_32:
            case EXACTION_SPECIFICATION:
                continue;

            case EXACTION_TERMINATE:
            default:
                terminate();
            }

            break;
        }

        catchinfo = (CatchInfo*)(iter.current_FP + ((ex_activecatchblock*)iter.info.action_pointer)->cinfo_ref);
        context->throwtype = (char*)catchinfo->typeinfo;
        context->location = catchinfo->location;
        context->dtor = 0;
        context->catchinfo = catchinfo;
    } else {
        context->catchinfo = 0;
    }

    iter.info = info;
    iter.current_SP = context->SP;
    iter.current_FP = context->FP;
    iter.current_R31 = context->GPR[31];

    for (action = ExPPC_CurrentAction(&iter);; action = ExPPC_NextAction(&iter)) {
        switch (action) {
        case EXACTION_CATCHBLOCK_32:
            if (__throw_catch_compare(context->throwtype, ((ex_catchblock_32*)iter.info.action_pointer)->catch_type,
                                      &catchoffset)) {
                break;
            }
            continue;

        case EXACTION_CATCHBLOCK:
            if (__throw_catch_compare(context->throwtype, ((ex_catchblock*)iter.info.action_pointer)->catch_type,
                                      &catchoffset)) {
                break;
            }
            continue;

        case EXACTION_SPECIFICATION:
            if (!ExPPC_IsInSpecification(context->throwtype, (ex_specification*)iter.info.action_pointer)) {
                ExPPC_HandleUnexpected(context, &info, (ex_specification*)iter.info.action_pointer);
            }
            continue;

        case EXACTION_ENDOFLIST:
        case EXACTION_DESTROYLOCAL:
        case EXACTION_DESTROYLOCALCOND:
        case EXACTION_DESTROYLOCALPOINTER:
        case EXACTION_DESTROYLOCALARRAY:
        case EXACTION_DESTROYBASE:
        case EXACTION_DESTROYMEMBER:
        case EXACTION_DESTROYMEMBERCOND:
        case EXACTION_DESTROYMEMBERARRAY:
        case EXACTION_DELETEPOINTER:
        case EXACTION_DELETEPOINTERCOND:
        case EXACTION_ACTIVECATCHBLOCK:
            continue;

        case EXACTION_TERMINATE:
        default:
            terminate();
        }

        break;
    }

    if (action == EXACTION_CATCHBLOCK_32) {
        ex_catchblock_32* catchblock_32 = (ex_catchblock_32*)iter.info.action_pointer;

        ExPPC_UnwindStack(context, &info, catchblock_32);

        catchinfo = (CatchInfo*)(context->FP + catchblock_32->cinfo_ref);
        catchinfo->location = context->location;
        catchinfo->typeinfo = context->throwtype;
        catchinfo->dtor = context->dtor;

        if (*context->throwtype == '*') {
            catchinfo->sublocation = &catchinfo->pointercopy;
            catchinfo->pointercopy = *(long*)context->location + catchoffset;
        } else {
            catchinfo->sublocation = (char*)context->location + catchoffset;
        }

        ExPPC_LongJump(context, info.TOC, info.current_function + catchblock_32->catch_pcoffset);
    } else {
        ex_catchblock* catchblock = (ex_catchblock*)iter.info.action_pointer;

        ExPPC_UnwindStack(context, &info, catchblock);

        catchinfo = (CatchInfo*)(context->FP + catchblock->cinfo_ref);
        catchinfo->location = context->location;
        catchinfo->typeinfo = context->throwtype;
        catchinfo->dtor = context->dtor;

        if (*context->throwtype == '*') {
            catchinfo->sublocation = &catchinfo->pointercopy;
            catchinfo->pointercopy = *(long*)context->location + catchoffset;
        } else {
            catchinfo->sublocation = (char*)context->location + catchoffset;
        }

        ExPPC_LongJump(context, info.TOC, info.current_function + catchblock->catch_pcoffset);
    }
}

// Entered (via an exception specification's handler) when a function throws
// something its specification does not allow.
extern "C" void __unexpected(CatchInfo* catchinfo) {
    ex_specification* unexp = (ex_specification*)catchinfo->stacktop;

#pragma exception_magic

    try {
        unexpected();
    } catch (...) {
        if (ExPPC_IsInSpecification((char*)((CatchInfo*)&__exception_magic)->typeinfo, unexp)) {
            throw;
        }

        if (ExPPC_IsInSpecification("!bad_exception!!", unexp)) {
            throw bad_exception();
        }

        if (ExPPC_IsInSpecification("!std::bad_exception!!", unexp)) {
            throw bad_exception();
        }
    }

    terminate();
}

static void ExPPC_UnwindStack(ThrowContext* context, MWExceptionInfo* info, void* catcher) {
    exaction_type action;

#pragma exception_terminate

    for (;;) {
        if (info->action_pointer == 0) {
            char* return_addr;

            return_addr = ExPPC_PopStackFrame(context, info);
            ExPPC_FindExceptionRecord(return_addr, info);

            if (info->exception_record == 0) {
                terminate();
            }

            context->FP = (ET_GetHasFramePtr(info->exception_record->et_field)) ? (char*)context->GPR[31]
                                                                               : context->SP;
            continue;
        }

        action = ((ex_destroylocal*)info->action_pointer)->action;

        switch (action & EXACTION_MASK) {
        case EXACTION_BRANCH:
            info->action_pointer = ((char*)info->exception_record) + ((ex_branch*)info->action_pointer)->target;
            break;

        case EXACTION_DESTROYLOCAL: {
            ex_destroylocal* dl = (ex_destroylocal*)info->action_pointer;
            DTORCALL_COMPLETE(dl->dtor, context->FP + dl->local);
            info->action_pointer += sizeof(ex_destroylocal);
            break;
        }

        case EXACTION_DESTROYLOCALCOND: {
            ex_destroylocalcond* dlc = (ex_destroylocalcond*)info->action_pointer;
            int cond = ex_destroylocalcond_GetRegCond(dlc->dlc_field) ? (local_cond_type)context->GPR[dlc->cond]
                                                                      : *(local_cond_type*)(context->FP + dlc->cond);

            if (cond) {
                DTORCALL_COMPLETE(dlc->dtor, context->FP + dlc->local);
            }
            info->action_pointer += sizeof(ex_destroylocalcond);
            break;
        }

        case EXACTION_DESTROYLOCALPOINTER: {
            ex_destroylocalpointer* dlp = (ex_destroylocalpointer*)info->action_pointer;
            void* pointer = ex_destroylocalpointer_GetRegPointer(dlp->dlp_field)
                                ? (void*)context->GPR[dlp->pointer]
                                : *(void**)(context->FP + dlp->pointer);

            DTORCALL_COMPLETE(dlp->dtor, pointer);
            info->action_pointer += sizeof(ex_destroylocalpointer);
            break;
        }

        case EXACTION_DESTROYLOCALARRAY: {
            ex_destroylocalarray* dla = (ex_destroylocalarray*)info->action_pointer;
            void* ptr = context->FP + dla->localarray;
            long n = dla->elements;
            long size = dla->element_size;

            for (ptr = (char*)ptr + size * n; n > 0; n--) {
                ptr = (char*)ptr - size;
                DTORCALL_COMPLETE(dla->dtor, ptr);
            }
            info->action_pointer += sizeof(ex_destroylocalarray);
            break;
        }

        case EXACTION_DESTROYBASE: {
            ex_destroymember* dm = (ex_destroymember*)info->action_pointer;
            void* objectptr = ex_destroymember_GetRegPointer(dm->dm_field) ? (void*)context->GPR[dm->objectptr]
                                                                           : *(void**)(context->FP + dm->objectptr);

            DTORCALL_PARTIAL(dm->dtor, (char*)objectptr + dm->offset);
            info->action_pointer += sizeof(ex_destroymember);
            break;
        }

        case EXACTION_DESTROYMEMBER: {
            ex_destroymember* dm = (ex_destroymember*)info->action_pointer;
            void* objectptr = ex_destroymember_GetRegPointer(dm->dm_field) ? (void*)context->GPR[dm->objectptr]
                                                                           : *(void**)(context->FP + dm->objectptr);

            DTORCALL_COMPLETE(dm->dtor, (char*)objectptr + dm->offset);
            info->action_pointer += sizeof(ex_destroymember);
            break;
        }

        case EXACTION_DESTROYMEMBERCOND: {
            ex_destroymembercond* dmc = (ex_destroymembercond*)info->action_pointer;
            void* objectptr = ex_destroymembercond_GetRegPointer(dmc->dmc_field)
                                  ? (void*)context->GPR[dmc->objectptr]
                                  : *(void**)(context->FP + dmc->objectptr);
            // (This one condition is a short in the original.)
            int cond = ex_destroymembercond_GetRegCond(dmc->dmc_field) ? (short)context->GPR[dmc->cond]
                                                                       : *(short*)(context->FP + dmc->cond);

            if (cond) {
                DTORCALL_PARTIAL(dmc->dtor, (char*)objectptr + dmc->offset);
            }
            info->action_pointer += sizeof(ex_destroymembercond);
            break;
        }

        case EXACTION_DESTROYMEMBERARRAY: {
            ex_destroymemberarray* dma = (ex_destroymemberarray*)info->action_pointer;
            void* ptr = ex_destroymemberarray_GetRegPointer(dma->dma_field) ? (void*)context->GPR[dma->objectptr]
                                                                            : *(void**)(context->FP + dma->objectptr);
            long n = dma->elements;
            long size = dma->element_size;

            ptr = (char*)ptr + dma->offset;

            for (ptr = (char*)ptr + size * n; n > 0; n--) {
                ptr = (char*)ptr - size;
                DTORCALL_COMPLETE(dma->dtor, ptr);
            }
            info->action_pointer += sizeof(ex_destroymemberarray);
            break;
        }

        case EXACTION_DELETEPOINTER: {
            ex_deletepointer* dp = (ex_deletepointer*)info->action_pointer;
            void* objectptr = ex_deletepointer_GetRegPointer(dp->dp_field) ? (void*)context->GPR[dp->objectptr]
                                                                           : *(void**)(context->FP + dp->objectptr);

            ((DeleteFunc)dp->deletefunc)(objectptr);
            info->action_pointer += sizeof(ex_deletepointer);
            break;
        }

        case EXACTION_DELETEPOINTERCOND: {
            ex_deletepointercond* dpc = (ex_deletepointercond*)info->action_pointer;
            void* objectptr = ex_deletepointercond_GetRegPointer(dpc->dpc_field)
                                  ? (void*)context->GPR[dpc->objectptr]
                                  : *(void**)(context->FP + dpc->objectptr);
            int cond = ex_deletepointercond_GetRegCond(dpc->dpc_field) ? (local_cond_type)context->GPR[dpc->cond]
                                                                       : *(local_cond_type*)(context->FP + dpc->cond);

            if (cond) {
                ((DeleteFunc)dpc->deletefunc)(objectptr);
            }
            info->action_pointer += sizeof(ex_deletepointercond);
            break;
        }

        case EXACTION_CATCHBLOCK: {
            ex_catchblock* cb = (ex_catchblock*)info->action_pointer;
            if (catcher == (void*)cb) {
                return;
            }
            info->action_pointer += sizeof(ex_catchblock);
            break;
        }

        case EXACTION_CATCHBLOCK_32: {
            ex_catchblock_32* cb = (ex_catchblock_32*)info->action_pointer;
            if (catcher == (void*)cb) {
                return;
            }
            info->action_pointer += sizeof(ex_catchblock_32);
            break;
        }

        case EXACTION_ACTIVECATCHBLOCK: {
            ex_activecatchblock* acb = (ex_activecatchblock*)info->action_pointer;
            CatchInfo* catchinfo;

            catchinfo = (CatchInfo*)(context->FP + acb->cinfo_ref);

            if (catchinfo->dtor) {
                if (context->location == catchinfo->location) {
                    context->dtor = catchinfo->dtor;
                } else {
                    DTORCALL_COMPLETE(catchinfo->dtor, catchinfo->location);
                }
            }
            info->action_pointer += sizeof(ex_activecatchblock);
            break;
        }

        case EXACTION_SPECIFICATION: {
            ex_specification* spec = (ex_specification*)info->action_pointer;
            if (catcher == (void*)spec) {
                return;
            }
            info->action_pointer += sizeof(ex_specification) - sizeof(char*) + spec->specs * sizeof(void*);
            break;
        }

        default:
            terminate();
        }

        if (action & EXACTION_ENDBIT) {
            info->action_pointer = 0;
        }
    }
}

// Restores the non-volatile registers a frame saved and steps to its caller;
// returns the caller's return address.
static char* ExPPC_PopStackFrame(ThrowContext* context, MWExceptionInfo* info) {
    char *SP, *callers_SP;
    double* FPR_save_area;
    long* GPR_save_area;
    int saved_GPRs, saved_FPRs;
    int i, j;

    callers_SP = *(char**)context->SP;

    saved_FPRs = ET_GetSavedFPRs(info->exception_record->et_field);
    if (ET_HasElfVector(info->exception_record->et_field)) {
        FPR_save_area = (double*)(callers_SP - saved_FPRs * 16);
    } else {
        FPR_save_area = (double*)(callers_SP - saved_FPRs * 8);
    }

    if (ET_HasElfVector(info->exception_record->et_field)) {
        for (i = 32 - saved_FPRs, j = 0; i < 32; ++i, ++j) {
            context->FPR[i].v.f[0] = ((MWE_GeckoVector64*)FPR_save_area)[j * 2 + 1].f[0];
            context->FPR[i].v.f[1] = ((MWE_GeckoVector64*)FPR_save_area)[j * 2 + 1].f[1];
            context->FPR[i].d = FPR_save_area[j * 2];
        }
    } else {
        for (i = 32 - saved_FPRs, j = 0; i < 32; ++i, ++j) {
            context->FPR[i].d = FPR_save_area[j];
        }
    }

    saved_GPRs = ET_GetSavedGPRs(info->exception_record->et_field);
    GPR_save_area = (long*)FPR_save_area;
    GPR_save_area -= saved_GPRs;

    for (i = 32 - saved_GPRs, j = 0; i < 32; ++i, ++j) {
        context->GPR[i] = GPR_save_area[j];
    }

    context->SP = callers_SP;
    return *(char**)(callers_SP + RETURN_ADDRESS);
}

static exaction_type ExPPC_NextAction(ActionIterator* iter) {
    exaction_type action;

    for (;;) {
        if (iter->info.action_pointer == 0 ||
            ((action = ((ex_destroylocal*)iter->info.action_pointer)->action) & EXACTION_ENDBIT) != 0) {
            char *return_addr, *callers_SP;

            callers_SP = *(char**)iter->current_SP;

            if (ET_GetSavedGPRs(iter->info.exception_record->et_field)) {
                iter->current_R31 =
                    *(long*)(callers_SP - ET_GetSavedFPRs(iter->info.exception_record->et_field) * 8 - 4);
            }

            return_addr = *(char**)(callers_SP + RETURN_ADDRESS);
            ExPPC_FindExceptionRecord(return_addr, &iter->info);

            if (iter->info.exception_record == 0) {
                terminate();
            }

            iter->current_SP = callers_SP;
            iter->current_FP = (ET_GetHasFramePtr(iter->info.exception_record->et_field))
                                   ? (char*)iter->current_R31
                                   : iter->current_SP;

            if (iter->info.action_pointer == 0) {
                continue;
            }
        } else {
            switch (action) {
            case EXACTION_DESTROYLOCAL:
                iter->info.action_pointer += sizeof(ex_destroylocal);
                break;
            case EXACTION_DESTROYLOCALCOND:
                iter->info.action_pointer += sizeof(ex_destroylocalcond);
                break;
            case EXACTION_DESTROYLOCALPOINTER:
                iter->info.action_pointer += sizeof(ex_destroylocalpointer);
                break;
            case EXACTION_DESTROYLOCALARRAY:
                iter->info.action_pointer += sizeof(ex_destroylocalarray);
                break;
            case EXACTION_DESTROYBASE:
            case EXACTION_DESTROYMEMBER:
                iter->info.action_pointer += sizeof(ex_destroymember);
                break;
            case EXACTION_DESTROYMEMBERCOND:
                iter->info.action_pointer += sizeof(ex_destroymembercond);
                break;
            case EXACTION_DESTROYMEMBERARRAY:
                iter->info.action_pointer += sizeof(ex_destroymemberarray);
                break;
            case EXACTION_DELETEPOINTER:
                iter->info.action_pointer += sizeof(ex_deletepointer);
                break;
            case EXACTION_DELETEPOINTERCOND:
                iter->info.action_pointer += sizeof(ex_deletepointercond);
                break;
            case EXACTION_CATCHBLOCK:
                iter->info.action_pointer += sizeof(ex_catchblock);
                break;
            case EXACTION_CATCHBLOCK_32:
                iter->info.action_pointer += sizeof(ex_catchblock_32);
                break;
            case EXACTION_ACTIVECATCHBLOCK:
                iter->info.action_pointer += sizeof(ex_activecatchblock);
                break;
            case EXACTION_SPECIFICATION:
                iter->info.action_pointer += sizeof(ex_specification) - sizeof(char*) +
                                             ((ex_specification*)iter->info.action_pointer)->specs * sizeof(void*);
                break;
            default:
                terminate();
            }
        }

        action = ((ex_destroylocal*)iter->info.action_pointer)->action & EXACTION_MASK;

        if (action == EXACTION_BRANCH) {
            iter->info.action_pointer =
                ((char*)iter->info.exception_record) + ((ex_branch*)iter->info.action_pointer)->target;
            action = ((ex_destroylocal*)iter->info.action_pointer)->action & EXACTION_MASK;
        }

        return action;
    }
}

static inline int ExPPC_FindExceptionFragment(char* returnaddr, FragmentInfo* frag) {
    ProcessInfo* f;
    int i;
    __eti_init_info* eti_info;

    for (i = 0, f = fragmentinfo; i < MAXFRAGMENTS; ++i, ++f) {
        if (f->active) {
            for (eti_info = f->exception_info; eti_info->code_size != 0; ++eti_info) {
                if (returnaddr >= eti_info->code_start &&
                    returnaddr < (char*)eti_info->code_start + eti_info->code_size) {
                    frag->exception_start = (ExceptionTableIndex*)eti_info->eti_start;
                    frag->exception_end = (ExceptionTableIndex*)eti_info->eti_end;
                    frag->code_start = 0;
                    frag->code_end = 0;
                    frag->data_start = 0;
                    frag->data_end = 0;
                    frag->TOC = f->TOC;
                    frag->active = f->active;
                    return 1;
                }
            }
        }
    }

    return 0;
}

// Looks up the exception table entry and current action list for returnaddr.
static void ExPPC_FindExceptionRecord(char* returnaddr, MWExceptionInfo* info) {
    FragmentInfo* fragment;
    FragmentInfo frag;
    ExceptionTableIndex *exceptionindex, *p;
    unsigned long returnoffset;
    long i, m, n;

    info->exception_record = 0;
    info->action_pointer = 0;

    if ((ExPPC_FindExceptionFragment(returnaddr, &frag)) == 0) {
        return;
    }
    fragment = &frag;

    info->code_section = fragment->code_start;
    info->data_section = fragment->data_start;
    info->TOC = fragment->TOC;

    returnoffset = returnaddr - fragment->code_start;
    exceptionindex = fragment->exception_start;

    for (i = 0, n = fragment->exception_end - fragment->exception_start;;) {
        if (i > n) {
            return;
        }

        p = &exceptionindex[m = (i + n) / 2];

        if (returnoffset < p->functionoffset) {
            n = m - 1;
        } else if (returnoffset > p->functionoffset + ETI_GetFunctionSize(p->eti_field)) {
            i = m + 1;
        } else {
            break;
        }
    }

    info->current_function = fragment->code_start + p->functionoffset;
    info->exception_record = ETI_GetDirectStore(p->eti_field)
                                 ? (ExceptionTableSmall*)(&p->exceptionoffset)
                                 : (ExceptionTableSmall*)(fragment->data_start + p->exceptionoffset);

    returnoffset -= p->functionoffset;

    if (ET_IsLargeTable(info->exception_record->et_field)) {
        ExceptionTableLarge* etl = (ExceptionTableLarge*)info->exception_record;
        ExceptionRangeLarge* erl;

        for (erl = etl->ranges; erl->start != 0; erl++) {
            unsigned long range_end = erl->start + (erl->size * 4);

            if (erl->start <= returnoffset && range_end >= returnoffset) {
                info->action_pointer = (char*)etl + erl->action;
                break;
            }
        }
    } else {
        ExceptionTableSmall* ets = (ExceptionTableSmall*)info->exception_record;
        ExceptionRangeSmall* ers;

        for (ers = ets->ranges; ers->start != 0; ers++) {
            if (ers->start <= returnoffset && ers->end >= returnoffset) {
                info->action_pointer = (char*)ets + ers->action;
                break;
            }
        }
    }
}

extern "C" void __unregister_fragment(int fragmentID) {
    ProcessInfo* f;

    if (fragmentID >= 0 && fragmentID < MAXFRAGMENTS) {
        f = &fragmentinfo[fragmentID];
        f->exception_info = 0;
        f->TOC = 0;
        f->active = 0;
    }
}

extern "C" int __register_fragment(struct __eti_init_info* info, char* TOC) {
    ProcessInfo* f;
    int i;

    for (i = 0, f = fragmentinfo; i < MAXFRAGMENTS; ++i, ++f) {
        if (f->active == 0) {
            f->exception_info = info;
            f->TOC = TOC;
            f->active = 1;
            return i;
        }
    }

    return -1;
}
