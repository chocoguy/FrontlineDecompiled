// Creation, destruction and event delivery for BS script objects.
// Hand-maintained.
#ifndef _GAME_BSOBJECT_H_
#define _GAME_BSOBJECT_H_

#include "types.h"
#include "game/bstypes.h"

#define BSEVENT_START 0x2C

// Delivers `event` to every thread of `obj` that declares a handler for it.
// Returns whether any thread handled it.
bool BSObjectTriggerEvent(BSObject* obj, unsigned short event, void* data, BSObject* sender,
                          bool immediate);  // 0x8003A46C
int DestroyBSObject(BSObject* obj);  // 0x8003A59C
// Returns 0, 0xD if the class isn't found, or BSCreateThread's error.
int CreateBSObject(const char* className, BSObject** out, BSGO_Basic* gameObject,
                   TriggerObject_struct* trigger, void** outAlias);  // 0x8003A630
int BSInitBSObject();  // 0x8003A77C
int BSObjectGetMemoryRequirements();  // 0x8003A7D8

#endif
