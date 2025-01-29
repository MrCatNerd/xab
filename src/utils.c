#include "pch.h"

#include "utils.h"
#include "length_string.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>

length_string_t ReadFile(const char *path) {
    xab_log(LOG_TRACE, "Reading file: %s\n", path);

    length_string_t buffer = {NULL, 0};

    if (access(path, F_OK) != 0) {
        xab_log(LOG_ERROR,
                "File: `%s` doesn't exist (probably) returning an empty string "
                "instead\n",
                path);

        buffer.str = calloc(1, sizeof(char));
        buffer.str[0] = '\0';
        buffer.len = 1;

        return buffer;
    }

    long length = 0;
    FILE *fp = fopen(path, "r");

    if (fp) {
        fseek(fp, 0, SEEK_END);
        length = ftell(fp);
        Assert(length >= 0);
        fseek(fp, 0, SEEK_SET);
        // maybe i should add an arg to ensure EOF or not but its just one byte
        char *fstr = calloc(length + 1, sizeof(char));
        Assert(fstr != NULL);

        size_t fread_ret = fread((void *)fstr, length, sizeof(*fstr), fp);

        if (fread_ret != sizeof(*fstr)) // this is probably not going to happen
                                        // very often so umm imma just log it
            xab_log(LOG_ERROR, "fread failed: %zu for file `%s\n", fread_ret,
                    path);

        buffer.str = fstr;
        buffer.str[length] = '\0'; // ensure EOF
        buffer.len = length + 1;
        fclose(fp);
    } else {
        xab_log(
            LOG_ERROR,
            "Reading file: `%s` failed, returning an empty string instead\n");

        buffer.str = calloc(1, sizeof(char));
        buffer.str[0] = '\0';
        buffer.len = 1;
    }

    return buffer;
}
