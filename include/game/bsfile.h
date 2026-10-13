// BS script class files: loading <name>.sin / <name>.cbs from the level
// big file. Hand-maintained.
#ifndef _GAME_BSFILE_H_
#define _GAME_BSFILE_H_

#include "types.h"

struct BSClass_struct;

// Loads a class (and its base class, if any). Returns 0, or 5 when a class
// inherits code but has no base class.
int BSFileReadClass(const char* name, bool, bool, BSClass_struct** out);  // 0x80035AA8
// Finds a loaded class or loads it.
int BSFileGetClass(const char* name, BSClass_struct** out);  // 0x80036960
void BSFileDeleteClass(BSClass_struct*);  // 0x80036A30
int BSInitFile();  // 0x80036A68
int BSFileGetMemoryRequirements();  // 0x80036A70

extern BSClass_struct* g_pcClassList;  // 0x8034E7D0

#endif
