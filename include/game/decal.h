// Bullet holes (hand-maintained). A fixed pool of 512 quads, recycled oldest
// first, sorted into one textured render bin per decal type; the decal types
// (texture, size, scale range, max spin) come from the level's decal.crp
// resource.
#ifndef _GAME_DECAL_H_
#define _GAME_DECAL_H_

#include "types.h"
#include "game_types.h"
#include "game/ISceneNode.h"
#include "game/compartment.h"
#include "game/texture.h"
#include "game/BulletDecal.h"

class CCollision;
struct CDmaPacket;
struct CDmaTag;
class CDrawContext;
class CRenderList;
class CScreen;
class SClsnContact;
struct LevelFileContentsStruct_;
struct ShapeFile;

#define BULLETDECAL_NUM_TYPES 13
#define BULLETDECAL_POOL_SIZE 0x200

// One decal type's quads, drawn with its texture. size 0x6C; vtable 0x80184F8C
class CBulletDecalTextureBin : public CUcodeRenderBin {
public:
    virtual bool IsUsed();  // 0x800831C4
    virtual void Link(CDmaTag*, CDmaPacket&);  // 0x800831D8
    virtual int Render(CDmaPacket&, void*);  // 0x80083394
    virtual ~CBulletDecalTextureBin() {}  // 0x80083E08
    CBulletDecalTextureBin() { m_pDecals = NULL; }  // 0x80083E74

    /* 0x20 */ int m_numDecals;
    /* 0x24 */ BulletDecal* m_pDecals;
    /* 0x28 */ CTexture m_texture;
    /* 0x68 */ ShapeFile* m_pShape;
};

// size 0x5CC; vtable 0x80184E70
class CBulletDecalManager : public CSceneNodeData, public CUcodeRenderBin {
public:
    virtual void Draw(CDrawContext&);  // 0x8008339C
    virtual bool IsUsed();  // 0x80083400
    virtual void Link(CDmaTag*, CDmaPacket&);  // 0x8008304C
    virtual int IsDrawEnabled() const;  // 0x80083414
    virtual int IsVisible(CDrawContext&) const;  // 0x8008341C
    void InitBulletDecalFromCollision(BulletDecal*, const SClsnContact*, bool);  // 0x80083424
    void AddBulletDecal(const CCollision&, bool);  // 0x8008385C
    void Reset();  // 0x8008397C
    void Init__NotTheVirtualOne();  // 0x800839E4
    static void InitClass(CRenderList*, CScreen*);  // 0x80083C44
    virtual ~CBulletDecalManager();  // 0x80083C50
    CBulletDecalManager();  // 0x80083D10

    /* 0x044 */ CBulletDecalTextureBin m_bins[BULLETDECAL_NUM_TYPES];
    /* 0x5C0 */ BulletDecal* m_pPool;
    /* 0x5C4 */ int m_numAdded;  // total ever added; the pool index wraps
    /* 0x5C8 */ LevelFileContentsStruct_* m_pDecalInfo;
};

#endif
