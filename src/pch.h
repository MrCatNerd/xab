#pragma once

// NOTE: order matters in some cases
// e.g. utils.h and length_string.h

// std
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>

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

#include "logger.h"
#include "utils.h"
#include "length_string.h"
#include "ansii_colors.h"
#include "vertex.h"
#include "tracy.h"
#include "x_data.h"

// these ones might be a bad idea
#include "framebuffer.h"
#include "shader.h"
#include "shader_cache.h"
#include "context.h"
#include "atom.h"
