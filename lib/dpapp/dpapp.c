#include "dpapp/dpapp.h"
#include "dpapp/os/dpevp_pri.h"
#include "dpapp/which.h"
#include <bits/pthreadtypes.h>
#include <libgen.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct _dpapp
{
    int machine_id;
    char* root_dir;
    char* bin_file;

    DPEVP_SET_MEMBER

    int cpunum;
    int cpuoff;
    void (*start_fun)(void*);
    void* start_arg;
    pthread_t* threads;
};
static struct _dpapp _gapp = {0};

_dpevp_set_t _gevps = {0};

const dpapp_info_t* dpapp_info()
{
    return (dpapp_info_t*)&_gapp;
}

static void _bindcpu(int id, pthread_t thread)
{
    int cpuId = abs(id + _gapp.cpuoff) % _gapp.cpunum;

    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpuId, &mask);
    int r = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &mask);
    if (r == 0) {
        dplog_debug("dpapp", "Node %d bind cpu %d ok", id, cpuId);
    } else {
        dplog_warn("dpapp", "Node %d failed to bind cpu %d: %s", id, cpuId,
            strerror(r));
    }
}

void dpapp_fini()
{
    DP_FREE(_gevps.evps);
    DP_FREE(_gapp.threads);
    DP_FREE(_gevps.each_count);
    if (_gevps.each_ids) {
        for (int i = 0; i < _gevps.type_count; i++) {
            DP_FREE(_gevps.each_ids[i]);
        }
        DP_FREE(_gevps.each_ids);
    }
    memset(&_gevps, 0, sizeof(_dpevp_set_t));
    memset(&_gapp, 0, sizeof(struct _dpapp));
}

dpret_t dpapp_init(const dpapp_arg_t* args, void (*start_fun)(void*),
    void* start_arg)
{
    if (_gevps.evps != NULL) {
        dplog_error("dpapp", "Each process can have only one dpapp instance");
        return DPE_REPEAT;
    }

    if (!dplog_init(args->log_file, args->log_level, args->log_tsacc)) {
        dplog_warn("dpapp", "Failed to initialize logger");
    }

    if (args->root_dir == NULL) {
        dplog_error("dpapp", "Root directory is not set");
        return DPE_INVAL;
    }

    if (args->type_count <= 0) {
        dplog_alert("dpapp", "Invalid type count %d", args->type_count);
        return DPE_INVAL;
    }

    int thread_count = 0;
    for (int i = 0; i < args->type_count; i++) {
        thread_count += args->each_count[i];
    }

    if (thread_count > 255) {
        dplog_alert("dpapp", "Too many threads %d", thread_count);
        return DPE_INVAL;
    }

    int cpunum = sysconf(_SC_NPROCESSORS_ONLN);
    _gapp.cpunum = cpunum;
    _gapp.cpuoff = args->cpuoff;
    _gapp.start_fun = start_fun;
    _gapp.start_arg = start_arg;

    // 忽略 SIGPIPE 信号
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGPIPE);
    sigprocmask(SIG_SETMASK, &mask, NULL);

    _gapp.machine_id = args->machine_id;
    _gevps.count = 1;

    _gevps.type_count = args->type_count; // 第一个类型为主线程
    _gevps.each_count = (int*)malloc(sizeof(int) * _gevps.type_count);
    _gevps.each_ids = (int**)calloc(_gevps.type_count, sizeof(int*));
    if (_gevps.each_count == NULL || _gevps.each_ids == NULL) {
        dplog_error("dpapp", "Failed to allocate thread layout table");
        dpapp_fini();
        return DPE_NOMEM;
    }

    _gevps.each_count[0] = 1;
    _gevps.each_ids[0] = (int*)malloc(sizeof(int));
    if (_gevps.each_ids[0] == NULL) {
        dplog_error("dpapp", "Failed to allocate master thread id");
        dpapp_fini();
        return DPE_NOMEM;
    }
    _gevps.each_ids[0][0] = 0;

    for (int i = 1; i < _gevps.type_count; i++) {
        int pc = args->each_count[i];
        if (pc > 0) { // 新建工作线程
            _gevps.each_ids[i] = (int*)malloc(pc * sizeof(int));
            if (_gevps.each_ids[i] == NULL) {
                dplog_error("dpapp",
                    "Failed to allocate worker thread ids for type %d", i);
                dpapp_fini();
                return DPE_NOMEM;
            }
            for (int n = 0; n < pc; n++) {
                _gevps.each_ids[i][n] = _gevps.count;
                _gevps.count++;
            }
            _gevps.each_count[i] = pc;
        } else { // 禁用此类型
            _gevps.each_ids[i] = NULL;
            _gevps.each_count[i] = 0;
        }
    }

    _gevps.evps = calloc(_gevps.count, sizeof(dpevp_t*));
    if (_gevps.evps == NULL) {
        dplog_error("dpapp", "Failed to allocate event loop table");
        dpapp_fini();
        return DPE_NOMEM;
    }

    _gapp.threads = calloc(_gevps.count, sizeof(pthread_t));
    if (_gapp.threads == NULL) {
        dplog_error("dpapp", "Failed to allocate thread table");
        dpapp_fini();
        return DPE_NOMEM;
    }

    _gapp.count = _gevps.count;
    _gapp.each_count = _gevps.each_count;
    _gapp.type_count = _gevps.type_count;
    _gapp.each_ids = _gevps.each_ids;
    _gapp.root_dir = strdup(args->root_dir);
    if (_gapp.root_dir == NULL) {
        dplog_error("dpapp", "Failed to duplicate root_dir");
        dpapp_fini();
        return DPE_NOMEM;
    }
    if (args->bin_file) {
        _gapp.bin_file = strdup(args->bin_file);
        if (_gapp.bin_file == NULL) {
            dplog_error("dpapp", "Failed to duplicate bin_file");
            dpapp_fini();
            return DPE_NOMEM;
        }
    }

    return DPE_OK;
}

static void* _dpapp_thread_main(void* param)
{
    dpevp_t* self = (dpevp_t*)param;
    dpevp_start(self, _gapp.start_fun, _gapp.start_arg);
    return NULL;
}

dpret_t dpapp_start(const dpapp_arg_t* args, void (*start_fun)(void*),
    void* start_arg)
{
    dpret_t ret = dpapp_init(args, start_fun, start_arg);
    if (dpret_iserr(ret)) {
        return ret;
    }

    dplog_info("dpapp", "Program: %s", _gapp.bin_file);
    dplog_info("dpapp", "Workspace: %s", _gapp.root_dir);
    dplog_info("dpapp", "Log file: %s", args->log_file);
    dplog_info("dpapp", "Thread: count=%d, type count=%d", _gapp.count,
        _gevps.type_count);

    static const char* _support_status[2] = {"disabled", "enabled"};
    dplog_info("dpapp", "SSL support: %s", _support_status[DPAPP_SSL_ENABLE]);
    dplog_info("dpapp", "LSQUIC support: %s", _support_status[DPAPP_LSQUIC_ENABLE]);
    dpevp_t** evps = _gevps.evps;
    pthread_t* thds = _gapp.threads;
    dpevp_t* evp = evps[0];

    evps[0] = dpevp_new(0, 0);
    if (evps[0] == NULL) {
        dplog_error("dpapp", "Failed to create master event loop");
        ret = DPE_UNINIT;
        goto error;
    }

    thds[0] = pthread_self();
    _bindcpu(0, thds[0]);

    for (int i = 1; i < _gevps.type_count; i++) {
        for (int j = 0; j < _gevps.each_count[i]; j++) {
            int id = _gevps.each_ids[i][j];
            evp = dpevp_new(i, id);
            if (evp == NULL) {
                dplog_error("dpapp", "Failed to create worker %d event loop", id);
                ret = DPE_UNINIT;
                goto error;
            }
            _gevps.evps[id] = evp;
            int r = pthread_create(&thds[id], NULL, _dpapp_thread_main, evp);
            if (r) {
                dplog_error("dpapp", "Failed to create worker thread %d: %s", id,
                    strerror(r));
                ret = DPE_UNINIT;
                goto error;
            }

            _bindcpu(id, thds[id]);
            dplog_info("dpapp", "Worker thread %d started (type=%d)", id, i);
        }
    }

    dplog_info("dpapp", "Master thread started");
    dpevp_start(evps[0], _gapp.start_fun, _gapp.start_arg);

    for (int i = 1; i < _gevps.count; i++) {
        pthread_join(thds[i], NULL);
    }

    if (dpret_isok(ret)) {
        dplog_info("dpapp", "Process shutdown complete");
    }

error:
    if (evps) {
        for (int i = 0; i < _gevps.count; i++) {
            dpevp_del(_gevps.evps[i]);
        }
    }

    dpapp_fini();
    return ret;
}

void dpapp_arg_init(dpapp_arg_t* args)
{
    memset(args, 0, sizeof(dpapp_arg_t));
    args->type_count = 1;
    for (int i = 0; i < DPAPP_TYPE_MAX; i++) {
        args->each_count[i] = 1;
    }
    args->log_level = DPLOG_L_NOTICE;
    args->log_tsacc = DPLOG_TA_SECOND;
    args->log_file = strdup("/dev/stdout");
}

void dpapp_arg_free(dpapp_arg_t* args)
{
    DP_FREE(args->log_file);
    DP_FREE(args->root_dir);
    DP_FREE(args->bin_file);
    memset(args, 0, sizeof(dpapp_arg_t));
}

#define ARGUMENT_THROW(e)                                                           \
    errmsg = e;                                                                     \
    goto FINAL;

dpret_t dpapp_arg_parse(dpapp_arg_t* oarg, int argc, const char** argv)
{
    if (argc <= 0) {
        return DPE_INVAL;
    }

    oarg->bin_file = strdup(find_executable(argv[0]));
    if (oarg->bin_file == NULL) {
        printf("Get self path error\n");
        return DPE_INVAL;
    }

    dpret_t ret = DPE_OK;
    dpret_t i = 1;
    const char* errmsg = NULL;
    for (; i < argc && argv[i]; i++) {
        const char* arg = argv[i];
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            ret = 1;
            ARGUMENT_THROW("");
        } else if (strcmp(arg, "-V") == 0 || strcmp(arg, "--version") == 0) {
            ret = 2;
            ARGUMENT_THROW("");
        } else if (strcmp(arg, "-m") == 0 || strcmp(arg, "--machine") == 0) {
            if ((++i) >= argc || argv[i][0] == '-') {
                ARGUMENT_THROW("machine id error");
            }
            int m = atoi(argv[i]);
            if (m < 0 || m > 31) {
                ARGUMENT_THROW("machine id invalid");
            }
            oarg->machine_id = m;
        } else if (strncmp(arg, "-n", 2) == 0) {
            if ((++i) >= argc || (argv[i][0] == '-' && argv[i][1] != '1')) {
                ARGUMENT_THROW("-n<t> argument error");
            }
            int n = atoi(arg + 2);
            int c = atoi(argv[i]);
            if (n < 1 || n > 63 || c > 255) {
                ARGUMENT_THROW("invalid type id or count");
            }
            oarg->each_count[n] = c;
        } else if (strcmp(arg, "-u") == 0 || strcmp(arg, "--cpu_off") == 0) {
            if ((++i) >= argc || argv[i][0] == '-') {
                ARGUMENT_THROW("cpu_off error");
            }
            oarg->cpuoff = atoi(argv[i]);
        } else if (strcmp(arg, "-d") == 0 || strcmp(arg, "--root_dir") == 0) {
            if ((++i) >= argc || argv[i][0] == '-') {
                ARGUMENT_THROW("root_dir error");
            }
            if (is_dir(argv[i])) {
                DP_FREE(oarg->root_dir);
                oarg->root_dir = strdup(absolute_path(argv[i]));
            } else {
                ARGUMENT_THROW("root_dir not exists");
            }
        } else if (strcmp(arg, "-l") == 0 || strcmp(arg, "--log_level") == 0) {
            if ((++i) >= argc || argv[i][0] == '-') {
                ARGUMENT_THROW("log_level error");
            }
            oarg->log_level = dplog_namel(argv[i]);
            if (oarg->log_level < 0) {
                ARGUMENT_THROW("log_level invalid");
            }
        } else if (strcmp(arg, "-o") == 0 || strcmp(arg, "--log_file") == 0) {
            if ((++i) >= argc || argv[i][0] == '-') {
                ARGUMENT_THROW("log_file error");
            }
            char* log_dir = dirname(strdup(argv[i]));
            if (is_dir(argv[i]) || !is_dir(log_dir)) {
                free(log_dir);
                ARGUMENT_THROW("log_file invalid");
            }
            free(log_dir);
            oarg->log_file = strdup(argv[i]);
        } else if (strcmp(arg, "-t") == 0 || strcmp(arg, "--log_tsacc") == 0) {
            if ((++i) >= argc || argv[i][0] == '-') {
                ARGUMENT_THROW("log_tsacc error");
            }
            int ta = atoi(argv[i]);
            if (ta < 0 || ta > 2) {
                ARGUMENT_THROW("log_tsacc invalid, must be 0, 1, or 2");
            }
            oarg->log_tsacc = (dplog_tsacc_e)ta;
        } else {
            break;
        }
    }

FINAL:
    if (errmsg && *errmsg != '\0') {
        printf("%s\n", errmsg);
        return DPE_INVAL;
    }

    if (oarg->root_dir == NULL) {
        char* bindup = strdup(oarg->bin_file);
        oarg->root_dir = strdup(dirname(dirname(bindup)));
        free(bindup);
    }

    oarg->argv = argv + i;
    oarg->argc = argc - i;
    return ret;
}
