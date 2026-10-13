#include "ea/sys/filesys.h"

// The memory callbacks default to ones that abort (FILESYS_init installs
// them if none are set).
FILESYSOPTS gFileSysOpts = {sizeof(FILESYSOPTS), NULL, NULL};
