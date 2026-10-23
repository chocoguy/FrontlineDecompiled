#include "ea/snd/aems.h"
#include <string.h>

// Asynchronous module bank load state.
int SNDAEMSalmbcurrentpriority = 100;
int SNDAEMSalmbpriority;
char* SNDAEMSalmbfilename;
int SNDAEMSalmbfileoffset;
char* SNDAEMSalmbstreamfilename;
int SNDAEMSalmbstreamfileoffset;
u8* SNDAEMSalmbploadbuf;
int SNDAEMSalmbloadbufsize;
void* (*SNDAEMSalmbmalloccb)(int size);
int SNDAEMSalmbopenop;
int SNDAEMSalmbcloseop;
int SNDAEMSalmbfirstreadop;
int SNDAEMSalmbsecondreadop;
int SNDAEMSalmbfhandle;
MODULEBANK* SNDAEMSalmbpheader;
MODULEBANK* SNDAEMSalmbpmb;
int SNDAEMSalmbmodulebankhandle;
int SNDAEMSalmbpatchbankhandle;
int SNDAEMSalmbmidibankhandle;
s8 SNDAEMSalmbsfxloaded;
s8 SNDAEMSalmbmidiloaded;

int (*SNDAEMS_asyncloadmodulebank)(const char* path, int offset, const char* streamFile, int streamOffset,
                                   void* loadBuffer, int loadBufferSize, void* (*alloc)(int)) =
    (int (*)(const char*, int, const char*, int, void*, int, void* (*)(int)))SNDAEMSI_asyncloadmodulebank;

// Server client: open, read the 0x38-byte start, read the rest into memory from the allocator,
// close, then load the sample banks it contains, and publish the bank.
void SNDAEMSI_almbservice() {
    if (SNDAEMSalmbopenop != 0) {
        if (FILESYS_opstatus(SNDAEMSalmbopenop) != 1) {
            return;
        }
        SNDAEMSalmbfhandle = FILESYS_completeop(SNDAEMSalmbopenop);
        SNDAEMSalmbopenop = 0;
        SNDAEMSalmbfirstreadop = FILESYS_read(SNDAEMSalmbfhandle, SNDAEMSalmbfileoffset, SNDAEMSalmbploadbuf,
                                              0x38, SNDAEMSalmbpriority, NULL);
    }
    if (SNDAEMSalmbfirstreadop != 0 && FILESYS_opstatus(SNDAEMSalmbfirstreadop) == 1) {
        MODULEBANK* start;
        int size;
        FILESYS_completeop(SNDAEMSalmbfirstreadop);
        start = (MODULEBANK*)SNDAEMSalmbploadbuf;
#ifdef FRONTLINE_NATIVE
        AEMSBankHeaderFromBigEndian(start);
#endif
        SNDAEMSalmbfirstreadop = 0;
        SNDAEMSalmbpmb = start;
        size = start->size - (start->sfxBankSize + start->midiBankSize);
        SNDAEMSalmbpheader = (MODULEBANK*)SNDAEMSalmbmalloccb(size);
        SNDI_memcpy(SNDAEMSalmbpheader, SNDAEMSalmbploadbuf, 0x38);
        SNDAEMSalmbpmb = SNDAEMSalmbpheader;
        SNDAEMSalmbsecondreadop = FILESYS_read(SNDAEMSalmbfhandle, SNDAEMSalmbfileoffset + 0x38,
                                               (u8*)SNDAEMSalmbpheader + 0x38, size - 0x38, SNDAEMSalmbpriority, NULL);
    }
    if (SNDAEMSalmbsecondreadop != 0 && FILESYS_opstatus(SNDAEMSalmbsecondreadop) == 1) {
        FILESYS_completeop(SNDAEMSalmbsecondreadop);
        SNDAEMSalmbcloseop = FILESYS_close(SNDAEMSalmbfhandle, SNDAEMSalmbpriority, NULL);
    }
    if (SNDAEMSalmbcloseop != 0 && FILESYS_opstatus(SNDAEMSalmbcloseop) == 1) {
        FILESYS_completeop(SNDAEMSalmbcloseop);
        if (SNDAEMSalmbpmb->sfxBankOffset != 0) {
            SNDAEMSalmbpatchbankhandle =
                SNDBANK_asyncload(SNDAEMSalmbfilename, SNDAEMSalmbfileoffset + SNDAEMSalmbpmb->sfxBankOffset,
                                  SNDAEMSalmbploadbuf, SNDAEMSalmbloadbufsize, SNDAEMSalmbmalloccb);
        } else {
            SNDAEMSalmbsfxloaded = 1;
        }
    }
    if (SNDAEMSalmbpmb != NULL) {
        if (SNDAEMSalmbpatchbankhandle >= 0 && !SNDAEMSalmbsfxloaded) {
            if (SNDBANK_asyncdone() <= 0) {
                return;
            }
            SNDAEMSalmbsfxloaded = 1;
        }
        if (SNDAEMSalmbsfxloaded) {
            if (SNDAEMSalmbmidibankhandle < 0) {
                if (SNDAEMSalmbpmb->midiBankOffset != 0) {
                    SNDAEMSalmbmidibankhandle =
                        SNDBANK_asyncload(SNDAEMSalmbfilename, SNDAEMSalmbfileoffset + SNDAEMSalmbpmb->midiBankOffset,
                                          SNDAEMSalmbploadbuf, SNDAEMSalmbloadbufsize, SNDAEMSalmbmalloccb);
                } else {
                    SNDAEMSalmbmidiloaded = 1;
                }
            }
            if (!SNDAEMSalmbmidiloaded && SNDBANK_asyncdone() > 0) {
                SNDAEMSalmbmidiloaded = 1;
            }
        }
    }
    if (SNDAEMSalmbmidiloaded) {
        iSNDserverremoveclient(SNDAEMSI_almbservice);
        SNDSYS_entercritical();
        SNDAEMSI_resolvemodulebank(SNDAEMSalmbpmb, SNDAEMSalmbstreamfilename, SNDAEMSalmbstreamfileoffset);
        SNDAEMSalmbpmb->rt->sfxBank = SNDAEMSalmbpatchbankhandle;
        SNDAEMSalmbpmb->rt->midiBank = SNDAEMSalmbmidibankhandle;
        sndaems.banks[SNDAEMSalmbmodulebankhandle] = SNDAEMSalmbpmb;
        SNDMEMI_free(SNDAEMSalmbfilename);
        if (SNDAEMSalmbstreamfilename != NULL) {
            SNDMEMI_free(SNDAEMSalmbstreamfilename);
        }
        sndaems.loading = 0;
        SNDSYS_leavecritical();
    }
}

// Start loading a module bank from a file; returns its slot (ready when
// SNDAEMS_asyncloadmodulebankdone says so).
int SNDAEMSI_asyncloadmodulebank(char* file, int offset, char* streamFile, int streamOffset, void* buffer, int size,
                                 void* (*alloc)(int)) {
    SNDAEMSalmbploadbuf = (u8*)buffer;
    sndaems.loading = 1;
    SNDAEMSalmbloadbufsize = size;
    SNDAEMSalmbmalloccb = alloc;
    SNDSYS_entercritical();
    SNDAEMSalmbfilename = (char*)SNDMEMI_allocz(strlen(file) + 1);
    strcpy(SNDAEMSalmbfilename, file);
    SNDAEMSalmbfileoffset = offset;
    if (streamFile != NULL && streamFile[0] != '\0') {
        SNDAEMSalmbstreamfilename = (char*)SNDMEMI_allocz(strlen(streamFile) + 1);
        strcpy(SNDAEMSalmbstreamfilename, streamFile);
    } else {
        SNDAEMSalmbstreamfilename = NULL;
    }
    SNDAEMSalmbstreamfileoffset = streamOffset;
    SNDSYS_leavecritical();
    SNDAEMSalmbfirstreadop = 0;
    SNDAEMSalmbsecondreadop = 0;
    SNDAEMSalmbsfxloaded = 0;
    SNDAEMSalmbmidiloaded = 0;
    SNDAEMSalmbpatchbankhandle = -1;
    SNDAEMSalmbmidibankhandle = -1;
    SNDAEMSalmbpriority = SNDAEMSalmbcurrentpriority;
    for (SNDAEMSalmbmodulebankhandle = 0; SNDAEMSalmbmodulebankhandle < 16; SNDAEMSalmbmodulebankhandle++) {
        if (sndaems.banks[SNDAEMSalmbmodulebankhandle] == NULL) {
            break;
        }
    }
    SNDAEMSalmbopenop = FILESYS_open(file, 1, SNDAEMSalmbpriority, NULL);
    iSNDserveraddclient(SNDAEMSI_almbservice);
    SNDAEMSI_almbservice();
    return SNDAEMSalmbmodulebankhandle;
}

extern "C" int SNDAEMS_asyncloadmodulebankdone(void) {
    return sndaems.loading == 0;
}
