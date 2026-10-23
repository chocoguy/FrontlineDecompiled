// Bullet holes: each hit takes the next decal from a 512-entry pool (the
// oldest once it wraps), sizes and spins a quad on the hit surface and puts
// it in the bin of its surface's decal type. Bins draw immediate-mode quads.
#include "game/decal.h"
#include "game/MathFun.h"
#include "game/Moh2.h"
#include "game/SClsnContact.h"
#include "game/collision.h"
#include "game/matrix.h"
#include "game/renderlist.h"
#include "game/scene.h"
#include "game/surfacetype.h"
#include "game/toilet.h"
#include "game/vector.h"
#include "game/mainloop.h"
#include "math.h"
#include <dolphin/gx.h>

// One decal type in decal.crp: six resource fields.
struct DecalTypeDef {
    /* 0x00 */ int textureKind;
    /* 0x04 */ PTR32(const char) texture;
    /* 0x08 */ int widthKind;
    /* 0x0C */ float width;
    /* 0x10 */ int heightKind;
    /* 0x14 */ float height;
    /* 0x18 */ int minScaleKind;
    /* 0x1C */ float minScale;
    /* 0x20 */ int maxScaleKind;
    /* 0x24 */ float maxScale;
    /* 0x28 */ int maxSpinKind;
    /* 0x2C */ float maxSpin;  // degrees in the file, radians after Init
};
FILE_SIZE_ASSERT(DecalTypeDef, 0x30);

// The decal.crp resource's fields: a type count, then the types.
struct DecalInfo {
    /* 0x00 */ int numTypesKind;
    /* 0x04 */ int numTypes;
    /* 0x08 */ DecalTypeDef types[1];
};

#define DECAL_DEPTH_OFFSET 0.005f

static BulletDecal g_DecalList[BULLETDECAL_POOL_SIZE];
static CMatrix g_Ident;
static CRenderList* g_pRenderList;
static CScreen* g_pScreen;
static CDrawContext* g_pContext;

static inline DecalInfo* GetDecalInfo(LevelFileContentsStruct_* res) {
    return (DecalInfo*)res->fields;
}

static inline int GetSurfaceDecalIndex(const SClsnContact* contact) {
    return CSurfaceType::GetDecalIndex(contact->m_pPoly->m_surfaceType);
}

CBulletDecalManager::CBulletDecalManager() {}

CBulletDecalManager::~CBulletDecalManager() {}

void CBulletDecalManager::InitClass(CRenderList* renderList, CScreen* screen) {
    g_pRenderList = renderList;
    g_pScreen = screen;
}

// Resets the pool's quads to white with unit UVs, loads each decal type's
// texture into its bin and registers with the scene and render list.
void CBulletDecalManager::Init__NotTheVirtualOne() {
    m_pPool = g_DecalList;
    for (int i = 0; i < BULLETDECAL_POOL_SIZE; i++) {
        BulletDecalVertex* v = g_DecalList[i].m_verts;
        v[0].u = 0.0f;
        v[0].v = 0.0f;
        v[0].r = v[0].g = v[0].b = v[0].a = 0xFF;
        v[1].u = 1.0f;
        v[1].v = 0.0f;
        v[1].r = v[1].g = v[1].b = v[1].a = 0xFF;
        v[2].u = 1.0f;
        v[2].v = 1.0f;
        v[2].r = v[2].g = v[2].b = v[2].a = 0xFF;
        v[3].u = 0.0f;
        v[3].v = 1.0f;
        v[3].r = v[3].g = v[3].b = v[3].a = 0xFF;
    }

    LevelFileContentsStruct_* res = TLT_FindNextResourceByType((TLTResourceID)0x14, NULL);
    m_pDecalInfo = res;
    if (GetDecalInfo(res)->numTypes > BULLETDECAL_NUM_TYPES) {
        DebugMsg("WARNING:  WARNING:  WARNING:  WARNING:  WARNING:  WARNING:  \n");
        DebugMsg("WARNING:  The engine is only aware of %d number of decals while\n", BULLETDECAL_NUM_TYPES);
        DebugMsg("decal.crp is specifying %d.  That's wasting memory\n", GetDecalInfo(res)->numTypes);
    } else if (GetDecalInfo(res)->numTypes < BULLETDECAL_NUM_TYPES) {
        DebugMsg("WARNING:  WARNING:  WARNING:  WARNING:  WARNING:  WARNING:  \n");
        DebugMsg("WARNING:  The engine is aware of %d number of decals while\n", BULLETDECAL_NUM_TYPES);
        DebugMsg("decal.crp is specifying %d.\n", GetDecalInfo(res)->numTypes);
        DebugMsg("Either specify more in decal.crp or reduce the engine number\n");
    }

    for (int i = 0; i < BULLETDECAL_NUM_TYPES; i++) {
        DecalInfo* info = GetDecalInfo(res);
        // types the file lacks fall back to the first texture
        const char* name = i < info->numTypes ? info->types[i].texture : info->types[0].texture;
        int size;
        ShapeFile* shape = (ShapeFile*)TLT_LoadFileFromLevelBigFile(name, &size);
        CBulletDecalTextureBin& bin = m_bins[i];
        bin.m_pDecals = NULL;
        bin.m_numDecals = 0;
        bin.m_pShape = shape;
        bin.m_texture.Set(bin.m_pShape, false);

        // insert after the first child
        CRenderBin* first = m_pChildren;
        if (first != NULL) {
            bin.m_pNext = first->m_pNext;
            if (first->m_pNext != NULL) {
                first->m_pNext->m_pPrev = &bin;
            }
            bin.m_pPrev = first;
            first->m_pNext = &bin;
        } else {
            m_pChildren = &bin;
        }

        GetDecalInfo(res)->types[i].maxSpin = GetDecalInfo(res)->types[i].maxSpin * 0.017453292519943295;
    }

    m_numAdded = 0;
    g_scene.Add(*this);
    m_type = 6;
    g_pRenderList->Register(g_BulletDecalManager);
}

// Empties every bin and the pool, then rejoins the scene.
void CBulletDecalManager::Reset() {
    m_sceneLinks[1] = 0;
    m_sceneLinks[0] = 0;
    g_scene.Add(*this);
    m_numAdded = 0;
    for (CRenderBin* bin = m_pChildren; bin != NULL; bin = bin->m_pNext) {
        CBulletDecalTextureBin* decalBin = (CBulletDecalTextureBin*)bin;
        decalBin->m_pDecals = NULL;
        decalBin->m_numDecals = 0;
    }
}

// Puts a decal at the collision's first contact; once the pool is full the
// oldest decal is unlinked from its bin and reused.
void CBulletDecalManager::AddBulletDecal(const CCollision& collision, bool flip) {
    const SClsnContact* contact = collision.m_pContacts;
    if (contact == NULL) {
        return;
    }

    BulletDecal* decal;
    if (m_numAdded >= BULLETDECAL_POOL_SIZE) {
        decal = &m_pPool[m_numAdded & (BULLETDECAL_POOL_SIZE - 1)];
        CBulletDecalTextureBin& old = m_bins[decal->m_type];
        old.m_numDecals--;
        BulletDecal* d = old.m_pDecals;
        if (decal == d) {
            old.m_pDecals = NULL;
        } else {
            for (; d != NULL; d = d->m_pNext) {
                if (d->m_pNext == decal) {
                    d->m_pNext = decal->m_pNext;
                    break;
                }
            }
        }
    } else {
        decal = &m_pPool[m_numAdded];
    }
    m_numAdded++;

    InitBulletDecalFromCollision(decal, contact, flip);
    CBulletDecalTextureBin& bin = m_bins[GetSurfaceDecalIndex(contact)];
    decal->m_pNext = bin.m_pDecals;
    bin.m_pDecals = decal;
    bin.m_numDecals++;
}

static inline void TransformVertex(const CMatrix& m, BulletDecalVertex& v) {
    CVector3 out;
    CVector3 in;
    in.x = v.x;
    in.y = v.y;
    in.z = v.z;
    m.TransformPoint(out, in);
    v.x = out.x;
    v.y = out.y;
    v.z = out.z;
}

static inline CVector3 Cross(const CVector3& a, const CVector3& b) {
    CVector3 r;
    r.x = a.y * b.z - a.z * b.y;
    r.y = a.z * b.x - a.x * b.z;
    r.z = a.x * b.y - a.y * b.x;
    return r;
}

// Builds the decal's quad: a random size within its type's scale range and a
// random spin about the surface normal, lifted slightly off the surface.
// `flip` puts it on the back of the surface.
void CBulletDecalManager::InitBulletDecalFromCollision(BulletDecal* decal, const SClsnContact* contact,
                                                       bool flip) {
    decal->m_type = GetSurfaceDecalIndex(contact);
    CVector3 up = contact->m_normal;
    DecalTypeDef& type = GetDecalInfo(m_pDecalInfo)->types[decal->m_type];
    float width = type.width;
    float height = type.height;
    float minScale = type.minScale;
    float maxScale = type.maxScale;
    float maxSpin = type.maxSpin;
    if (flip) {
        up.x = up.x * -1.0f;
        up.y = up.y * -1.0f;
        up.z = up.z * -1.0f;
    }

    CVector3 front;
    CVector3 right;
    if (fabsf(up.z) < fabsf(up.x)) {
        front = Cross(up, CVector3(0.0f, 0.0f, -1.0f));
    } else {
        front = Cross(up, CVector3(-1.0f, 0.0f, 0.0f));
    }
    right = Cross(front, up);

    CMatrix m;
    CMatrix spin;
    m.SetRight(right);
    m.SetFront(front);
    m.SetUp(up);
    m.SetPos(contact->m_point);
    spin.BuildRotZ(MathFunRandomRealSigned(-maxSpin, maxSpin));
    m.Multiply(spin, m);

    float scale = MathFunRandomReal(minScale, maxScale);
    float w = width * scale;
    float h = height * scale;
    BulletDecalVertex* v = decal->m_verts;
    v[0].x = -w;
    v[0].y = h;
    v[0].z = DECAL_DEPTH_OFFSET;
    v[1].x = w;
    v[1].y = h;
    v[1].z = DECAL_DEPTH_OFFSET;
    v[2].x = w;
    v[2].y = -h;
    v[2].z = DECAL_DEPTH_OFFSET;
    v[3].x = -w;
    v[3].y = -h;
    v[3].z = DECAL_DEPTH_OFFSET;
    for (int i = 0; i < 4; i++) {
        TransformVertex(m, v[i]);
    }
}

int CBulletDecalManager::IsVisible(CDrawContext&) const {
    return 1;
}

int CBulletDecalManager::IsDrawEnabled() const {
    return 1;
}

bool CBulletDecalManager::IsUsed() {
    return m_numAdded > 0;
}

void CBulletDecalManager::Draw(CDrawContext& ctx) {
    g_pContext = &ctx;
    for (int i = 0; i < BULLETDECAL_NUM_TYPES; i++) {
        if (m_bins[i].m_numDecals > 0) {
            g_pRenderList->Render(m_bins[i], NULL);
        }
    }
}

int CBulletDecalTextureBin::Render(CDmaPacket&, void*) {
    return 0;
}

// Draws every decal in the bin as one batch of quads, depth-tested without
// depth writes.
void CBulletDecalTextureBin::Link(CDmaTag*, CDmaPacket& packet) {
    m_texture.Load(packet, NULL);
    GXSetZMode(1, 3, 0);
    BulletDecal* decal = m_pDecals;
    int count = 0;
    for (BulletDecal* d = decal; d != NULL; d = d->m_pNext) {
        count++;
    }
    GXBegin(0x80, 0, count * 4);
    for (; decal != NULL; decal = decal->m_pNext) {
        for (int i = 0; i < 4; i++) {
            BulletDecalVertex& v = decal->m_verts[i];
            GXPosition3f32(v.x, v.y, v.z);
            GXColor4u8(v.r, v.g, v.b, v.a);
            GXTexCoord2f32(v.u, v.v);
        }
    }
    GXSetZMode(1, 3, 1);
}

bool CBulletDecalTextureBin::IsUsed() {
    return m_pDecals != NULL;
}

// Vertex format and TEV setup shared by every decal bin: textured, vertex
// coloured, alpha tested and blended.
void CBulletDecalManager::Link(CDmaTag*, CDmaPacket&) {
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(0xB, 1);
    GXSetVtxDesc(0xD, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 0xB, 1, 5, 0);
    GXSetVtxAttrFmt(0, 0xD, 1, 4, 0);
    GXSetZCompLoc(1);
    GXSetAlphaCompare(6, 0, 0, 3, 0xFF);
    GXSetNumChans(1);
    GXSetNumTexGens(1);
    GXSetTexCoordGen2(0, 1, 4, 0x3C, 0, 0x7D);
    GXSetNumTevStages(1);
    GXSetBlendMode(1, 4, 5, 5);
    GXSetCullMode(0);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetTevColorIn(0, 0xF, 8, 0xA, 0xF);
    GXSetTevColorOp(0, 0, 0, 0, 1, 0);
    GXSetTevAlphaIn(0, 7, 4, 5, 7);
    GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
}
