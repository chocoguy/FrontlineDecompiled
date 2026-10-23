#include "ea/snd/sbasync.h"

SNDBANKASYNCSTATE sndbas = {NULL, 100, 0};

// Collect finished reads, in order.
void SNDBANKI_asynccompletereads() {
    int i = sndbas.cur->readsDone % 4;
    while (sndbas.cur->readsIssued > sndbas.cur->readsDone) {
        SNDBANKASYNC* b = sndbas.cur;
        SNDBANKASYNCBLOCK* blk = &b->blocks[i];
        if (blk->state != 1) {
            break;
        }
        if (FILESYS_opstatus(blk->op) != 1) {
            break;
        }
        if (FILESYS_completeop(blk->op) > 0) {
            blk->state = 2;
        } else {
            blk->state = 0;
        }
        i++;
        sndbas.cur->readsDone++;
        if (i >= 4) {
            i = 0;
        }
    }
}

// Start reads into free blocks (at most four outstanding).
void SNDBANKI_asyncissuereads() {
    int size = 0xFFFFFF;
    int i = sndbas.cur->readsIssued % 4;
    while (sndbas.cur->readsDone > sndbas.cur->readsIssued - 4) {
        SNDBANKASYNC* b = sndbas.cur;
        int pos = b->readsIssued * b->blockSize;
        SNDBANKASYNCBLOCK* blk;
        if (b->header != NULL) {
            size = b->total - pos;
            if (size <= 0) {
                break;
            }
        }
        if (size > b->blockSize) {
            size = b->blockSize;
        }
        blk = &b->blocks[i];
        if (blk->state != 0) {
            break;
        }
        blk->op = FILESYS_read(b->file, b->offset + pos, blk->buffer, size, sndbas.priority, NULL);
        blk->state = 1;
        i++;
        sndbas.cur->readsIssued++;
        if (i >= 4) {
            i = 0;
        }
    }
}

// Copy read blocks into the header; the block where the header ends goes on to ARAM from
// `partial`.
void SNDBANKI_asyncxferhdr() {
    int i;
    if (sndbas.cur->headerLeft <= 0) {
        return;
    }
    i = sndbas.cur->consumed % 4;
    while (sndbas.cur->downloaded < sndbas.cur->readsDone) {
        SNDBANKASYNC* b = sndbas.cur;
        int n = b->blockSize;
        SNDBANKASYNCBLOCK* blk = &b->blocks[i];
        if (b->headerLeft < n) {
            n = b->headerLeft;
        }
        SNDI_memcpy((u8*)b->header + b->blockSize * b->downloaded, blk->buffer, n);
        sndbas.cur->headerLeft -= n;
        if (sndbas.cur->headerLeft <= 0 && n < sndbas.cur->blockSize) {
            sndbas.cur->partial = n;
            return;
        }
        blk->state = 0;
        i++;
        sndbas.cur->consumed++;
        sndbas.cur->downloaded++;
        if (i >= 4) {
            i = 0;
        }
    }
}

// Send read blocks of sample data to ARAM.
void SNDBANKI_asyncissuedownloads() {
    int i;
    if (sndbas.cur->sampleLeft <= 0) {
        return;
    }
    i = sndbas.cur->downloaded % 4;
    while (sndbas.cur->consumed < sndbas.cur->downloaded + 4) {
        SNDBANKASYNC* b = sndbas.cur;
        SNDBANKASYNCBLOCK* blk = &b->blocks[i];
        if (blk->state != 2) {
            break;
        }
        if (b->sampleLeft <= 0) {
            blk->state = 0;
            break;
        }
        if (b->partial < b->blockSize) {
            int n = b->blockSize - b->partial;
            if (n > b->sampleLeft) {
                n = b->sampleLeft;
            }
            blk->download = SNDPLATFORM_download(0x200, blk->buffer + b->partial, b->aram + b->aramPos, n);
            blk->state = 3;
            sndbas.cur->aramPos += n;
            sndbas.cur->sampleLeft -= n;
        }
        if (blk->state == 3) {
            sndbas.cur->partial = 0;
            sndbas.cur->consumed++;
        }
        i++;
        if (i >= 4) {
            i = 0;
        }
    }
}

static inline void SNDBANKI_asyncfinishdownloads() {
    int i = sndbas.cur->downloaded % 4;
    while (sndbas.cur->consumed > sndbas.cur->downloaded) {
        SNDBANKASYNCBLOCK* blk = &sndbas.cur->blocks[i];
        if (SNDPLATFORM_downloadcomplete(blk->download) == 0) {
            break;
        }
        if (blk->download <= 0) {
            break;
        }
        blk->state = 0;
        i++;
        sndbas.cur->downloaded++;
        if (i >= 4) {
            i = 0;
        }
    }
}

// 100 Hz client while loading: keep the pipeline moving; once everything is in, close the
// file and publish the bank.
void SNDBANKI_asyncserver() {
    SNDBANKSLOT* slot;
    BANKVER5* data;

    SNDBANKI_asynccompletereads();
    SNDBANKI_asyncxferhdr();
    SNDBANKI_asyncfinishdownloads();
    SNDBANKI_asyncissuedownloads();
    SNDBANKI_asyncissuereads();
    if (sndbas.cur->headerLeft > 0 || sndbas.cur->sampleLeft > 0) {
        return;
    }
    if (sndbas.cur->closeOp == 0) {
        sndbas.cur->closeOp = FILESYS_close(sndbas.cur->file, sndbas.priority, NULL);
    }
    if (sndbas.cur->consumed != sndbas.cur->downloaded) {
        return;
    }
    if (FILESYS_opstatus(sndbas.cur->closeOp) != 1) {
        return;
    }
    FILESYS_completeop(sndbas.cur->closeOp);
    SNDSYS_remove100hzclient(SNDBANKI_asyncserver);
    slot = &sndgs.banks[sndbas.cur->bank];
    slot->data = sndbas.cur->header;
    data = slot->data;
    slot->aram = sndbas.cur->aram;
#ifdef FRONTLINE_NATIVE
    SNDBankFromBigEndian(data);
#endif
    if (data->sampleSize != 0) {
        int state = 0;
        int i;
        for (i = 0; i < data->numPatches; i++) {
            if (data->patchOffsets[i] != 0) {
                SNDBANKI_asyncresolvepatch(0x200, (TAGGEDPATCH*)((u8*)&data->patchOffsets[i] + data->patchOffsets[i]),
                                           (char*)slot->aram, &state);
            }
        }
    }
    slot->busy = 0;
    sndbas.cur = NULL;
}

// Server client until the first block (the bank's start) is in: then size the header and
// ARAM and hand over to the 100 Hz client.
void SNDBANKI_asyncservice() {
    SNDBANKASYNC* b;

    if (sndbas.cur->openOp != 0) {
        int status = FILESYS_opstatus(sndbas.cur->openOp);
        if (status == 1) {
            sndbas.cur->file = FILESYS_completeop(sndbas.cur->openOp);
            sndbas.cur->openOp = 0;
            SNDBANKI_asyncissuereads();
        } else if (status == 0) {
            return;
        } else {
            FILESYS_completeop(sndbas.cur->openOp);
            sndbas.result = -19;
            if (sndbas.cur->active) {
                iSNDserverremoveclient(SNDBANKI_asyncservice);
            }
            sndgs.banks[sndbas.cur->bank].data = NULL;
            sndgs.banks[sndbas.cur->bank].busy = 0;
            sndbas.cur = NULL;
            return;
        }
    }

    b = sndbas.cur;
    if (FILESYS_opstatus(b->blocks[0].op) != 1) {
        return;
    }
    FILESYS_completeop(b->blocks[0].op);
    b->blocks[0].state = 2;
    sndbas.cur->readsDone++;
    iSNDserverremoveclient(SNDBANKI_asyncservice);
    sndbas.cur->active = 0;
    sndbas.cur->header = (BANKVER5*)b->blocks[0].buffer;
#ifdef FRONTLINE_NATIVE
    SNDBankHeaderFromBigEndian(sndbas.cur->header);
#endif
    sndbas.cur->headerLeft = sndbas.cur->header->headerSize;
    sndbas.cur->total = sndbas.cur->header->headerSize;
    sndbas.cur->sampleLeft = sndbas.cur->header->sampleSize;
    sndbas.cur->total += sndbas.cur->header->sampleSize;
    sndbas.cur->header = (BANKVER5*)sndbas.cur->alloc(sndbas.cur->header->headerSize);
    if (sndbas.cur->sampleLeft != 0) {
        sndbas.cur->aram = (u8*)SNDPLATFORM_memalloc(0x200, sndbas.cur->sampleLeft + 0x40);
    } else {
        sndbas.cur->aram = NULL;
    }
    SNDBANKI_asynccompletereads();
    SNDBANKI_asyncxferhdr();
    SNDBANKI_asyncfinishdownloads();
    SNDBANKI_asyncissuedownloads();
    SNDBANKI_asyncissuereads();
    SNDSYS_add100hzclient(SNDBANKI_asyncserver);
}

// Start loading a bank from `name` at `offset`; `buffer` holds the state and four read blocks.
// Returns the bank index (or the error if it failed at once).
extern "C" int SNDBANK_asyncload(const char* name, int offset, void* buffer, int size, void* (*alloc)(int)) {
    SNDBANKASYNC* b = (SNDBANKASYNC*)buffer;
    u16 blockSize = (size - 0x1C0) >> 2;
    int i;

    sndbas.result = 0;
    sndbas.cur = b;
    b->alloc = alloc;
    sndbas.cur->offset = offset;
    sndbas.cur->partial = 0;
    sndbas.cur->total = 0;
    sndbas.cur->headerLeft = 0;
    sndbas.cur->aramPos = 0;
    sndbas.cur->header = NULL;
    sndbas.cur->readsIssued = 0;
    sndbas.cur->readsDone = 0;
    sndbas.cur->consumed = 0;
    sndbas.cur->downloaded = 0;
    sndbas.cur->closeOp = 0;
    sndbas.cur->blockSize = blockSize;
    if (sndbas.cur->blockSize > 0x6000) {
        sndbas.cur->blockSize = 0x6000;
    }
    sndbas.cur->blockSize &= ~0x3F;
    sndbas.cur->openOp = FILESYS_open(name, 1, sndbas.priority, NULL);
    for (i = 0; i < 4; i++) {
        SNDBANKASYNCBLOCK* blk = &sndbas.cur->blocks[i];
        blk->download = -1;
        blk->state = 0;
        blk->buffer = (u8*)b + (sndbas.cur->blockSize + 0x40) * i + 0xC0;
        blk->buffer += 0x40;
        blk->buffer = (u8*)((uptr)(blk->buffer) & ~0x3F);
    }
    for (i = 0; i < sndgs.opts.caps.maxBanks; i++) {
        if (sndgs.banks[i].data == NULL) {
            sndgs.banks[i].busy = 1;
            sndgs.banks[i].data = (BANKVER5*)1;
            sndbas.cur->bank = i;
            break;
        }
    }
    iSNDserveraddclient(SNDBANKI_asyncservice);
    sndbas.cur->active = 1;
    SNDBANKI_asyncservice();
    if (sndbas.cur == NULL) {
        return sndbas.result;
    }
    return sndbas.cur->bank;
}

extern "C" int SNDBANK_asyncdone(void) {
    if (sndbas.cur != NULL) {
        return 0;
    }
    if (sndbas.result == 0) {
        return 1;
    }
    return sndbas.result;
}
