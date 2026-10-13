// CBulletFactory: the level's bullet parameters (by difficulty) and pools
// of 64 projectile and 64 thrown bullets. Hand-maintained.
#ifndef _GAME_BULLETFACTORY_H_
#define _GAME_BULLETFACTORY_H_

#include "types.h"
#include "game_types.h"
#include "game/vector.h"

class CBullet;
class CBulletFactory;
class CSprite;
class ISceneNode;

// One projectile bullet type's parameters (size 0x40; little-endian in the
// file).
struct ProjectileBulletProperties_struct {
    /* 0x00 */ f32 m_weight;
    /* 0x04 */ u8 unk_0x4[0x14];
    /* 0x18 */ char m_spriteName[0x14];  // the ammo icon
    /* 0x2C */ CSprite* m_pSprite;
    /* 0x30 */ f32 m_damage;
    /* 0x34 */ s16 m_lifetime;  // time to live
    /* 0x36 */ u8 unk_0x36[0x2];
    /* 0x38 */ f32 m_tracerWidth;  // 0: no tracer
    /* 0x3C */ u8 unk_0x3C[0x4];
};

// One thrown bullet type's parameters (size 0x4C).
struct ThrownBulletProperties_struct {
    /* 0x00 */ f32 m_weight;
    /* 0x04 */ u8 unk_0x4[0x14];
    /* 0x18 */ char m_spriteName[0x14];
    /* 0x2C */ CSprite* m_pSprite;
    /* 0x30 */ u32 field_0x30;
    /* 0x34 */ u32 field_0x34;
    /* 0x38 */ f32 m_friction;  // sliding loss on floors (scaled by how flat they are)
    /* 0x3C */ f32 m_bounce;  // bounce restitution
    /* 0x40 */ f32 m_damage;
    /* 0x44 */ f32 m_radius;  // blast radius
    /* 0x48 */ s16 m_lifetime;  // fuse
    /* 0x4A */ s8 m_timed;  // explodes when its fuse runs out (else a dud)
    /* 0x4B */ s8 m_canCook;
};

enum {
    NUM_PROJECTILE_BULLET_TYPES = 0x1F,
    NUM_THROWN_BULLET_TYPES = 0xB,
    BULLET_POOL_SIZE = 0x40,
};

// A pool of bullets of class T, linked through CBullet::m_next (inline
// helper; size 0x14).
template <class T, class P, class E>
class CBulletFactoryHelper {
public:
    CBulletFactoryHelper() {
        m_props = NULL;
        m_pool = NULL;
        m_free = NULL;
        m_active = NULL;
        m_ready = 0;
    }
    ~CBulletFactoryHelper() {}

    void Init(P* props) {
        m_props = props;
        Build();
        m_ready = 1;
    }
    // Rebuilds the pool (every bullet free).
    void Reset() {
        delete[] m_pool;
        Build();
    }
    void Shutdown() {
        delete[] m_pool;
        m_pool = NULL;
        m_ready = 0;
    }
    // Takes a free bullet (NULL when all are in use).
    T* CreateBullet(E type, CVector3 start, CVector3 dir, float speed, EClsnId clsn, float damage, ISceneNode* firedBy) {
        T* bullet = m_free;
        if (bullet != NULL) {
            m_free = (T*)bullet->m_next;
            bullet->m_next = m_active;
            m_active = bullet;
            bullet->Init(type, &m_props[type], start, dir, speed, clsn, damage, firedBy);
        }
        return bullet;
    }
    void DestroyBullet(T* bullet) {
        bullet->Shutdown();
        CBullet** link = (CBullet**)&m_active;
        T* b = m_active;
        while (b != bullet) {
            link = &b->m_next;
            b = (T*)b->m_next;
        }
        *link = bullet->m_next;
        bullet->m_next = m_free;
        m_free = bullet;
    }

    /* 0x0 */ T* m_pool;
    /* 0x4 */ T* m_free;
    /* 0x8 */ T* m_active;
    /* 0xC */ P* m_props;
    /* 0x10 */ u8 m_ready;

private:
    void Build() {
        m_pool = new T[BULLET_POOL_SIZE];
        m_free = m_pool;
        m_active = NULL;
        for (int i = 0; i < BULLET_POOL_SIZE - 1; i++) {
            m_pool[i].m_next = &m_pool[i + 1];
        }
        m_pool[BULLET_POOL_SIZE - 1].m_next = NULL;
    }
};

class CProjectileBullet;
class CThrownBullet;

// size 0x34
class CBulletFactory {
public:
    /* 0x0 */ ProjectileBulletProperties_struct* m_projectileProps;  // [NUM_PROJECTILE_BULLET_TYPES]
    /* 0x4 */ ThrownBulletProperties_struct* m_thrownProps;  // [NUM_THROWN_BULLET_TYPES]
    /* 0x8 */ CBulletFactoryHelper<CProjectileBullet, ProjectileBulletProperties_struct, EProjectileBulletTypes> m_projectiles;
    /* 0x1C */ CBulletFactoryHelper<CThrownBullet, ThrownBulletProperties_struct, EThrownBulletTypes> m_thrown;
    /* 0x30 */ u8 m_ready;
    /* 0x31 */ u8 unk_0x31[0x3];
    float GetBulletWeight(bool, int);  // 0x800CCDA8
    void DeleteBulletGUISprites();  // 0x800CCDD0
    void LoadBulletGUISprites();  // 0x800CCF68
    void LoadBulletParameters();  // 0x800CD15C
    int CanBulletTypeCook(bool, int);  // 0x800CD7AC
    CSprite* GetBulletSprite(bool, int);  // 0x800CD7E0
    void DestroyBullet(CBullet*);  // 0x800CD810
    CBullet* CreateBullet(int, bool, CVector3, CVector3, float, EClsnId, float, ISceneNode*);  // 0x800CD84C
    void Shutdown();  // 0x800CD8E8
    void Reset();  // 0x800CD944
    void Init();  // 0x800CD97C
    ~CBulletFactory();  // 0x800CD9D0
    CBulletFactory();  // 0x800CDA34
};

extern CBulletFactory g_BulletFactory;  // mainloop.cpp
extern CBulletFactory* g_pBulletFactory;

#endif
