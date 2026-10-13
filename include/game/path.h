// PATH: EA's interactive music player. A map file (.mpf) describes a
// project: a graph of nodes (each a piece of a stream, with branches to
// other nodes by a control level), events that jump tracks to nodes, and
// fade curves. Tracks (up to 24 per project, 4 projects) play nodes on
// PATHX streams, synchronised on beats. Hand-maintained.
#ifndef _GAME_PATH_H_
#define _GAME_PATH_H_

#include "types.h"
#include "game_types.h"
#include "ea/real.h"

#define PATH_MAXPROJECTS 4
#define PATH_MAXTRACKS 24
#define PATH_MAXEVENTS 16

// A track handle: instance bits (0x10000000 << n), project bits
// (0x01000000 << project id) and one bit per track. -1 is every track.
#define PATH_INSTANCEMASK 0xF0000000
#define PATH_PROJECTMASK 0x0F000000
#define PATH_TRACKMASK 0x00FFFFFF

// A branch: the node to go to for control levels lo..hi. size 0x4
struct PATHBRANCH {
    /* 0x0 */ s8 lo;
    /* 0x1 */ s8 hi;
    /* 0x2 */ s16 node;
};

// A node of the map. size 0xC + branches
struct PATHFINDNODE {
    /* 0x0 */ s16 wave;  // 1-based wave; 0 a marker, -1 a loop, -2 random
    /* 0x2 */ u16 track : 5;
    /* 0x2 */ u16 beat : 6;
    /* 0x2 */ u16 eventNode : 1;  // entering it keeps the event's level
    /* 0x2 */ s16 loops : 4;
    /* 0x4 */ u8 remap;  // node remap list (0: none)
    /* 0x5 */ u8 numBranches : 5;
    /* 0x5 */ u8 type : 3;  // 1: branch on a random level
    /* 0x6 */ u8 bars : 4;
    /* 0x6 */ u8 beatsPerBar : 4;
    /* 0x7 */ u8 beatStep : 4;   // beats between entry points
    /* 0x7 */ u8 beatStart : 4;  // first entry beat (1-based)
    // MWCC packs a u16 run that follows u8 bitfields wrongly (fadeIn ends
    // up at 0xA); the zero-width fields restart the runs at 0x8 and 0xA.
    u16 : 0;
    /* 0x8 */ u16 beatUnit : 4;
    /* 0x8 */ u16 sync : 2;  // 1 on a beat, 2 at the end
    /* 0x8 */ u16 fadeIn : 5;  // fade curve (1-based)
    /* 0x8 */ u16 fadeOut : 5;
    u8 : 0;
    /* 0xA */ u8 alignBeat : 1;  // starts on the lead track's beat
    /* 0xA */ u8 unk_bits : 7;
    /* 0xB */ u8 unk_0xB;
    /* 0xC */ PATHBRANCH branches[1];
};

// The map file header.
struct PATHMAP {
    /* 0x00 */ u32 magic;  // 'PFDx'
    /* 0x04 */ u8 version;  // 3
    /* 0x05 */ u8 subversion;  // 4
    /* 0x06 */ u8 unk_0x6[0x6];
    /* 0x0C */ u8 id;  // the project id
    /* 0x0D */ u8 numTracks;
    /* 0x0E */ u8 numSections;  // event value columns per event
    /* 0x0F */ u8 numEvents;
    /* 0x10 */ u8 numValues;
    /* 0x11 */ u8 tableIndex;
    /* 0x12 */ u16 numNodes;
    /* 0x14 */ u8 unk_0x14[0x10];
    /* 0x24 */ s16 nodeOffsets[1];  // in words from the start of the map
};

// What an event does to a track (also a track's current/saved state).
// size 0x4
struct PATHEVENTVALUE {
    /* 0x0 */ s8 level;  // volume 0..127 (< 0: unchanged)
    /* 0x1 */ u8 flags;  // &3 restore mode, &8 crossfade, &0x70 sync
    /* 0x2 */ s16 node;  // -1 none
};

// A queued event. size 0x10
struct PATHFINDEVENT {
    /* 0x0 */ s16 event;  // < 0: a jump to node -1 - event
    /* 0x2 */ u16 sync : 7;  // 0x10 beat, 0x20 bar, 0x40 immediately
    /* 0x2 */ u16 priority : 2;
    /* 0x2 */ u16 hold : 1;  // started tracks wait for the others
    /* 0x2 */ u16 unk_bit : 1;
    /* 0x2 */ u16 track : 5;  // the track that set the time
    /* 0x4 */ u32 time;  // when to start (0: not yet known)
    /* 0x8 */ u32 expire;
    /* 0xC */ u32 mask;  // tracks still to start (-1: a pause)
};

// One of a buffer's requests. size 0x10
struct PATHREQUEST {
    /* 0x0 */ s32 node;
    /* 0x4 */ s32 length;
    /* 0x8 */ s32 beat;
    /* 0xC */ s32 request;  // PATHX request (-1 none)
};

// A track, at the start of the buffer given to PATH_createtrack.
// size 0xE8
struct PATHTRACK {
    /* 0x00 */ s8 index;
    /* 0x01 */ s8 control;  // branch level 0..127
    /* 0x02 */ s8 paused;
    /* 0x03 */ s8 volume;  // 0..100
    /* 0x04 */ u32 fadeStart;
    /* 0x08 */ s8 fadeFrom;
    /* 0x09 */ s8 fadeCurve;  // 1-based (0: the project's default)
    /* 0x0A */ s8 fadeTo;  // -1 not fading
    /* 0x0B */ u8 unk_0xB;
    /* 0x0C */ s32 loops : 4;  // left on loopNode (-1: endless)
    /* 0x0C */ s32 beat : 5;  // next beat of the node (<= 0 none)
    /* 0x0C */ u32 unk_bits : 5;
    /* 0x0C */ u32 numRequests : 5;
    /* 0x0C */ u32 requestIndex : 5;
    /* 0x0C */ u32 unk_bits2 : 8;
    /* 0x10 */ s32 latency;  // ms
    /* 0x14 */ u32 timerService;  // serviced by the timer (latency < 50 ms)
    /* 0x18 */ s16 loopNode;
    /* 0x1A */ s16 eventId;  // the event to restore from (-1 none)
    /* 0x1C */ PATHFINDNODE* node;  // the playing marker node
    /* 0x20 */ PATHEVENTVALUE cur;   // level, flags, node
    /* 0x24 */ PATHEVENTVALUE saved;
    /* 0x28 */ PATHEVENTVALUE unk_0x28;
    /* 0x2C */ s32 beatLength;  // ms
    /* 0x30 */ s32 barLength;
    /* 0x34 */ u32 startTime;
    /* 0x38 */ u32 releaseTime;  // when to release the held request
    /* 0x3C */ int pathx;  // the PATHX track
    /* 0x40 */ s32 request;
    /* 0x44 */ char file[0x80];
    /* 0xC4 */ char name[0x20];
    /* 0xE4 */ PATHREQUEST* requests;
};

// A loaded map. size 0x1A8
struct PATHPROJECT {
    /* 0x000 */ s32 leadTrack;  // the playing track with the most latency
    /* 0x004 */ u32 leadLatency;
    /* 0x008 */ s32 leadBeat;
    /* 0x00C */ s32 taskLatency;
    /* 0x010 */ s32 timerLatency;
    /* 0x014 */ s32 fadeTime;
    /* 0x018 */ s32 fadeCurve;
    /* 0x01C */ s8 control;
    /* 0x020 */ u32 id;  // instance and project bits
    /* 0x024 */ PATHMAP* map;
    /* 0x028 */ s16* nodeOffsets;
    /* 0x02C */ void* nodesEnd;
    /* 0x030 */ u8* eventTable;  // [track][event][section] -> value
    /* 0x034 */ PATHEVENTVALUE* values;
    /* 0x038 */ u32* tables;
    /* 0x03C */ u32* fadeOffsets;
    /* 0x040 */ s32 (*waves)[2];  // offset, length
    /* 0x044 */ u32* fades;
    /* 0x048 */ PATHFINDEVENT events[PATH_MAXEVENTS];
    /* 0x148 */ PATHTRACK* tracks[PATH_MAXTRACKS];
};

void PATHI_servicetimer();  // 0x8010D084
void PATHI_servicetask();  // 0x8010D308
void PATHI_serviceproject(int, int);  // 0x8010D58C
void PATHI_crossfadetracks(int, unsigned int, PATHFINDEVENT*);  // 0x8010DF10
int PATHI_choosebeat(PATHFINDNODE*, PATHFINDNODE*, int*, int*);  // 0x8010E254
int PATHI_queuenode(PATHTRACK*, unsigned int);  // 0x8010E54C
void PATHI_seeknextnode(int);  // 0x8010E988
void PATHI_eventrestore(short);  // 0x8010ED14
int PATHI_eventstart(int);  // 0x8010EE50
int PATHI_enternode(int, int, int);  // 0x8010F748
void PATHI_setfadevolume(PATHTRACK*);  // 0x8010FB0C
void PATHI_sortprojects();  // 0x8010FD80

#ifdef __cplusplus
extern "C" {
#endif

// The PATH interactive music player. `track` -1 means every track.
int PATH_fade(int track, int volume, int ms);  // 0x8010ACD4
int PATH_volume(int track, char volume);  // 0x8010AEC4, 0..100
int PATH_stop(int track);  // 0x8010B078
int PATH_pause(int track, char pause);  // 0x8010B1B8
// Destroys a track; returns its buffer for freeing, NULL when none is left.
void* PATH_destroy(int track);  // 0x8010B330
int PATH_jump(int track, int node, char sync);  // 0x8010B6CC
// `event` is the event in the low byte, the project id in bits 8..11;
// `flags` &3 priority, &8 crossfade, &0x70 sync; -1 restores the event.
int PATH_event(int track, short event, int flags);  // 0x8010B93C
int PATH_control(int track, int level);  // 0x8010C098, 0..127
int PATH_latency(int track, unsigned short ms);  // 0x8010C280
int PATH_overheadtrack(int numRequests, int stream);  // 0x8010C330
// Creates track `index` of the project `id` (instance and project bits) in
// `buffer`; returns its handle, or a negative error.
int PATH_createtrackfrombuf(int id, int index, const char* file, int latency, SNDPLAYOPTS* opts, int numRequests,
                            int stream, void* buffer, int bufferSize, void* bank);  // 0x8010C384
int PATH_createtrack(int id, int index, const char* file, int latency, SNDPLAYOPTS* opts, int numRequests,
                     int stream, void* buffer, int bufferSize);  // 0x8010CB8C
// Registers a loaded map (.mpf); returns the project id.
int PATH_addmapfile(void* map);  // 0x8010CBBC

extern void (*songprogress)(u32 id, s16 node);  // 0x8034EF6C

#ifdef __cplusplus
}
#endif

#endif
