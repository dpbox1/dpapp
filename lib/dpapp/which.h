/** @file which.h
 *  @brief 路径规范化、`$PATH` 查找与简单文件系统探测。
 *
 *  部分 API 返回静态缓冲区指针，非线程安全，每次调用覆盖上次结果。 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>

/** @brief 规范化路径（折叠 `.` / `..`）。
 *  @return 静态存储指针；下次调用覆盖。 */
const char* normalize_path(const char* path);
/** @brief 由相对/绝对路径构造绝对路径。
 *  @return 静态存储指针；下次调用覆盖。 */
const char* absolute_path(const char* fullpath);
/** @brief 在 `PATH` 中查找可执行文件（同 execvp）。
 *  @return 静态存储指针；未找到返回 NULL。 */
const char* find_executable(const char* cmd);

/** @brief 路径存在且为目录。
 *  @param path 待检测路径。
 *  @return 是目录 true，否则 false。 */
bool is_dir(const char* path);
/** @brief 路径存在且为普通文件。
 *  @param path 待检测路径。
 *  @return 是普通文件 true，否则 false。 */
bool is_file(const char* path);

/** @brief 递归创建目录（含父目录）。
 *  @return 成功 true，失败 false。 */
bool mkdir_p(const char* path);
#ifdef __cplusplus
}
#endif
