#include <stdio.h>

#include <epoxy/gl.h>
#include <epoxy/egl.h>
#include <epoxy/egl_generated.h>

#include "logging.h"
#include "utils.h"
#include "egl_test.h"

void test_egl_pointers_different_file() {
    Assert(glGenBuffers != NULL && "Yo dawg, you need to fix your pointers");
    LOG("-- OGL and EGL function pointers are loaded...\n");
}
