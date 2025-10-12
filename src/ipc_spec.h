#pragma once

#define IPC_PROTO_VERSION 1
#define IPC_DIR "/tmp/xab"
#define IPC_PATH IPC_DIR "/xab_uds"

typedef enum IpcCommands {
    // stuff
    IPC_INVALID = -1,
    IPC_NONE = 0,

    // ipc stuff
    IPC_RESTART = 1,
    IPC_XAB_SHUTDOWN = 2,
    IPC_CLIENT_DISCONNECT = 3,

    // change stuff
    IPC_CHANGE_BACKGROUNDS = 4,
    IPC_PAUSE_VIDEOS = 6,
    IPC_UNPAUSE_VIDEOS = 7,
    IPC_TOGGLE_PAUSE_VIDEOS = 8,

    // get stuff
    IPC_GET_MONITORS = 9,
    IPC_GET_CAPABILITES = 10,
} IpcCommands_e;

typedef enum IpcXabCapabilities {
    IpcXabCapabilitiesNone = 0,
    IpcXabCapabilitiesMultimonitor = 1 << 0,
} IpcXabCapabilities;
