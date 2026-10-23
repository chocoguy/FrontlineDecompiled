#include "game/bulletfactory.h"
#include "game/bullet.h"
#include "game/projectilebullet.h"
#include "game/thrownbullet.h"
#include "game/sprite.h"
#include "game/toilet.h"
#include "game/memory.h"
#include "game/endian.h"
#include "game/ShellMenu.h"
#include "string.h"

void DebugMsg(const char*, ...);

float CBulletFactory::GetBulletWeight(bool projectile, int type) {
    if (projectile) {
        return m_projectileProps[type].m_weight;
    }
    return m_thrownProps[type].m_weight;
}

// Deletes the ammo icons (types sharing a file share the sprite, which is
// deleted once).
void CBulletFactory::DeleteBulletGUISprites() {
    for (int i = 0; i < NUM_PROJECTILE_BULLET_TYPES; i++) {
        ProjectileBulletProperties_struct* p = &m_projectileProps[i];
        if (p->m_spriteName[0] == 0) {
            continue;
        }
        for (int j = 0; j < i; j++) {
            if (strcmp(p->m_spriteName, m_projectileProps[j].m_spriteName) == 0) {
                m_projectileProps[i].m_pSprite = NULL;
                break;
            }
        }
        if (m_projectileProps[i].m_pSprite != NULL) {
            delete m_projectileProps[i].m_pSprite;
            m_projectileProps[i].m_pSprite = NULL;
        }
    }
    for (int i = 0; i < NUM_THROWN_BULLET_TYPES; i++) {
        ThrownBulletProperties_struct* p = &m_thrownProps[i];
        if (p->m_spriteName[0] == 0) {
            continue;
        }
        for (int j = 0; j < i; j++) {
            if (strcmp(p->m_spriteName, m_thrownProps[j].m_spriteName) == 0) {
                m_thrownProps[i].m_pSprite = NULL;
                break;
            }
        }
        if (m_thrownProps[i].m_pSprite != NULL) {
            delete m_thrownProps[i].m_pSprite;
            m_thrownProps[i].m_pSprite = NULL;
        }
    }
}

static inline CSprite* NewIcon(const char* name) {
    void* mem = DWI_alloc(NULL, sizeof(CSprite), 0x400);
    CSprite* sprite = NULL;
    if (mem != NULL) {
        sprite = ::new (mem) CSprite(name, (CSprite::ESpriteLoadType)1, (ERenderPriority)0x6F);
    }
    return sprite;
}

// Loads each type's ammo icon (reusing an earlier type's for the same file).
void CBulletFactory::LoadBulletGUISprites() {
    for (int i = 0; i < NUM_PROJECTILE_BULLET_TYPES; i++) {
        m_projectileProps[i].m_pSprite = NULL;
        char* name = m_projectileProps[i].m_spriteName;
        if (name[0] == 0) {
            continue;
        }
        for (int j = 0; j < i; j++) {
            if (strcmp(name, m_projectileProps[j].m_spriteName) == 0) {
                m_projectileProps[i].m_pSprite = m_projectileProps[j].m_pSprite;
                break;
            }
        }
        if (m_projectileProps[i].m_pSprite == NULL) {
            m_projectileProps[i].m_pSprite = NewIcon(name);
            // No texture: no icon.
            if (m_projectileProps[i].m_pSprite->m_file == NULL) {
                m_projectileProps[i].m_pSprite = NULL;
            }
        }
    }
    for (int i = 0; i < NUM_THROWN_BULLET_TYPES; i++) {
        m_thrownProps[i].m_pSprite = NULL;
        char* name = m_thrownProps[i].m_spriteName;
        if (name[0] == 0) {
            continue;
        }
        for (int j = 0; j < i; j++) {
            if (strcmp(name, m_thrownProps[j].m_spriteName) == 0) {
                m_thrownProps[i].m_pSprite = m_thrownProps[j].m_pSprite;
                break;
            }
        }
        if (m_thrownProps[i].m_pSprite == NULL) {
            m_thrownProps[i].m_pSprite = NewIcon(name);
        }
    }
}

// Reads a little-endian value at `at`.
template <class T>
static inline T ReadLE(const u8* at) {
    T value;
    memcpy(&value, at, sizeof(T));
    EndianSwap(value);
    return value;
}

// The level's bullet file for the difficulty (multiplayer has its own):
// two counts, then records of a kind (1 skipped, 2 projectile, 3 thrown),
// the type, and that type's parameters.
void CBulletFactory::LoadBulletParameters() {
    LevelFileContentsStruct_* level = TLT_FindNextResourceByType((TLTResourceID)0xB, NULL);
    u8* file;
    if (g_bInMultiplayerMode) {
        file = (u8*)TLT_LoadFileFromLevelBigFile(ADDR_TO_PTR(char, level->fields[65].value), NULL);
    } else {
        switch (g_Shell.Get_currentDifficulty()) {
        case 1:
            file = (u8*)TLT_LoadFileFromLevelBigFile(ADDR_TO_PTR(char, level->fields[4].value), NULL);
            break;
        case 2:
            file = (u8*)TLT_LoadFileFromLevelBigFile(ADDR_TO_PTR(char, level->fields[5].value), NULL);
            break;
        case 3:
            file = (u8*)TLT_LoadFileFromLevelBigFile(ADDR_TO_PTR(char, level->fields[6].value), NULL);
            break;
        default:
            DebugMsg("Unknown difficulty level.  Assuming Level TE\n");
            file = (u8*)TLT_LoadFileFromLevelBigFile(ADDR_TO_PTR(char, level->fields[4].value), NULL);
            break;
        }
    }
    int numProjectile = ReadLE<int>(file + 4);
    int numThrown = ReadLE<int>(file + 8);
    m_projectileProps = (ProjectileBulletProperties_struct*)new u8[numProjectile * sizeof(ProjectileBulletProperties_struct)];
    m_thrownProps = (ThrownBulletProperties_struct*)new u8[numThrown * sizeof(ThrownBulletProperties_struct)];
    s16 at = 0xC;
    int loaded = 0;
    while (loaded < numThrown + numProjectile) {
        s16 kind = ReadLE<s16>(file + at);
        at += 2;
        switch (kind) {
        case 1:
            at += 0x52;
            break;
        case 2: {
            s16 type = ReadLE<s16>(file + at);
            ProjectileBulletProperties_struct* p = &m_projectileProps[type];
            memcpy(p, file + (s16)(at + 2), sizeof(*p));
            at += 0x42;
            loaded++;
            EndianSwap(*(u32*)&p->m_pSprite);
            EndianSwap(p->m_lifetime);
            EndianSwap(*(u32*)&p->m_weight);
            EndianSwap(*(u32*)&p->m_damage);
            EndianSwap(*(u32*)&p->m_tracerWidth);
            break;
        }
        case 3: {
            s16 type = ReadLE<s16>(file + at);
            ThrownBulletProperties_struct* p = &m_thrownProps[type];
            memcpy(p, file + (s16)(at + 2), sizeof(*p));
            EndianSwap(*(u32*)&p->m_pSprite);
            EndianSwap(p->m_lifetime);
            EndianSwap(*(u32*)&p->m_weight);
            EndianSwap(p->field_0x30);
            EndianSwap(p->field_0x34);
            EndianSwap(*(u32*)&p->m_friction);
            at += 0x4E;
            loaded++;
            EndianSwap(*(u32*)&p->m_bounce);
            EndianSwap(*(u32*)&p->m_damage);
            EndianSwap(*(u32*)&p->m_radius);
            break;
        }
        }
    }
    TLT_CloseFile(file);
}

// Only thrown bullets (grenades) of a cookable type.
int CBulletFactory::CanBulletTypeCook(bool projectile, int type) {
    if (projectile) {
        return 0;
    }
    return m_thrownProps[type].m_canCook != 0;
}

CSprite* CBulletFactory::GetBulletSprite(bool projectile, int type) {
    if (projectile) {
        return m_projectileProps[type].m_pSprite;
    }
    return m_thrownProps[type].m_pSprite;
}

void CBulletFactory::DestroyBullet(CBullet* bullet) {
    if (bullet->field_0x28 == 1) {
        m_projectiles.DestroyBullet((CProjectileBullet*)bullet);
    } else {
        m_thrown.DestroyBullet((CThrownBullet*)bullet);
    }
}

CBullet* CBulletFactory::CreateBullet(int type, bool projectile, CVector3 start, CVector3 dir, float speed, EClsnId clsn,
                                      float damage, ISceneNode* firedBy) {
    if (projectile) {
        return m_projectiles.CreateBullet((EProjectileBulletTypes)type, start, dir, speed, clsn, damage, firedBy);
    }
    return m_thrown.CreateBullet((EThrownBulletTypes)type, start, dir, speed, clsn, damage, firedBy);
}

void CBulletFactory::Shutdown() {
    DeleteBulletGUISprites();
    m_projectiles.Shutdown();
    m_thrown.Shutdown();
    delete[] (u8*)m_thrownProps;
    delete[] (u8*)m_projectileProps;
    m_thrownProps = NULL;
    m_projectileProps = NULL;
    m_ready = 0;
}

void CBulletFactory::Reset() {
    m_projectiles.Reset();
    m_thrown.Reset();
}

void CBulletFactory::Init() {
    LoadBulletParameters();
    LoadBulletGUISprites();
    m_projectiles.Init(m_projectileProps);
    m_thrown.Init(m_thrownProps);
    m_ready = 1;
}

CBulletFactory::~CBulletFactory() {}

CBulletFactory::CBulletFactory() {
    m_ready = 0;
    m_projectileProps = NULL;
    m_thrownProps = NULL;
}
