#include <TRK_MINNOW_DOLPHIN/trk.h>

void usr_put_initialize(void) {}

// Prints through OSReport one character at a time, with the debugger link
// marked disconnected so OSReport goes to the console.
BOOL usr_puts_serial(const char* msg) {
    BOOL result;
    char c;
    char buf[2];

    result = FALSE;
    while (result == FALSE && (c = *msg++) != '\0') {
        BOOL connected = GetTRKConnected();

        buf[0] = c;
        buf[1] = '\0';
        SetTRKConnected(FALSE);
        OSReport(buf);
        SetTRKConnected(connected);
        result = FALSE;
    }
    return result;
}
