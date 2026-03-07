// std
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

// xcb
#include <xcb/xcb.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_aux.h>
#include <xcb/xproto.h>
#ifdef HAVE_LIBXRANDR
#include <xcb/randr.h>
#endif /* HAVE_LIBXRANDR */

// libepoxy
#include <epoxy/gl.h>
#include <epoxy/egl.h>
#include <epoxy/common.h>
