#pragma once

// -- color macros -- //
/// they don't support pure black lmao
#define TRACY_NOCOLOR 0x000000
#define TRACY_COLOR_GREY 0x808080
#define TRACY_COLOR_BLACK 0x000001
#define TRACY_COLOR_WHITE 0xffffff
#define TRACY_COLOR_RED 0xff0000
#define TRACY_COLOR_GREEN 0x00ff00
#define TRACY_COLOR_BLUE 0x0000ff

// only if tracy is enabled
#ifdef TRACY_ENABLE

#include <tracy/TracyC.h>

#define ON_TRACY(stuff) stuff

/// 1-byte ints (0->255) color rgb to hex color rgb
#define TRACY_COLOR_RGB(red, green, blue) ((red << 16) | (green << 8) | (blue))
/// normalized floats (0->1) rgb to hex color rgb
#define TRACY_COLOR_RGBF(red, green, blue)                                     \
    (((int)(red * 255) << 16) | ((int)(green * 255) << 8) | ((int)(blue * 255)))
// ------------------ //

#else // TRACY_ENABLE

// haha macro abuse go bRRR
#define ON_TRACY(stuff)

typedef const void *TracyCZoneCtx;

typedef const void *TracyCLockCtx;

// stolen from tracy/TracyC.h
// cuz it might not be installed

#define TracyCZone(c, x)
#define TracyCZoneN(c, x, y)
#define TracyCZoneC(c, x, y)
#define TracyCZoneNC(c, x, y, z)
#define TracyCZoneEnd(c)
#define TracyCZoneText(c, x, y)
#define TracyCZoneName(c, x, y)
#define TracyCZoneColor(c, x)
#define TracyCZoneValue(c, x)

#define TracyCAlloc(x, y)
#define TracyCFree(x)
#define TracyCMemoryDiscard(x)
#define TracyCSecureAlloc(x, y)
#define TracyCSecureFree(x)
#define TracyCSecureMemoryDiscard(x)

#define TracyCAllocN(x, y, z)
#define TracyCFreeN(x, y)
#define TracyCSecureAllocN(x, y, z)
#define TracyCSecureFreeN(x, y)

#define TracyCFrameMark
#define TracyCFrameMarkNamed(x)
#define TracyCFrameMarkStart(x)
#define TracyCFrameMarkEnd(x)
#define TracyCFrameImage(x, y, z, w, a)

#define TracyCPlot(x, y)
#define TracyCPlotF(x, y)
#define TracyCPlotI(x, y)
#define TracyCPlotConfig(x, y, z, w, a)

#define TracyCMessage(x, y)
#define TracyCMessageL(x)
#define TracyCMessageC(x, y, z)
#define TracyCMessageLC(x, y)
#define TracyCAppInfo(x, y)

#define TracyCZoneS(x, y, z)
#define TracyCZoneNS(x, y, z, w)
#define TracyCZoneCS(x, y, z, w)
#define TracyCZoneNCS(x, y, z, w, a)

#define TracyCAllocS(x, y, z)
#define TracyCFreeS(x, y)
#define TracyCMemoryDiscardS(x, y)
#define TracyCSecureAllocS(x, y, z)
#define TracyCSecureFreeS(x, y)
#define TracyCSecureMemoryDiscardS(x, y)

#define TracyCAllocNS(x, y, z, w)
#define TracyCFreeNS(x, y, z)
#define TracyCSecureAllocNS(x, y, z, w)
#define TracyCSecureFreeNS(x, y, z)

#define TracyCMessageS(x, y, z)
#define TracyCMessageLS(x, y)
#define TracyCMessageCS(x, y, z, w)
#define TracyCMessageLCS(x, y, z)

#define TracyCLockCtx(l)
#define TracyCLockAnnounce(l)
#define TracyCLockTerminate(l)
#define TracyCLockBeforeLock(l)
#define TracyCLockAfterLock(l)
#define TracyCLockAfterUnlock(l)
#define TracyCLockAfterTryLock(l, x)
#define TracyCLockMark(l)
#define TracyCLockCustomName(l, x, y)

#define TracyCIsConnected 0
#define TracyCIsStarted 0

#ifdef TRACY_FIBERS
#define TracyCFiberEnter(fiber)
#define TracyCFiberLeave
#endif

#endif // NDEF TRACY_ENABLE
