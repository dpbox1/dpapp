/** @file dplog.h
 *  @ingroup dpapp_log
 *  @brief 日志 API。
 *
 *  级别（由低到高，仅当消息级别 ≤ 当前配置时才输出）：
 *  - debug：调试细节
 *  - info：一般信息
 *  - notice：关键信息， 业务事件等
 *  - warn：可继续的异常
 *  - error：操作失败
 *  - alert：线程/进程无法继续
 *
 *  级别与输出文件可通过 dpapp 命令行选项配置。 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/** @brief 日志级别；仅当级别 ≤ 当前配置时才输出。 */
typedef enum
{
    DPLOG_L_DEBUG,
    DPLOG_L_INFO,
    DPLOG_L_NOTICE,
    DPLOG_L_WARN,
    DPLOG_L_ERROR,
    DPLOG_L_ALERT,
} dplog_level_e;

/** @brief `dplog_nowts` 时间戳精度。 */
typedef enum
{
    DPLOG_TA_SECOND = 0, ///< 秒
    DPLOG_TA_MILLIS = 1, ///< 毫秒
    DPLOG_TA_MICROS = 2, ///< 微秒
} dplog_tsacc_e;

/** @brief 当前日志级别。 */
dplog_level_e dplog_curlevel();
/** @brief 当前时间戳精度。 */
dplog_tsacc_e dplog_curtsacc();
/** @brief 当前级别名称字符串。 */
const char* dplog_curlname();
/** @brief 设置全局日志级别。 */
void dplog_setlevel(dplog_level_e l);
/** @brief 设置 `dplog_nowts` 时间戳精度。 */
void dplog_settsacc(dplog_tsacc_e ta);
/** @brief 级别 → 名称。 */
const char* dplog_lname(dplog_level_e l);
/** @brief 名称 → 级别。 */
dplog_level_e dplog_namel(const char* n);
/** @brief 级别 → 短名称。 */
const char* dplog_sname(dplog_level_e l);

/** @brief 初始化日志。
 *  @param file 输出路径（文件或 `/dev/stdout`）。
 *  @return 成功关闭旧文件并返回 true；失败保留旧文件。 */
bool dplog_init(const char* file, dplog_level_e level, dplog_tsacc_e ta);
/** @brief 无 domain 前缀的 printf 风格输出。 */
void dplog_print(const char* fmt, ...);

/** @brief 带 domain 的格式化日志。 */
void dplog_write(dplog_level_e level, const char* domain, const char* fmt, ...);
/** @brief 输出 debug 级日志。 */
void dplog_debug(const char* domain, const char* fmt, ...);
/** @brief 输出 info 级日志。 */
void dplog_info(const char* domain, const char* fmt, ...);
/** @brief 输出 notice 级日志。 */
void dplog_notice(const char* domain, const char* fmt, ...);
/** @brief 输出 warn 级日志。 */
void dplog_warn(const char* domain, const char* fmt, ...);
/** @brief 输出 error 级日志。 */
void dplog_error(const char* domain, const char* fmt, ...);
/** @brief 输出 alert 级日志。 */
void dplog_alert(const char* domain, const char* fmt, ...);

/** @brief 无 domain 的格式化日志（dpelog = dp log embedded）。 */
void dpelog_write(dplog_level_e level, const char* fmt, ...);
/** @brief 无 domain 的 debug 级日志。 */
void dpelog_debug(const char* fmt, ...);
/** @brief 无 domain 的 info 级日志。 */
void dpelog_info(const char* fmt, ...);
/** @brief 无 domain 的 notice 级日志。 */
void dpelog_notice(const char* fmt, ...);
/** @brief 无 domain 的 warn 级日志。 */
void dpelog_warn(const char* fmt, ...);
/** @brief 无 domain 的 error 级日志。 */
void dpelog_error(const char* fmt, ...);
/** @brief 无 domain 的 alert 级日志。 */
void dpelog_alert(const char* fmt, ...);

/** @brief 当前本地时间（只读 `struct tm`）。 */
const struct tm* dplog_nowtm();
/** @brief 单调时钟时间戳。
 *  @param ta DPLOG_TA_SECOND / MILLIS / MICROS。
 *  @return 与 `ta` 匹配的时间单位。 */
int64_t dplog_nowts(dplog_tsacc_e ta);

/** @brief 单调时钟秒数（同 `dplog_nowts(DPLOG_TA_SECOND)`）。 */
#define dplog_second() dplog_nowts(DPLOG_TA_SECOND)
/** @brief 单调时钟毫秒（同 `dplog_nowts(DPLOG_TA_MILLIS)`）。 */
#define dplog_millis() dplog_nowts(DPLOG_TA_MILLIS)
/** @brief 单调时钟微秒（同 `dplog_nowts(DPLOG_TA_MICROS)`）。 */
#define dplog_micros() dplog_nowts(DPLOG_TA_MICROS)

#ifdef __cplusplus
}
#endif
