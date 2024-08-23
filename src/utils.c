#include "utils.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

const char *ReadFile(const char *path) {
    char *buffer = NULL;
    long length;
    FILE *f = fopen(path, "rb");

    if (f) {
        fseek(f, 0, SEEK_END);
        length = ftell(f);
        fseek(f, 0, SEEK_SET);
        buffer = malloc(length + 1);
        if (buffer) {
            fread(buffer, 1, length, f);
        }
        fclose(f);
    }

    buffer[length] = '\0';

    Assert(buffer != NULL && "Could not read file %s" STR(path));

    return buffer;
}
