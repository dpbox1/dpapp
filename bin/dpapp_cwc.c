#include "dpcwc/dpcwc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_help()
{
    printf("Usage: dpapp [system...] [external...] [module] [module...]\n");
    printf(DPAPP_ARG_HELP);
    printf(DPCWC_ARG_HELP);
}

int main(int argc, const char** argv)
{
    DPAPP_ARG_NEW(args);
    dpret_t ret = dpapp_arg_parse(&args, argc, argv);
    if (dpret_iserr(ret)) {
        print_help();
        return -ret;
    } else if (ret == 1) {
        print_help();
        return 0;
    } else if (ret == 2) {
        printf("%s\n", DPAPP_VERSION_DETAIL);
        return 0;
    }

    if (args.argc <= 0) {
        printf("No module file specified\n");
        print_help();
        return EINVAL;
    }

    ret = dpcwc_start(&args);

    dpapp_arg_free(&args);
    return ret;
}
