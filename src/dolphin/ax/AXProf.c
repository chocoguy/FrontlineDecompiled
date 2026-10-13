#include <dolphin/ax.h>

static AXPROFILE* __AXProfile;
static u32 __AXMaxProfiles;
static u32 __AXCurrentProfile;
static u32 __AXProfileInitialized;

AXPROFILE* __AXGetCurrentProfile(void) {
    AXPROFILE* profile;
    if (__AXProfileInitialized) {
        profile = &__AXProfile[__AXCurrentProfile];
        __AXCurrentProfile++;
        __AXCurrentProfile %= __AXMaxProfiles;
        return profile;
    }
    return NULL;
}
