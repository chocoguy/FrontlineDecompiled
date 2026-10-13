#include "ea/snd/sndi.h"

extern "C" void SNDSYS_add100hzclient(void (*client)(void)) {
    if (sndgs.inited) {
        sndgs.client100hz[sndgs.num100hzClients] = client;
        sndgs.num100hzClients++;
    }
}

extern "C" void SNDSYS_remove100hzclient(void (*client)(void)) {
    int i;
    int n;
    if (sndgs.inited) {
        SNDSYS_entercritical();
        n = sndgs.num100hzClients;
        for (i = 0; i < n; i++) {
            if (sndgs.client100hz[i] == client) {
                sndgs.num100hzClients--;
                for (; i < sndgs.num100hzClients; i++) {
                    sndgs.client100hz[i] = sndgs.client100hz[i + 1];
                }
                break;
            }
        }
        SNDSYS_leavecritical();
    }
}
