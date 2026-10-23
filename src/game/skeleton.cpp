#include "game/skeleton.h"

#include <string.h>
#include "game/matrix.h"

static void _SkelRelocateHierarchyNode(CharJointHierarchy_t* node, void* base) {
    EndianSwap(*(u32*)&node->parent);
    for (int i = 0; i < node->numChildren; i++) {
        EndianSwap(*(u32*)&node->children[i]);
    }
    node->parent = (CharJointHierarchy_t*)((u8*)base + (u32)PTR_TO_ADDR(node->parent));
    for (int i = 0; i < node->numChildren; i++) {
        node->children[i] = (CharJointHierarchy_t*)((u8*)base + (u32)PTR_TO_ADDR(node->children[i]));
    }
}

int CharTraverseSkeleton(CharJointHierarchy_t* node, void (*fn)(CharJointHierarchy_t*, void*),
                         void* data) {
    int result = -1;
    while (result < node->field_0x1) {
        fn(node, data);
        if (node->numChildren > 1) {
            for (int i = 0; i < node->numChildren; i++) {
                result = CharTraverseSkeleton(node->children[i], fn, data);
            }
        } else {
            result = node->field_0x1;
        }
        node = node->children[0];
    }
    return result;
}

void CharRelocateSkeleton(CharSkel_t* skel) {
    EndianSwap(*skel);
    if (skel->flags & CHARSKEL_RELOCATED) {
        return;
    }
    skel->joints = (CharJoint_t*)((u8*)skel + (u32)PTR_TO_ADDR(skel->joints));
    skel->root = (CharJointHierarchy_t*)((u8*)skel + (u32)PTR_TO_ADDR(skel->root));
    skel->matrices = (CharSkelMatrixEntry_t*)((u8*)skel + (u32)PTR_TO_ADDR(skel->matrices));
    for (u32 i = 0; i < skel->numMatrices; i++) {
        CharSkelMatrixEntry_t* entry = &skel->matrices[i];
        EndianSwap(entry->joint);
        EndianSwap(*(u32*)&entry->matrix);
        skel->matrices[i].matrix = (CMatrix*)((u8*)skel + (u32)PTR_TO_ADDR(skel->matrices[i].matrix));
        skel->matrices[i].matrix->EndianSwap();
    }
    CharTraverseSkeleton(skel->root, _SkelRelocateHierarchyNode, skel);
    skel->root->parent = skel->root;
    for (u32 i = 0; i < skel->numJoints; i++) {
        EndianSwap(*(u32*)&skel->joints[i].node);
        skel->joints[i].node = (CharJointHierarchy_t*)((u8*)skel + (u32)PTR_TO_ADDR(skel->joints[i].node));
    }
    skel->flags |= CHARSKEL_RELOCATED;
}

int CharSkelGetJointByName(CharSkel_t* skel, char* name) {
    int i;
    CharJoint_t* joint = skel->joints;
    for (i = 0; i < skel->numJoints; i++, joint++) {
        if (strcmp(name, joint->name) == 0) {
            break;
        }
    }
    if (i == skel->numJoints) {
        i = -1;
    }
    return i;
}
