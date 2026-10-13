#include "ea/snd/sndi.h"

void iSNDserveraddclient(void (*client)()) {
    sndgs.serverClients[sndgs.numServerClients] = client;
    sndgs.numServerClients++;
}

void iSNDserverremoveclient(void (*client)()) {
    int i;
    int n = sndgs.numServerClients;
    for (i = 0; i < n; i++) {
        if (sndgs.serverClients[i] == client) {
            sndgs.numServerClients--;
            for (; i < sndgs.numServerClients; i++) {
                sndgs.serverClients[i] = sndgs.serverClients[i + 1];
            }
            return;
        }
    }
}

extern "C" void SNDSYS_service(void) {
    int i;
    if (sndgs.inited) {
        for (i = 0; i < sndgs.numServerClients; i++) {
            sndgs.serverClients[i]();
        }
    }
}
