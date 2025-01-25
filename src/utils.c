#include "pch.h"

#include "utils.h"
#include "length_string.h"
#include "logger.h"

length_string_t ReadFile(const char *path) {
    xab_log(LOG_TRACE, "Reading file: \n", path);

    length_string_t buffer = {NULL, 0};

    if (access(path, F_OK) != 0)
        return buffer;

    long length = 0;
    FILE *f = fopen(path, "rb");

    if (f) {
        fseek(f, 0, SEEK_END);
        length = ftell(f);
        Assert(length >= 0);
        fseek(f, 0, SEEK_SET);
        // maybe i should add an arg to ensure EOF or not but its just one byte
        buffer.str = malloc(length + 1);
        Assert(buffer.str != NULL);

        (void)fread((void *)buffer.str, 1, length, f);
        buffer.str[length] = '\0'; // ensure EOF
        buffer.len = length + 1;
        fclose(f);
    }

    return buffer;
}
