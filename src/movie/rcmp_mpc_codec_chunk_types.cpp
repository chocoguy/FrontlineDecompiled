// The stream chunk types the MPC (MPEG) video codec decodes.
#include "movie/hwvideodisplaylist.h"

namespace {
const u32 ChunkTypes[] = {'MPCh'};
}

bool RCMP::MPC_CODEC_is_chunk_for_codec(unsigned int type) {
    for (int i = 0; i < (int)(sizeof(ChunkTypes) / sizeof(ChunkTypes[0])); i++) {
        if (type == ChunkTypes[i]) {
            return true;
        }
    }
    return false;
}
