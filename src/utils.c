#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "utils.h"

const char *ReadFile(const char *path) {
    char *buffer = NULL;
    long length = 0;
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

    buffer[length] = '\0'; // ensure EOF

    Assert(
        buffer != NULL &&
        "Could not read file %s\n" STR(
            path)); // FIXME: STR(path) not working, i need a new assert system

    return buffer;
}

void program_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "-- ERROR: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}
