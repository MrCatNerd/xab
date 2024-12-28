#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>

#include "utils.h"

const char *ReadFile(const char *path) {
    if (access(path, F_OK) != 0)
        return NULL;

    char *buffer = NULL;
    long length = 0;
    FILE *f = fopen(path, "rb");

    if (f) {
        fseek(f, 0, SEEK_END);
        length = ftell(f);
        fseek(f, 0, SEEK_SET);
        buffer = malloc(length + 1);
        Assert(buffer != NULL &&
               "Error allocating memory while reading file: '%s'" STR(
                   path)); // FIXME: STR(path) not working, i need a new assert
                           // system

        (void)fread(buffer, 1, length, f);
        buffer[length] = '\0'; // ensure EOF
        fclose(f);
    }

    return buffer;
}
