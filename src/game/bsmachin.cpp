// The BS script virtual machine: a stack machine whose opcodes are 32-bit
// words (low byte: opcode, high half: a signed operand or two byte operands),
// plus state changes (NEXTSTATE) that run exit handlers and swap message
// handler registrations, and the property reader scripts use to read their
// trigger's level-editor data.
#include "game/bsmachin.h"
#ifdef FRONTLINE_NATIVE
#include "native/ai.h"
#endif
#include "game/BSGO_Basic.h"
#include "game/bsbif.h"
#include "game/bsfile.h"
#include "game/bsmessage.h"
#include "game/bstypes.h"
#include "game/bsutil.h"
#include "game/propdat.h"

typedef void (*BSOpCodeFunc)();

#define BS_NUM_OPCODES 33

// Operand fields of the current instruction word.
#define BS_OPERAND(w) ((s32)(w) >> 16)    // signed high half
#define BS_BYTE2(w) (((u32)(w) >> 16) & 0xFF)
#define BS_BYTE3(w) ((u32)(w) >> 24)

// Operand types of binary operators (left type in the low byte).
#define BS_INT 2
#define BS_FLOAT 3
#define BS_II (BS_INT | BS_INT << 8)
#define BS_FI (BS_FLOAT | BS_INT << 8)    // float left, int right
#define BS_IF (BS_INT | BS_FLOAT << 8)    // int left, float right
#define BS_FF (BS_FLOAT | BS_FLOAT << 8)

u32* g_piStackTop;
u32* g_piStackBase;
BSObject* g_pBSObject;
static u32* g_piIP;
static u32* g_piFrame;
static int g_iStopThread;
static u32* g_piCodeBase;  // segment table: segment addresses
static u32* g_piGlobals;
static PTR32(BSHandler_struct)* g_ppsteStateEntries;
BSMachineThread_struct* g_pbmtThread;
static ISceneNode* g_pObj;  // scene node of the running object
static BSMessageListEntry_struct* g_MessageList;
static BSMessageListEntry_struct* g_MessageListHead;  // free entries
static u8* g_pBSMachineMemory;
static int g_pBSMachineMemoryOffset;

static BSOpCodeFunc g_bocfOpCodeFuncs[BS_NUM_OPCODES];
static int g_iDefaultStackSize = 0x40;  // words

// Code address (segment << 24 | word offset) to pointer.
static inline u32* CodeAddress(u32* segments, u32 address) {
    return ADDR_TO_PTR(u32, segments[address >> 24]) + (address & 0xFFFFFF);
}

static inline f32& AsFloat(u32& word) {
    return *(f32*)&word;
}

static inline s32& AsInt(u32& word) {
    return *(s32*)&word;
}

// Registers the message handlers of the thread's state `state` (level `level`).
static inline void RegisterStateMessages(BSObject* obj, BSMachineThread_struct* thread, BSHandler_struct* handler,
                                         u16 state, u16 level) {
    BSCode_MessageHandlerEntry_struct* entry = handler->m_messages;
    int count = handler->m_numMessages;
    for (int i = 0; i < count; i++, entry++) {
        BSMessageRegistration_struct* replaced;
        BSMessageRegistration_struct* reg = BSRegisterMessage(entry, obj, state, thread, &replaced);
        BSMessageListEntry_struct* e = g_MessageListHead;
        if (g_MessageListHead != NULL) {
            g_MessageListHead = g_MessageListHead->m_next;
        }
        e->m_next = thread->m_messages;
        e->m_replaced = replaced;
        e->m_registration = reg;
        e->m_level = level;
        e->m_state = state;
        thread->m_messages = e;
    }
}

static inline void FreeListEntry(BSMessageListEntry_struct* e) {
    BSMessageRemoveHandler(e->m_registration);
    e->m_next = g_MessageListHead;
    g_MessageListHead = e;
}

// Switches to the state on top of the stack. Leaving for a state at the same
// or a shallower level first runs the exit handler (event 1) of the current
// state or a parent at least as deep, with the new state as its argument.
void BSOpCodeFunc_NEXTSTATE() {
    bool exiting = false;
    bool registerNew = true;
    BSMachineThread_struct* thread = g_pbmtThread;
    u16 current = thread->m_state;
    u32 newState = *g_piStackTop;
    BSCode_EventHandlerEntry_struct* entry = NULL;
    u8 newLevel = g_ppsteStateEntries[(u16)newState]->m_level;
    bool notDeeper = newLevel <= g_ppsteStateEntries[current]->m_level;
    if (!(thread->m_flags & BSTHREAD_EXITING) && notDeeper) {
        PTR32(BSHandler_struct)* handlers = thread->m_code->m_handlers;
        for (u16 s = current; s != 0xFFFF && !exiting;) {
            BSHandler_struct* handler = handlers[s];
            if (handler->m_level < newLevel) {
                break;
            }
            entry = handler->m_events;
            for (int n = handler->m_numEvents; n > 0; n--, entry++) {
                u16 event = entry->m_event;
                if (event == 1) {
                    if (entry->m_flags & BSEVT_DECLARED) {
                        exiting = true;
                    }
                    break;
                }
                if (event > 1) {
                    break;
                }
            }
            s = handler->m_parent;
        }
    }
    if (!exiting) {
        thread->m_flags &= ~BSTHREAD_EXITING;
        if (thread->m_state != (u16)newState) {
            if (notDeeper) {
                // Drop the handlers of states being left.
                BSMessageListEntry_struct** link = &thread->m_messages;
                for (BSMessageListEntry_struct* e = thread->m_messages; e != NULL;) {
                    BSMessageListEntry_struct* next = e->m_next;
                    if (e->m_level > newLevel || (e->m_level == newLevel && e->m_state != (u16)newState)) {
                        *link = next;
                        FreeListEntry(e);
                        if (e->m_replaced != NULL) {
                            e->m_replaced->m_flags &= ~BSMSGREG_REPLACED;
                        }
                    } else {
                        if (e->m_state == (u16)newState) {
                            registerNew = false;
                        }
                        link = &e->m_next;
                    }
                    e = next;
                }
            } else {
                registerNew = true;
            }
            thread->m_state = newState;
            thread->m_handler = g_ppsteStateEntries[(u16)newState];
            if (registerNew) {
                RegisterStateMessages(g_pBSObject, thread, thread->m_handler, newState, newLevel);
            }
        }
        u32 address = thread->m_handler->m_entry;
        g_piStackTop = g_piFrame - 1;
        g_piIP = CodeAddress(g_piCodeBase, address);
        return;
    }
    thread->m_flags |= BSTHREAD_EXITING;
    g_piStackTop = g_piFrame + 1;
    u32 address = entry->m_address;
    g_piStackTop[-1] = (u16)newState;
    *g_piStackTop = (u32)PTR_TO_ADDR(g_pBSObject);
    g_piIP = CodeAddress(g_piCodeBase, address);
}

void BSOpCodeFunc_TRACE() {
    g_piIP++;
}

void BSOpCodeFunc_BREAK() {
    g_iStopThread = 1;
    g_piIP += BS_OPERAND(*g_piIP);
}

// Returns from a function: byte 3 is the number of argument/local words
// below the saved frame, byte 2 whether a value is returned.
void BSOpCodeFunc_RETURN() {
    u32* frame = g_piFrame;
    u32* saved = frame + BS_BYTE3(*g_piIP);
    g_piFrame = ADDR_TO_PTR(u32, saved[0]);
    if (BS_BYTE2(*g_piIP) != 0) {
        u32* top = g_piStackTop;
        g_piStackTop = frame;
        *frame = *top;
    } else {
        g_piStackTop = frame - 1;
    }
    g_piIP = ADDR_TO_PTR(u32, saved[1]);
}

void BSOpCodeFunc_BIFNCALL() {
    g_iCurrentBIFIndex = BS_OPERAND(*g_piIP);
    g_pBuiltInFunctions[g_iCurrentBIFIndex].m_func((int**)&g_piStackTop, g_pObj);
    g_piIP++;
}

#ifdef FRONTLINE_NATIVE
// Rewritten's AI (native/ai.h): the BIFNCALL a built-in was called from.
u32* NativeBSInstruction() {
    return g_piIP;
}
#endif

// Calls the function at the address in the next word; byte 2 is the number
// of arguments already pushed.
void BSOpCodeFunc_FNCALL() {
    u32* ip = g_piIP;
    u32* top = g_piStackTop;
    g_piStackTop = top + 2;
    top[1] = (u32)PTR_TO_ADDR(g_piFrame);
    top[2] = (u32)PTR_TO_ADDR((ip + 2));
    g_piFrame = top - BS_BYTE2(ip[0]) + 1;
    g_piIP = CodeAddress(g_piCodeBase, ip[1]);
}

// Pushes property byte 2 of the running object's trigger, or with byte 3 set,
// of the trigger in frame word 3.
void BSOpCodeFunc_GP() {
    u32 w = *g_piIP;
    xyzProperty_Struct* data;
    if (BS_BYTE3(w) != 0) {
        data = (xyzProperty_Struct*)(ADDR_TO_PTR(TriggerObject_struct, g_piFrame[3]))->m_data;
    } else {
        data = (xyzProperty_Struct*)g_pBSObject->m_trigger->m_data;
    }
    g_piStackTop++;
    *g_piStackTop = LoadPropertyData(data, BS_BYTE2(w));
    g_piIP++;
}

// Pushes string `operand` of the running class.
void BSOpCodeFunc_LS() {
    *++g_piStackTop = (u32)PTR_TO_ADDR(g_pBSObject->m_class->m_strings[BS_OPERAND(*g_piIP)]);
    g_piIP++;
}

// Replaces the pointer on top with its word `operand`.
void BSOpCodeFunc_LM() {
    *g_piStackTop = (ADDR_TO_PTR(u32, *g_piStackTop))[BS_OPERAND(*g_piIP)];
    g_piIP++;
}

// Stores the second word into word `operand` of the pointer on top.
void BSOpCodeFunc_SM() {
    (ADDR_TO_PTR(u32, g_piStackTop[0]))[BS_OPERAND(*g_piIP)] = g_piStackTop[-1];
    g_piStackTop -= 2;
    g_piIP++;
}

void BSOpCodeFunc_LG() {
    *++g_piStackTop = g_piGlobals[BS_OPERAND(*g_piIP)];
    g_piIP++;
}

void BSOpCodeFunc_SG() {
    u32* top = g_piStackTop--;
    g_piGlobals[BS_OPERAND(*g_piIP)] = *top;
    g_piIP++;
}

void BSOpCodeFunc_LW() {
    *++g_piStackTop = g_piFrame[BS_OPERAND(*g_piIP)];
    g_piIP++;
}

void BSOpCodeFunc_SW() {
    u32* top = g_piStackTop--;
    g_piFrame[BS_OPERAND(*g_piIP)] = *top;
    g_piIP++;
}

void BSOpCodeFunc_PUSH() {
    *++g_piStackTop = g_piIP[1];
    g_piIP += 2;
}

void BSOpCodeFunc_POP() {
    int count = BS_OPERAND(*g_piIP);
    g_piIP++;
    g_piStackTop -= count;
}

// Converts the top word to type `operand`.
void BSOpCodeFunc_CAST() {
    u32* top = g_piStackTop;
    switch (BS_OPERAND(*g_piIP)) {
    case BS_INT:
        AsInt(*top) = (s32)AsFloat(*top);
        break;
    case BS_FLOAT:
        AsFloat(*top) = (f32)AsInt(*top);
        break;
    }
    g_piIP++;
}

// Binary comparisons: replace the two top words with 0 or 1.
#define BS_COMPARE_OP(name, op)                                       \
    void BSOpCodeFunc_##name() {                                      \
        u32* top;                                                     \
        switch ((u32)*g_piIP >> 16) {                                 \
        case BS_II:                                                   \
            top = g_piStackTop;                                       \
            top[-1] = AsInt(top[-1]) op AsInt(top[0]);                \
            break;                                                    \
        case BS_FI:                                                   \
            top = g_piStackTop;                                       \
            top[-1] = AsFloat(top[-1]) op (f32)AsInt(top[0]);         \
            break;                                                    \
        case BS_IF:                                                   \
            top = g_piStackTop;                                       \
            top[-1] = (f32)AsInt(top[-1]) op AsFloat(top[0]);         \
            break;                                                    \
        case BS_FF:                                                   \
            top = g_piStackTop;                                       \
            top[-1] = AsFloat(top[-1]) op AsFloat(top[0]);            \
            break;                                                    \
        default:                                                      \
            g_piStackTop[-1] = 0;                                     \
            break;                                                    \
        }                                                             \
        g_piStackTop--;                                               \
        g_piIP++;                                                     \
    }

BS_COMPARE_OP(NE, !=)
BS_COMPARE_OP(EQ, ==)
BS_COMPARE_OP(LTE, <=)
BS_COMPARE_OP(LT, <)
BS_COMPARE_OP(GTE, >=)
BS_COMPARE_OP(GT, >)

void BSOpCodeFunc_NOT() {
    *g_piStackTop = *g_piStackTop == 0;
    g_piIP++;
}

void BSOpCodeFunc_OR() {
    u32* top = g_piStackTop;
    top[-1] = (u8)(top[-1] != 0 || top[0] != 0);
    g_piStackTop--;
    g_piIP++;
}

void BSOpCodeFunc_AND() {
    u32* top = g_piStackTop;
    top[-1] = (u8)(top[-1] != 0 && top[0] != 0);
    g_piStackTop--;
    g_piIP++;
}

void BSOpCodeFunc_NEG() {
    switch ((u32)*g_piIP >> 16) {
    case BS_INT:
        AsInt(*g_piStackTop) = -AsInt(*g_piStackTop);
        break;
    case BS_FLOAT:
        AsFloat(*g_piStackTop) = -AsFloat(*g_piStackTop);
        break;
    default:
        *g_piStackTop = 0;
        break;
    }
    g_piIP++;
}

// Binary arithmetic: the result has the float type if either operand does.
#define BS_ARITH_OP(name, op)                                                  \
    void BSOpCodeFunc_##name() {                                               \
        u32* top;                                                              \
        switch ((u32)*g_piIP >> 16) {                                          \
        case BS_II:                                                            \
            top = g_piStackTop;                                                \
            AsInt(top[-1]) = AsInt(top[-1]) op AsInt(top[0]);                  \
            break;                                                             \
        case BS_FI:                                                            \
            top = g_piStackTop;                                                \
            AsFloat(top[-1]) = AsFloat(top[-1]) op (f32)AsInt(top[0]);         \
            break;                                                             \
        case BS_IF:                                                            \
            top = g_piStackTop;                                                \
            AsFloat(top[-1]) = (f32)AsInt(top[-1]) op AsFloat(top[0]);         \
            break;                                                             \
        case BS_FF:                                                            \
            top = g_piStackTop;                                                \
            AsFloat(top[-1]) = AsFloat(top[-1]) op AsFloat(top[0]);            \
            break;                                                             \
        default:                                                               \
            g_piStackTop[-1] = 0;                                              \
            break;                                                             \
        }                                                                      \
        g_piStackTop--;                                                        \
        g_piIP++;                                                              \
    }

BS_ARITH_OP(MULT, *)
BS_ARITH_OP(DIV, /)
BS_ARITH_OP(SUB, -)
BS_ARITH_OP(ADD, +)

void BSOpCodeFunc_JMP() {
    g_piIP += BS_OPERAND(*g_piIP);
}

// Branches by `operand` if the popped word is zero.
void BSOpCodeFunc_BNE() {
    if (*g_piStackTop != 0) {
        g_piIP++;
    } else {
        g_piIP += BS_OPERAND(*g_piIP);
    }
    g_piStackTop--;
}

// Runs `thread` of `obj` until it stops (BREAK), then restores the machine
// state of the caller (threads may run nested, from built-ins or events).
int BSExecuteThread(BSObject* obj, BSMachineThread_struct* thread) {
    BSObject* savedObject = g_pBSObject;
    BSMachineThread_struct* savedThread = g_pbmtThread;
    g_pBSObject = obj;
    u32* savedCodeBase = g_piCodeBase;
    g_pbmtThread = thread;
    u32* savedTop = g_piStackTop;
    u32* savedFrame = g_piFrame;
    u32* savedIP = g_piIP;
    g_piCodeBase = thread->m_code->m_code;
    u32* savedGlobals = g_piGlobals;
    PTR32(BSHandler_struct)* savedStates = g_ppsteStateEntries;
    g_piStackTop = thread->m_sp;
    int savedBIF = g_iCurrentBIFIndex;
    ISceneNode* savedNode = g_pObj;
    g_piFrame = thread->m_frame;
    g_piIP = thread->m_pc;
    g_piGlobals = thread->m_globals;
    g_ppsteStateEntries = thread->m_code->m_handlers;
    BSGO_Basic* go = obj->m_gameObject;
    g_pObj = go != NULL ? go->GetSceneNode() : NULL;
    g_iStopThread = 0;
    while (g_iStopThread == 0) {
        g_bocfOpCodeFuncs[*g_piIP & 0xFF]();
    }
    thread->m_pc = g_piIP;
    thread->m_flags &= ~BSTHREAD_EXITING;
    g_piStackTop = savedTop;
    g_piIP = savedIP;
    g_piFrame = savedFrame;
    g_piCodeBase = savedCodeBase;
    g_iCurrentBIFIndex = savedBIF;
    g_piGlobals = savedGlobals;
    g_ppsteStateEntries = savedStates;
    g_pbmtThread = savedThread;
    g_pObj = savedNode;
    g_pBSObject = savedObject;
    g_iStopThread = 0;
    return 0;
}

int BSDestroyThread(BSMachineThread_struct* thread) {
    BSMessageListEntry_struct* e = thread->m_messages;
    while (e != NULL) {
        BSMessageListEntry_struct* next = e->m_next;
        FreeListEntry(e);
        e = next;
    }
    return 0;
}

// Starts `thread` in the code's initial state and runs it.
int BSCreateThread(BSObject* obj, BSCode_struct* code, BSMachineThread_struct* thread, void* globals) {
    thread->m_code = code;
    thread->m_sp = g_piStackTop + 1;
    thread->m_frame = thread->m_sp + 1;
    thread->m_flags = 0;
    thread->m_globals = (u32*)globals;
    thread->m_state = code->field_0x16;
    thread->m_handler = thread->m_code->m_handlers[thread->m_state];
    thread->m_pc = CodeAddress(code->m_code, thread->m_handler->m_entry);
    thread->m_messages = NULL;
    BSHandler_struct* handler = thread->m_handler;
    BSCode_MessageHandlerEntry_struct* entry = handler->m_messages;
    int count = handler->m_numMessages;
    for (int i = 0; i < count; i++, entry++) {
        BSMessageRegistration_struct* replaced;
        BSMessageRegistration_struct* reg = BSRegisterMessage(entry, obj, thread->m_state, thread, &replaced);
        BSMessageListEntry_struct* e = g_MessageListHead;
        if (g_MessageListHead != NULL) {
            g_MessageListHead = g_MessageListHead->m_next;
        }
        e->m_next = thread->m_messages;
        e->m_replaced = replaced;
        e->m_registration = reg;
        e->m_level = thread->m_handler->m_level;
        e->m_state = thread->m_state;
        thread->m_messages = e;
    }
    BSExecuteThread(obj, thread);
    return 0;
}

int BSEndMachine() {
    BSClass_struct* cls = g_pcClassList;
    while (cls != NULL) {
        BSClass_struct* next = cls->m_next;
        BSFileDeleteClass(cls);
        cls = next;
    }
    g_pcClassList = NULL;
    return 0;
}

int BSInitMachine() {
    g_pBSMachineMemory = (u8*)BSUtilGetMemory(
        g_iDefaultStackSize * 4 + g_iBSMessageRegistrationListSize * sizeof(BSMessageListEntry_struct), 0);
    g_pBSMachineMemoryOffset = 0;
    g_iStopThread = 0;
    g_pcClassList = NULL;
    g_bocfOpCodeFuncs[0] = BSOpCodeFunc_BNE;
    g_bocfOpCodeFuncs[1] = BSOpCodeFunc_JMP;
    g_bocfOpCodeFuncs[2] = BSOpCodeFunc_ADD;
    g_bocfOpCodeFuncs[3] = BSOpCodeFunc_SUB;
    g_bocfOpCodeFuncs[4] = BSOpCodeFunc_DIV;
    g_bocfOpCodeFuncs[5] = BSOpCodeFunc_MULT;
    g_bocfOpCodeFuncs[6] = BSOpCodeFunc_NEG;
    g_bocfOpCodeFuncs[7] = BSOpCodeFunc_EQ;
    g_bocfOpCodeFuncs[30] = BSOpCodeFunc_NE;
    g_bocfOpCodeFuncs[8] = BSOpCodeFunc_LT;
    g_bocfOpCodeFuncs[9] = BSOpCodeFunc_GT;
    g_bocfOpCodeFuncs[10] = BSOpCodeFunc_LTE;
    g_bocfOpCodeFuncs[11] = BSOpCodeFunc_GTE;
    g_bocfOpCodeFuncs[12] = BSOpCodeFunc_AND;
    g_bocfOpCodeFuncs[13] = BSOpCodeFunc_OR;
    g_bocfOpCodeFuncs[14] = BSOpCodeFunc_NOT;
    g_bocfOpCodeFuncs[15] = BSOpCodeFunc_LW;
    g_bocfOpCodeFuncs[16] = BSOpCodeFunc_SW;
    g_bocfOpCodeFuncs[17] = BSOpCodeFunc_PUSH;
    g_bocfOpCodeFuncs[18] = BSOpCodeFunc_POP;
    g_bocfOpCodeFuncs[19] = BSOpCodeFunc_CAST;
    g_bocfOpCodeFuncs[20] = BSOpCodeFunc_FNCALL;
    g_bocfOpCodeFuncs[21] = BSOpCodeFunc_RETURN;
    g_bocfOpCodeFuncs[22] = BSOpCodeFunc_BREAK;
    g_bocfOpCodeFuncs[23] = BSOpCodeFunc_NEXTSTATE;
    g_bocfOpCodeFuncs[24] = BSOpCodeFunc_LG;
    g_bocfOpCodeFuncs[25] = BSOpCodeFunc_SG;
    g_bocfOpCodeFuncs[26] = BSOpCodeFunc_BIFNCALL;
    g_bocfOpCodeFuncs[27] = BSOpCodeFunc_LM;
    g_bocfOpCodeFuncs[28] = BSOpCodeFunc_SM;
    g_bocfOpCodeFuncs[29] = BSOpCodeFunc_GP;
    g_bocfOpCodeFuncs[31] = BSOpCodeFunc_TRACE;
    g_bocfOpCodeFuncs[32] = BSOpCodeFunc_LS;

    g_MessageList = (BSMessageListEntry_struct*)g_pBSMachineMemory;
    g_pBSMachineMemoryOffset = g_iBSMessageRegistrationListSize * sizeof(BSMessageListEntry_struct);
    for (int i = 0; i < g_iBSMessageRegistrationListSize - 1; i++) {
        g_MessageList[i].m_next = &g_MessageList[i + 1];
    }
    g_MessageList[g_iBSMessageRegistrationListSize - 1].m_next = NULL;
    u32* stack = (u32*)(g_pBSMachineMemory + g_pBSMachineMemoryOffset);
    g_MessageListHead = g_MessageList;
    g_piStackBase = stack;
    g_pBSMachineMemoryOffset += g_iDefaultStackSize * 4;
    g_piStackTop = stack;
    g_piFrame = stack + 1;
    return 0;
}

int BSMachineGetMemoryRequirements() {
    return g_iDefaultStackSize * 4 + g_iBSMessageRegistrationListSize * sizeof(BSMessageListEntry_struct);
}

// Property data. Every placed object starts with the core properties (0-21);
// each type adds its own from 22 on. Positions are 12.4 fixed point, read
// as floats.

static inline int FixedToFloatBits(s16 value) {
    f32 f = value * 0.0625f;
    return *(int*)&f;
}

int LoadPropertyData(xyzProperty_Struct* data, int property) {
    switch (data->m_type) {
    case 1:
        return _LoadPropertyData((MOH_core_Struct*)data, property);
    case 2:
        return _LoadPropertyData((MOH_mechanic_Struct*)data, property);
    case 3:
        return _LoadPropertyData((MOH_enemy_Struct*)data, property);
    case 4:
        return _LoadPropertyData((MOH_powerup_Struct*)data, property);
    case 5:
        return _LoadPropertyData((MOH_wayPoint_Struct*)data, property);
    case 6:
        return _LoadPropertyData((MOH_pathPoint_Struct*)data, property);
    case 7:
        return _LoadPropertyData((MOH_envMod_Struct*)data, property);
    case 8:
        return _LoadPropertyData((MOH_mechanicEnvMod_Struct*)data, property);
    case 9:
        return _LoadPropertyData((MOH_particle_Struct*)data, property);
    case 10:
        return _LoadPropertyData((MOH_projectileGenerator_Struct*)data, property);
    case 11:
        return _LoadPropertyData((MOH_projectileGeneratorTarget_Struct*)data, property);
    case 12:
        return _LoadPropertyData((MOH_cameraShaker_Struct*)data, property);
    case 13:
        return _LoadPropertyData((MOH_playerPathController_Struct*)data, property);
    case 14:
        return _LoadPropertyData((MOH_animatedLight_Struct*)data, property);
    case 15:
        return _LoadPropertyData((MOH_fogParams_Struct*)data, property);
    default:
        return 0;
    }
}

struct BSProp_animatedLight {
    /* 0x0 */ u8 unk_0x0[0x2C];
    /* 0x2C */ s16 field_0x2C;
    /* 0x2E */ s16 field_0x2E;
    /* 0x30 */ s16 field_0x30;
    /* 0x32 */ s16 field_0x32;
    /* 0x34 */ u8 field_0x34;
    /* 0x35 */ u8 field_0x35;
    /* 0x36 */ s16 field_0x36;
    /* 0x38 */ s16 field_0x38;
    /* 0x3A */ s16 field_0x3A;
    /* 0x3C */ s16 field_0x3C;
    /* 0x3E */ u8 unk_0x3E[0xA];
    /* 0x48 */ s32 field_0x48;
    /* 0x4C */ u8 unk_0x4C[0x4];
    /* 0x50 */ s32 field_0x50;
    /* 0x54 */ u8 unk_0x54[0x1C];
    /* 0x70 */ u8 field_0x70;
    /* 0x71 */ u8 unk_0x71[0x1];
    /* 0x72 */ s16 field_0x72;
};

struct BSProp_cameraShaker {
    /* 0x0 */ u8 unk_0x0[0x2C];
    /* 0x2C */ s16 field_0x2C;
    /* 0x2E */ s16 field_0x2E;
    /* 0x30 */ s16 field_0x30;
    /* 0x32 */ s16 field_0x32;
    /* 0x34 */ u8 field_0x34;
    /* 0x35 */ u8 field_0x35;
    /* 0x36 */ s16 field_0x36;
    /* 0x38 */ s16 field_0x38;
    /* 0x3A */ s16 field_0x3A;
    /* 0x3C */ s16 field_0x3C;
    /* 0x3E */ u8 unk_0x3E[0xA];
    /* 0x48 */ s32 field_0x48;
    /* 0x4C */ u8 unk_0x4C[0x4];
    /* 0x50 */ s32 field_0x50;
    /* 0x54 */ u8 unk_0x54[0x1C];
    /* 0x70 */ u8 field_0x70;
    /* 0x71 */ u8 field_0x71;
    /* 0x72 */ u8 unk_0x72[0x2];
    /* 0x74 */ s32 field_0x74;
    /* 0x78 */ s32 field_0x78;
    /* 0x7C */ s32 field_0x7C;
    /* 0x80 */ s32 field_0x80;
    /* 0x84 */ s32 field_0x84;
};

struct BSProp_core {
    /* 0x0 */ u8 unk_0x0[0x2C];
    /* 0x2C */ s16 field_0x2C;
    /* 0x2E */ s16 field_0x2E;
    /* 0x30 */ s16 field_0x30;
    /* 0x32 */ s16 field_0x32;
    /* 0x34 */ u8 field_0x34;
    /* 0x35 */ u8 field_0x35;
    /* 0x36 */ s16 field_0x36;
    /* 0x38 */ s16 field_0x38;
    /* 0x3A */ s16 field_0x3A;
    /* 0x3C */ s16 field_0x3C;
    /* 0x3E */ u8 unk_0x3E[0xA];
    /* 0x48 */ s32 field_0x48;
    /* 0x4C */ u8 unk_0x4C[0x4];
    /* 0x50 */ s32 field_0x50;
};

struct BSProp_enemy {
    /* 0x0 */ u8 unk_0x0[0x2C];
    /* 0x2C */ s16 field_0x2C;
    /* 0x2E */ s16 field_0x2E;
    /* 0x30 */ s16 field_0x30;
    /* 0x32 */ s16 field_0x32;
    /* 0x34 */ u8 field_0x34;
    /* 0x35 */ u8 field_0x35;
    /* 0x36 */ s16 field_0x36;
    /* 0x38 */ s16 field_0x38;
    /* 0x3A */ s16 field_0x3A;
    /* 0x3C */ s16 field_0x3C;
    /* 0x3E */ u8 unk_0x3E[0xA];
    /* 0x48 */ s32 field_0x48;
    /* 0x4C */ u8 unk_0x4C[0x4];
    /* 0x50 */ s32 field_0x50;
    /* 0x54 */ u8 unk_0x54[0x1C];
    /* 0x70 */ u8 field_0x70;
    /* 0x71 */ u8 field_0x71;
    /* 0x72 */ u8 field_0x72;
    /* 0x73 */ u8 field_0x73;
    /* 0x74 */ u8 field_0x74;
    /* 0x75 */ u8 field_0x75;
    /* 0x76 */ u8 field_0x76;
    /* 0x77 */ u8 field_0x77;
    /* 0x78 */ u8 field_0x78;
    /* 0x79 */ u8 field_0x79;
    /* 0x7A */ u8 field_0x7A;
    /* 0x7B */ u8 field_0x7B;
    /* 0x7C */ u8 field_0x7C;
    /* 0x7D */ u8 unk_0x7D[0x1];
    /* 0x7E */ s16 field_0x7E;
    /* 0x80 */ s16 field_0x80;
    /* 0x82 */ s16 field_0x82;
    /* 0x84 */ s16 field_0x84;
    /* 0x86 */ s16 field_0x86;
    /* 0x88 */ s16 field_0x88;
    /* 0x8A */ s16 field_0x8A;
    /* 0x8C */ s16 field_0x8C;
    /* 0x8E */ s16 field_0x8E;
    /* 0x90 */ s32 field_0x90;
    /* 0x94 */ s32 field_0x94;
    /* 0x98 */ s32 field_0x98;
    /* 0x9C */ s32 field_0x9C;
    /* 0xA0 */ s32 field_0xA0;
    /* 0xA4 */ s32 field_0xA4;
    /* 0xA8 */ s32 field_0xA8;
    /* 0xAC */ s32 field_0xAC;
};

struct BSProp_envMod {
    /* 0x0 */ u8 unk_0x0[0x2C];
    /* 0x2C */ s16 field_0x2C;
    /* 0x2E */ s16 field_0x2E;
    /* 0x30 */ s16 field_0x30;
    /* 0x32 */ s16 field_0x32;
    /* 0x34 */ u8 field_0x34;
    /* 0x35 */ u8 field_0x35;
    /* 0x36 */ s16 field_0x36;
    /* 0x38 */ s16 field_0x38;
    /* 0x3A */ s16 field_0x3A;
    /* 0x3C */ s16 field_0x3C;
    /* 0x3E */ u8 unk_0x3E[0xA];
    /* 0x48 */ s32 field_0x48;
    /* 0x4C */ u8 unk_0x4C[0x4];
    /* 0x50 */ s32 field_0x50;
    /* 0x54 */ u8 unk_0x54[0x1C];
    /* 0x70 */ s32 field_0x70;
};

struct BSProp_fogParams {
    /* 0x0 */ u8 unk_0x0[0x2C];
    /* 0x2C */ s16 field_0x2C;
    /* 0x2E */ s16 field_0x2E;
    /* 0x30 */ s16 field_0x30;
    /* 0x32 */ s16 field_0x32;
    /* 0x34 */ u8 field_0x34;
    /* 0x35 */ u8 field_0x35;
    /* 0x36 */ s16 field_0x36;
    /* 0x38 */ s16 field_0x38;
    /* 0x3A */ s16 field_0x3A;
    /* 0x3C */ s16 field_0x3C;
    /* 0x3E */ u8 unk_0x3E[0xA];
    /* 0x48 */ s32 field_0x48;
    /* 0x4C */ u8 unk_0x4C[0x4];
    /* 0x50 */ s32 field_0x50;
    /* 0x54 */ u8 unk_0x54[0x1C];
    /* 0x70 */ s16 field_0x70;
    /* 0x72 */ s16 field_0x72;
    /* 0x74 */ s16 field_0x74;
    /* 0x76 */ u8 unk_0x76[0x2];
    /* 0x78 */ s32 field_0x78;
    /* 0x7C */ s32 field_0x7C;
};

struct BSProp_mechanicEnvMod {
    /* 0x0 */ u8 unk_0x0[0x2C];
    /* 0x2C */ s16 field_0x2C;
    /* 0x2E */ s16 field_0x2E;
    /* 0x30 */ s16 field_0x30;
    /* 0x32 */ s16 field_0x32;
    /* 0x34 */ u8 field_0x34;
    /* 0x35 */ u8 field_0x35;
    /* 0x36 */ s16 field_0x36;
    /* 0x38 */ s16 field_0x38;
    /* 0x3A */ s16 field_0x3A;
    /* 0x3C */ s16 field_0x3C;
    /* 0x3E */ u8 unk_0x3E[0xA];
    /* 0x48 */ s32 field_0x48;
    /* 0x4C */ u8 unk_0x4C[0x4];
    /* 0x50 */ s32 field_0x50;
    /* 0x54 */ u8 unk_0x54[0x1C];
    /* 0x70 */ u8 field_0x70;
    /* 0x71 */ u8 field_0x71;
    /* 0x72 */ u8 unk_0x72[0x2];
    /* 0x74 */ s32 field_0x74;
    /* 0x78 */ s32 field_0x78;
};

struct BSProp_mechanic {
    /* 0x0 */ u8 unk_0x0[0x2C];
    /* 0x2C */ s16 field_0x2C;
    /* 0x2E */ s16 field_0x2E;
    /* 0x30 */ s16 field_0x30;
    /* 0x32 */ s16 field_0x32;
    /* 0x34 */ u8 field_0x34;
    /* 0x35 */ u8 field_0x35;
    /* 0x36 */ s16 field_0x36;
    /* 0x38 */ s16 field_0x38;
    /* 0x3A */ s16 field_0x3A;
    /* 0x3C */ s16 field_0x3C;
    /* 0x3E */ u8 unk_0x3E[0xA];
    /* 0x48 */ s32 field_0x48;
    /* 0x4C */ u8 unk_0x4C[0x4];
    /* 0x50 */ s32 field_0x50;
    /* 0x54 */ u8 unk_0x54[0x1C];
    /* 0x70 */ u8 field_0x70;
    /* 0x71 */ u8 field_0x71;
    /* 0x72 */ u8 unk_0x72[0x2];
    /* 0x74 */ s32 field_0x74;
};

struct BSProp_particle {
    /* 0x0 */ u8 unk_0x0[0x2C];
    /* 0x2C */ s16 field_0x2C;
    /* 0x2E */ s16 field_0x2E;
    /* 0x30 */ s16 field_0x30;
    /* 0x32 */ s16 field_0x32;
    /* 0x34 */ u8 field_0x34;
    /* 0x35 */ u8 field_0x35;
    /* 0x36 */ s16 field_0x36;
    /* 0x38 */ s16 field_0x38;
    /* 0x3A */ s16 field_0x3A;
    /* 0x3C */ s16 field_0x3C;
    /* 0x3E */ u8 unk_0x3E[0xA];
    /* 0x48 */ s32 field_0x48;
    /* 0x4C */ u8 unk_0x4C[0x4];
    /* 0x50 */ s32 field_0x50;
    /* 0x54 */ u8 unk_0x54[0x1C];
    /* 0x70 */ u8 field_0x70;
    /* 0x71 */ u8 field_0x71;
    /* 0x72 */ u8 field_0x72;
    /* 0x73 */ u8 field_0x73;
    /* 0x74 */ u8 field_0x74;
    /* 0x75 */ u8 field_0x75;
    /* 0x76 */ u8 field_0x76;
    /* 0x77 */ u8 field_0x77;
    /* 0x78 */ u8 field_0x78;
    /* 0x79 */ u8 field_0x79;
    /* 0x7A */ u8 field_0x7A;
    /* 0x7B */ u8 field_0x7B;
    /* 0x7C */ u8 field_0x7C;
    /* 0x7D */ u8 field_0x7D;
    /* 0x7E */ u8 field_0x7E;
    /* 0x7F */ u8 field_0x7F;
    /* 0x80 */ u8 field_0x80;
    /* 0x81 */ u8 unk_0x81[0x1];
    /* 0x82 */ s16 field_0x82;
    /* 0x84 */ s16 field_0x84;
    /* 0x86 */ u8 unk_0x86[0x2];
    /* 0x88 */ s32 field_0x88;
    /* 0x8C */ s32 field_0x8C;
    /* 0x90 */ s32 field_0x90;
    /* 0x94 */ s32 field_0x94;
    /* 0x98 */ s32 field_0x98;
    /* 0x9C */ s32 field_0x9C;
    /* 0xA0 */ s32 field_0xA0;
    /* 0xA4 */ s32 field_0xA4;
    /* 0xA8 */ s32 field_0xA8;
    /* 0xAC */ s32 field_0xAC;
    /* 0xB0 */ s32 field_0xB0;
    /* 0xB4 */ s32 field_0xB4;
    /* 0xB8 */ s32 field_0xB8;
    /* 0xBC */ s32 field_0xBC;
    /* 0xC0 */ s32 field_0xC0;
    /* 0xC4 */ s32 field_0xC4;
    /* 0xC8 */ s32 field_0xC8;
    /* 0xCC */ s32 field_0xCC;
    /* 0xD0 */ s32 field_0xD0;
    /* 0xD4 */ s32 field_0xD4;
    /* 0xD8 */ s32 field_0xD8;
    /* 0xDC */ s32 field_0xDC;
    /* 0xE0 */ s32 field_0xE0;
    /* 0xE4 */ s32 field_0xE4;
    /* 0xE8 */ s32 field_0xE8;
};

struct BSProp_pathPoint {
    /* 0x0 */ u8 unk_0x0[0x2C];
    /* 0x2C */ s16 field_0x2C;
    /* 0x2E */ s16 field_0x2E;
    /* 0x30 */ s16 field_0x30;
    /* 0x32 */ s16 field_0x32;
    /* 0x34 */ u8 field_0x34;
    /* 0x35 */ u8 field_0x35;
    /* 0x36 */ s16 field_0x36;
    /* 0x38 */ s16 field_0x38;
    /* 0x3A */ s16 field_0x3A;
    /* 0x3C */ s16 field_0x3C;
    /* 0x3E */ u8 unk_0x3E[0xA];
    /* 0x48 */ s32 field_0x48;
    /* 0x4C */ u8 unk_0x4C[0x4];
    /* 0x50 */ s32 field_0x50;
};

struct BSProp_playerPathController {
    /* 0x0 */ u8 unk_0x0[0x2C];
    /* 0x2C */ s16 field_0x2C;
    /* 0x2E */ s16 field_0x2E;
    /* 0x30 */ s16 field_0x30;
    /* 0x32 */ s16 field_0x32;
    /* 0x34 */ u8 field_0x34;
    /* 0x35 */ u8 field_0x35;
    /* 0x36 */ s16 field_0x36;
    /* 0x38 */ s16 field_0x38;
    /* 0x3A */ s16 field_0x3A;
    /* 0x3C */ s16 field_0x3C;
    /* 0x3E */ u8 unk_0x3E[0xA];
    /* 0x48 */ s32 field_0x48;
    /* 0x4C */ u8 unk_0x4C[0x4];
    /* 0x50 */ s32 field_0x50;
    /* 0x54 */ u8 unk_0x54[0x1C];
    /* 0x70 */ u8 field_0x70;
    /* 0x71 */ u8 field_0x71;
    /* 0x72 */ s16 field_0x72;
    /* 0x74 */ s16 field_0x74;
    /* 0x76 */ u8 unk_0x76[0x2];
    /* 0x78 */ s32 field_0x78;
    /* 0x7C */ s32 field_0x7C;
};

struct BSProp_powerup {
    /* 0x0 */ u8 unk_0x0[0x2C];
    /* 0x2C */ s16 field_0x2C;
    /* 0x2E */ s16 field_0x2E;
    /* 0x30 */ s16 field_0x30;
    /* 0x32 */ s16 field_0x32;
    /* 0x34 */ u8 field_0x34;
    /* 0x35 */ u8 field_0x35;
    /* 0x36 */ s16 field_0x36;
    /* 0x38 */ s16 field_0x38;
    /* 0x3A */ s16 field_0x3A;
    /* 0x3C */ s16 field_0x3C;
    /* 0x3E */ u8 unk_0x3E[0xA];
    /* 0x48 */ s32 field_0x48;
    /* 0x4C */ u8 unk_0x4C[0x4];
    /* 0x50 */ s32 field_0x50;
    /* 0x54 */ u8 unk_0x54[0x1C];
    /* 0x70 */ u8 field_0x70;
};

struct BSProp_projectileGeneratorTarget {
    /* 0x0 */ u8 unk_0x0[0x2C];
    /* 0x2C */ s16 field_0x2C;
    /* 0x2E */ s16 field_0x2E;
    /* 0x30 */ s16 field_0x30;
    /* 0x32 */ s16 field_0x32;
    /* 0x34 */ u8 field_0x34;
    /* 0x35 */ u8 field_0x35;
    /* 0x36 */ s16 field_0x36;
    /* 0x38 */ s16 field_0x38;
    /* 0x3A */ s16 field_0x3A;
    /* 0x3C */ s16 field_0x3C;
    /* 0x3E */ u8 unk_0x3E[0xA];
    /* 0x48 */ s32 field_0x48;
    /* 0x4C */ u8 unk_0x4C[0x4];
    /* 0x50 */ s32 field_0x50;
    /* 0x54 */ u8 unk_0x54[0x1C];
    /* 0x70 */ u8 field_0x70;
    /* 0x71 */ u8 field_0x71;
    /* 0x72 */ s16 field_0x72;
    /* 0x74 */ s32 field_0x74;
    /* 0x78 */ s32 field_0x78;
    /* 0x7C */ s32 field_0x7C;
    /* 0x80 */ s32 field_0x80;
    /* 0x84 */ s32 field_0x84;
    /* 0x88 */ s32 field_0x88;
};

struct BSProp_projectileGenerator {
    /* 0x0 */ u8 unk_0x0[0x2C];
    /* 0x2C */ s16 field_0x2C;
    /* 0x2E */ s16 field_0x2E;
    /* 0x30 */ s16 field_0x30;
    /* 0x32 */ s16 field_0x32;
    /* 0x34 */ u8 field_0x34;
    /* 0x35 */ u8 field_0x35;
    /* 0x36 */ s16 field_0x36;
    /* 0x38 */ s16 field_0x38;
    /* 0x3A */ s16 field_0x3A;
    /* 0x3C */ s16 field_0x3C;
    /* 0x3E */ u8 unk_0x3E[0xA];
    /* 0x48 */ s32 field_0x48;
    /* 0x4C */ u8 unk_0x4C[0x4];
    /* 0x50 */ s32 field_0x50;
    /* 0x54 */ u8 unk_0x54[0x1C];
    /* 0x70 */ u8 field_0x70;
    /* 0x71 */ u8 field_0x71;
    /* 0x72 */ s16 field_0x72;
    /* 0x74 */ s32 field_0x74;
    /* 0x78 */ s32 field_0x78;
    /* 0x7C */ s32 field_0x7C;
    /* 0x80 */ s32 field_0x80;
    /* 0x84 */ s32 field_0x84;
    /* 0x88 */ s32 field_0x88;
    /* 0x8C */ s32 field_0x8C;
    /* 0x90 */ s32 field_0x90;
    /* 0x94 */ s32 field_0x94;
};

struct BSProp_wayPoint {
    /* 0x0 */ u8 unk_0x0[0x2C];
    /* 0x2C */ s16 field_0x2C;
    /* 0x2E */ s16 field_0x2E;
    /* 0x30 */ s16 field_0x30;
    /* 0x32 */ s16 field_0x32;
    /* 0x34 */ u8 field_0x34;
    /* 0x35 */ u8 field_0x35;
    /* 0x36 */ s16 field_0x36;
    /* 0x38 */ s16 field_0x38;
    /* 0x3A */ s16 field_0x3A;
    /* 0x3C */ s16 field_0x3C;
    /* 0x3E */ u8 unk_0x3E[0xA];
    /* 0x48 */ s32 field_0x48;
    /* 0x4C */ u8 unk_0x4C[0x4];
    /* 0x50 */ s32 field_0x50;
    /* 0x54 */ u8 unk_0x54[0x1C];
    /* 0x70 */ u8 field_0x70;
};

int _LoadPropertyData(MOH_fogParams_Struct* data, int property) {
    const BSProp_fogParams* p = (const BSProp_fogParams*)data;
    switch (property) {
    case 0:
        return p->field_0x36;
    case 2:
        return p->field_0x38;
    case 4:
        return p->field_0x35;
    case 5:
        return p->field_0x48;
    case 7:
        return p->field_0x50;
    case 8:
        return p->field_0x2C;
    case 9:
        return FixedToFloatBits(p->field_0x2E);
    case 10:
        return FixedToFloatBits(p->field_0x30);
    case 11:
        return FixedToFloatBits(p->field_0x32);
    case 18:
        return p->field_0x34 & 1;
    case 19:
        return p->field_0x3A;
    case 20:
        return (p->field_0x34 >> 1) & 1;
    case 21:
        return p->field_0x3C;
    case 22:
        return p->field_0x78;
    case 23:
        return p->field_0x7C;
    case 24:
        return p->field_0x70;
    case 25:
        return p->field_0x72;
    case 26:
        return p->field_0x74;
    default:
        return 0;
    }
}

int _LoadPropertyData(MOH_animatedLight_Struct* data, int property) {
    const BSProp_animatedLight* p = (const BSProp_animatedLight*)data;
    switch (property) {
    case 0:
        return p->field_0x36;
    case 2:
        return p->field_0x38;
    case 4:
        return p->field_0x35;
    case 5:
        return p->field_0x48;
    case 7:
        return p->field_0x50;
    case 8:
        return p->field_0x2C;
    case 9:
        return FixedToFloatBits(p->field_0x2E);
    case 10:
        return FixedToFloatBits(p->field_0x30);
    case 11:
        return FixedToFloatBits(p->field_0x32);
    case 18:
        return p->field_0x34 & 1;
    case 19:
        return p->field_0x3A;
    case 20:
        return (p->field_0x34 >> 1) & 1;
    case 21:
        return p->field_0x3C;
    case 24:
        return p->field_0x70;
    case 25:
        return p->field_0x72;
    default:
        return 0;
    }
}

int _LoadPropertyData(MOH_playerPathController_Struct* data, int property) {
    const BSProp_playerPathController* p = (const BSProp_playerPathController*)data;
    switch (property) {
    case 0:
        return p->field_0x36;
    case 2:
        return p->field_0x38;
    case 4:
        return p->field_0x35;
    case 5:
        return p->field_0x48;
    case 7:
        return p->field_0x50;
    case 8:
        return p->field_0x2C;
    case 9:
        return FixedToFloatBits(p->field_0x2E);
    case 10:
        return FixedToFloatBits(p->field_0x30);
    case 11:
        return FixedToFloatBits(p->field_0x32);
    case 18:
        return p->field_0x34 & 1;
    case 19:
        return p->field_0x3A;
    case 20:
        return (p->field_0x34 >> 1) & 1;
    case 21:
        return p->field_0x3C;
    case 22:
        return p->field_0x70;
    case 23:
        return p->field_0x7C;
    case 24:
        return p->field_0x71;
    case 25:
        return p->field_0x72;
    case 26:
        return p->field_0x74;
    case 27:
        return p->field_0x78;
    default:
        return 0;
    }
}

int _LoadPropertyData(MOH_cameraShaker_Struct* data, int property) {
    const BSProp_cameraShaker* p = (const BSProp_cameraShaker*)data;
    switch (property) {
    case 0:
        return p->field_0x36;
    case 2:
        return p->field_0x38;
    case 4:
        return p->field_0x35;
    case 5:
        return p->field_0x48;
    case 7:
        return p->field_0x50;
    case 8:
        return p->field_0x2C;
    case 9:
        return FixedToFloatBits(p->field_0x2E);
    case 10:
        return FixedToFloatBits(p->field_0x30);
    case 11:
        return FixedToFloatBits(p->field_0x32);
    case 18:
        return p->field_0x34 & 1;
    case 19:
        return p->field_0x3A;
    case 20:
        return (p->field_0x34 >> 1) & 1;
    case 21:
        return p->field_0x3C;
    case 22:
        return p->field_0x70;
    case 23:
        return p->field_0x84;
    case 24:
        return p->field_0x71;
    case 25:
        return p->field_0x74;
    case 26:
        return p->field_0x78;
    case 27:
        return p->field_0x7C;
    case 28:
        return p->field_0x80;
    default:
        return 0;
    }
}

int _LoadPropertyData(MOH_projectileGeneratorTarget_Struct* data, int property) {
    const BSProp_projectileGeneratorTarget* p = (const BSProp_projectileGeneratorTarget*)data;
    switch (property) {
    case 0:
        return p->field_0x36;
    case 2:
        return p->field_0x38;
    case 4:
        return p->field_0x35;
    case 5:
        return p->field_0x48;
    case 7:
        return p->field_0x50;
    case 8:
        return p->field_0x2C;
    case 9:
        return FixedToFloatBits(p->field_0x2E);
    case 10:
        return FixedToFloatBits(p->field_0x30);
    case 11:
        return FixedToFloatBits(p->field_0x32);
    case 18:
        return p->field_0x34 & 1;
    case 19:
        return p->field_0x3A;
    case 20:
        return (p->field_0x34 >> 1) & 1;
    case 21:
        return p->field_0x3C;
    case 22:
        return p->field_0x70;
    case 23:
        return p->field_0x88;
    case 24:
        return p->field_0x71;
    case 25:
        return p->field_0x72;
    case 26:
        return p->field_0x74;
    case 27:
        return p->field_0x78;
    case 28:
        return p->field_0x7C;
    case 29:
        return p->field_0x80;
    case 30:
        return p->field_0x84;
    default:
        return 0;
    }
}

int _LoadPropertyData(MOH_projectileGenerator_Struct* data, int property) {
    const BSProp_projectileGenerator* p = (const BSProp_projectileGenerator*)data;
    switch (property) {
    case 0:
        return p->field_0x36;
    case 2:
        return p->field_0x38;
    case 4:
        return p->field_0x35;
    case 5:
        return p->field_0x48;
    case 7:
        return p->field_0x50;
    case 8:
        return p->field_0x2C;
    case 9:
        return FixedToFloatBits(p->field_0x2E);
    case 10:
        return FixedToFloatBits(p->field_0x30);
    case 11:
        return FixedToFloatBits(p->field_0x32);
    case 18:
        return p->field_0x34 & 1;
    case 19:
        return p->field_0x3A;
    case 20:
        return (p->field_0x34 >> 1) & 1;
    case 21:
        return p->field_0x3C;
    case 22:
        return p->field_0x70;
    case 23:
        return p->field_0x90;
    case 24:
        return p->field_0x71;
    case 25:
        return p->field_0x72;
    case 26:
        return p->field_0x74;
    case 27:
        return p->field_0x78;
    case 28:
        return p->field_0x7C;
    case 29:
        return p->field_0x80;
    case 30:
        return p->field_0x94;
    case 31:
        return p->field_0x84;
    case 32:
        return p->field_0x88;
    case 33:
        return p->field_0x8C;
    default:
        return 0;
    }
}

int _LoadPropertyData(MOH_particle_Struct* data, int property) {
    const BSProp_particle* p = (const BSProp_particle*)data;
    switch (property) {
    case 0x0:
        return p->field_0x36;
    case 0x2:
        return p->field_0x38;
    case 0x4:
        return p->field_0x35;
    case 0x5:
        return p->field_0x48;
    case 0x7:
        return p->field_0x50;
    case 0x8:
        return p->field_0x2C;
    case 0x9:
        return FixedToFloatBits(p->field_0x2E);
    case 0xA:
        return FixedToFloatBits(p->field_0x30);
    case 0xB:
        return FixedToFloatBits(p->field_0x32);
    case 0x12:
        return p->field_0x34 & 1;
    case 0x13:
        return p->field_0x3A;
    case 0x14:
        return (p->field_0x34 >> 1) & 1;
    case 0x15:
        return p->field_0x3C;
    case 0x17:
        return p->field_0x82;
    case 0x18:
        return p->field_0x84;
    case 0x19:
        return p->field_0x88;
    case 0x1A:
        return p->field_0x8C;
    case 0x1B:
        return p->field_0x90;
    case 0x1C:
        return p->field_0x94;
    case 0x1D:
        return p->field_0x98;
    case 0x1E:
        return p->field_0x9C;
    case 0x1F:
        return p->field_0xA0;
    case 0x20:
        return p->field_0xA4;
    case 0x21:
        return p->field_0xA8;
    case 0x22:
        return p->field_0xAC;
    case 0x23:
        return p->field_0xB0;
    case 0x24:
        return p->field_0xB4;
    case 0x25:
        return p->field_0xB8;
    case 0x26:
        return p->field_0xBC;
    case 0x27:
        return p->field_0xC0;
    case 0x28:
        return p->field_0xC4;
    case 0x29:
        return p->field_0xC8;
    case 0x2A:
        return p->field_0x71;
    case 0x2B:
        return p->field_0x72;
    case 0x2C:
        return p->field_0x73;
    case 0x2D:
        return p->field_0x74;
    case 0x2E:
        return p->field_0x75;
    case 0x2F:
        return p->field_0x76;
    case 0x30:
        return p->field_0x77;
    case 0x31:
        return p->field_0x78;
    case 0x32:
        return p->field_0x79;
    case 0x33:
        return p->field_0xCC;
    case 0x34:
        return p->field_0xD0;
    case 0x35:
        return p->field_0xD4;
    case 0x36:
        return p->field_0xD8;
    case 0x37:
        return p->field_0xDC;
    case 0x38:
        return p->field_0xE0;
    case 0x39:
        return p->field_0xE4;
    case 0x3A:
        return p->field_0xE8;
    case 0x3B:
        return p->field_0x7A;
    case 0x3C:
        return p->field_0x7B;
    case 0x3D:
        return p->field_0x7C;
    case 0x3E:
        return p->field_0x7D;
    case 0x3F:
        return p->field_0x7E;
    case 0x40:
        return p->field_0x7F;
    case 0x41:
        return p->field_0x80;
    case 0x42:
        return p->field_0x70 & 1;
    default:
        return 0;
    }
}

int _LoadPropertyData(MOH_mechanicEnvMod_Struct* data, int property) {
    const BSProp_mechanicEnvMod* p = (const BSProp_mechanicEnvMod*)data;
    switch (property) {
    case 0:
        return p->field_0x36;
    case 2:
        return p->field_0x38;
    case 4:
        return p->field_0x35;
    case 5:
        return p->field_0x48;
    case 7:
        return p->field_0x50;
    case 8:
        return p->field_0x2C;
    case 9:
        return FixedToFloatBits(p->field_0x2E);
    case 10:
        return FixedToFloatBits(p->field_0x30);
    case 11:
        return FixedToFloatBits(p->field_0x32);
    case 18:
        return p->field_0x34 & 1;
    case 19:
        return p->field_0x3A;
    case 20:
        return (p->field_0x34 >> 1) & 1;
    case 21:
        return p->field_0x3C;
    case 22:
        return p->field_0x70;
    case 23:
        return p->field_0x74;
    case 24:
        return p->field_0x71;
    case 25:
        return p->field_0x78;
    default:
        return 0;
    }
}

int _LoadPropertyData(MOH_envMod_Struct* data, int property) {
    const BSProp_envMod* p = (const BSProp_envMod*)data;
    switch (property) {
    case 0:
        return p->field_0x36;
    case 2:
        return p->field_0x38;
    case 4:
        return p->field_0x35;
    case 5:
        return p->field_0x48;
    case 7:
        return p->field_0x50;
    case 8:
        return p->field_0x2C;
    case 9:
        return FixedToFloatBits(p->field_0x2E);
    case 10:
        return FixedToFloatBits(p->field_0x30);
    case 11:
        return FixedToFloatBits(p->field_0x32);
    case 18:
        return p->field_0x34 & 1;
    case 19:
        return p->field_0x3A;
    case 20:
        return (p->field_0x34 >> 1) & 1;
    case 21:
        return p->field_0x3C;
    case 22:
        return p->field_0x70;
    default:
        return 0;
    }
}

int _LoadPropertyData(MOH_pathPoint_Struct* data, int property) {
    const BSProp_pathPoint* p = (const BSProp_pathPoint*)data;
    switch (property) {
    case 0:
        return p->field_0x36;
    case 2:
        return p->field_0x38;
    case 4:
        return p->field_0x35;
    case 5:
        return p->field_0x48;
    case 7:
        return p->field_0x50;
    case 8:
        return p->field_0x2C;
    case 9:
        return FixedToFloatBits(p->field_0x2E);
    case 10:
        return FixedToFloatBits(p->field_0x30);
    case 11:
        return FixedToFloatBits(p->field_0x32);
    case 18:
        return p->field_0x34 & 1;
    case 19:
        return p->field_0x3A;
    case 20:
        return (p->field_0x34 >> 1) & 1;
    case 21:
        return p->field_0x3C;
    default:
        return 0;
    }
}

int _LoadPropertyData(MOH_wayPoint_Struct* data, int property) {
    const BSProp_wayPoint* p = (const BSProp_wayPoint*)data;
    switch (property) {
    case 0:
        return p->field_0x36;
    case 2:
        return p->field_0x38;
    case 4:
        return p->field_0x35;
    case 5:
        return p->field_0x48;
    case 7:
        return p->field_0x50;
    case 8:
        return p->field_0x2C;
    case 9:
        return FixedToFloatBits(p->field_0x2E);
    case 10:
        return FixedToFloatBits(p->field_0x30);
    case 11:
        return FixedToFloatBits(p->field_0x32);
    case 18:
        return p->field_0x34 & 1;
    case 19:
        return p->field_0x3A;
    case 20:
        return (p->field_0x34 >> 1) & 1;
    case 21:
        return p->field_0x3C;
    case 22:
        return p->field_0x70 & 1;
    case 23:
        return (p->field_0x70 >> 1) & 1;
    case 24:
        return (p->field_0x70 >> 2) & 1;
    case 25:
        return (p->field_0x70 >> 3) & 1;
    case 26:
        return (p->field_0x70 >> 4) & 1;
    case 27:
        return (p->field_0x70 >> 5) & 1;
    case 28:
        return (p->field_0x70 >> 6) & 1;
    case 29:
        return (p->field_0x70 >> 7) & 1;
    default:
        return 0;
    }
}

int _LoadPropertyData(MOH_powerup_Struct* data, int property) {
    const BSProp_powerup* p = (const BSProp_powerup*)data;
    switch (property) {
    case 0:
        return p->field_0x36;
    case 2:
        return p->field_0x38;
    case 4:
        return p->field_0x35;
    case 5:
        return p->field_0x48;
    case 7:
        return p->field_0x50;
    case 8:
        return p->field_0x2C;
    case 9:
        return FixedToFloatBits(p->field_0x2E);
    case 10:
        return FixedToFloatBits(p->field_0x30);
    case 11:
        return FixedToFloatBits(p->field_0x32);
    case 18:
        return p->field_0x34 & 1;
    case 19:
        return p->field_0x3A;
    case 20:
        return (p->field_0x34 >> 1) & 1;
    case 21:
        return p->field_0x3C;
    case 22:
        return p->field_0x70;
    default:
        return 0;
    }
}

int _LoadPropertyData(MOH_enemy_Struct* data, int property) {
    const BSProp_enemy* p = (const BSProp_enemy*)data;
    switch (property) {
    case 0x0:
        return p->field_0x36;
    case 0x2:
        return p->field_0x38;
    case 0x4:
        return p->field_0x35;
    case 0x5:
        return p->field_0x48;
    case 0x7:
        return p->field_0x50;
    case 0x8:
        return p->field_0x2C;
    case 0x9:
        return FixedToFloatBits(p->field_0x2E);
    case 0xA:
        return FixedToFloatBits(p->field_0x30);
    case 0xB:
        return FixedToFloatBits(p->field_0x32);
    case 0x12:
        return p->field_0x34 & 1;
    case 0x13:
        return p->field_0x3A;
    case 0x14:
        return (p->field_0x34 >> 1) & 1;
    case 0x15:
        return p->field_0x3C;
    case 0x16:
        return p->field_0x70;
    case 0x17:
        return p->field_0x90;
    case 0x18:
        return p->field_0x71;
    case 0x19:
        return p->field_0x94;
    case 0x1A:
        return p->field_0x72;
    case 0x1B:
        return p->field_0x98;
    case 0x1C:
        return p->field_0x9C;
    case 0x1D:
        return p->field_0xA0;
    case 0x1E:
        return p->field_0x73;
    case 0x1F:
        return p->field_0x74;
    case 0x20:
        return p->field_0x75;
    case 0x21:
        return p->field_0xA4;
    case 0x22:
        return p->field_0xA8;
    case 0x23:
        return FixedToFloatBits(p->field_0x7E);
    case 0x24:
        return FixedToFloatBits(p->field_0x80);
    case 0x25:
        return FixedToFloatBits(p->field_0x82);
    case 0x26:
        return p->field_0x76;
    case 0x27:
        return p->field_0x77;
    case 0x28:
        return p->field_0x78;
    case 0x29:
        return p->field_0x79;
    case 0x2A:
        return p->field_0x84;
    case 0x2B:
        return p->field_0x7A;
    case 0x2C:
        return p->field_0xAC;
    case 0x2D:
        return FixedToFloatBits(p->field_0x86);
    case 0x2E:
        return FixedToFloatBits(p->field_0x88);
    case 0x2F:
        return FixedToFloatBits(p->field_0x8A);
    case 0x30:
        return FixedToFloatBits(p->field_0x8C);
    case 0x31:
        return FixedToFloatBits(p->field_0x8E);
    case 0x32:
        return p->field_0x7B;
    case 0x33:
        return p->field_0x7C;
    default:
        return 0;
    }
}

int _LoadPropertyData(MOH_mechanic_Struct* data, int property) {
    const BSProp_mechanic* p = (const BSProp_mechanic*)data;
    switch (property) {
    case 0:
        return p->field_0x36;
    case 2:
        return p->field_0x38;
    case 4:
        return p->field_0x35;
    case 5:
        return p->field_0x48;
    case 7:
        return p->field_0x50;
    case 8:
        return p->field_0x2C;
    case 9:
        return FixedToFloatBits(p->field_0x2E);
    case 10:
        return FixedToFloatBits(p->field_0x30);
    case 11:
        return FixedToFloatBits(p->field_0x32);
    case 18:
        return p->field_0x34 & 1;
    case 19:
        return p->field_0x3A;
    case 20:
        return (p->field_0x34 >> 1) & 1;
    case 21:
        return p->field_0x3C;
    case 22:
        return p->field_0x70;
    case 23:
        return p->field_0x74;
    case 24:
        return p->field_0x71;
    default:
        return 0;
    }
}

int _LoadPropertyData(MOH_core_Struct* data, int property) {
    const BSProp_core* p = (const BSProp_core*)data;
    switch (property) {
    case 0:
        return p->field_0x36;
    case 2:
        return p->field_0x38;
    case 4:
        return p->field_0x35;
    case 5:
        return p->field_0x48;
    case 7:
        return p->field_0x50;
    case 8:
        return p->field_0x2C;
    case 9:
        return FixedToFloatBits(p->field_0x2E);
    case 10:
        return FixedToFloatBits(p->field_0x30);
    case 11:
        return FixedToFloatBits(p->field_0x32);
    case 18:
        return p->field_0x34 & 1;
    case 19:
        return p->field_0x3A;
    case 20:
        return (p->field_0x34 >> 1) & 1;
    case 21:
        return p->field_0x3C;
    default:
        return 0;
    }
}

