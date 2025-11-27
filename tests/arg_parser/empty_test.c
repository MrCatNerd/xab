#include "meson_error_codes.h"
#include "arg_parser.h"

#include <assert.h>

#if EXIT_SUCCESS != MESON_OK
#error "EXIT_SUCCESS != MESON_OK"
#endif

int main(void) {
    int argc = 0;
    char *argv[] = {NULL};

    struct argument_options opts = parse_args(argc, argv);
    // shouldn't be called because argc <= 1 so it'll exit(EXIT_SUCCESS)
    // also there's no memory so clean_opts is not necessary when it's exiting
    clean_opts(&opts);
    return MESON_FAIL;
}
