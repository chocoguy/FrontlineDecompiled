// The world object: the level geometry as a scene node. Up to four
// compartments are resident; Acquire() names the ones wanted and the data
// streamer reads each missing compartment's .cpt and .cdb into a cache block
// asynchronously, reusing the block of a compartment no longer wanted.
// Also the effects of bullets hitting the world (sparks, decals, sounds,
// breakable texture swaps).
#include "game/world_object.h"

#include "game/compartment.h"
#include "game/cdbobject.h"
#include "game/collision.h"
#include "game/SClsnContact.h"
#include "game/bullet.h"
#include "game/AIDoodad.h"
#include "game/particlesystem.h"
#include "game/decal.h"
#include "game/mainloop.h"
#include "game/surfacetype.h"
#include "game/sound.h"
#include "game/bsbifunc.h"
#include "game/toilet.h"
#include "game/matrix.h"
#include "game/memory.h"
#include "ea/real.h"
#include "math.h"

#define CACHE_MEMORY 0x900000   // compartment cache plus the shared compartment
#define NUM_CACHE_BLOCKS 4
#define READ_CHUNK 524288.0f   // bytes read per frame (scaled by the frame time)

// A .cpt or .cdb file's size, its second word. The headers are big-endian
// until CCompartment::Init swaps them natively.
#ifdef FRONTLINE_NATIVE
#define FILE_SIZE_WORD(header) ((int)__builtin_bswap32(((u32*)(header))[1]))
#else
#define FILE_SIZE_WORD(header) (((int*)(header))[1])
#endif

// Streaming states (SRequest::m_state).
enum {
    STREAM_GET_BLOCK,
    STREAM_FREE_OLD,
    STREAM_OPEN_CPT,
    STREAM_READ_CPT,
    STREAM_WAIT_CPT,
    STREAM_OPEN_CDB,
    STREAM_READ_CDB,
    STREAM_WAIT_CDB,
    STREAM_DONE
};

CFilenameTable g_CptFnameTable;
CFilenameTable g_CdbFnameTable;

void (*CWorldObject::GetCptFilename)(int, char*, int) = GetCptFilenameFromBigFile;
void (*CWorldObject::GetCdbFilename)(int, char*, int) = GetCdbFilenameFromBigFile;
int CWorldObject::m_sCompartmentSize;

void GetCptFilenameFromBigFile(int index, char* name, int size) {
    GetCompartmentCPTNameFromIndex(index, name, size);
}

void GetCdbFilenameFromBigFile(int index, char* name, int size) {
    GetCompartmentCDBNameFromIndex(index, name, size);
}

// Moves the list into a new pool of `capacity` nodes.
void SCompartmentList::reserve(int capacity, char* name) {
    if (capacity <= m_capacity) {
        return;
    }
    SCompartmentNode* pool =
        (SCompartmentNode*)DWI_allocalign(name, capacity * sizeof(SCompartmentNode), 0x10, 0x400);
    SCompartmentNode* next = NULL;
    for (SCompartmentNode* n = &pool[capacity - 1]; n != pool; n--) {
        n->next = next;
        next = n;
    }
    pool->next = next;

    SCompartmentNode* oldPool = m_pool;
    bool ownedOld = m_ownsPool;
    SCompartmentNode* first = m_head.next;
    m_capacity = capacity;
    m_pool = pool;
    m_free = pool;
    m_ownsPool = true;
    SCompartmentNode* tail = &m_head;
    int size = 0;
    for (SCompartmentNode* n = first; n != &m_head;) {
        SCompartmentNode* following = n->next;
        SCompartmentNode* copy = m_free;
        m_free = copy->next;
        copy->prev = NULL;
        copy->next = NULL;
        new (&copy->value) CCompartment(n->value);
        n->value.~CCompartment();
        tail->next = copy;
        copy->prev = tail;
        tail = copy;
        size++;
        n = following;
    }
    tail->next = &m_head;
    m_head.prev = tail;
    m_size = size;
    if (ownedOld) {
        MEM_free(oldPool);
    }
}

// Sizes the cache blocks to the largest compartment (at most a quarter of
// what the shared compartment leaves of CACHE_MEMORY).
CWorldObject::CWorldObject() : m_volume(4) {
    m_cache.m_memory = NULL;
    m_cache.m_free = NULL;
    m_cache.m_blockSize = 0;
    m_cache.m_numBlocks = 0;
    m_cache.m_numUsed = 0;
    m_cache.m_ownsMemory = false;
    m_drawEnabled = true;
    field_0x164 = 0;
    m_compartments.reserve(4, "World Object Cache");
    m_requests.reserve(4, "World Object Requests");
    CCompartment::LoadShared();
    m_sCompartmentSize = (CACHE_MEMORY - CCompartment::GetSharedSize()) / NUM_CACHE_BLOCKS;
    int largest = 0;
    int i = 0;
    for (;;) {
        int size = GetCompartmentSizeFromIndex(i);
        if (size > largest) {
            largest = size;
        } else if (size < 0) {
            break;
        }
        i++;
    }
    if (largest < m_sCompartmentSize) {
        m_sCompartmentSize = largest;
    }
    m_sCompartmentSize += 0x80;
    int blockSize = m_sCompartmentSize;
    m_cache.m_memory = (u8*)DWI_allocalign(NULL, blockSize * NUM_CACHE_BLOCKS, 0x10, 0x400);
    m_cache.m_free = NULL;
    m_cache.m_blockSize = blockSize;
    m_cache.m_numBlocks = NUM_CACHE_BLOCKS;
    m_cache.m_numUsed = 0;
    m_cache.m_ownsMemory = true;
    for (int b = NUM_CACHE_BLOCKS - 1; b >= 0; b--) {
        void* block = m_cache.m_memory + m_cache.m_blockSize * b;
        *(void**)block = m_cache.m_free;
        m_cache.m_free = block;
    }
}

CWorldObject::~CWorldObject() {}

static inline bool InSet(const CWorldObject::SAcquireSet& set, int index) {
    bool missing = true;
    for (int i = 0; i < 4 && missing; i++) {
        missing = set.m_ids[i] != index;
    }
    return !missing;
}

// Cancels requests for compartments no longer wanted, marks the resident
// ones not in the set for eviction and requests the missing ones.
void CWorldObject::Acquire(const CWorldObject::SAcquireSet& set, bool resetProgress) {
    if (resetProgress) {
        m_loadProgress = 0.0f;
        m_loadTotal = 0;
    }
    dwi::list<SRequest>::node* n = m_requests.begin();
    while (n != m_requests.end()) {
        dwi::list<SRequest>::node* req = n;
        n = n->next;
        if (!InSet(set, req->value.m_index)) {
            ASYNCFILE_cancel(req->value.m_read);
            FILESYS_closesync(req->value.m_file, 100);
            char* block = req->value.m_block;
            if (block != NULL) {
                m_cache.m_numUsed--;
                *(void**)block = m_cache.m_free;
                m_cache.m_free = block;
            }
            m_requests.erase(req);
        }
    }
    for (SCompartmentNode* c = m_compartments.begin(); c != m_compartments.end(); c = c->next) {
        c->value.m_evict = !InSet(set, c->value.m_index);
    }
    for (int i = 0; i < 4; i++) {
        int index = set.m_ids[i];
        if (GetCompartment(index) != NULL) {
            continue;
        }
        bool pending = false;
        for (dwi::list<SRequest>::node* r = m_requests.begin(); r != m_requests.end(); r = r->next) {
            if (index == r->value.m_index) {
                pending = true;
                break;
            }
        }
        if (!pending) {
            SRequest request;
            request.m_index = index;
            request.m_state = STREAM_GET_BLOCK;
            request.m_file = 0;
            request.m_read = 0;
            request.m_reused = false;
            request.m_block = NULL;
            request.m_pos = NULL;
            request.m_offset = 0;
            request.m_cpt = NULL;
            request.m_cdb = NULL;
            m_requests.push_back(request);
            m_loadTotal++;
        }
    }
}

CCompartment* CWorldObject::GetCompartment(int index) {
    for (SCompartmentNode* c = m_compartments.begin(); c != m_compartments.end(); c = c->next) {
        if (index == c->value.m_index) {
            return &c->value;
        }
    }
    return NULL;
}

void CWorldObject::HandleExplosion(const CExplosion& explosion) {
    for (SCompartmentNode* c = m_compartments.begin(); c != m_compartments.end(); c = c->next) {
        c->value.HandleExplosion(explosion);
    }
}

// Advances the first request until it has to wait for the disc. Each
// compartment's file starts with a header whose second word is its size.
void CWorldObject::ServiceDataStreamer(float dt) {
    if (dt < 1.0f) {
        dt = 1.0f;
    }
    bool yield = false;
    int chunk = (int)(READ_CHUNK * dt);
    while (!yield && m_requests.m_size != 0) {
        SRequest& req = m_requests.begin()->value;
        switch (req.m_state) {
        case STREAM_GET_BLOCK:
            if (m_cache.m_numUsed < m_cache.m_numBlocks) {
                char* block = (char*)m_cache.m_free;
                m_cache.m_free = *(void**)block;
                m_cache.m_numUsed++;
                req.m_block = block;
            } else {
                // Take the block of a compartment no longer wanted (like
                // the original, assumes there is one).
                SCompartmentNode* c = m_compartments.begin();
                while (c != m_compartments.end() && !c->value.m_evict) {
                    c = c->next;
                }
                CCompartment& old = c->value;
                req.m_block = old.m_name;
                req.m_reused = true;
                old.m_drawEnabled = false;
                m_volume.RemoveSubVolume(*(const CCDBObject*)old.GetLocalBoundingVolume((ISceneNode::EVolumeType)0));
                yield = true;
            }
            req.m_pos = req.m_block;
            req.m_state = STREAM_FREE_OLD;
            break;
        case STREAM_FREE_OLD:
            if (req.m_reused) {
                for (SCompartmentNode* c = m_compartments.begin(); c != m_compartments.end(); c = c->next) {
                    if (c->value.m_name == req.m_block) {
                        m_compartments.erase(c);
                        break;
                    }
                }
            }
            req.m_state = STREAM_OPEN_CPT;
            break;
        case STREAM_OPEN_CPT: {
            char name[0x80];
            req.m_pos = (char*)(((uptr)(req.m_pos) + 0x7F) & ~0x7F);
            req.m_offset = 0;
            req.m_cpt = (CPTHeader*)req.m_pos;
            req.m_state = STREAM_READ_CPT;
            GetCptFilename(req.m_index, name, sizeof(name));
            FILESYS_opensync(name, 0x21, 100, &req.m_file);
            break;
        }
        case STREAM_READ_CPT:
            req.m_read = ASYNCFILE_read(req.m_file, req.m_offset, req.m_pos, chunk);
            yield = true;
            req.m_state = STREAM_WAIT_CPT;
            break;
        case STREAM_WAIT_CPT:
            if (ASYNCFILE_getstatus(req.m_read) == 1) {
                int bytes = 0;
                ASYNCFILE_release(req.m_read, NULL, &bytes);
                req.m_pos += bytes;
                req.m_offset += bytes;
                int size = FILE_SIZE_WORD(req.m_cpt);
                m_loadProgress += (float)bytes / (float)size;
                if ((int)(req.m_pos - (char*)req.m_cpt) < FILE_SIZE_WORD(req.m_cpt)) {
                    req.m_state = STREAM_READ_CPT;
                } else {
                    FILESYS_closesync(req.m_file, 100);
                    req.m_state = STREAM_OPEN_CDB;
                }
            } else {
                yield = true;
            }
            break;
        case STREAM_OPEN_CDB: {
            char name[0x80];
            req.m_pos = (char*)(((uptr)(req.m_pos) + 0xF) & ~0xF);
            req.m_offset = 0;
            req.m_cdb = (CDB*)req.m_pos;
            req.m_state = STREAM_READ_CDB;
            GetCdbFilename(req.m_index, name, sizeof(name));
            FILESYS_opensync(name, 0x21, 100, &req.m_file);
            break;
        }
        case STREAM_READ_CDB:
            req.m_read = ASYNCFILE_read(req.m_file, req.m_offset, req.m_pos, chunk);
            yield = true;
            req.m_state = STREAM_WAIT_CDB;
            break;
        case STREAM_WAIT_CDB:
            if (ASYNCFILE_getstatus(req.m_read) == 1) {
                int status = 0;
                int bytes = 0;
                ASYNCFILE_release(req.m_read, &status, &bytes);
                req.m_pos += bytes;
                req.m_offset += bytes;
                int size = FILE_SIZE_WORD(req.m_cdb);
                m_loadProgress += (float)bytes / (float)size;
                if ((int)(req.m_pos - (char*)req.m_cdb) < FILE_SIZE_WORD(req.m_cdb)) {
                    req.m_state = STREAM_READ_CDB;
                } else {
                    FILESYS_closesync(req.m_file, 100);
                    req.m_state = STREAM_DONE;
                }
            } else {
                yield = true;
            }
            break;
        case STREAM_DONE: {
            CCompartment blank;
            m_compartments.push_back(blank);
            CCompartment& c = m_compartments.m_head.prev->value;
            c.Init(req.m_index, req.m_block, req.m_cpt, req.m_cdb);
            m_volume.AddSubVolume(*(const CCDBObject*)c.GetLocalBoundingVolume((ISceneNode::EVolumeType)0));
            yield = true;
            m_requests.erase(m_requests.begin());
            break;
        }
        }
    }
}

// Breakable textures swap (and stop colliding) when shot; otherwise sparks,
// a bullet hole, the surface's impact sound and a noise the AI can hear.
// Shoot-through surfaces let the bullet continue.
void CWorldObject::HandleBulletCollision(CBullet* bullet, const CCollision& clsn) {
    bool collide = true;
    const SClsnContact* contact = clsn.m_pContacts;
    const SClsnPoly* poly = contact->m_pPoly;
    if (poly->GetTexSwap() >= 0) {
        for (SCompartmentNode* c = m_compartments.begin(); c != m_compartments.end(); c = c->next) {
            if (contact->m_pVolume == c->value.GetLocalBoundingVolume((ISceneNode::EVolumeType)0)) {
                collide = (u8)c->value.TextureSwapCollisionDisabled(poly->GetTexSwap()) == 0;
                if (collide) {
                    c->value.TextureSwap(poly->GetTexSwap());
                }
                break;
            }
        }
    }
    if (!collide) {
        bullet->Penetrate(clsn);
        return;
    }

    // Effects face the bullet's direction reflected off the surface.
    CMatrix m;
    m.Ident();
    CVector3 dir;
    bullet->GetForward(dir);
    float d = contact->m_normal.z * dir.z + (contact->m_normal.x * dir.x + contact->m_normal.y * dir.y);
    float k = -2.0f * d;
    CVector3 up;
    up.x = k * contact->m_normal.x;
    up.y = k * contact->m_normal.y;
    up.z = k * contact->m_normal.z;
    up.x = up.x + dir.x;
    up.y = up.y + dir.y;
    up.z = up.z + dir.z;
    up.Normalize();
    bool grazing = false;
    if (d > 0.0001f) {
        grazing = true;
    }
    CVector3 front;
    CVector3 right;
    CVector3 x(1.0f, 0.0f, 0.0f);
    if ((float)fabs(up.z * x.z + (up.x * x.x + up.y * x.y)) <= 0.9999f) {
        front.Cross(up, x);
        front.Normalize();
        right.Cross(front, up);
    } else {
        CVector3 z(0.0f, 0.0f, 1.0f);
        right.Cross(up, z);
        right.Normalize();
        front.Cross(up, right);
    }
    m.SetRight(right);
    m.SetFront(front);
    m.SetUp(up);
    m.SetPos(contact->m_point);

    bool projectile = bullet->AsProjectile() != NULL;
    int surface = poly->m_surfaceType;
    if ((s16)surface >= 0) {
        u8 shootThru = CSurfaceType::IsShootThru((s16)surface);
        if (projectile || shootThru) {
            u32 crc = CSurfaceType::GetParticleCRC(poly->m_surfaceType);
            if (crc != 0) {
                CParticleSystem* ps = CParticleSystem::Create(crc);
                if (ps != NULL) {
                    ps->SetLocalToWorld(m);
                }
            }
        }
        if (projectile && CSurfaceType::GetDecalIndex(poly->m_surfaceType) != -1) {
            g_BulletDecalManager.AddBulletDecal(clsn, grazing);
        }
        int sound = CSurfaceType::GetSoundIndex(poly->m_surfaceType);
        if (sound != 0) {
            PlayBackgroundImpactSound(bullet, sound);
            CAIFilterObject* filter = NULL;
            if (bullet->GetFiredBy() != NULL) {
                filter = bullet->GetFiredBy()->GetAIDoodad()->m_pFilter;
            }
            MakeNoise(NULL, filter, contact->m_point, 0x34, 0.6f, false, 2);
        }
        if (shootThru) {
            bullet->Penetrate(clsn);
        }
    } else {
        if (projectile) {
            CParticleSystem* ps = CParticleSystem::Create(0xE9AC3F2E);
            if (ps != NULL) {
                ps->SetLocalToWorld(m);
            }
        }
        PlayBackgroundImpactSound(bullet, 1);
        CAIFilterObject* filter = NULL;
        if (bullet->GetFiredBy() != NULL) {
            filter = bullet->GetFiredBy()->GetAIDoodad()->m_pFilter;
        }
        MakeNoise(NULL, filter, contact->m_point, 0x34, 0.6f, false, 2);
    }
}

int CWorldObject::BeginUpdate(float dt) {
    if (m_requests.m_size != 0) {
        ServiceDataStreamer(dt);
    }
    CVector3 min;
    CVector3 max;
    m_volume.GetExtents(min, max);
    SetExtents(min, max);
    END_RETURN_NOTHING
}

void CWorldObject::Draw(CDrawContext& ctx) {
    for (SCompartmentNode* c = m_compartments.begin(); c != m_compartments.end(); c = c->next) {
        if (c->value.IsDrawEnabled() && c->value.IsVisible(ctx)) {
            c->value.Draw(ctx);
        }
    }
}

void CWorldObject::DrawShadows(CDrawContext& ctx) {
    for (SCompartmentNode* c = m_compartments.begin(); c != m_compartments.end(); c = c->next) {
        if (c->value.IsDrawEnabled() && c->value.IsVisible(ctx)) {
            c->value.DrawShadows(ctx);
        }
    }
}

const IVolume* CWorldObject::GetLocalBoundingVolume(ISceneNode::EVolumeType) const {
    return &m_volume;
}

const IVolume* CWorldObject::GetWorldBoundingVolume(ISceneNode::EVolumeType) const {
    return &m_volume;
}

int CWorldObject::IsVisible(CDrawContext&) const {
    return 1;
}

int CWorldObject::IsDrawEnabled() const {
    return m_drawEnabled;
}

int CWorldObject::GetCollisionId() const {
    return 0;
}
