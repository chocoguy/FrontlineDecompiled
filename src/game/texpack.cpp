#include "game/texpack.h"

#include <stdlib.h>
#include <string.h>
#include "game/Moh2.h"
#include "game/offsetptr.h"
#include "game/texture.h"

static int _tex_compare(const char* a, const char* b) {
    return stricmp(a, b);
}

// Called on a pack in place: fix up the offsets and set up each texture.
CTexturePack::CTexturePack() {
    offsetPtr(m_names, (int)this);
    offsetPtr(m_textures, (int)this);
    for (u32 i = 0; i < m_numTextures; i++) {
        offsetPtr(m_textures[i], (int)this);
        if (m_textures[i]->m_pData != NULL) {
            offsetPtr(m_textures[i]->m_pData, (int)this);
        }
        m_textures[i]->Set((ShapeFile*)m_textures[i]->m_pData, false);
    }
}

CTexture* CTexturePack::FindTexture(char* name) {
    char(*found)[16] = (char(*)[16])bsearch(name, m_names, m_numTextures, sizeof(*m_names),
                                            (int (*)(const void*, const void*))_tex_compare);
    if (found != NULL) {
        return m_textures[found - m_names];
    }
    DebugMsg("WARNING! Could not find \"%s\" in texture pack.\n", name);
    return NULL;
}
