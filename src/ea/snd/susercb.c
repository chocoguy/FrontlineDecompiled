#include "ea/snd/sndi.h"

extern "C" void SNDSYS_adduserdataclient(void (*client)(SNDUSERDATACBINFO*)) {
    if (sndgs.inited) {
        sndgs.userDataClients[sndgs.numUserDataClients] = client;
        sndgs.numUserDataClients++;
    }
}

extern "C" void SNDSYS_removeuserdataclient(void (*client)(SNDUSERDATACBINFO*), int) {
    int i;
    int n;
    SNDSYS_entercritical();
    n = sndgs.numUserDataClients;
    for (i = 0; i < n; i++) {
        if (sndgs.userDataClients[i] == client) {
            sndgs.numUserDataClients--;
            for (; i < sndgs.numUserDataClients; i++) {
                sndgs.userDataClients[i] = sndgs.userDataClients[i + 1];
            }
            break;
        }
    }
    SNDSYS_leavecritical();
}
