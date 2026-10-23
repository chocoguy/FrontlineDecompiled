#include "ea/snd/aems.h"
#include <string.h>

SNDAEMSSTATE sndaems;

void AEMSI_updateinparam(AEMSCOMPDYNAMIC* d, MODULE* module);
void AEMSI_updatetrigger(AEMSCOMPDYNAMIC* d, MODULE* module);
void AEMSI_updatestatic(AEMSCOMPDYNAMIC* d, MODULE* module);
void AEMSI_updateoscillator(AEMSCOMPDYNAMIC* d, MODULE* module);
void SNDAEMSI_updateplayer(AEMSCOMPDYNAMIC* d, MODULE* module);
void AEMSI_updaterangetrig(AEMSCOMPDYNAMIC* d, MODULE* module);
void AEMSI_updateenvelope(AEMSCOMPDYNAMIC* d, MODULE* module);
void AEMSI_updatecounter(AEMSCOMPDYNAMIC* d, MODULE* module);
void AEMSI_updaterandom(AEMSCOMPDYNAMIC* d, MODULE* module);
void AEMSI_updatemux(AEMSCOMPDYNAMIC* d, MODULE* module);
void AEMSI_updatedemux(AEMSCOMPDYNAMIC* d, MODULE* module);
void AEMSI_updateadd(AEMSCOMPDYNAMIC* d, MODULE* module);
void AEMSI_updatesubtract(AEMSCOMPDYNAMIC* d, MODULE* module);
void AEMSI_updatescale(AEMSCOMPDYNAMIC* d, MODULE* module);
void AEMSI_updatemultiply(AEMSCOMPDYNAMIC* d, MODULE* module);
void AEMSI_updatedivide(AEMSCOMPDYNAMIC* d, MODULE* module);
void AEMSI_updatemodulo(AEMSCOMPDYNAMIC* d, MODULE* module);
void AEMSI_updatetable(AEMSCOMPDYNAMIC* d, MODULE* module);
void AEMSI_updatetable2(AEMSCOMPDYNAMIC* d, MODULE* module);
void AEMSI_updatedelaytrig(AEMSCOMPDYNAMIC* d, MODULE* module);
void AEMSI_updatedelayline(AEMSCOMPDYNAMIC* d, MODULE* module);
void AEMSI_updatewarp(AEMSCOMPDYNAMIC* d, MODULE* module);
void AEMSI_updatemax(AEMSCOMPDYNAMIC* d, MODULE* module);
void AEMSI_updatemin(AEMSCOMPDYNAMIC* d, MODULE* module);
void AEMSI_updatestategen(AEMSCOMPDYNAMIC* d, MODULE* module);
void SNDAEMSI_updatedestroy(AEMSCOMPDYNAMIC* d, MODULE* module);
void SNDAEMSI_updatesend(AEMSCOMPDYNAMIC* d, MODULE* module);
void SNDAEMSI_updateramp(AEMSCOMPDYNAMIC* d, MODULE* module);
void SNDAEMSI_updatemerge(AEMSCOMPDYNAMIC* d, MODULE* module);

AEMSCOMPINFO aemscompinfo[33] __attribute__((aligned(32))) = {
    {NULL, 0, 0, 0},
    {AEMSI_updateinparam, 0x0C, 0x18, 1},
    {AEMSI_updatetrigger, 0x0C, 0x18, 1},
    {AEMSI_updatetrigger, 0x0C, 0x18, 1},
    {SNDAEMSI_updateplayer, 0x14, 0x44, 1},
    {AEMSI_updaterangetrig, 0x1C, 0x1C, 1},
    {AEMSI_updateenvelope, 0x18, 0x20, 1},
    {AEMSI_updatecounter, 0x18, 0x18, 1},
    {AEMSI_updaterandom, 0x1C, 0x20, 1},
    {AEMSI_updatemux, 0x10, 0x14, 1},
    {AEMSI_updatedemux, 0x10, 0x14, 2},
    {AEMSI_updateadd, 0x14, 0x14, 1},
    {AEMSI_updatesubtract, 0x10, 0x14, 1},
    {AEMSI_updatescale, 0x14, 0x14, 1},
    {AEMSI_updatemultiply, 0x10, 0x14, 1},
    {AEMSI_updatedivide, 0x0C, 0x14, 1},
    {AEMSI_updatemodulo, 0x0C, 0x14, 1},
    {AEMSI_updatetable, 0x18, 0x14, 1},
    {AEMSI_updatedelaytrig, 0x10, 0x1C, 1},
    {AEMSI_updatedelayline, 0x10, 0x20, 1},
    {AEMSI_updatestatic, 0x10, 0x10, 0},
    {AEMSI_updateoscillator, 0x10, 0x1C, 1},
    {AEMSI_updatewarp, 0x0C, 0x14, 1},
    {AEMSI_updatemax, 0x10, 0x14, 1},
    {AEMSI_updatemin, 0x10, 0x14, 1},
    {AEMSI_updatestategen, 0x14, 0x18, 1},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {SNDAEMSI_updatedestroy, 0x0C, 0x18, 1},
    {SNDAEMSI_updatesend, 0x14, 0x1C, 1},
    {AEMSI_updatetable2, 0x20, 0x14, 1},
    {SNDAEMSI_updateramp, 0x0C, 0x3C, 1},
    {SNDAEMSI_updatemerge, 0x0C, 0x14, 1},
};

int (*SNDAEMS_addsystem)(void* system) = SNDAEMSI_addsystem;
int (*SNDAEMS_removesystem)(void) = SNDAEMSI_removesystem;
int (*SNDAEMS_removemodulebank)(int bank) = SNDAEMSI_removemodulebank;
int (*SNDAEMS_beginevent)(void* event) = SNDAEMSI_beginevent;
int (*SNDAEMS_updateevent)(int handle, void* event) = SNDAEMSI_updateevent;
int (*SNDAEMS_endevent)(int handle, void* event) = SNDAEMSI_endevent;
int (*SNDAEMS_setglobal)(int global, int value) = SNDAEMSI_setglobal;

static inline AEMSCOMP* AEMS_nextcomp(AEMSCOMP* comp) {
    return (AEMSCOMP*)((u8*)comp + aemscompinfo[comp->type].staticSize);
}

static inline AEMSCOMPDYNAMIC* AEMS_nextdyn(AEMSCOMPDYNAMIC* d) {
    return (AEMSCOMPDYNAMIC*)((u8*)d + aemscompinfo[d->comp->type].dynamicSize);
}

static inline AEMSCOMPDYNAMIC* AEMS_compdyn(MODULE* module, MODULEINSTANCE* inst, int comp) {
    return (AEMSCOMPDYNAMIC*)(inst->dyn + module->rt->compOffsets[comp]);
}

// The bank slot holding a bank id (16 if none).
static inline int AEMS_findbank(u16 bankId) {
    int b;
    for (b = 0; b < 16; b++) {
        MODULEBANK* bank = sndaems.banks[b];
        if (bank != NULL && bankId == bank->bankId) {
            break;
        }
    }
    return b;
}

// Where an output's value lives (see AEMSCOMPINFO::outputKind).
static inline s32* AEMS_outputsrc(AEMSCOMPDYNAMIC* d, int output) {
    s32* src = NULL;
    u16 kind = aemscompinfo[d->comp->type].outputKind;
    if (kind == 1) {
        src = &d->out;
        if (output > 0) {
            src += output;
        }
    } else if (kind == 0) {
        src = &d->comp->p[0];
    } else if (kind == 2) {
        src = ADDR_TO_PTR(s32, d->out) + output;
    }
    return src;
}

// field += base: a file offset (a PTR32 field natively) becomes a pointer.
#ifdef FRONTLINE_NATIVE
#define AEMS_RELOCATE(field, T, base) NativeRelocate(field, PTR_TO_ADDR(base))
#else
#define AEMS_RELOCATE(field, T, base) field = (T*)((u8*)field + (u32)(base))
#endif

void SNDAEMSI_restore() {
    SNDAEMS_removesystem();
}

// Install a system file: relocate it, start the 100 Hz update.
int SNDAEMSI_addsystem(void* data) {
    AEMSSYSTEM* sys = (AEMSSYSTEM*)data;
    int i;

#ifdef FRONTLINE_NATIVE
    AEMSSystemFromBigEndian(sys);
#endif
    SNDAEMS_linkbank();
    SNDSYS_entercritical();
    sndaems.system = sys;
    sndgs.restoreHook0 = SNDAEMSI_restore;
    sndaems.counter = 0;
    sndaems.tickMs = 10;
    SNDSYS_add100hzclient(AEMSI_timerupdate);
    sndaems.system = sys;
    sndaems.numSendClients = 0;

    sys->params = (AEMSPARAM*)((u8*)sys + (u32)PTR_TO_ADDR(sys->params));
    sndaems.system->trigs = (AEMSTRIG*)((u8*)sys + (u32)PTR_TO_ADDR(sndaems.system->trigs));
    sndaems.system->events = (AEMSEVENT*)((u8*)sys + (u32)PTR_TO_ADDR(sndaems.system->events));
    sndaems.system->globals = (AEMSLINKLIST*)((u8*)sys + (u32)PTR_TO_ADDR(sndaems.system->globals));
    for (i = 0; i < sndaems.system->numEvents; i++) {
        AEMSEVENT* ev = &sndaems.system->events[i];
        int j;
        ev->params = (AEMSLINKLIST*)((u8*)sys + (u32)PTR_TO_ADDR(ev->params));
        for (j = 0; j < ev->numParams; j++) {
            ev->params[j].indices = (s32*)((u8*)sys + (u32)PTR_TO_ADDR(ev->params[j].indices));
        }
        ev->begin.indices = (s32*)((u8*)sys + (u32)PTR_TO_ADDR(ev->begin.indices));
        ev->update.indices = (s32*)((u8*)sys + (u32)PTR_TO_ADDR(ev->update.indices));
        ev->end.indices = (s32*)((u8*)sys + (u32)PTR_TO_ADDR(ev->end.indices));
    }
    for (i = 0; i < sndaems.system->numGlobals; i++) {
        sndaems.system->globals[i].indices = (s32*)((u8*)sys + (u32)PTR_TO_ADDR(sndaems.system->globals[i].indices));
    }
    if (sndaems.system->numGlobals > 0) {
        sndaems.globals = (s32*)SNDMEMI_allocz(sndaems.system->numGlobals << 2);
        memset(sndaems.globals, 0, sndaems.system->numGlobals << 2);
    }
    sndaems.banks = (PTR32(MODULEBANK)*)SNDMEMI_allocz(0x40);
    memset(sndaems.banks, 0, 0x40);
    SNDSYS_leavecritical();
    return 0;
}

int SNDAEMSI_removesystem() {
    SNDSYS_entercritical();
    sndaems.numSendClients = 0;
    if (sndaems.system != NULL) {
        int i;
        SNDSYS_remove100hzclient(AEMSI_timerupdate);
        for (i = 0; i < 16; i++) {
            if (sndaems.banks[i] != NULL) {
                SNDAEMS_removemodulebank(i);
            }
        }
        SNDMEMI_free(sndaems.banks);
        sndaems.banks = NULL;
        if (sndaems.system->numGlobals > 0) {
            SNDMEMI_free(sndaems.globals);
        }
        sndaems.system = NULL;
        sndaems.globals = NULL;
    }
    SNDSYS_leavecritical();
    return 0;
}

// Relocate a module bank, give its modules runtime state, and resolve the system's parameter
// and trigger links that point into it.
void SNDAEMSI_resolvemodulebank(MODULEBANK* bank, char* streamFile, int streamOffset) {
    int i;
    int m;

#ifdef FRONTLINE_NATIVE
    AEMSBankHeaderFromBigEndian(bank);
    AEMSBankFromBigEndian(bank);
#endif
    SNDSYS_entercritical();
    bank->rt = (MODULEBANKRT*)SNDMEMI_allocz(0x10);
    memset(bank->rt, 0, 0x10);
    if (bank->entries != NULL) {
        AEMS_RELOCATE(bank->entries, AEMSBANKENTRY, bank);
        for (i = 0; i < bank->numEntries; i++) {
            AEMS_RELOCATE(bank->entries[i].data, u8, bank);
            bank->entries[i].unk_0x0 = 0;
        }
    }
    if (bank->version >= 5 && bank->streams != NULL) {
        AEMS_RELOCATE(bank->streams, AEMSSTREAMENTRY, bank);
    }
    AEMS_RELOCATE(bank->modules, MODULE, bank);

    for (m = 0; m < bank->numModules; m++) {
        MODULE* module = AEMS_module(bank, m);
        AEMSCOMP* comp;
        int offset;
        int c;

        module->rt = (MODULERT*)SNDMEMI_allocz(0xC);
        module->rt->count = 0;
        module->rt->instances = (PTR32(MODULEINSTANCE)*)SNDMEMI_allocz(module->numInstances << 2);
        for (i = 0; i < module->numInstances; i++) {
            module->rt->instances[i] = NULL;
        }
        module->rt->compOffsets = (s32*)SNDMEMI_allocz(module->numComps << 2);
        AEMS_RELOCATE(module->comps, AEMSCOMP, bank);
        comp = module->comps;
        offset = 0;
        for (c = 0; c < module->numComps; c++) {
            AEMS_RELOCATE(comp->outputs, AEMSOUTPUT, bank);
            for (i = 0; i < comp->numOutputs; i++) {
                comp->outputs[i].targets = (AEMSTARGET*)((u8*)bank + (u32)PTR_TO_ADDR(comp->outputs[i].targets));
            }
            if (comp->type == AEMSCOMP_PLAYER) {
                AEMSPLAYERTABLE* table;
                comp->p[0] += (u32)PTR_TO_ADDR(bank);
                table = ADDR_TO_PTR(AEMSPLAYERTABLE, comp->p[0]);
                if ((u32)PTR_TO_ADDR(table->entries) < (u32)PTR_TO_ADDR(bank)) {
                    AEMS_RELOCATE(table->entries, u32, bank);
                }
                comp->p[1] = (s32)PTR_TO_ADDR(bank);
            } else if (comp->type == AEMSCOMP_ENVELOPE) {
                comp->p[0] += (u32)PTR_TO_ADDR(bank);
            } else if (comp->type == AEMSCOMP_RANDOM && comp->p[2] != 0) {
                comp->p[2] += (u32)PTR_TO_ADDR(bank);
            } else if (comp->type == AEMSCOMP_TABLE) {
                comp->p[0] += (u32)PTR_TO_ADDR(bank);
            } else if (comp->type == AEMSCOMP_TABLE2) {
                comp->p[0] += (u32)PTR_TO_ADDR(bank);
            } else if (comp->type == AEMSCOMP_STATEGEN) {
                comp->p[1] += (u32)PTR_TO_ADDR(bank);
            }
            module->rt->compOffsets[c] = offset;
            offset += aemscompinfo[comp->type].dynamicSize;
            comp = AEMS_nextcomp(comp);
        }
    }

    for (i = 0; i < sndaems.system->numParams; i++) {
        AEMSPARAM* p = &sndaems.system->params[i];
        if (p->bankId == bank->bankId) {
            for (m = 0; m < bank->numModules; m++) {
                MODULE* module = AEMS_module(bank, m);
                if (module->id == p->moduleId) {
                    AEMSCOMP* comp = module->comps;
                    int c;
                    for (c = 0; c < module->numComps; c++) {
                        if (comp->hash == p->compHash) {
                            p->resolved = p->bankId;
                            p->moduleIndex = m;
                            p->compIndex = c;
                            m = bank->numModules;
                            break;
                        }
                        comp = AEMS_nextcomp(comp);
                    }
                }
            }
        }
    }
    for (i = 0; i < sndaems.system->numTrigs; i++) {
        AEMSTRIG* t = &sndaems.system->trigs[i];
        if (t->bankId == bank->bankId) {
            for (m = 0; m < bank->numModules; m++) {
                MODULE* module = AEMS_module(bank, m);
                if (module->id == t->moduleId) {
                    AEMSCOMP* comp = module->comps;
                    int c;
                    for (c = 0; c < module->numComps; c++) {
                        if (comp->hash == t->compHash) {
                            t->resolved = t->bankId;
                            t->moduleIndex = m;
                            t->compIndex = c;
                            m = bank->numModules;
                            break;
                        }
                        comp = AEMS_nextcomp(comp);
                    }
                }
            }
        }
    }

    if (streamFile != NULL && streamFile[0] != '\0') {
        bank->rt->streamFile = (char*)SNDMEMI_allocz(strlen(streamFile) + 1);
        strcpy(bank->rt->streamFile, streamFile);
    } else {
        bank->rt->streamFile = NULL;
    }
    bank->rt->streamOffset = streamOffset;
    SNDSYS_leavecritical();
}

int SNDAEMSI_removemodulebank(int b) {
    MODULEBANK* bank;
    int i;
    int m;

    SNDSYS_entercritical();
    bank = sndaems.banks[b];
    if (bank->rt->streamFile != NULL) {
        SNDMEMI_free(bank->rt->streamFile);
    }
    for (i = 0; i < sndaems.system->numParams; i++) {
        AEMSPARAM* p = &sndaems.system->params[i];
        if (bank->bankId == p->bankId) {
            p->resolved = 0xFFFF;
        }
    }
    for (i = 0; i < sndaems.system->numTrigs; i++) {
        AEMSTRIG* t = &sndaems.system->trigs[i];
        if (bank->bankId == t->bankId) {
            t->resolved = 0xFFFF;
        }
    }
    for (m = 0; m < bank->numModules; m++) {
        MODULE* module = AEMS_module(bank, m);
        for (i = 0; i < module->numInstances; i++) {
            MODULEINSTANCE* inst = module->rt->instances[i];
            if (inst != NULL) {
                SNDAEMSI_destroyinstance(module, inst->handle);
            }
        }
        SNDMEMI_free(module->rt->compOffsets);
        SNDMEMI_free(module->rt->instances);
        SNDMEMI_free(module->rt);
    }
    if (bank->rt->sfxBank >= 0) {
        SNDbankremove(bank->rt->sfxBank);
    }
    if (bank->rt->midiBank >= 0) {
        SNDbankremove(bank->rt->midiBank);
    }
    SNDMEMI_free(bank->rt);
    sndaems.banks[b] = NULL;
    SNDSYS_leavecritical();
    return 0;
}

// event: [event number, parameters...]. Returns the new handle, or an error.
int SNDAEMSI_beginevent(void* data) {
    s32* event = (s32*)data;
    int num = event[0];
    s32* params = event + 1;
    int handle = sndaems.counter;
    int result = -8;
    int trig;
    int i;

    sndaems.counter = handle + 1;
    if (sndaems.counter < 0) {
        sndaems.counter = 0;
    }
    trig = SNDAEMSI_stufftrigger(num, 0, handle);
    for (i = 0; i < sndaems.system->events[num].numParams; i++) {
        if (SNDAEMSI_stuffparam(*params, num, i, handle) == 0) {
            result = 0;
        }
        params++;
    }
    if (trig >= 0) {
        for (i = 0; i < sndaems.system->numGlobals; i++) {
            SNDAEMSI_setglobal(i, sndaems.globals[i]);
        }
    } else if (result < 0) {
        handle = trig;
    }
    SNDAEMSI_updatemodules(1);
    return handle;
}

int SNDAEMSI_updateevent(int handle, void* data) {
    s32* event = (s32*)data;
    int num = event[0];
    s32* params = event + 1;
    int result = -8;
    int i;

    for (i = 0; i < sndaems.system->events[num].numParams; i++) {
        if (SNDAEMSI_stuffparam(*params, num, i, handle) >= 0) {
            result = 0;
        }
        params++;
    }
    if (SNDAEMSI_stufftrigger(num, 1, handle) >= 0) {
        result = 0;
    }
    SNDAEMSI_updatemodules(1);
    return result;
}

int SNDAEMSI_endevent(int handle, void* data) {
    s32* event = (s32*)data;
    int num = event[0];
    s32* params = event + 1;
    int i;

    for (i = 0; i < sndaems.system->events[num].numParams; i++) {
        SNDAEMSI_stuffparam(*params, num, i, handle);
        params++;
    }
    SNDAEMSI_stufftrigger(num, 2, handle);
    SNDAEMSI_updatemodules(1);
    return 0;
}

static inline int AEMS_clamp(AEMSPARAM* p, int value) {
    if (value > p->max) {
        value = p->max;
    } else if (value < p->min) {
        value = p->min;
    }
    return value;
}

// Set a global and push it (clamped by each parameter in turn) into every instance.
int SNDAEMSI_setglobal(int global, int value) {
    AEMSLINKLIST* list;
    int i;

    if (global >= sndaems.system->numGlobals) {
        return -8;
    }
    list = &sndaems.system->globals[global];
    for (i = 0; i < list->count; i++) {
        AEMSPARAM* p = &sndaems.system->params[list->indices[i]];
        int b;
        value = AEMS_clamp(p, value);
        b = AEMS_findbank(p->resolved);
        if (b < 16) {
            MODULE* module = AEMS_module(sndaems.banks[b], p->moduleIndex);
            int j;
            for (j = 0; j < module->numInstances; j++) {
                MODULEINSTANCE* inst = module->rt->instances[j];
                if (inst != NULL) {
                    AEMS_compdyn(module, inst, p->compIndex)->out = value;
                    inst->dirty = 1;
                }
            }
        }
    }
    sndaems.globals[global] = value;
    return 0;
}

// Write an event parameter into the instances it drives (only the event's own instance for
// modules owned by the event).
int SNDAEMSI_stuffparam(int value, int event, int param, int handle) {
    AEMSLINKLIST* list = &sndaems.system->events[event].params[param];
    int result = -8;
    int i;

    for (i = 0; i < list->count; i++) {
        AEMSPARAM* p = &sndaems.system->params[list->indices[i]];
        int b;
        value = AEMS_clamp(p, value);
        b = AEMS_findbank(p->resolved);
        if (b < 16) {
            MODULE* module = AEMS_module(sndaems.banks[b], p->moduleIndex);
            int j;
            if (module->event == event) {
                for (j = 0; j < module->numInstances; j++) {
                    MODULEINSTANCE* inst = module->rt->instances[j];
                    if (inst != NULL && inst->handle == handle) {
                        result = 0;
                        AEMS_compdyn(module, inst, p->compIndex)->out = value;
                        inst->dirty = 1;
                    }
                }
            } else {
                for (j = 0; j < module->numInstances; j++) {
                    MODULEINSTANCE* inst = module->rt->instances[j];
                    if (inst != NULL) {
                        result = 0;
                        AEMS_compdyn(module, inst, p->compIndex)->out = value;
                        inst->dirty = 1;
                    }
                }
            }
        }
    }
    return result;
}

// Fire an event's begin (0), update (1) or end (2) triggers; begin creates instances.
int SNDAEMSI_stufftrigger(int event, int type, int handle) {
    AEMSEVENT* ev = &sndaems.system->events[event];
    AEMSLINKLIST* list;
    int result = -8;
    int created = 0;
    int i;

    if (type == 0) {
        list = &ev->begin;
    } else if (type == 1) {
        list = &ev->update;
    } else {
        list = &ev->end;
    }
    if (list == NULL) {
        return -8;
    }
    for (i = 0; i < list->count; i++) {
        AEMSTRIG* t = &sndaems.system->trigs[list->indices[i]];
        int b = AEMS_findbank(t->resolved);
        MODULEBANK* bank;
        MODULE* module;
        int j;

        if (b >= 16) {
            continue;
        }
        bank = sndaems.banks[b];
        module = AEMS_module(bank, t->moduleIndex);
        if (module->event == event) {
            if (t->compIndex == 0) {
                if (type == 0) {
                    created = SNDAEMSI_createinstance(bank, module, handle);
                    if (created >= 0) {
                        result = created;
                        for (j = 0; j < module->numInstances; j++) {
                            MODULEINSTANCE* inst = module->rt->instances[j];
                            if (inst != NULL && inst->handle == handle) {
                                AEMS_compdyn(module, inst, t->compIndex)->out = 1;
                                inst->dirty = 1;
                            }
                        }
                    }
                }
            } else {
                for (j = 0; j < module->numInstances; j++) {
                    MODULEINSTANCE* inst = module->rt->instances[j];
                    if (inst != NULL && inst->handle == handle) {
                        result = 0;
                        AEMS_compdyn(module, inst, t->compIndex)->out = 1;
                        inst->dirty = 1;
                    }
                }
            }
        } else {
            for (j = 0; j < module->numInstances; j++) {
                MODULEINSTANCE* inst = module->rt->instances[j];
                if (inst != NULL) {
                    result = 0;
                    AEMS_compdyn(module, inst, t->compIndex)->out = 1;
                    inst->dirty = 1;
                }
            }
        }
    }
    if (result < 0 && created < 0) {
        result = created;
    }
    return result;
}

void AEMSI_timerupdate() {
    SNDAEMSI_updatemodules(0);
}

void SNDAEMSI_updatemodules(int type) {
    int b;
    for (b = 0; b < 16; b++) {
        if (sndaems.banks[b] != NULL) {
            int m;
            for (m = 0; m < sndaems.banks[b]->numModules; m++) {
                SNDAEMSI_updatemodule(b, m, type);
            }
        }
    }
}

// Run each dirty instance's components in order, then copy their outputs along the links.
void SNDAEMSI_updatemodule(int b, int m, int type) {
    MODULE* module = AEMS_module(sndaems.banks[b], m);
    int i;

    if (module->updateType != type) {
        return;
    }
    for (i = 0; i < module->numInstances; i++) {
        MODULEINSTANCE* inst = module->rt->instances[i];
        int c;
        if (inst == NULL || !inst->dirty) {
            continue;
        }
        for (c = 0; c < inst->numExec; c++) {
            AEMSCOMPDYNAMIC* d = inst->exec[c];
            int k;
            aemscompinfo[d->comp->type].update(d, module);
            for (k = 0; k < d->numOutputs; k++) {
                *d->outputs[k].dst = *d->outputs[k].src;
            }
        }
        if (*inst->destroy->in > 0) {
            SNDAEMSI_destroyinstance(module, inst->handle);
        }
    }
}

void AEMSI_updateinparam(AEMSCOMPDYNAMIC* d, MODULE* module) {}

// One pass high after being set.
void AEMSI_updatetrigger(AEMSCOMPDYNAMIC* d, MODULE* module) {
    if (d->out < 1) {
        return;
    }
    if (d->s[0] == 0) {
        d->s[0] = 1;
    } else {
        d->s[0] = 0;
        d->out = 0;
    }
}

void AEMSI_updatestatic(AEMSCOMPDYNAMIC* d, MODULE* module) {}

// in[0] period (ms, read while two inputs are wired), in[1] amplitude.
// s[0] low half: phase step; s[1]: phase (16 bits).
void AEMSI_updateoscillator(AEMSCOMPDYNAMIC* d, MODULE* module) {
    AEMSCOMP* comp = d->comp;
    s16 wave;
    s16 unipolar;
    u32 phase;

    if (d->connected == 2) {
        int period = d->in[0];
        int step;
        if (period <= 0) {
            period = 1;
        }
        step = (sndaems.tickMs << 16) / period;
        if (step > 0xFFFF) {
            step = 0xFFFF;
        }
        *(u16*)&d->s[0] = step;
    }
    d->s[1] += *(u16*)&d->s[0];
    if ((u32)d->s[1] >= 0x10000) {
        d->s[1] -= 0x10000;
    }
    wave = ((s16*)comp->p)[0];
    unipolar = ((s16*)comp->p)[1];
    phase = d->s[1];
    if (wave == 0) {
        d->out = (d->in[1] * iSNDsin(phase >> 6)) >> 16;
        if (unipolar == 1) {
            d->out += d->in[1];
            d->out >>= 1;
        }
    } else if (wave == 1) {
        if (phase >= 0x8000) {
            d->out = d->in[1];
        } else if (unipolar == 1) {
            d->out = 0;
        } else {
            d->out = -d->in[1];
        }
    } else if (wave == 2 && unipolar == 1) {
        d->out = (u32)(phase * d->in[1]) >> 16;
    } else if (wave == 2) {
        d->out = (u32)(phase * d->in[1]) >> 15;
        d->out -= d->in[1];
    } else if (wave == 3 && unipolar == 0) {
        if (phase < 0x8000) {
            d->out = (u32)(phase * d->in[1]) >> 14;
        } else {
            d->out = (u32)((0x10000 - phase) * d->in[1]) >> 14;
        }
        d->out -= d->in[1];
    } else if (wave == 3) {
        if (phase < 0x8000) {
            d->out = (u32)(phase * d->in[1]) >> 15;
        } else {
            d->out = (u32)((0x10000 - phase) * d->in[1]) >> 15;
        }
    }
}

// Clamp pitch and volume, pick the sound on the gate's rising edge, and hand over to the
// player for its kind.
void SNDAEMSI_updateplayer(AEMSCOMPDYNAMIC* dyn, MODULE* module) {
    AEMSCOMPDYNAMICPLAYER* d = (AEMSCOMPDYNAMICPLAYER*)dyn;
    AEMSCOMP* comp = d->comp;

    if (d->in[0] > 0xFFFF) {
        d->in[0] = 0xFFFF;
    } else if (d->in[0] < 0) {
        d->in[0] = 0;
    }
    if (d->in[2] > 0x7FFF) {
        d->in[2] = 0x7FFF;
    } else if (d->in[2] < 0) {
        d->in[2] = 0;
    }
    if (d->lastGate != d->in[7] && d->in[7] == 1 && d->lastGate == 0) {
        AEMSPLAYERTABLE* table = ADDR_TO_PTR(AEMSPLAYERTABLE, comp->p[0]);
        int index = d->in[6];
        if (index >= table->count) {
            index = table->count - 1;
        }
        d->entry = table->entries[index];
    }
    if (d->entry >= 0xC000) {
        sndaems.updatePlayerStream(d);
    } else if (d->entry >= 0x8000) {
        sndaems.updatePlayer8000(d);
    } else if (d->entry >= 0x4000) {
        sndaems.updatePlayerBank(d);
    }
}

// High for one pass when in[0] enters [p0, p1], re-armed when it enters [p2, p3].
void AEMSI_updaterangetrig(AEMSCOMPDYNAMIC* d, MODULE* module) {
    AEMSCOMP* comp = d->comp;
    s32 v = d->in[0];

    if (v >= comp->p[0] && v <= comp->p[1] && d->s[1] == 0) {
        d->s[1] = 1;
        d->s[0] = 1;
        d->out = 1;
        return;
    }
    if (d->s[0] > 0) {
        d->s[0]--;
        return;
    }
    if (v >= comp->p[2] && v <= comp->p[3] && d->s[1] == 1) {
        d->s[1] = 0;
        return;
    }
    d->out = 0;
}

// Breakpoint envelope. in[0]: 1 run, 3 release (jump to the sustain point), 2 hold.
// s[0] time in the segment, s[1] low half: segment, high half: last gate; s[2] segment start.
void AEMSI_updateenvelope(AEMSCOMPDYNAMIC* d, MODULE* module) {
    AEMSCOMP* comp = d->comp;
    SNDENVSEG* points = ADDR_TO_PTR(SNDENVSEG, comp->p[0]);
    s16 numPoints = ((s16*)comp->p)[2];
    s16 sustain = ((s16*)comp->p)[3];
    s16* seg = &((s16*)&d->s[1])[0];
    s16* lastGate = &((s16*)&d->s[1])[1];
    int gate = d->in[0];

    if (gate == 1 && *lastGate == 0) {
        d->s[0] = 0;
        *seg = 0;
        d->out = comp->p[2];
        d->s[2] = comp->p[2];
    } else if (gate == 3 && *lastGate != 3 && sustain - 1 > *seg) {
        d->s[0] = 0;
        *seg = sustain - 1;
        d->s[2] = d->out = points[*seg - 1].level;
    } else if ((gate == 3 || gate == 1) && *seg < numPoints) {
        SNDENVSEG* point;
        d->s[0] += sndaems.tickMs;
        point = &points[*seg];
        if (d->s[0] >= point->time) {
            d->s[0] = 0;
            d->s[2] = points[*seg].level;
            (*seg)++;
            if (*seg >= numPoints) {
                d->out = 0;
            } else {
                d->out = d->s[2];
            }
        } else {
            s64 frac = ((s64)d->s[0] << 32) / point->time;
            d->out = (s32)((frac * (point->level - d->s[2])) >> 32);
            d->out += d->s[2];
        }
    } else if (gate != 2) {
        d->out = 0;
    }
    *lastGate = d->in[0];
}

// Counts between p0 and p1 (up if p2's byte is set) on each trigger; in[1] in range loads it.
void AEMSI_updatecounter(AEMSCOMPDYNAMIC* d, MODULE* module) {
    AEMSCOMP* comp = d->comp;
    s32 load = d->in[1];

    if (load >= comp->p[0] && load <= comp->p[1]) {
        d->out = load;
    } else if (d->in[0] > 0 && d->s[0] == 0) {
        if (*(u8*)&comp->p[2] != 0) {
            d->out++;
            if (d->out > comp->p[1]) {
                d->out = comp->p[0];
            }
        } else {
            d->out--;
            if (d->out < comp->p[0]) {
                d->out = comp->p[1];
            }
        }
    }
    d->s[0] = d->in[0];
}

// Random value in [p0, p1] on each trigger. Mode (p3's byte) 0: weighted by the byte table at
// p2, 1: uniform, 2: shuffled (no value twice per round; s[0] the counts, s[1] low half the round).
void AEMSI_updaterandom(AEMSCOMPDYNAMIC* d, MODULE* module) {
    AEMSCOMP* comp = d->comp;
    int found = 0;
    u8 mode;

    if (d->in[0] > 0 && d->s[2] == 0) {
        mode = *(u8*)&comp->p[3];
        if (mode == 2) {
            u16* counts = ADDR_TO_PTR(u16, d->s[0]);
            s16* round = (s16*)&d->s[1];
            u32 range = comp->p[1] + 1 - comp->p[0];
            int start = iSNDrandom() % range;
            while (!found) {
                int i;
                for (i = start; i <= comp->p[1] - comp->p[0] && !found; i++) {
                    if (*round != counts[i]) {
                        counts[i]++;
                        found = 1;
                        d->out = i + comp->p[0];
                    }
                }
                for (i = 0; i < start && !found; i++) {
                    if (*round != counts[i]) {
                        counts[i]++;
                        found = 1;
                        d->out = i + comp->p[0];
                    }
                }
                if (!found) {
                    if (d->out == start + comp->p[0]) {
                        start++;
                    }
                    if (start > comp->p[1]) {
                        start = comp->p[0];
                    }
                    (*round)++;
                }
            }
        } else if (mode == 0) {
            u32 r = iSNDrandom() % 100;
            int sum = 0;
            int n = comp->p[1] - comp->p[0];
            int i;
            if (n >= 0) {
                for (i = 0; i < n + 1; i++) {
                    sum += (ADDR_TO_PTR(u8, comp->p[2]))[i];
                    if (sum > (int)r) {
                        d->out = i + comp->p[0];
                        break;
                    }
                }
            }
        } else if (mode == 1) {
            int min = comp->p[0];
            d->out = min + iSNDrandom() % (u32)(comp->p[1] + 1 - min);
        }
    }
    d->s[2] = d->in[0];
}

void AEMSI_updatemux(AEMSCOMPDYNAMIC* d, MODULE* module) {
    int select = d->in[0];
    if (select > 0) {
        d->out = d->in[select];
    } else {
        d->out = 0;
    }
}

// Routes in[1] to output in[0]-1, zero to the others.
void AEMSI_updatedemux(AEMSCOMPDYNAMIC* d, MODULE* module) {
    AEMSCOMP* comp = d->comp;
    int i;
    for (i = 0; i < comp->p[0]; i++) {
        if (i == d->in[0] - 1) {
            (ADDR_TO_PTR(s32, d->out))[i] = d->in[1];
        } else {
            (ADDR_TO_PTR(s32, d->out))[i] = 0;
        }
    }
}

void AEMSI_updateadd(AEMSCOMPDYNAMIC* d, MODULE* module) {
    AEMSCOMP* comp = d->comp;
    int sum = 0;
    int i;
    for (i = 0; i < comp->p[0]; i++) {
        sum += d->in[i];
    }
    if (sum > comp->p[1]) {
        d->out = comp->p[1];
    } else {
        d->out = sum;
    }
}

void AEMSI_updatesubtract(AEMSCOMPDYNAMIC* d, MODULE* module) {
    int v = d->in[0] - d->in[1];
    if (v < d->comp->p[0]) {
        d->out = d->comp->p[0];
    } else {
        d->out = v;
    }
}

// in[0] * in[1] / p0 * in[2] / p0 ... (64-bit products).
void AEMSI_updatescale(AEMSCOMPDYNAMIC* d, MODULE* module) {
    AEMSCOMP* comp = d->comp;
    int i;
    d->out = d->in[0];
    for (i = 1; i < comp->p[1]; i++) {
        d->out = (s32)(((s64)d->out * d->in[i]) / comp->p[0]);
    }
}

void AEMSI_updatemultiply(AEMSCOMPDYNAMIC* d, MODULE* module) {
    int v = d->in[0] * d->in[1];
    if (v > d->comp->p[0]) {
        d->out = d->comp->p[0];
    } else {
        d->out = v;
    }
}

void AEMSI_updatedivide(AEMSCOMPDYNAMIC* d, MODULE* module) {
    if (d->in[1] > 0) {
        d->out = d->in[0] / d->in[1];
    } else {
        d->out = 0;
    }
}

void AEMSI_updatemodulo(AEMSCOMPDYNAMIC* d, MODULE* module) {
    d->out = d->in[0] % d->in[1];
}

// Lookup in an s16 table spread over [min, max].
void AEMSI_updatetable(AEMSCOMPDYNAMIC* d, MODULE* module) {
    AEMSCOMP* comp = d->comp;
    s16 size = ((s16*)comp->p)[2];
    s16 min = ((s16*)comp->p)[3];
    s16 max = ((s16*)comp->p)[4];

    d->out = d->in[0];
    if (d->out < min) {
        d->out = min;
    } else if (d->out > max) {
        d->out = max;
    }
    d->out = (size - 1) * (d->out - min) / (max - min);
    d->out = (ADDR_TO_PTR(s16, comp->p[0]))[d->out];
}

// Lookup in a table of p4-byte entries (4, 2 or 1) spread over [p2, p3].
void AEMSI_updatetable2(AEMSCOMPDYNAMIC* d, MODULE* module) {
    AEMSCOMP* comp = d->comp;

    d->out = d->in[0];
    if (d->out < comp->p[2]) {
        d->out = comp->p[2];
    } else if (d->out > comp->p[3]) {
        d->out = comp->p[3];
    }
    d->out = comp->p[1] * (d->out - comp->p[2]) / (comp->p[3] - comp->p[2] + 1);
    if (comp->p[4] == 4) {
        d->out = (ADDR_TO_PTR(s32, comp->p[0]))[d->out];
    } else if (comp->p[4] == 2) {
        d->out = (ADDR_TO_PTR(s16, comp->p[0]))[d->out];
    } else {
        d->out = (ADDR_TO_PTR(s8, comp->p[0]))[d->out];
    }
}

// Fires once in[1] ms (at most p0) after a trigger. s[0] timer (-1 idle), s[1] last trigger.
void AEMSI_updatedelaytrig(AEMSCOMPDYNAMIC* d, MODULE* module) {
    AEMSCOMP* comp = d->comp;
    int t;
    int delay;

    if (d->in[0] > 0 && d->s[1] == 0) {
        d->s[0] = 0;
    }
    t = d->s[0];
    delay = d->in[1];
    if ((t >= delay || t > comp->p[0]) && d->out == 0) {
        d->out = 1;
    } else if (t > delay || t > comp->p[0]) {
        d->out = 0;
        d->s[0] = -1;
    } else {
        d->out = 0;
    }
    d->s[1] = d->in[0];
    if (d->s[0] >= 0) {
        d->s[0] += sndaems.tickMs;
    }
}

// in[0] delayed by in[1] ms (up to p0) through a ring of s16 samples.
// s[0] the ring, s[1] write position, s[2] read position.
void AEMSI_updatedelayline(AEMSCOMPDYNAMIC* d, MODULE* module) {
    AEMSCOMP* comp = d->comp;
    s16* ring = ADDR_TO_PTR(s16, d->s[0]);
    s16* write;
    s16* read;
    int n;
    int delay;

    write = ADDR_TO_PTR(s16, d->s[1]) + 1;
    d->s[1] = (s32)PTR_TO_ADDR(write);
    if ((u32)PTR_TO_ADDR(write) >= (u32)PTR_TO_ADDR((ring + comp->p[0] / sndaems.tickMs))) {
        d->s[1] = (s32)PTR_TO_ADDR(ring);
    }
    write = ADDR_TO_PTR(s16, d->s[1]);
    *write = d->in[0];
    n = comp->p[0] / sndaems.tickMs;
    delay = d->in[1] / sndaems.tickMs;
    if (delay > n - 1) {
        read = write - n + 1;
    } else {
        read = write - delay;
    }
    if ((u32)PTR_TO_ADDR(read) < (u32)PTR_TO_ADDR(ring)) {
        read += comp->p[0] / sndaems.tickMs;
    }
    d->s[2] = (s32)PTR_TO_ADDR(read);
    d->out = *read;
}

void AEMSI_updatewarp(AEMSCOMPDYNAMIC* d, MODULE* module) {
    d->out = d->in[0];
}

void AEMSI_updatemax(AEMSCOMPDYNAMIC* d, MODULE* module) {
    int max = 0;
    int i;
    for (i = 0; i < d->comp->p[0]; i++) {
        if (d->in[i] > max) {
            max = d->in[i];
        }
    }
    d->out = max;
}

void AEMSI_updatemin(AEMSCOMPDYNAMIC* d, MODULE* module) {
    int min = 0x7FFFFFFF;
    int i;
    for (i = 0; i < d->comp->p[0]; i++) {
        if (d->in[i] < min) {
            min = d->in[i];
        }
    }
    d->out = min;
}

// Outputs value i of the table at p1 when input i rises. s[0] the last inputs (s16).
void AEMSI_updatestategen(AEMSCOMPDYNAMIC* d, MODULE* module) {
    AEMSCOMP* comp = d->comp;
    s16* last = ADDR_TO_PTR(s16, d->s[0]);
    int i;
    for (i = 0; i < comp->p[0]; i++) {
        if (last[i] <= 0 && d->in[i] > 0) {
            d->out = (ADDR_TO_PTR(s16, comp->p[1]))[i];
        }
        last[i] = d->in[i];
    }
}

// s[0]: the instance handle.
void SNDAEMSI_updatedestroy(AEMSCOMPDYNAMIC* d, MODULE* module) {
    if (d->in[0] > 0) {
        SNDAEMSI_destroyinstance(module, d->s[0]);
    }
}

// Begins event p1 with the inputs as parameters when any of them changed, and tells the send
// clients. s[0] the instance handle, s[1] the last inputs.
void SNDAEMSI_updatesend(AEMSCOMPDYNAMIC* d, MODULE* module) {
    AEMSCOMP* comp = d->comp;
    s32* last = ADDR_TO_PTR(s32, d->s[1]);
    int changed = 0;
    int i;

    for (i = 0; i < comp->p[0]; i++) {
        if (last[i] != d->in[i]) {
            changed = 1;
        }
        last[i] = d->in[i];
    }
    if (changed == 1) {
        s32 event[18];
        s32* p = event;
        *p = comp->p[1];
        for (i = 0; i < comp->p[0]; i++) {
            *++p = d->in[i];
        }
        SNDAEMS_beginevent(event);
        for (i = 0; i < sndaems.numSendClients; i++) {
            sndaems.sendClients[i](d->s[0], event);
        }
    }
}

// Linear ramp to in[2] over in[0] ms, stepped in[1] times per pass. s[0] ramp time, s[1]
// start, s[2] target, s[8] step, s[9] value (s64s, 8-aligned in the block).
void SNDAEMSI_updateramp(AEMSCOMPDYNAMIC* d, MODULE* module) {
    s64* step = ADDR_TO_PTR(s64, d->s[8]);
    s64* acc = ADDR_TO_PTR(s64, d->s[9]);

    if (d->in[2] == d->out) {
        return;
    }
    if (d->in[2] != d->s[2] || d->in[0] != d->s[0]) {
        d->s[1] = d->out;
        *acc = (s64)(d->out << 4) << 32;
        d->s[2] = d->in[2];
        d->s[0] = d->in[0];
        if (d->s[0] <= 0) {
            d->out = d->s[2];
            return;
        }
        *step = ((s64)sndaems.tickMs * (d->s[2] - d->s[1])) << 24;
        *step = *step / d->s[0];
    } else {
        int rising = d->out < d->s[2];
        *acc += *step * d->in[1];
        d->out = (s32)(*acc >> 32) >> 4;
        if (rising) {
            if (d->out > d->s[2]) {
                d->out = d->s[2];
            }
        } else if (d->out < d->s[2]) {
            d->out = d->s[2];
        }
    }
}

// One pass high when any input is high (then low for a pass).
void SNDAEMSI_updatemerge(AEMSCOMPDYNAMIC* d, MODULE* module) {
    int i;
    if (d->out == 1) {
        d->out = 0;
        return;
    }
    for (i = 0; i < d->comp->numInputs; i++) {
        if (d->in[i] > 0) {
            d->out = 1;
            return;
        }
    }
}

// Extra space a component's state needs after the fixed blocks.
static inline int AEMS_extrasize(AEMSCOMP* comp, int tick) {
    if (comp->type == AEMSCOMP_DEMUX) {
        return comp->p[0] << 2;
    } else if (comp->type == AEMSCOMP_DELAYLINE) {
        return ((comp->p[0] / tick + 1) << 1) + 2;
    } else if (comp->type == AEMSCOMP_RANDOM && comp->p[2] == 0 && *(u8*)&comp->p[3] == 2) {
        return ((comp->p[1] - comp->p[0] + 1) << 1) + 2;
    } else if (comp->type == AEMSCOMP_STATEGEN) {
        return (comp->p[0] << 1) + 2;
    } else if (comp->type == AEMSCOMP_SEND) {
        return comp->p[0] << 2;
    }
    return 0;
}

// Allocate and set up an instance: header, component states, then their inputs and buffers.
int SNDAEMSI_createinstance(MODULEBANK* bank, MODULE* module, int handle) {
    MODULERT* rt = module->rt;
    MODULEINSTANCE* inst;
    AEMSCOMPDYNAMIC* d;
    AEMSCOMP* comp;
    u8* extra;
    int dynSize = 0;
    int size;
    int slot;
    int tick;
    int c;

    if (rt->count >= module->numInstances) {
        return -9;
    }
    for (slot = 0; slot < module->numInstances; slot++) {
        if (rt->instances[slot] == NULL) {
            break;
        }
    }

    size = 0x1C;
    comp = module->comps;
    tick = sndaems.tickMs;
    for (c = 0; c < module->numComps; c++) {
        AEMSCOMPINFO* info = &aemscompinfo[comp->type];
        size += info->dynamicSize;
        dynSize += info->dynamicSize;
        size += comp->numInputs << 2;
        size += AEMS_extrasize(comp, tick);
        comp = (AEMSCOMP*)((u8*)comp + info->staticSize);
    }

    rt->instances[slot] = (MODULEINSTANCE*)SNDMEMI_allocz(size);
    inst = module->rt->instances[slot];
    memset(rt->instances[slot], 0, size);
    inst->dirty = 0;
    inst->handle = handle;
    inst->dyn = (u8*)inst + 0x1C;
    extra = inst->dyn + dynSize;
    comp = module->comps;
    d = (AEMSCOMPDYNAMIC*)inst->dyn;
    for (c = 0; c < module->numComps; c++) {
        AEMSCOMPINFO* info;
        d->comp = comp;
        d->in = (s32*)extra;
        extra += comp->numInputs << 2;
        if (comp->type == AEMSCOMP_DEMUX) {
            d->out = (s32)PTR_TO_ADDR(extra);
            extra += comp->p[0] << 2;
        } else if (comp->type == AEMSCOMP_DELAYLINE) {
            d->s[0] = (s32)PTR_TO_ADDR(extra);
            extra += (comp->p[0] / sndaems.tickMs + 1) << 1;
            if ((u32)PTR_TO_ADDR(extra) & 2) {
                extra += 2;
            }
            d->s[1] = d->s[0];
            d->s[2] = d->s[0];
        } else if (comp->type == AEMSCOMP_RANDOM && comp->p[2] == 0 && *(u8*)&comp->p[3] == 2) {
            d->s[0] = (s32)PTR_TO_ADDR(extra);
            extra += (comp->p[1] - comp->p[0] + 1) << 1;
            if ((u32)PTR_TO_ADDR(extra) & 2) {
                extra += 2;
            }
        } else if (comp->type == AEMSCOMP_STATEGEN) {
            d->s[0] = (s32)PTR_TO_ADDR(extra);
            extra += comp->p[0] << 1;
            if ((u32)PTR_TO_ADDR(extra) & 2) {
                extra += 2;
            }
        } else if (comp->type == AEMSCOMP_DELAYTRIG) {
            d->s[0] = -1;
        } else if (comp->type == AEMSCOMP_DESTROY) {
            d->s[0] = handle;
        } else if (comp->type == AEMSCOMP_SEND) {
            d->s[0] = handle;
            d->s[1] = (s32)PTR_TO_ADDR(extra);
            extra += comp->p[0] << 2;
        } else if (comp->type == AEMSCOMP_RAMP) {
            d->s[8] = ((u32)PTR_TO_ADDR(d) + 0x27) & ~7;
            d->s[9] = d->s[8] + 8;
        } else if (comp->type == AEMSCOMP_COUNTER) {
            if (*(u8*)&comp->p[2] != 0) {
                d->out = comp->p[0];
            } else {
                d->out = comp->p[1];
            }
        }
        info = &aemscompinfo[comp->type];
        d = (AEMSCOMPDYNAMIC*)((u8*)d + info->dynamicSize);
        comp = (AEMSCOMP*)((u8*)comp + info->staticSize);
    }
    inst->dirty = 1;
    module->rt->count++;
    if (bank->version > 7) {
        SNDAEMSI_reordermodule2(module, inst);
    } else {
        SNDAEMSI_reordermodule(module, inst);
    }
    return 0;
}

// Free an instance, stopping what its players play.
int SNDAEMSI_destroyinstance(MODULE* module, int handle) {
    MODULERT* rt = module->rt;
    MODULEINSTANCE* inst = NULL;
    AEMSCOMPDYNAMIC* d;
    int i;
    int c;

    if (rt->count == 0) {
        return -1;
    }
    for (i = 0; i < module->numInstances; i++) {
        inst = rt->instances[i];
        if (inst != NULL && inst->handle == handle) {
            break;
        }
    }
    if (i >= module->numInstances) {
        return -13;
    }
    rt->instances[i] = NULL;
    module->rt->count--;
    d = (AEMSCOMPDYNAMIC*)inst->dyn;
    for (c = 0; c < module->numComps; c++) {
        AEMSCOMP* comp = d->comp;
        if (comp->type == AEMSCOMP_PLAYER && d->out != 0) {
            AEMSCOMPDYNAMICPLAYER* player = (AEMSCOMPDYNAMICPLAYER*)d;
            if (player->entry >= 0xC000) {
                player->in[7] = 0;
                if (player->stream > -1) {
                    SNDSTRM_purge(player->stream);
                    player->handle = 0x7FFFFFFF;
                    SNDAEMSI_streamupdatestatus(player);
                }
                player->lastGate = 0;
                sndaems.updatePlayerStream(player);
            } else if (player->entry >= 0x8000) {
                sndaems.stopPlayer8000(player);
            } else if (player->entry >= 0x4000) {
                SNDstop(player->handle);
            }
        }
        d = (AEMSCOMPDYNAMIC*)((u8*)d + aemscompinfo[comp->type].dynamicSize);
    }
    SNDMEMI_free(inst->exec);
    SNDMEMI_free(inst);
    return 0;
}

// Wire one connection: a static source copies its value once, others get a link.
static inline void AEMS_wire(MODULE* module, MODULEINSTANCE* inst, AEMSCOMPDYNAMIC* d, int output,
                             AEMSTARGET* target, AEMSLINK** link) {
    AEMSCOMPDYNAMIC* td = AEMS_compdyn(module, inst, target->comp);
    if (d->comp->type == AEMSCOMP_STATIC) {
        td->in[target->input] = *AEMS_outputsrc(d, output);
    } else {
        d->outputs[d->numOutputs].dst = &td->in[target->input];
        (*link)++;
        d->outputs[d->numOutputs].src = AEMS_outputsrc(d, output);
        d->numOutputs++;
    }
}

// Old banks: order the components so each runs after all its inputs (sources first), and
// build the links.
void SNDAEMSI_reordermodule(MODULE* module, MODULEINSTANCE* inst) {
    AEMSCOMPDYNAMIC* d;
    AEMSLINK* link;
    int total = 0;
    int done;
    int c;

    inst->numExec = 0;
    d = (AEMSCOMPDYNAMIC*)inst->dyn;
    for (c = 0; c < module->numComps; c++) {
        AEMSCOMP* comp = d->comp;
        d->numOutputs = 0;
        if (comp->type == AEMSCOMP_WARP) {
            d->connected = 1;
        } else {
            d->connected = 0;
        }
        if (comp->type == AEMSCOMP_DESTROY) {
            inst->destroy = d;
        }
        if (comp->type != AEMSCOMP_STATIC) {
            int k;
            for (k = 0; k < comp->numOutputs; k++) {
                total += comp->outputs[k].count;
            }
        }
        d->scheduled = 0;
        d = AEMS_nextdyn(d);
    }
    inst->exec = (PTR32(AEMSCOMPDYNAMIC)*)SNDMEMI_allocz((module->numComps << 2) + (total << 3));
    if (total != 0) {
        inst->links = (AEMSLINK*)((u8*)inst->exec + (module->numComps << 2));
    } else {
        inst->links = NULL;
    }

    link = inst->links;
    d = (AEMSCOMPDYNAMIC*)inst->dyn;
    for (c = 0; c < module->numComps; c++) {
        AEMSCOMP* comp = d->comp;
        if (comp->numInputs == 0) {
            int k;
            if (comp->type != AEMSCOMP_STATIC) {
                inst->exec[inst->numExec++] = d;
            }
            d->outputs = link;
            for (k = 0; k < comp->numOutputs; k++) {
                AEMSOUTPUT* out = &comp->outputs[k];
                int t;
                for (t = 0; t < out->count; t++) {
                    AEMSCOMPDYNAMIC* td = AEMS_compdyn(module, inst, out->targets[t].comp);
                    td->connected++;
                    AEMS_wire(module, inst, d, k, &out->targets[t], &link);
                }
            }
            d->scheduled = 1;
        }
        d = AEMS_nextdyn(d);
    }

    done = 0;
    while (done == 0) {
        AEMSLINK* start = link;
        done = 1;
        d = (AEMSCOMPDYNAMIC*)inst->dyn;
        for (c = 0; c < module->numComps; c++) {
            AEMSCOMP* comp = d->comp;
            if (comp->numInputs != 0 && (s8)d->connected >= comp->numInputs && (s8)d->scheduled == 0 &&
                comp->type != AEMSCOMP_DESTROY) {
                int k;
                inst->exec[inst->numExec++] = d;
                done = 0;
                d->outputs = start;
                for (k = 0; k < comp->numOutputs; k++) {
                    AEMSOUTPUT* out = &comp->outputs[k];
                    int t;
                    for (t = 0; t < out->count; t++) {
                        AEMSCOMPDYNAMIC* td = AEMS_compdyn(module, inst, out->targets[t].comp);
                        start++;
                        link++;
                        d->outputs[d->numOutputs].dst = &td->in[out->targets[t].input];
                        d->outputs[d->numOutputs].src = AEMS_outputsrc(d, k);
                        d->numOutputs++;
                        td->connected++;
                        d->scheduled = 1;
                    }
                }
                d->scheduled = 1;
            }
            d = AEMS_nextdyn(d);
        }
    }
}

// Version 8 banks are stored in update order; the last component is the destroy.
void SNDAEMSI_reordermodule2(MODULE* module, MODULEINSTANCE* inst) {
    PTR32(AEMSCOMPDYNAMIC)* exec;
    AEMSCOMPDYNAMIC* d;
    AEMSLINK* link;
    int c;

    inst->numExec = module->numExec;
    inst->exec = (PTR32(AEMSCOMPDYNAMIC)*)SNDMEMI_allocz((module->numExec << 2) + (module->numLinks << 3));
    exec = inst->exec;
    if (module->numLinks != 0) {
        inst->links = (AEMSLINK*)((u8*)inst->exec + (module->numExec << 2));
    } else {
        inst->links = NULL;
    }
    inst->destroy = (AEMSCOMPDYNAMIC*)(inst->dyn + module->rt->compOffsets[module->numComps - 1]);

    link = inst->links;
    d = (AEMSCOMPDYNAMIC*)inst->dyn;
    for (c = 0; c < module->numComps; c++) {
        AEMSCOMP* comp = d->comp;
        int k;
        d->numOutputs = 0;
        if (comp->numInputs == 0) {
            if (comp->type != AEMSCOMP_STATIC) {
                *exec++ = d;
            }
            d->outputs = link;
            for (k = 0; k < comp->numOutputs; k++) {
                AEMSOUTPUT* out = &comp->outputs[k];
                int t;
                for (t = 0; t < out->count; t++) {
                    AEMS_wire(module, inst, d, k, &out->targets[t], &link);
                }
            }
        } else {
            *exec++ = d;
            d->outputs = link;
            for (k = 0; k < comp->numOutputs; k++) {
                AEMSOUTPUT* out = &comp->outputs[k];
                int t;
                for (t = 0; t < out->count; t++) {
                    AEMSCOMPDYNAMIC* td = AEMS_compdyn(module, inst, out->targets[t].comp);
                    link++;
                    d->outputs[d->numOutputs].dst = &td->in[out->targets[t].input];
                    d->outputs[d->numOutputs].src = AEMS_outputsrc(d, k);
                    d->numOutputs++;
                }
            }
        }
        d->scheduled = 1;
        d->connected = comp->numInputs;
        d = AEMS_nextdyn(d);
    }
}
