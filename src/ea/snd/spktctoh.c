#include "ea/snd/spktplay.h"

int SNDPKTPLAYI_voicetopackethandle(int voice) {
    int i;
    for (i = 0; i < sndgs.opts.caps.maxPacketPlayers; i++) {
        if (sndpps.players[i] != NULL && voice == SNDVOICEI_get(sndpps.players[i]->voice)) {
            return i;
        }
    }
    return -1;
}
