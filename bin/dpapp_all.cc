#include "dpapp/dpapp.h"

#if DPAPP_HAS_CPP
#include "dpcpp/dpcpp.hh"
#endif

#if DPAPP_HAS_CWC
#include "dpcwc/dpcwc.h"
#endif

#if DPAPP_HAS_LUA
#include "dplua/dplua.h"
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>

void print_help()
{
    printf("Usage: dpapp [system...] [external...] [module] [module...]\n");
    printf(DPAPP_ARG_HELP);

#if DPAPP_HAS_CWC
    printf("\n");
    printf(DPCWC_ARG_HELP);
#endif

#if DPAPP_HAS_LUA
    printf("\n");
    printf(DPLUA_ARG_HELP);
#endif
}

int main(int argc, char** argv)
{
    DPAPP_ARG_NEW(args);
    dpret_t ret = dpapp_arg_parse(&args, argc, (const char**)argv);
    if (dpret_iserr(ret)) {
        print_help();
        ret = -ret;
        goto exit;
    } else if (ret == 1) {
        print_help();
        ret = 0;
        goto exit;
    } else if (ret == 2) {
        printf("%s\n", DPAPP_VERSION_DETAIL);
        ret = 0;
        goto exit;
    }

    if (args.argc <= 0) {
        printf("No module file specified\n");
        print_help();
        ret = EINVAL;
        goto exit;
    }

    ret = DPE_INVAL;
#if DPAPP_HAS_CPP
    if (dpret_iserr(ret))
        ret = dpcpp::start(&args);
#endif
#if DPAPP_HAS_CWC
    if (dpret_iserr(ret))
        ret = dpcwc_start(&args);
#endif
#if DPAPP_HAS_LUA
    if (dpret_iserr(ret))
        ret = dplua_start(&args);
#endif

    if (dpret_iserr(ret)) {
        dplog_error("dpapp", "Failed to start module: %s (%d)", args.argv[0], ret);
    }

exit:
    dpapp_arg_free(&args);
    return ret;
}
