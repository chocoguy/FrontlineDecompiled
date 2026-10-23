// Level property data: loads the level's .bpd (every placed object's
// property record, particle and light definitions, light volumes, AI paths)
// and its property BSP tree (.pbsp), byte-swaps and patches their offsets
// into pointers, creates the placed objects' scripts, finds the mounted
// machine guns, and picks player start points.
#include "game/propdat.h"
#include "game/AIFilter.h"
#include "game/BSGO_Basic.h"
#include "game/ISceneNode.h"
#include "game/Moh2.h"
#include "game/bpdlight.h"
#include "game/bsobject.h"
#include "game/bstypes.h"
#include "game/light.h"
#include "game/matrix.h"
#include "game/objcreate.h"
#include "game/offsetptr.h"
#include "game/particlesystem.h"
#include "game/player.h"
#include "game/quaternion.h"
#include "game/scene.h"
#include "game/ShellMenu.h"
#include "game/toilet.h"
#include "game/vector.h"
#include "math.h"
#include "stdlib.h"

TriggerObject_struct g_pTriggerObjects[MAX_TRIGGERS];
MGTriggerObject g_pMGTriggerObject[MAX_MG_POINTS];
int g_playerStartIndices[MAX_PLAYER_STARTS];
u8 g_takenPlayerStarts[MAX_PLAYER_STARTS];
u8* g_pRawPropertyData;
int g_pNumTriggers;
BPDHeader* g_pBPDHeader;
u8* g_pPBSPHeader;
_PropBSPNode* g_pPBSPHead;
BPDLightVolume* g_pLightVolumes;
int g_numPlayerStarts;

// Object classes (xyzProperty_Struct::m_classCRC / TriggerObjectData field 0x48).
#define CRC_MG_POINT 0x5B607813  // waypoint a soldier mans a gun from
#define CRC_MG_GUN0 0x29F985D6   // machine gun mechanics
#define CRC_MG_GUN1 0xF6798FE8
#define CRC_MG_GUN2 0x6E39BE8F
#define CRC_PLAYER_START 0x271BD574
#define CRC_PLAYER_RESTART 0xFD092133  // where a restarted level puts the player
#define CRC_COMPARTMENT_LOADER 0x4D078F23
#define CRC_PLAYER_PATH_CONTROLLER 0xD4717F38
#define CRC_FOG_PARAMS 0x35FB0439

static inline void Swap16(u8* p) {
#ifdef FRONTLINE_NATIVE
    return;  // little-endian data on a little-endian CPU: nothing to swap
#endif
    u8 t = p[0];
    p[0] = p[1];
    p[1] = t;
}

static inline void Swap32(u8* p) {
#ifdef FRONTLINE_NATIVE
    return;  // little-endian data on a little-endian CPU: nothing to swap
#endif
    u8 t = p[0];
    p[0] = p[3];
    p[3] = t;
    t = p[1];
    p[1] = p[2];
    p[2] = t;
}

// A placed object's data, by offset.
static inline u32& Word(void* data, int offset) {
    return *(u32*)((u8*)data + offset);
}

static inline TriggerObjectData_struct* Data(TriggerObject_struct* trigger) {
    return trigger->m_data;
}

static inline u32 KindCRC(TriggerObjectData_struct* data) {
    return (u32)data->field_0x48;
}

void EndianSwap(MOH_fogParams_Struct& data) {
    u8* p = (u8*)&data;
    EndianSwap(*(MOH_core_Struct*)&data);
    Swap16(p + 0x70);
    Swap16(p + 0x72);
    Swap16(p + 0x74);
    Swap32(p + 0x78);
    Swap32(p + 0x7C);
}

void EndianSwap(MOH_animatedLight_Struct& data) {
    u8* p = (u8*)&data;
    EndianSwap(*(MOH_core_Struct*)&data);
    Swap16(p + 0x72);
    Swap32(p + 0x74);
    Swap32(p + 0x78);
}

void EndianSwap(MOH_playerPathController_Struct& data) {
    u8* p = (u8*)&data;
    EndianSwap(*(MOH_core_Struct*)&data);
    Swap16(p + 0x72);
    Swap16(p + 0x74);
    Swap32(p + 0x78);
    Swap32(p + 0x7C);
}

void EndianSwap(MOH_projectileGeneratorTarget_Struct& data) {
    u8* p = (u8*)&data;
    EndianSwap(*(MOH_core_Struct*)&data);
    Swap16(p + 0x72);
    Swap32(p + 0x74);
    Swap32(p + 0x78);
    Swap32(p + 0x7C);
    Swap32(p + 0x80);
    Swap32(p + 0x84);
    Swap32(p + 0x88);
}

void EndianSwap(MOH_projectileGenerator_Struct& data) {
    u8* p = (u8*)&data;
    EndianSwap(*(MOH_core_Struct*)&data);
    Swap16(p + 0x72);
    Swap32(p + 0x74);
    Swap32(p + 0x78);
    Swap32(p + 0x7C);
    Swap32(p + 0x80);
    Swap32(p + 0x84);
    Swap32(p + 0x88);
    Swap32(p + 0x8C);
    Swap32(p + 0x90);
    Swap32(p + 0x94);
}

void EndianSwap(MOH_particle_Struct& data) {
    u8* p = (u8*)&data;
    EndianSwap(*(MOH_core_Struct*)&data);
    Swap16(p + 0x82);
    Swap16(p + 0x84);
    Swap32(p + 0x88);
    Swap32(p + 0x8C);
    Swap32(p + 0x90);
    Swap32(p + 0x94);
    Swap32(p + 0x98);
    Swap32(p + 0x9C);
    Swap32(p + 0xA0);
    Swap32(p + 0xA4);
    Swap32(p + 0xA8);
    Swap32(p + 0xAC);
    Swap32(p + 0xB0);
    Swap32(p + 0xB4);
    Swap32(p + 0xB8);
    Swap32(p + 0xBC);
    Swap32(p + 0xC0);
    Swap32(p + 0xC4);
    Swap32(p + 0xC8);
    Swap32(p + 0xCC);
    Swap32(p + 0xD0);
    Swap32(p + 0xD4);
    Swap32(p + 0xD8);
    Swap32(p + 0xDC);
    Swap32(p + 0xE0);
    Swap32(p + 0xE4);
    Swap32(p + 0xE8);
    Swap32(p + 0xEC);
}

void EndianSwap(MOH_mechanicEnvMod_Struct& data) {
    u8* p = (u8*)&data;
    EndianSwap(*(MOH_core_Struct*)&data);
    Swap32(p + 0x74);
    Swap32(p + 0x78);
}

void EndianSwap(MOH_enemy_Struct& data) {
    u8* p = (u8*)&data;
    EndianSwap(*(MOH_core_Struct*)&data);
    Swap16(p + 0x7E);
    Swap16(p + 0x80);
    Swap16(p + 0x82);
    Swap16(p + 0x84);
    Swap16(p + 0x86);
    Swap16(p + 0x88);
    Swap16(p + 0x8A);
    Swap16(p + 0x8C);
    Swap16(p + 0x8E);
    Swap32(p + 0x90);
    Swap32(p + 0x94);
    Swap32(p + 0x98);
    Swap32(p + 0x9C);
    Swap32(p + 0xA0);
    Swap32(p + 0xA4);
    Swap32(p + 0xA8);
    Swap32(p + 0xAC);
}

void EndianSwap(MOH_mechanic_Struct& data) {
    u8* p = (u8*)&data;
    EndianSwap(*(MOH_core_Struct*)&data);
    Swap32(p + 0x74);
}

void EndianSwap(MOH_core_Struct& data) {
    u8* p = (u8*)&data;
    Swap16(p + 0x2C);
    Swap16(p + 0x2E);
    Swap16(p + 0x30);
    Swap16(p + 0x32);
    Swap16(p + 0x36);
    Swap16(p + 0x38);
    Swap16(p + 0x3A);
    Swap16(p + 0x3C);
    Swap32(p + 0x40);
    Swap32(p + 0x44);
    Swap32(p + 0x48);
    Swap32(p + 0x4C);
    Swap32(p + 0x50);
    Swap32(p + 0x54);
    Swap32(p + 0x58);
    Swap32(p + 0x5C);
    Swap32(p + 0x60);
    Swap32(p + 0x64);
    Swap32(p + 0x68);
}

void EndianSwap(xyzProperty_Struct& data) {
    u8* p = (u8*)&data;
    Swap32(p + 0x0);
    Swap32(p + 0x4);
    Swap32(p + 0x8);
    Swap32(p + 0xC);
    Swap32(p + 0x10);
    Swap32(p + 0x14);
    Swap32(p + 0x18);
    Swap32(p + 0x1C);
    Swap32(p + 0x20);
    Swap32(p + 0x24);
    Swap32(p + 0x28);
}

void EndianSwap(BPDLight& data) {
    u8* p = (u8*)&data;
    Swap32(p + 0x0);
    Swap32(p + 0x4);
    Swap32(p + 0x8);
    Swap32(p + 0xC);
    Swap32(p + 0x10);
    Swap32(p + 0x14);
    Swap32(p + 0x18);
    Swap32(p + 0x1C);
    Swap32(p + 0x20);
    Swap32(p + 0x24);
    Swap32(p + 0x28);
    Swap32(p + 0x2C);
}

void EndianSwap(BPDLightVolume& data) {
    u8* p = (u8*)&data;
    Swap32(p + 0x0);
    Swap32(p + 0x4);
    Swap32(p + 0x8);
    Swap32(p + 0xC);
    Swap32(p + 0x10);
    Swap32(p + 0x14);
    Swap32(p + 0x18);
    Swap32(p + 0x1C);
    Swap32(p + 0x20);
    Swap32(p + 0x24);
    Swap32(p + 0x28);
    Swap32(p + 0x2C);
}

void EndianSwap(BPDHeader& data) {
    u8* p = (u8*)&data;
    Swap32(p + 0x0);
    Swap32(p + 0x4);
    Swap32(p + 0x8);
    Swap32(p + 0xC);
    Swap32(p + 0x10);
    Swap32(p + 0x14);
    Swap32(p + 0x18);
    Swap32(p + 0x1C);
    Swap32(p + 0x20);
    Swap32(p + 0x24);
    Swap32(p + 0x28);
    Swap32(p + 0x2C);
    Swap32(p + 0x30);
    Swap32(p + 0x34);
    Swap32(p + 0x38);
    Swap32(p + 0x3C);
    Swap32(p + 0x40);
    Swap32(p + 0x44);
    Swap32(p + 0x48);
    Swap32(p + 0x4C);
    Swap32(p + 0x50);
    Swap32(p + 0x54);
}

// The closest machine gun to `node` within `maxDist` (index into
// g_pMGTriggerObject, or -1).
int SearchForClosestMachineGun(ISceneNode* node, float maxDist) {
    float best = 10000.0f;
    int found = -1;
    for (u32 i = 0; i < MAX_MG_POINTS; i++) {
        TriggerObject_struct* gun = g_pMGTriggerObject[i].m_gun;
        if (gun != NULL) {
            CVector3 pos;
            node->GetPosition(pos);
            TriggerObjectData_struct* data = gun->m_data;
            float dy = pos.y - data->m_position[1];
            float dx = pos.x - data->m_position[0];
            float dz = pos.z - data->m_position[2];
            float dist = dz * dz + (dx * dx + dy * dy);
            if (dist > 0.0f) {
                dist = sqrtf(dist);
            }
            if (dist < best && dist < maxDist) {
                best = dist;
                found = i;
            }
        }
    }
    return found;
}

int IsMGUsed(TriggerObject_struct* point) {
    return point->m_flags & 1;
}

void MarkMGAsUsed(TriggerObject_struct* point, bool used) {
    if (used) {
        point->m_flags |= 1;
    } else {
        point->m_flags &= ~1;
    }
}

static inline bool IsMachineGun(TriggerObjectData_struct* data) {
    u32 crc = KindCRC(data);
    return data->m_type == 2 && (crc == CRC_MG_GUN0 || crc == CRC_MG_GUN1 || crc == CRC_MG_GUN2);
}

// Pairs each machine gun waypoint with the closest machine gun (within
// sqrt(10)); a waypoint farther away is reported and ignored.
void FindAllMGPointsInBPDFile() {
    MGTriggerObject* entry = g_pMGTriggerObject;
    TriggerObject_struct* point = g_pTriggerObjects;
    for (u32 i = 0; i < (u32)g_pNumTriggers; i++, point++) {
        TriggerObjectData_struct* data = point->m_data;
        if (data->m_type != 5 || KindCRC(data) != CRC_MG_POINT) {
            continue;
        }
        float best = 1e37f;
        TriggerObject_struct* closest = NULL;
        TriggerObject_struct* gun = g_pTriggerObjects;
        for (u32 n = g_pNumTriggers; n > 0; n--, gun++) {
            TriggerObjectData_struct* g = gun->m_data;
            if (IsMachineGun(g)) {
                float dy = g->m_position[1] - data->m_position[1];
                float dx = g->m_position[0] - data->m_position[0];
                float dz = g->m_position[2] - data->m_position[2];
                float distSq = dz * dz + (dx * dx + dy * dy);
                if (distSq < best) {
                    best = distSq;
                    closest = gun;
                }
            }
        }
        if (closest != NULL) {
            if (best > 10.0f) {
                DebugMsg("++++++++++++++++++++++++ START MMG WARNING ++++++++++++++++++++++++");
                TriggerObjectData_struct* d = point->m_data;
                DebugMsg("WARNING:  Possible bad waypoint placement at x: %d  y: %d  z: %d\n", (int)d->m_position[0],
                         (int)d->m_position[1], (int)d->m_position[2]);
                DebugMsg("          Closest machine gun mechanic is %d away\n", (int)best);
                DebugMsg("          We are going to ignore the waypoint\n");
                DebugMsg("+++++++++++++++++++++++++ END MMG WARNING +++++++++++++++++++++++++");
            } else {
                entry->m_point = point;
                entry->m_gun = closest;
                entry++;
            }
        }
    }
}

void FreePropertyMemory() {
    if (g_pPBSPHeader != NULL) {
        TLT_CloseFile(g_pPBSPHeader);
        g_pPBSPHeader = NULL;
    }
    if (g_pBPDHeader != NULL) {
        TLT_CloseFile(g_pBPDHeader);
        g_pBPDHeader = NULL;
    }
}

// Patches a leaf list: an offset into the file, or (when empty) the address
// of the field itself; `count` u16 entries are byte-swapped.
static inline void PatchLeafList(char* base, PTR32(u16)& list, int count, bool present) {
    if (present) {
        list = (u16*)&base[PTR32_RAW(list)];
        for (int i = 0; i < count; i++) {
            Swap16((u8*)&list[i]);
        }
    } else {
        list = (u16*)&list;
    }
}

void PatchUpPropBSPTreeLeaf(char* base, _PropBSPLeaf* leaf) {
    EndianSwap(*leaf);
    PatchLeafList(base, leaf->m_triggers, leaf->m_numTriggers, leaf->m_numTriggers != 0);
    PatchLeafList(base, leaf->m_list2C, leaf->m_numList2C, (s32)PTR32_RAW(leaf->m_list2C) > 0);
    PatchLeafList(base, leaf->m_list28, leaf->m_numList28, leaf->m_numList28 != 0);
    if (leaf->m_numList1C != 0) {
        leaf->m_list1C = (u16(*)[2]) & base[PTR32_RAW(leaf->m_list1C)];
        for (int i = 0; i < leaf->m_numList1C; i++) {
            Swap16((u8*)&leaf->m_list1C[i][0]);
            Swap16((u8*)&leaf->m_list1C[i][1]);
        }
    } else {
        leaf->m_list1C = (u16(*)[2]) & leaf->m_list1C;
    }
    PatchLeafList(base, leaf->m_list24, leaf->m_numList24, leaf->m_numList24 != 0);
}

void EndianSwap(_PropBSPLeaf& leaf) {
    u8* p = (u8*)&leaf;
    Swap32(p + 0x0);
    Swap16(p + 0x4);
    Swap16(p + 0x6);
    Swap16(p + 0x8);
    Swap16(p + 0xA);
    for (int offset = 0xC; offset < 0x30; offset += 4) {
        Swap32(p + offset);
    }
}

// Swaps and patches a node's children, then recurses into them.
void PatchUpPropBSPTreeNode(char* base, _PropBSPNode* node) {
    EndianSwap(*node);
    for (int side = 0; side < 2; side++) {
        node->m_children[side] = &base[PTR32_RAW(node->m_children[side])];
    }
    for (int side = 0; side < 2; side++) {
        if (node->m_isLeaf[side]) {
            PatchUpPropBSPTreeLeaf(base, (_PropBSPLeaf*)node->m_children[side]);
        } else {
            PatchUpPropBSPTreeNode(base, (_PropBSPNode*)node->m_children[side]);
        }
    }
}

void EndianSwap(_PropBSPNode& node) {
    EndianSwap(node.m_split, false);
    EndianSwap(*(PBSPNodeOrLeaf*)&node.m_children[0]);
    EndianSwap(*(PBSPNodeOrLeaf*)&node.m_children[1]);
}

void EndianSwap(PBSPNodeOrLeaf& ptr) {
    Swap32((u8*)&ptr);
}

void EndianSwap(float& value, bool) {
    Swap32((u8*)&value);
}

// Loads the level's property BSP tree (patched once; a restart keeps it).
void LoadPropBSPTree(char* file, bool reset) {
    u8* header = (u8*)TLT_LoadFileFromLevelBigFile(file, NULL);
    g_pPBSPHeader = header;
    if (!reset) {
        Swap32(header + 0x20);
        Swap32(header + 0x24);
    }
    _PropBSPNode* root = (_PropBSPNode*)&header[Word(g_pPBSPHeader, 0x24)];
    g_pPBSPHead = root;
    if (!reset) {
        PatchUpPropBSPTreeNode((char*)header, root);
    }
}

// Creates player `player` at its start: the first player start (in
// multiplayer, the one numbered `player`), or with `restart`, the restart
// point if there is one.
void FindPlayerStartTriggerAndCreatePlayer(bool restart, int player) {
    TriggerObject_struct* start = NULL;
    TriggerObject_struct* restartPoint = NULL;
    TriggerObject_struct* trigger = g_pTriggerObjects;
    for (u32 n = g_pNumTriggers; n > 0; n--, trigger++) {
        TriggerObjectData_struct* data = trigger->m_data;
        if (data->m_type != 2) {
            continue;
        }
        if (start == NULL && KindCRC(data) == CRC_PLAYER_START) {
            if (!g_bInMultiplayerMode || data->field_0x38 == player) {
                start = trigger;
                if (!restart) {
                    break;
                }
            }
        } else if (restartPoint == NULL && KindCRC(data) == CRC_PLAYER_RESTART) {
            restartPoint = trigger;
            if (restart) {
                break;
            }
        }
    }
    if (restart && restartPoint != NULL) {
        start = restartPoint;
    }
    if (start != NULL) {
        CreateObject(start, 0xFFFF, ADDR_TO_PTR(void, player));
    }
}

// Respawns player `player` (multiplayer) at a random player start no player
// is within sqrt-distance 10 of.
void RestartPlayer(int player) {
    int i;
    if (g_numPlayerStarts == 0) {
        TriggerObject_struct* trigger = g_pTriggerObjects;
        int index = 0;
        for (u32 n = g_pNumTriggers; n > 0; n--, trigger++, index++) {
            TriggerObjectData_struct* data = trigger->m_data;
            if (data->m_type == 2 && KindCRC(data) == CRC_PLAYER_START) {
                g_playerStartIndices[g_numPlayerStarts++] = index;
            }
        }
    }
    for (i = 0; i < MAX_PLAYER_STARTS; i++) {
        g_takenPlayerStarts[i] = i >= g_numPlayerStarts;
    }
    for (i = 0; i < g_numPlayerStarts; i++) {
        TriggerObjectData_struct* data = g_pTriggerObjects[g_playerStartIndices[i]].m_data;
        float z = data->m_position[2];
        float y = data->m_position[1];
        float x = data->m_position[0];
        for (int p = 0; p < g_NumPlayers; p++) {
            CVector3 pos;
            g_scene.GetPlayer(p)->GetPosition(pos);
            float dx = pos.x - x;
            float dy = pos.y - y;
            float dz = pos.z - z;
            float dist = dz * dz + (dx * dx + dy * dy);
            if (dist > 0.0f) {
                dist = sqrtf(dist);
            }
            if (dist < 10.0f) {
                g_takenPlayerStarts[i] = 1;
                break;
            }
        }
    }
    int first = rand() % MAX_PLAYER_STARTS;
    for (i = 0; i < MAX_PLAYER_STARTS; i++) {
        int s = (first + i) % MAX_PLAYER_STARTS;
        if (!g_takenPlayerStarts[s]) {
            TriggerObject_struct* trigger = &g_pTriggerObjects[g_playerStartIndices[s]];
            CMatrix m;
            ((CQuaternion*)trigger->m_data->m_orientation)->GetMatrix(m);
            TriggerObjectData_struct* data = trigger->m_data;
            CVector3 pos;
            pos.x = data->m_position[0];
            pos.y = data->m_position[1];
            pos.z = data->m_position[2];
            m.SetPos(pos);
            g_scene.GetPlayer(player)->SetTMLocalToWorld(m);
            return;
        }
    }
}

// Patches a message (or group) list: [0] = count, then the entries.
static inline void PatchList(MOH_core_Struct* core, int offset) {
    u32& field = Word(core, offset);
    if (field != 0) {
        field = (u32)PTR_TO_ADDR(core) + field;
        u32* list = ADDR_TO_PTR(u32, field);
        Swap32((u8*)&list[0]);
        for (u32 i = 1; i <= list[0]; i++) {
            Swap32((u8*)&list[i]);
        }
    }
}

static inline void PatchPointer(MOH_core_Struct* core, int offset) {
    u32& field = Word(core, offset);
    if (field != 0) {
        field = (u32)PTR_TO_ADDR(core) + field;
    }
}

// Turns a placed object's offsets into pointers: its message lists (player
// enter/leave, object enter/leave, groups, player activate), script name and
// two more pointers. The object-activate list is dropped.
void PatchUpCore(MOH_core_Struct* core) {
    PatchList(core, 0x58);
    PatchList(core, 0x5C);
    PatchList(core, 0x60);
    PatchList(core, 0x64);
    PatchList(core, 0x44);
    PatchList(core, 0x68);
    PatchPointer(core, 0x40);
    PatchPointer(core, 0x4C);
    PatchPointer(core, 0x54);
    if (Word(core, 0x6C) != 0) {
        Word(core, 0x6C) = 0;
    }
}

// Particle tweakers: {name, data} pairs.
static inline void PatchTweakers(PTR32(PTR32(void))& tweakers, int count, BPDHeader* header) {
    if (tweakers != NULL) {
        offsetPtr(tweakers, (int)PTR_TO_ADDR(header));
        for (int i = 0; i < count; i++) {
            Swap32((u8*)&tweakers[i * 2]);
            Swap32((u8*)&tweakers[i * 2 + 1]);
            if (tweakers[i * 2] != NULL) {
                offsetPtr(*(PTR32(char)*)&tweakers[i * 2], (int)PTR_TO_ADDR(header));
            }
            if (tweakers[i * 2 + 1] != NULL) {
                offsetPtr(tweakers[i * 2 + 1], (int)PTR_TO_ADDR(header));
            }
        }
    }
}

// Swaps and patches the whole .bpd; returns the size of the property records.
int PatchUpAllPropertyData(BPDHeader* header) {
    offsetPtr(header->m_properties, (int)PTR_TO_ADDR(header));
    u8* first = header->m_properties;
    int count = header->m_numProperties;
    TriggerObject_struct* trigger = g_pTriggerObjects;
    u8* record = first;
    int i;
    if (header->m_numTextures != 0) {
        offsetPtr(header->m_textures, (int)PTR_TO_ADDR(header));
        for (i = 0; i < header->m_numTextures; i++) {
            Swap32((u8*)&header->m_textures[i]);
            offsetPtr(header->m_textures[i], (int)PTR_TO_ADDR(header));
            CParticleSystem::Register((ShapeFile*)header->m_textures[i]);
        }
    }
    if (header->m_particles != NULL) {
        PTR32(MOH_particle_Struct) particle = header->m_particles;
        offsetPtr(particle, (int)PTR_TO_ADDR(header));
        for (i = 0; i < header->m_numParticles; i++) {
            EndianSwap(*(xyzProperty_Struct*)particle);
            EndianSwap(*particle);
            offsetPtr(*(PTR32(void)*)((u8*)particle + 0xEC), (int)PTR_TO_ADDR(header));
            particle = (MOH_particle_Struct*)((u8*)particle + 0xF0);
        }
        PTR32(char) particles = (char*)header->m_particles;
        offsetPtr(particles, (int)PTR_TO_ADDR(header));
        CParticleSystem::Register(particles, header->m_numParticles);
    }
    if (header->m_lights != NULL) {
        PTR32(MOH_animatedLight_Struct) light = header->m_lights;
        offsetPtr(light, (int)PTR_TO_ADDR(header));
        for (i = 0; i < header->m_numLights; i++) {
            EndianSwap(*(xyzProperty_Struct*)light);
            EndianSwap(*light);
            offsetPtr(*(PTR32(u32)*)((u8*)light + 0x74), (int)PTR_TO_ADDR(header));
            offsetPtr(*(PTR32(f32)*)((u8*)light + 0x78), (int)PTR_TO_ADDR(header));
            f32* intensities = *(PTR32(f32)*)((u8*)light + 0x78);
            for (int f = 0; f < *(s16*)((u8*)light + 0x72); f++) {
                Swap32((u8*)&intensities[f]);
            }
            light = (MOH_animatedLight_Struct*)((u8*)light + 0x7C);
        }
        PTR32(char) lights = (char*)header->m_lights;
        offsetPtr(lights, (int)PTR_TO_ADDR(header));
        CLight::Register(lights, header->m_numLights);
    }
    if (header->m_lightVolumes != NULL) {
        offsetPtr(header->m_lightVolumes, (int)PTR_TO_ADDR(header));
        for (i = 0; i < header->m_numLightVolumes; i++) {
            EndianSwap(header->m_lightVolumes[i]);
            BPDLightVolume* volume = &header->m_lightVolumes[i];
            if (volume->numPlanes > 0) {
                offsetPtr(volume->planes, (int)PTR_TO_ADDR(header));
                for (int n = 0; n < header->m_lightVolumes[i].numPlanes; n++) {
                    BPDPlane* plane = &header->m_lightVolumes[i].planes[n];
                    Swap32((u8*)&plane->n[0]);
                    Swap32((u8*)&plane->n[1]);
                    Swap32((u8*)&plane->n[2]);
                    Swap32((u8*)&plane->d);
                }
            }
            volume = &header->m_lightVolumes[i];
            if (volume->numLights > 0) {
                offsetPtr(volume->lights, (int)PTR_TO_ADDR(header));
                for (int n = 0; n < header->m_lightVolumes[i].numLights; n++) {
                    EndianSwap(header->m_lightVolumes[i].lights[n]);
                }
            }
        }
    }
    PatchTweakers(header->m_tweakers0, header->m_numTweakers0, header);
    PatchTweakers(header->m_tweakers1, header->m_numTweakers1, header);
    if (header->m_polyPaths != NULL) {
        offsetPtr(header->m_polyPaths, (int)PTR_TO_ADDR(header));
        u8* path = (u8*)header->m_polyPaths;
        for (i = 0; i < header->m_numPolyPaths; i++, path += 0xC) {
            Swap32(path + 0x0);
            Swap32(path + 0x4);
            Swap32(path + 0x8);
        }
    }

    for (u32 n = 0; n < (u32)count; n++, trigger++) {
        trigger->m_data = (TriggerObjectData_struct*)record;
        EndianSwap(*(xyzProperty_Struct*)trigger->m_data);
        xyzProperty_Struct* prop = (xyzProperty_Struct*)trigger->m_data;
        bool done = false;
        switch (prop->m_classCRC) {
        case CRC_COMPARTMENT_LOADER: {
            done = true;
            for (int offset = 0x2C; offset <= 0x3A; offset += 2) {
                Swap16((u8*)prop + offset);
            }
            break;
        }
        case CRC_PLAYER_PATH_CONTROLLER:
            EndianSwap(*(MOH_playerPathController_Struct*)prop);
            PatchUpCore((MOH_core_Struct*)trigger->m_data);
            done = true;
            break;
        case CRC_FOG_PARAMS:
            EndianSwap(*(MOH_fogParams_Struct*)prop);
            PatchUpCore((MOH_core_Struct*)trigger->m_data);
            done = true;
            break;
        }
        if (!done) {
            prop = (xyzProperty_Struct*)trigger->m_data;
            u32 type = prop->m_type;
            switch (type) {
            case 0:
                break;
            case 1:
            case 4:
            case 5:
            case 6:
            case 7:
                EndianSwap(*(MOH_core_Struct*)prop);
                PatchUpCore((MOH_core_Struct*)trigger->m_data);
                break;
            case 2:
                EndianSwap(*(MOH_mechanic_Struct*)prop);
                PatchUpCore((MOH_core_Struct*)trigger->m_data);
                break;
            case 8:
                EndianSwap(*(MOH_mechanicEnvMod_Struct*)prop);
                PatchUpCore((MOH_core_Struct*)trigger->m_data);
                break;
            case 3:
                EndianSwap(*(MOH_enemy_Struct*)prop);
                PatchUpCore((MOH_core_Struct*)trigger->m_data);
                break;
            case 9:
                EndianSwap(*(MOH_particle_Struct*)prop);
                PatchUpCore((MOH_core_Struct*)trigger->m_data);
                offsetPtr(*(PTR32(void)*)((u8*)trigger->m_data + 0xEC), (int)PTR_TO_ADDR(header));
                break;
            case 14: {
                EndianSwap(*(MOH_animatedLight_Struct*)prop);
                PatchUpCore((MOH_core_Struct*)trigger->m_data);
                u8* light = (u8*)trigger->m_data;
                // (These offsets are relative to the record.)
                offsetPtr(*(PTR32(u32)*)(light + 0x74), (int)PTR_TO_ADDR(record));
                offsetPtr(*(PTR32(f32)*)(light + 0x78), (int)PTR_TO_ADDR(record));
                for (int f = 0; f < *(s16*)(light + 0x72); f++) {
                    Swap32((u8*)&(*(PTR32(f32)*)(light + 0x78))[f]);
                }
                break;
            }
            case 10:
                EndianSwap(*(MOH_projectileGenerator_Struct*)prop);
                PatchUpCore((MOH_core_Struct*)trigger->m_data);
                break;
            case 11:
                EndianSwap(*(MOH_projectileGeneratorTarget_Struct*)prop);
                PatchUpCore((MOH_core_Struct*)trigger->m_data);
                break;
            default:
                if (type < 0x10) {
                    EndianSwap(*(MOH_core_Struct*)prop);
                    PatchUpCore((MOH_core_Struct*)trigger->m_data);
                } else {
                    DebugMsg("Unknown property type\n");
                }
                break;
            }
        }
        record += ((xyzProperty_Struct*)trigger->m_data)->m_size;
    }
    return record - first;
}

// Loads the level's placed objects and creates their scripts (and their
// particle systems and lights). A restart reuses the loaded data.
void LoadPropertyData(char* file, bool reset) {
    if (!reset) {
        int size;
        g_pBPDHeader = (BPDHeader*)TLT_LoadFileFromLevelBigFile(file, &size);
        EndianSwap(*g_pBPDHeader);
        g_pNumTriggers = g_pBPDHeader->m_numProperties;
        PatchUpAllPropertyData(g_pBPDHeader);
        g_aigAIFilterGlobalObject.CreateSplinePathManager(g_pBPDHeader, g_pBPDHeader->m_polyPaths,
                                                          g_pBPDHeader->m_numPolyPaths);
        g_aigAIFilterGlobalObject.CreateAStarPathNodeList(g_pBPDHeader, g_pBPDHeader->m_pathNodes,
                                                          g_pBPDHeader->m_numPathNodes);
        g_pRawPropertyData = g_pBPDHeader->m_properties;
        g_pLightVolumes = g_pBPDHeader->m_lightVolumes;
    } else {
        g_aigAIFilterGlobalObject.ResetSplinePathManager(g_pBPDHeader, g_pBPDHeader->m_polyPaths,
                                                         g_pBPDHeader->m_numPolyPaths);
    }
    TriggerObject_struct* trigger = g_pTriggerObjects;
    for (u32 i = 0; i < (u32)g_pNumTriggers; i++, trigger++) {
        trigger->m_flags = 0;
        TriggerObjectData_struct* data = trigger->m_data;
        if (data->m_type == 0) {
            continue;
        }
        const char* script = ADDR_TO_PTR(const char, Word(data, 0x40));
        if (script != NULL) {
            CreateBSObject(script, &trigger->m_object, NULL, trigger, NULL);
            BSObjectTriggerEvent(trigger->m_object, BSEVENT_START, NULL, NULL, true);
        } else {
            trigger->m_object = NULL;
        }
        data = trigger->m_data;
        if (data->m_type == 9 && Word(data, 0x40) == 0) {
            CParticleSystem::Create(data);
        }
        data = trigger->m_data;
        if (data->m_type == 14 && Word(data, 0x40) == 0) {
            CLight::Create(data);
        }
    }
    for (int m = 0; m < MAX_MG_POINTS; m++) {
        g_pMGTriggerObject[m].m_point = NULL;
        g_pMGTriggerObject[m].m_gun = NULL;
        g_pMGTriggerObject[m].field_0x8 = 0;
    }
    FindAllMGPointsInBPDFile();
}

