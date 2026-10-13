// BS script messages: threads register handlers for message IDs; sending a
// message queues a call to every matching handler, run by BSProcessMessages.
// Hand-maintained.
#ifndef _GAME_BSMESSAGE_H_
#define _GAME_BSMESSAGE_H_

#include "types.h"

struct BSCode_MessageHandlerEntry_struct;
struct BSMachineThread_struct;
struct BSMessageRegistration_struct;
struct BSObject;
struct TriggerObject_struct;

// Whether a and b share a group in category (category >> 16).
bool BSMessageIsGroupCategoryMember(TriggerObject_struct* a, TriggerObject_struct* b,
                                    unsigned int category);  // 0x80039B84
// Runs up to `max` queued messages.
int BSProcessMessages(int max);  // 0x80039CA0
void BSMessageRemoveHandler(BSMessageRegistration_struct*);  // 0x80039EB0
// sender (or, without one, trigger) sends `message`, optionally to `target`.
int BSSendMessage(unsigned short message, BSObject* sender, TriggerObject_struct* trigger,
                  BSObject* target, void* data);  // 0x80039F10
// Registers a handler for `thread`; *replaced gets the registration it supersedes.
BSMessageRegistration_struct* BSRegisterMessage(BSCode_MessageHandlerEntry_struct* entry,
                                                BSObject* obj, unsigned short state,
                                                BSMachineThread_struct* thread,
                                                BSMessageRegistration_struct** replaced);  // 0x8003A1BC
int BSEndMessage();  // 0x8003A2E0
int BSInitMessage();  // 0x8003A2F4
int BSMessageGetMemoryRequirements();  // 0x8003A45C

extern int g_iBSMessageRegistrationListSize;  // 0x8034E82C

#endif
