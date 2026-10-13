#define EOF -1

// Read callback state for scanning from a string (sscanf, strtol).
typedef struct {
    char* NextChar;
    int NullCharDetected;
} __InStrCtrl;

// __ReadProcActions
#define __GetAChar 0
#define __UngetAChar 1
#define __TestForError 2

int __StringRead(void* isc, int ch, int Action) {
    char RetVal;
    __InStrCtrl* Iscp = (__InStrCtrl*)isc;

    switch (Action) {
    case __GetAChar:
        RetVal = *(Iscp->NextChar);
        if (RetVal == '\0') {
            Iscp->NullCharDetected = 1;
            return EOF;
        } else {
            Iscp->NextChar++;
            return (unsigned char)RetVal;
        }

    case __UngetAChar:
        if (Iscp->NullCharDetected == 0) {
            Iscp->NextChar--;
        } else {
            Iscp->NullCharDetected = 0;
        }
        return ch;

    case __TestForError:
        return Iscp->NullCharDetected;
    }

    return 0;
}
