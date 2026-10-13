// dmgeom: per-object node variants of DM meshes and the part list.
#include "game/dmgeom.h"
#include "game/dmesh.h"

// Lists the parts of each node's chosen variant (a part index of 0xFFFF
// still takes a slot, as the original). Returns the count.
int DMGeomBuildPartList(DMGeomPartData_T* parts, CDMesh* mesh, DMGeometryState_T* state) {
    int count = 0;
    if (state != NULL) {
        for (u32 n = 0; n < mesh->m_numNodes; n++) {
            u32 variant = state->m_variant[n];
            if (variant == 0xFF) {
                continue;
            }
            CDMeshNode& node = mesh->m_nodes[n];
            if (variant >= node.m_numVariants) {
                continue;
            }
            CDMeshVariant& v = mesh->m_variants[variant + node.m_firstVariant];
            u16* index = &mesh->m_partIndices[v.m_firstPart];
            for (int i = v.m_numParts; i > 0; i--) {
                if (*index != 0xFFFF) {
                    parts->m_part = *index;
                    parts->m_listSize = 0;
                    parts->m_list = NULL;
                }
                index++;
                parts++;
                count++;
            }
        }
    }
    return count;
}

void DMGeomResetState(DMGeometryState_T* state) {
    for (int i = 0; i < DMGEOM_MAX_NODES; i++) {
        state->m_variant[i] = 0xFF;
        state->m_dirty |= 1 << i;
    }
}

void DMGeomSetNodeState(DMGeometryState_T* state, long node, long variant) {
    if (state->m_variant[node] != variant) {
        state->m_variant[node] = variant;
        state->m_dirty |= 1 << node;
    }
}
