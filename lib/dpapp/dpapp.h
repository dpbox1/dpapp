/** @file dpapp.h
 *  @ingroup dpapp_start
 *  @brief dpapp 启动、CLI 解析与进程级运行时参数。
 *
 *  libdpapp 提供事件循环（epoll）、I/O、SSL、QUIC、线程模型等核心能力；
 *  模块细节见 dpevp.h、dpefd.h、dpret.h 等。
 *
 *  典型流程：`dpapp_arg_init` → `dpapp_arg_parse` → `dpapp_start`。 */

#pragma once

#include "dpapp/dpasc.h"
#include "dpapp/dpbuf.h"
#include "dpapp/dpdef.h"
#include "dpapp/dpefd.h"
#include "dpapp/dpevp.h"
#include "dpapp/dplog.h"
#include "dpapp/dpqic.h"
#include "dpapp/dpret.h"
#include "dpapp/dpssl.h"
#include "dpapp/which.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief 全局运行时快照（机器 id、路径、线程布局）。 */
typedef struct
{
    int machine_id; ///< `-m` / `--machine`
    char* root_dir; ///< 工作树根目录（`-d`）
    char* bin_file; ///< 由 argv[0] 推导的可执行路径

    DPEVP_SET_MEMBER
} dpapp_info_t;

/** @brief 只读全局状态；`dpapp_start` 成功后有效，进程生命周期内不变。 */
const dpapp_info_t* dpapp_info();

/** @brief 线程 type 数量上限（`-n*` 索引）；约束 `each_count[]`。 */
#define DPAPP_TYPE_MAX 64

/** @brief 由 `dpapp_arg_init` / `dpapp_arg_parse` 填充的可变 CLI/运行时选项。 */
typedef struct
{
    int argc;                       ///< 解析后剩余参数个数
    const char** argv;              ///< 解析后剩余参数
    int machine_id;                 ///< 同 `dpapp_info_t::machine_id`
    int type_count;                 ///< 线程种类数（`-n*` 系列）
    int each_count[DPAPP_TYPE_MAX]; ///< 各 type 的 worker 数
    int cpuoff;                     ///< CPU 亲和偏移（`-u`）
    dplog_level_e log_level;        ///< `-l`
    dplog_tsacc_e log_tsacc;        ///< `-t`
    char* root_dir;                 ///< 堆分配；parse 后归调用方
    char* log_file;                 ///< 堆分配；parse 后归调用方
    char* bin_file;                 ///< 堆分配；parse 后归调用方
} dpapp_arg_t;

/** @brief 清零并填入内置默认值（见 `DPAPP_ARG_HELP`）。 */
void dpapp_arg_init(dpapp_arg_t* args);
/** @brief 声明 `name__` 为 `dpapp_arg_t` 并调用 `dpapp_arg_init`。 */
#define DPAPP_ARG_NEW(name__)                                                       \
    dpapp_arg_t name__;                                                             \
    dpapp_arg_init(&name__);

/** @brief 释放 `args` 中堆字符串（root_dir、log_file、bin_file）。 */
void dpapp_arg_free(dpapp_arg_t* args);
/** @brief 解析命令行。
 *  @return 0 成功，1 已打印 help，2 已打印 version。 */
dpret_t dpapp_arg_parse(dpapp_arg_t* oarg, int argc, const char** argv);

// clang-format off
/** `dpapp_arg_parse` / `--help` 输出的多行帮助文本。 */
#define DPAPP_ARG_HELP                                                                              \
"System options:\n"                                                                                 \
"-h [ --help ]                            Produce help message\n"                                   \
"-V [ --version ]                         Print version message\n"                                  \
"-m [ --machine ] arg (0)                 Machine id, max 0 ~ 31\n"                                 \
"-n<t> arg (1)                            Thread number of type, n1 ~ n63.\n"                       \
"                                         Disable thread type where <= 0\n"                         \
"-u [ --cpu_off ] arg (1)                 Bind cpu id offset\n"                                     \
"-d [ --root_dir ] arg                    Root directory where dpapp running\n"                     \
"-o [ --log_file ] arg ('/dev/stdout')    Log output file\n"                                        \
"-l [ --log_level ] arg (notice)          Log level: debug, info, notice, warning, error, alert\n"  \
"-t [ --log_tsacc ] arg (0)               Log time accuracy:\n"                                     \
"                                         0: second, 1:milliseconds, 2:microseconds\n"

// clang-format on

/** @brief 启动 worker 线程与事件运行时（进程内单实例）。
 *  @param args 已解析选项；root_dir 与线程布局须有效。
 *  @param start_fun 初始化后可选一次性回调（可为 NULL）。
 *  @param start_arg 传给 `start_fun` 的 opaque 指针。
 *  @return 0 成功，负 dpret_t 失败。 */
dpret_t dpapp_start(const dpapp_arg_t* args, void (*start_fun)(void*),
    void* start_arg);

#ifdef __cplusplus
}
#endif
