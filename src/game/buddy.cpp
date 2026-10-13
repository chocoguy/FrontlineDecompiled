// Buddies (script built-ins): friendly soldiers that advance with the
// player. Each attached buddy has a slot and a path group; when the player
// reaches checkpoint n, a buddy at an earlier checkpoint is sent (event
// 0xD3) along path group + its next checkpoint. Buddies beyond the slots
// wait in a ready list and replace attached buddies that die.
#include "game/buddy.h"
#include "game/BSGO_Basic.h"
#include "game/ISceneNode.h"
#include "game/Moh2.h"
#include "game/Soldier_object.h"
#include "game/bsbif.h"
#include "game/bsmachin.h"
#include "game/bsobject.h"
#include "game/bstypes.h"
#include "dwi/list.h"
#include "dwi/vector.h"

#define BUDDYEVT_FOLLOW_PATH 0xD3  // data: the path to walk
#define BUDDYEVT_ATTACHED 0xD6

dwi::vector<BuddyInfo> g_attached_buddies;
dwi::list<BuddyInfo> g_ready_buddies;
dwi::vector<int> g_buddy_path_base;  // per slot
bool g_bInitialized;
int g_player_checkpoint;

// For the debug messages: a script object's placed id and position.
struct BuddyDesc {
    s16 m_id;
    float m_x;
    float m_y;
    float m_z;
    int m_extra;  // field 0x5C of the game object's script data
};

static inline void Describe(BSObject* script, BuddyDesc& d) {
    d.m_x = 0.0f;
    d.m_id = 0;
    d.m_y = 0.0f;
    d.m_z = 0.0f;
    TriggerObject_struct* trigger = script->m_trigger;
    if (trigger != NULL) {
        TriggerObjectData_struct* data = trigger->m_data;
        if (data != NULL) {
            d.m_x = data->m_position[0];
            d.m_y = data->m_position[1];
            d.m_z = data->m_position[2];
        }
        if (data != NULL) {
            d.m_id = data->field_0x38;
        }
    }
    d.m_extra = *(int*)((u8*)script->m_gameObject->GetScriptData() + 0x5C);
}

static inline BSObject* BuddyScript(CSoldierObject* soldier) {
    return ((ISceneNode*)soldier)->GetScriptObject();
}

static inline CSoldierObject* SoldierOf(BSObject* script) {
    return script->m_gameObject->GetSceneNode()->AsSoldierObject();
}

void DestroyBuddyDataStructures() {
    g_bInitialized = false;
    g_player_checkpoint = 0;
    g_attached_buddies.clear();
    g_ready_buddies.clear();
    g_buddy_path_base.clear();
}

// (numSlots, numReady): sets up empty slots.
void BIFunc_CreateBuddyDataStructures(int** sp, void*) {
    int numSlots = BIFArg(sp, 0);
    int numReady = BIFArg(sp, 1);
    if (!g_bInitialized) {
        g_attached_buddies.reserve(numSlots, "Buddy Attach List");
        g_ready_buddies.reserve(numReady, "Buddy Read List");
        g_buddy_path_base.reserve(numSlots, "Buddy Path List");
    } else {
        g_attached_buddies.clear();
        g_ready_buddies.clear();
        g_buddy_path_base.clear();
    }
    for (int i = 0; i < numSlots; i++) {
        BuddyInfo empty;
        empty.m_soldier = NULL;
        empty.m_checkpoint = 0;
        empty.m_moving = 0;
        empty.m_inUse = false;
        empty.m_pathBase = 0;
        g_attached_buddies.push_back(empty);
        g_buddy_path_base.push_back(0);
    }
    g_bInitialized = true;
    BIFPop(sp);
}

// Sends the buddy along its next path if the player is ahead of it.
static inline void GiveBuddyOrders(BuddyInfo* b) {
    DebugMsg("BUDDY_DEBUG: Attempting GiveBuddyOrders\n");
    if (b->m_checkpoint < g_player_checkpoint) {
        if (b->m_moving == 0) {
            int next = b->m_checkpoint + 1;
            BSObjectTriggerEvent(BuddyScript(b->m_soldier), BUDDYEVT_FOLLOW_PATH, (void*)(b->m_pathBase + next),
                                 g_pBSObject, false);
            b->m_moving = 1;
            BuddyDesc d;
            Describe(BuddyScript(b->m_soldier), d);
            DebugMsg("BUDDY_DEBUG: GiveBuddyOrders advancing buddy %x[%3d @(%4d,%4d,%4d)] (current=%d player=%d) "
                     "using path %d\n",
                     b->m_soldier, d.m_id, (int)d.m_x, (int)d.m_y, (int)d.m_z, b->m_checkpoint, g_player_checkpoint,
                     b->m_pathBase + next);
        } else {
            DebugMsg("BUDDY_DEBUG: Already moving GiveBuddyOrders\n");
        }
    } else {
        DebugMsg("BUDDY_DEBUG: Stopping buddy\n");
        b->m_moving = 0;
    }
}

static inline void DumpAttached() {
    DebugMsg("BUDDY_DEBUG: Dumping attached list...\n");
    for (int i = 0; i < g_attached_buddies.size(); i++) {
        BuddyInfo* b = &g_attached_buddies[i];
        if (b->m_inUse == true) {
            CSoldierObject* soldier = b->m_soldier;
            BuddyDesc d;
            Describe(BuddyScript(soldier), d);
            DebugMsg("BUDDY_DEBUG:   Attached=%x[%3d @(%4d,%4d,%4d) %4d] In Slot %d\n", soldier, d.m_id, (int)d.m_x,
                     (int)d.m_y, (int)d.m_z, d.m_extra, i);
        }
    }
}

static inline void DumpReady() {
    DebugMsg("BUDDY_DEBUG: Dumping ready list...\n");
    int i = 0;
    for (dwi::list<BuddyInfo>::node* n = g_ready_buddies.begin(); n != g_ready_buddies.end(); n = n->next) {
        CSoldierObject* soldier = n->value.m_soldier;
        BuddyDesc d;
        Describe(BuddyScript(soldier), d);
        DebugMsg("BUDDY_DEBUG:   Ready=%x[%3d @(%4d,%4d,%4d) %4d] In Slot %d\n", soldier, d.m_id, (int)d.m_x,
                 (int)d.m_y, (int)d.m_z, d.m_extra, i);
        i++;
    }
}

// Puts the buddy in the first free slot (and tells it, event 0xD6), or
// on the ready list when all slots are taken.
void AddBuddy(BuddyInfo info, bool giveOrders) {
    for (int i = 0; i < g_attached_buddies.size(); i++) {
        BuddyInfo* b = &g_attached_buddies[i];
        if (!b->m_inUse) {
            BuddyDesc d;
            Describe(BuddyScript(info.m_soldier), d);
            DebugMsg("BUDDY_DEBUG: UsingBuddy=%x[%3d @(%4d,%4d,%4d) %4d] In Slot %d\n", info.m_soldier, d.m_id,
                     (int)d.m_x, (int)d.m_y, (int)d.m_z, d.m_extra, i);
            g_attached_buddies[i] = info;
            g_attached_buddies[i].m_inUse = true;
            g_attached_buddies[i].m_pathBase = g_buddy_path_base[i];
            if (giveOrders) {
                GiveBuddyOrders(&g_attached_buddies[i]);
            }
            BSObjectTriggerEvent(BuddyScript(info.m_soldier), BUDDYEVT_ATTACHED, NULL, g_pBSObject, false);
            return;
        }
    }
    g_ready_buddies.push_back(info);
}

// (checkpoint): adds the running soldier as a buddy at that checkpoint.
void BIFunc_AddBuddy(int** sp, void* self) {
    CSoldierObject* soldier = ((ISceneNode*)self)->AsSoldierObject();
    int checkpoint = BIFArg(sp, 0);
    BuddyDesc d;
    Describe(g_pBSObject, d);
    DebugMsg("BUDDY_DEBUG: AddBuddy buddy=%x[%3d @(%4d,%4d,%4d) %4d] progress=%d\n", soldier, d.m_id, (int)d.m_x,
             (int)d.m_y, (int)d.m_z, d.m_extra, checkpoint);
    BuddyInfo info;
    info.m_soldier = soldier;
    info.m_checkpoint = checkpoint;
    info.m_moving = 0;
    info.m_inUse = true;
    info.m_pathBase = 0;
    AddBuddy(info, false);
    DumpAttached();
    DumpReady();
    BIFPop(sp);
}

// The running soldier asks for its first orders.
void BIFunc_GetInitialBuddyOrders(int** sp, void* self) {
    CSoldierObject* soldier = ((ISceneNode*)self)->AsSoldierObject();
    for (BuddyInfo* b = g_attached_buddies.begin(); b != g_attached_buddies.end(); b++) {
        if (b->m_soldier == soldier) {
            BuddyDesc d;
            Describe(BuddyScript(soldier), d);
            DebugMsg("BUDDY_DEBUG: GetInitialBuddyOrders=%x[%3d @(%4d,%4d,%4d) %4d]\n", soldier, d.m_id, (int)d.m_x,
                     (int)d.m_y, (int)d.m_z, d.m_extra);
            GiveBuddyOrders(b);
            break;
        }
    }
    DumpAttached();
    DumpReady();
    BIFPop(sp);
}

// (script): frees its slot (the first ready buddy takes it over), or takes
// it off the ready list.
void BIFunc_BuddyDied(int** sp, void*) {
    BSObject* script = (BSObject*)BIFArg(sp, 0);
    CSoldierObject* soldier = SoldierOf(script);
    if (soldier != NULL) {
        bool found = false;
        for (BuddyInfo* b = g_attached_buddies.begin(); b != g_attached_buddies.end(); b++) {
            if (b->m_soldier == soldier) {
                b->m_inUse = false;
                b->m_soldier = NULL;
                BuddyDesc d;
                Describe(script, d);
                DebugMsg("BUDDY_DEBUG: BuddyDied=%x[%3d @(%4d,%4d,%4d) %4d]\n", soldier, d.m_id, (int)d.m_x,
                         (int)d.m_y, (int)d.m_z, d.m_extra);
                if (g_ready_buddies.size() > 0) {
                    BuddyInfo next = g_ready_buddies.begin()->value;
                    AddBuddy(next, true);
                    g_ready_buddies.erase(g_ready_buddies.begin());
                }
                found = true;
                break;
            }
        }
        if (!found) {
            for (dwi::list<BuddyInfo>::node* n = g_ready_buddies.begin(); n != g_ready_buddies.end(); n = n->next) {
                if (n->value.m_soldier == soldier) {
                    g_ready_buddies.erase(n);
                    break;
                }
            }
        }
        DumpAttached();
        DumpReady();
    }
    BIFPop(sp);
}

// (script, checkpoint): the buddy reached `checkpoint`; returns whether it
// was sent on.
void BIFunc_UpdateBuddyProgress(int** sp, void*) {
    BSObject* script = (BSObject*)BIFArg(sp, 0);
    int checkpoint = BIFArg(sp, 1);
    CSoldierObject* soldier = SoldierOf(script);
    bool found = false;
    int result = 1;
    DebugMsg("BUDDY_DEBUG: Attempting UpdateBuddyProgress\n");
    for (BuddyInfo* b = g_attached_buddies.begin(); b != g_attached_buddies.end(); b++) {
        if (b->m_soldier == soldier && b->m_inUse) {
            BuddyDesc d;
            Describe(script, d);
            DebugMsg("BUDDY_DEBUG: UpdateBuddyProgress Buddy=%x[%3d @(%4d,%4d,%4d) %4d] Checkpoint=%d->%d\n", soldier,
                     d.m_id, (int)d.m_x, (int)d.m_y, (int)d.m_z, d.m_extra, b->m_checkpoint, checkpoint);
            if (b->m_checkpoint >= checkpoint) {
                DebugMsg("BUDDY_DEBUG: Already past there\n");
            } else {
                DebugMsg("BUDDY_DEBUG: Haven't reached yet\n");
                b->m_checkpoint = checkpoint;
                b->m_moving = 0;
                GiveBuddyOrders(b);
                result = b->m_moving != 0;
            }
            found = true;
            break;
        }
    }
    if (!found) {
        for (dwi::list<BuddyInfo>::node* n = g_ready_buddies.begin(); n != g_ready_buddies.end(); n = n->next) {
            if (n->value.m_soldier == soldier && n->value.m_checkpoint < checkpoint) {
                BuddyDesc d;
                Describe(script, d);
                // (The original passes more arguments than the format uses.)
                DebugMsg("BUDDY_DEBUG: Update Potential Buddy Progress Buddy=%x Checkpoint=%d\n", soldier, d.m_id,
                         (int)d.m_x, (int)d.m_y, (int)d.m_z, checkpoint);
                n->value.m_checkpoint = checkpoint;
            }
        }
    }
    DumpReady();
    BIFPop(sp);
    BIFReturn(sp, result);
}

// (checkpoint): the player reached it; sends the attached buddies on.
void BIFunc_UpdateBuddyCheckpoints(int** sp, void*) {
    g_player_checkpoint = BIFArg(sp, 0);
    DebugMsg("BUDDY_DEBUG: UpdateBuddyCheckpoints Checkpoint=%d\n", g_player_checkpoint);
    for (BuddyInfo* b = g_attached_buddies.begin(); b != g_attached_buddies.end(); b++) {
        if (b->m_inUse) {
            GiveBuddyOrders(b);
        }
    }
    DumpAttached();
    BIFPop(sp);
}

// (slot, pathBase)
void BIFunc_SetBuddyPathGroup(int** sp, void*) {
    int index = BIFArg(sp, 0);
    int base = BIFArg(sp, 1);
    DebugMsg("BUDDY_DEBUG: SetBuddyPathGroup Index=%d PathBase=%d\n", index, base);
    g_buddy_path_base[index] = base;
    BIFPop(sp);
}
