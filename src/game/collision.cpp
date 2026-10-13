#include "game/collision.h"

#include <stdio.h>
#include "dwi/factory.h"
#include "game/IMovingSceneNode.h"
#include "game/ISceneNode.h"
#include "game/IVolume.h"
#include "game/SClsnContact.h"
#include "game/player.h"
#include "game/scene.h"

// Functions are written in reverse address order: the game was compiled with
// deferred inlining, which emits them last to first.

#define MAX_CONTACTS 300

extern const float TIME_STEP_EPSILON;  // scene.cpp

const float CONTACT_EPSILON = 0.015625f;
const float CONTACT_EPSILON_SQRD = 0.00024414063f;

static SClsnContact g_aClsnContacts[MAX_CONTACTS];
static dwi::CFactory<SClsnContact, SClsnContact*> g_contactPool("Collision Contact Pool",
                                                                MAX_CONTACTS, g_aClsnContacts);

// Copies everything but the list link.
static inline void CopyContact(SClsnContact* dst, const SClsnContact& src) {
    dst->m_depth = src.m_depth;
    dst->m_normal.Copy(src.m_normal);
    dst->m_point.Copy(src.m_point);
    dst->m_pPoly = src.m_pPoly;
    dst->m_pVolume = src.m_pVolume;
}

static inline void CopyFlags(u8& dst, u8 src) {
    dst = (dst & ~CLSN_FLAG_NEAREST) | (src & CLSN_FLAG_NEAREST);
    dst = (dst & ~CLSN_FLAG_SINGLE_TEST) | (src & CLSN_FLAG_SINGLE_TEST);
    dst = (dst & ~CLSN_FLAG_VOLUME_TYPE) | (src & CLSN_FLAG_VOLUME_TYPE);
}

CCollision::CCollision() {
    m_pNode0 = NULL;
    m_pNode1 = NULL;
    m_time = -1.0f;
    m_fraction = 1.0f;
    m_flags &= ~CLSN_FLAG_NEAREST;
    m_flags &= ~CLSN_FLAG_SINGLE_TEST;
    m_flags &= ~CLSN_FLAG_VOLUME_TYPE;
    m_result = 0;
    m_pContacts = NULL;
    m_numContacts = 0;
}

CCollision::CCollision(const CCollision& other) {
    m_pNode0 = other.m_pNode0;
    m_pNode1 = other.m_pNode1;
    m_time = other.m_time;
    m_fraction = other.m_fraction;
    CopyFlags(m_flags, other.m_flags);
    m_result = other.m_result;
    m_pContacts = NULL;
    m_numContacts = 0;
    // The copy's list comes out in reverse order.
    for (const SClsnContact* c = other.m_pContacts; c != NULL; c = c->m_pNext) {
        AddContact(*c);
    }
}

CCollision::~CCollision() {
    ClearContacts();
}

void CCollision::operator=(const CCollision& other) {
    if (&other != this) {
        // (The count is reset below rather than per contact.)
        while (m_pContacts != NULL) {
            SClsnContact* c = m_pContacts;
            g_contactPool.Free(c);
            m_pContacts = c->m_pNext;
        }
        m_pContacts = NULL;
        m_numContacts = 0;
        m_pNode0 = other.m_pNode0;
        m_pNode1 = other.m_pNode1;
        m_time = other.m_time;
        m_fraction = other.m_fraction;
        CopyFlags(m_flags, other.m_flags);
        m_result = other.m_result;
        for (const SClsnContact* c = other.m_pContacts; c != NULL; c = c->m_pNext) {
            AddContact(*c);
        }
    }
}

void CCollision::AddContact(const SClsnContact& contact) {
    SClsnContact* c = g_contactPool.Alloc();
    CopyContact(c, contact);
    c->m_pNext = m_pContacts;
    m_pContacts = c;
    m_numContacts++;
}

void CCollision::SwapOrder() {
    ISceneNode* node = m_pNode0;
    m_pNode0 = m_pNode1;
    m_pNode1 = node;
}

int CCollision::Test(float& fraction) {
    static bool isCSFWarningEnabled = true;
    static const ISceneNode::EVolumeType volumeTypes[3] = {
        (ISceneNode::EVolumeType)0, (ISceneNode::EVolumeType)1, (ISceneNode::EVolumeType)2};

    ISceneNode::EVolumeType type = volumeTypes[(m_flags & CLSN_FLAG_VOLUME_TYPE) >> 4];
    const IVolume* volume0 = m_pNode0->GetWorldBoundingVolume(type);
    const IVolume* volume1 = m_pNode1->GetWorldBoundingVolume(type);
    int worst = 0;

    if (m_flags & CLSN_FLAG_SINGLE_TEST) {
        worst = volume0->TestCollision(*volume1, *this, false);
        return worst;
    }

    float startTime = m_time;
    float hi = startTime;
    float lo = 0.0f;
    static int MAX_NUM_LOOPS = (int)(1.0f / TIME_STEP_EPSILON) + 1;
    int maxLoops = MAX_NUM_LOOPS;

    int i;
    for (i = 1; i <= maxLoops; i++) {
        if (m_time < TIME_STEP_EPSILON) {
            break;
        }
        int result = volume0->TestCollision(*volume1, *this, i == 1);
        if (result > worst) {
            worst = result;
        }
        switch (result) {
        case CLSN_TOUCHING:
            fraction = m_time / startTime;
            if (m_flags & CLSN_FLAG_NEAREST) {
                m_time *= m_fraction;
            }
            isCSFWarningEnabled = true;
            return worst;
        case CLSN_NONE:
            lo = m_time;
            m_flags &= ~CLSN_FLAG_NEAREST;
            if (i == 1) {
                isCSFWarningEnabled = true;
                return worst;
            }
            break;
        case CLSN_PENETRATING:
            hi = m_time;
            ClearContacts();
            break;
        }
        m_time = 0.5f * (lo + hi);
        m_pNode0->AttemptUpdate(m_time);
        volume0 = m_pNode0->GetWorldBoundingVolume(type);
        m_pNode1->AttemptUpdate(m_time);
        volume1 = m_pNode1->GetWorldBoundingVolume(type);
    }

    // No contact time found: stop both nodes where they were and stop them
    // colliding with each other for a while.
    if (isCSFWarningEnabled) {
        char msg[256];
        sprintf(msg, "\n\tContact search failed (inner loop): %d of %d iterations", i, maxLoops);
        isCSFWarningEnabled = false;
    }
    IMovingSceneNode* moving = m_pNode0->AsMovingNode();
    if (moving != NULL) {
        moving->Halt();
    }
    m_pNode0->AttemptUpdate(0.0f);
    m_pNode0->CommitUpdate();
    moving = m_pNode1->AsMovingNode();
    if (moving != NULL) {
        moving->Halt();
    }
    m_pNode1->AttemptUpdate(0.0f);
    m_pNode1->CommitUpdate();
    g_scene.ExcludeCollision(*m_pNode0, *m_pNode1, 10);
    CPlayerObject* player0 = m_pNode0->AsPlayerObject();
    CPlayerObject* player1 = m_pNode1->AsPlayerObject();
    if (player0 != NULL) {
        player0->m_gravity = 0;
    }
    if (player1 != NULL) {
        player1->m_gravity = 0;
    }
    m_result = 0;
    m_fraction = 1.0f;
    ClearContacts();
    return CLSN_NONE;
}

void CCollision::ClearContacts() {
    while (m_pContacts != NULL) {
        SClsnContact* c = m_pContacts;
        g_contactPool.Free(c);
        m_pContacts = c->m_pNext;
        m_numContacts--;
    }
}
