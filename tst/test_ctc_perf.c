#include "dpapp/dpdef.h"
#include "dpapp/dplog.h"
#include "dpcwc/dpcwc.h"
#include "dpcwc/dpcwc_aux.h"
#include <stdlib.h>

struct ctc_bench_cfg
{
    int bench;
    int pearc;
};

struct ctc_benchmark_data
{
    int64_t new_time;
    int64_t run_time;
    int64_t wait_time;
};

static dpret_t ctc_run(dpele_t* task, dpv64_t arg)
{
    struct ctc_benchmark_data* data = (struct ctc_benchmark_data*)arg.ptr;
    data->run_time = dplog_millis();
    (void)task;
    return 100;
}

static dpret_t start_test(dpele_t* tt, dpv64_t arg)
{
    (void)tt;
    const struct ctc_bench_cfg* cfg = (const struct ctc_bench_cfg*)arg.ptr;
    const int bench = cfg->bench;
    const int pearc = cfg->pearc;

    dpctc_t** tasks = (dpctc_t**)malloc(pearc * sizeof(dpctc_t*));
    struct ctc_benchmark_data* datas = (struct ctc_benchmark_data*)malloc(
        pearc * sizeof(struct ctc_benchmark_data));
    struct ctc_benchmark_data* data = NULL;

    int64_t tbeg = dplog_millis();
    dplog_notice("ctc", "start task benchmark: %ld", tbeg);

    int64_t total_create_time = 0;
    int64_t total_await_time = 0;

    int64_t total_run_time = 0;
    int64_t total_wait_time = 0;

    int count = bench * pearc;
    for (int b = 0; b < bench; b++) {
        int64_t create_start = dplog_millis();
        for (int n = 0; n < pearc; n++) {
            data = &datas[n];
            data->new_time = dplog_millis();
            tasks[n] = dpele_new(dpctc_init_type(), 0, 0);
            dpevp_add(tasks[n], dpctc_submit(), DPV64_PTR(ctc_run), DPV64_PTR(data));
        }
        int64_t create_end = dplog_millis();
        total_create_time += (create_end - create_start);

        for (int n = 0; n < pearc; n++) {
            dpcwc_await(tasks[n], DPV64_NULL);
            data = &datas[n];
            data->wait_time = dplog_millis();

            total_wait_time += (data->wait_time - data->run_time);
            total_run_time += (data->run_time - data->new_time);
            dpele_del(tasks[n]);
        }
        int64_t await_end = dplog_millis();
        total_await_time += (await_end - create_end);
    }
    int64_t tend = dplog_millis();
    dplog_notice("ctc", "over task benchmark: %d in %ld ms", count, tend - tbeg);
    dplog_notice("ctc", "total create time: %ld ms", total_create_time);
    dplog_notice("ctc", "total await time: %ld ms", total_await_time);
    dplog_notice("ctc", "pear run time: %ld ms", total_run_time / count);
    dplog_notice("ctc", "pear wait time: %ld ms", total_wait_time / count);
    free(tasks);
    free(datas);

    return DPE_OK;
}

static dpv64_t ctc_init00(dpv64_t arg1, dpv64_t arg2)
{
    struct ctc_bench_cfg cfg = {
        .bench = (int)arg1.s64,
        .pearc = (int)arg2.s64,
    };

    int64_t tbeg = dplog_millis();
    dpcwc_ctc_each(-1, start_test, DPV64_PTR(&cfg));
    int64_t tend = dplog_millis();

    const dpapp_info_t* info = dpapp_info();
    int64_t sec = (tend - tbeg) / 1000 + 1;
    int64_t speed = (info->each_count[1] * (cfg.bench * cfg.pearc)) / sec;
    dplog_notice("ctc", "over event benchmark: %ld task/s", speed);
    return DPV64_NULL;
}

extern dpret_t dpcwc__test_ctc(int argc, char** argv, dpapp_hdr_t* hdrs)
{
    int bench = 1000;
    int pearc = 10000;

    if (argc > 1) {
        bench = atoi(argv[1]);
    }
    if (argc > 2) {
        pearc = atoi(argv[2]);
    }

    hdrs[0].init = ctc_init00;
    hdrs[0].init_arg1 = (dpv64_t){.s64 = bench};
    hdrs[0].init_arg2 = (dpv64_t){.s64 = pearc};
    return 2;
}
