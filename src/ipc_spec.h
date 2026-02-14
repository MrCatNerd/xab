#pragma once

#define IPC_PROTO_VERSION 1
#define IPC_DIR "/tmp/xab"
#define IPC_PATH IPC_DIR "/xab_uds"

typedef enum IpcCommands {
    // misc
    IPC_INVALID = -1,
    IPC_NONE = 0,

    // set state
    IPC_RESTART = 1,
    IPC_SHUTDOWN = 2,
    IPC_CLIENT_DISCONNECT = 3,

    // still havn't decided if I want multiple backgrounds at once or one at a
    // time
    IPC_CHANGE_BACKGROUND = 4,
    IPC_DELETE_BACKGROUND = 5,
    IPC_PAUSE_VIDEO = 6,
    IPC_UNPAUSE_VIDEO = 7,
    IPC_TOGGLE_PAUSE_VIDEO = 8,

    // get state
    IPC_GET_MONITORS = 9,
    IPC_GET_BACKGROUNDS = 10,
    IPC_GET_CAPABILITES = 11,
} IpcCommands_e;

typedef enum IpcXabCapabilities {
    IpcXabCapabilitiesNone = 0,
    IpcXabCapabilitiesCustomPositioning = 1 << 0,
    IpcXabCapabilitiesMonitors = 1 << 1,
} IpcXabCapabilities;
