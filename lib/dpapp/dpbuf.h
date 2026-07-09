/** @file dpbuf.h
 *  @ingroup dpapp_buf
 *  @brief 可增长字节缓冲区。
 *
 *  异步读写路径的主要载体。布局：`rbeg`/`rend`/`wbeg`/`size` 划分
 *  已消费/可读/可写/容量；`dpbuf_new_r` 共享底层存储（引用计数），
 *  `dpbuf_del` 减引用至零时释放。 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define DPBUF_MAX_SIZE 0x4fffffff ///< 扩容硬上限（约 1 GiB）
#define DPBUF_X_SIZE   65536      ///< 默认块 64 KiB
#define DPBUF_L_SIZE   16384      ///< 16 KiB
#define DPBUF_M_SIZE   4096       ///< 4 KiB
#define DPBUF_S_SIZE   1024       ///< 1 KiB

/** @brief 内部引用计数存储；API 用户视为 opaque。 */
typedef struct dpbuf_data dpbuf_data_t;

/** @brief 可增长字节缓冲：单块内存上的消费/可读/可写窗口。 */
typedef struct dpbuf
{
    uint32_t utype : 8;  ///< 应用类型标签（dpbuf_utype_e）
    uint32_t uflag : 24; ///< 应用标志（dpbuf_uflag_e）
    uint32_t flag;       ///< 内部标志（DPBUF_*）
    int32_t rbeg;        ///< 可读区起始
    int32_t rend;        ///< 可读区结束（不含）
    int32_t wbeg;        ///< 下次写入位置
    int32_t size;        ///< 已分配容量
    dpbuf_data_t* data;  ///< 底层存储 + 引用计数
} dpbuf_t;

/** @brief 逻辑载荷类型。 */
typedef enum
{
    DPBUF_UT_TEXT,
#define DPBUF_UT_TEXT DPBUF_UT_TEXT
    DPBUF_UT_BLOB,
#define DPBUF_UT_BLOB DPBUF_UT_BLOB
    DPBUF_UT_ERRO,
#define DPBUF_UT_ERRO DPBUF_UT_ERRO
    DPBUF_UT_JSON,
#define DPBUF_UT_JSON DPBUF_UT_JSON
    DPBUF_UT_USER,
#define DPBUF_UT_USER DPBUF_UT_USER
} dpbuf_utype_e;

/** @brief 应用自定义标志位，OR 进 `uflag`。 */
typedef enum
{
    DPBUF_UF_SHORT = 1 << 0,
#define DPBUF_UF_SHORT DPBUF_UF_SHORT
    DPBUF_UF_ALL = 0xFFFFFF,
#define DPBUF_UF_ALL DPBUF_UF_ALL
} dpbuf_uflag_e;

/** 外部载荷不由 dpbuf_del 释放；容量不超过初始 size。 */
#define DPBUF_CONSTDATA (1 << 1)
/** 读后不回收可写 slack（默认会 compact）。 */
#define DPBUF_NORECYCLE (1 << 2)

/** 内部标志掩码；高位保留。 */
#define DPBUF_FLAGMASK 0xF

/** `dpbuf_new_d` 模式：只读窗口。 */
#define DPBUF_INIT_R (1 << 8)
/** `dpbuf_new_d` 模式：只写窗口。 */
#define DPBUF_INIT_W (1 << 9)

/** 创建时拷贝载荷；覆盖该次分配的 DPBUF_CONSTDATA 语义。 */
#define DPBUF_DUP_DATA (1 << 10)

/** 只读 + 调用方持有底层（DPBUF_CONSTDATA）。 */
#define DPBUF_INIT_CR (DPBUF_INIT_R | DPBUF_CONSTDATA)
/** 只写 + 调用方持有底层。 */
#define DPBUF_INIT_CW (DPBUF_INIT_W | DPBUF_CONSTDATA)

/** @name 分配与视图 */
/**@{*/

/** @brief 分配空缓冲，初始可写容量 `size` 字节。 */
dpbuf_t* dpbuf_new(int size);

/** @brief 包装调用方内存；`size` < 0 时按 C 字符串取 strlen。 */
dpbuf_t* dpbuf_new_d(void* data, int size, int mode);
/** @brief printf 风格格式化到新缓冲。 */
dpbuf_t* dpbuf_new_f(const char* fmt, ...);
/** @brief `dpbuf_new_f` 的 va_list 版。 */
dpbuf_t* dpbuf_new_v(const char* fmt, va_list args);

/** @brief 同一底层存储的第二视图（refcount++）；复制 utype/uflag。 */
dpbuf_t* dpbuf_new_r(const dpbuf_t* self);

/** @brief 深拷贝可读窗口到新缓冲。 */
dpbuf_t* dpbuf_dup_r(const dpbuf_t* self);
/** @brief 深拷贝尾部 slack（cedata..wbeg）。 */
dpbuf_t* dpbuf_dup_e(const dpbuf_t* self);

/** @brief 减引用；为零时释放底层。 */
void dpbuf_del(dpbuf_t* self);

/**@}*/

/** @name 就地初始化与视图 */
/**@{*/

/** @brief 在已存在的未初始化 `dpbuf_t` 上建立空缓冲，初始可写容量 `size` 字节。
 *  `self` 由调用方分配（栈、堆或嵌入结构体成员均可）；本函数不分配 `self` 本身。
 *  @return 成功 true，失败 false。 */
bool dpbuf_init(dpbuf_t* self, int size);

/** @brief 在已存在的未初始化 `dpbuf_t` 上包装调用方内存；语义同 `dpbuf_new_d`。
 *  @return 成功 true，失败 false。 */
bool dpbuf_init_d(dpbuf_t* self, void* data, int size, int mode);

/** @brief `dpbuf_init_f` 的 va_list 版。
 *  @return 成功 true，失败 false。 */
bool dpbuf_init_v(dpbuf_t* self, const char* fmt, va_list args);

/** @brief 在已存在的未初始化 `dpbuf_t` 上按 printf 风格格式化初始化。
 *  @return 成功 true，失败 false。 */
bool dpbuf_init_f(dpbuf_t* self, const char* fmt, ...);

/** @brief 在已存在的未初始化 `dpbuf_t` 上共享 `other` 底层存储（refcount++）；复制
 * utype/uflag。
 *  @return 成功 true，失败 false。 */
bool dpbuf_init_r(dpbuf_t* self, const dpbuf_t* other);

/** @brief 在已存在的 `dpbuf_t` 上建立 `other` 可读窗口的共享视图（同 `dpbuf_init_r`）。
 *  @return 成功 true，失败 false。 */
bool dpbuf_from_r(dpbuf_t* self, const dpbuf_t* other);

/** @brief 在已存在的 `dpbuf_t` 上建立 `other` 尾部 slack 的共享视图。
 *  @return 成功 true，失败 false。 */
bool dpbuf_from_e(dpbuf_t* self, const dpbuf_t* other);

/** @brief 释放 `self` 持有的底层存储引用；不释放 `self` 本身及其所在存储。
 *  可与 `dpbuf_init` 系列配对，在 `self` 生命周期结束前调用。 */
void dpbuf_fini(dpbuf_t* self);

/**@}*/

/** @name 用户标签与存储信息 */
/**@{*/

/** @brief 从 `self` 复制 utype/uflag 到 `dst`。 */
void dpbuf_cpusr(const dpbuf_t* self, dpbuf_t* dst);

/** @brief 返回共享 `dpbuf_data_t` 的当前引用计数。 */
size_t dpbuf_refc(dpbuf_t* self);

/** @brief 返回已分配总容量（字节）。 */
int dpbuf_size(const dpbuf_t* self);

/** @brief 返回底层数组基址（偏移 0）。 */
void* dpbuf_data(const dpbuf_t* self);

/** @brief 返回逻辑载荷类型（`dpbuf_utype_e`）。 */
int dpbuf_utype(const dpbuf_t* self);

/** @brief 设置逻辑载荷类型。 */
void dpbuf_set_utype(dpbuf_t* self, int utype);

/** @brief 测试 `uflag` 位是否已设置。
 *  @return `self->uflag & uflag` 非零为 true。 */
int dpbuf_uflag(const dpbuf_t* self, int uflag);

/** @brief 清除 `uflag` 中指定位。 */
void dpbuf_rmv_uflag(dpbuf_t* self, int uflag);

/** @brief 将 `uflag` 指定位 OR 进 `self->uflag`。 */
void dpbuf_add_uflag(dpbuf_t* self, int uflag);

/**@}*/

/** @name 回收与容量 */
/**@{*/

/** compact 回收空间；`force` 强制回收。 */
void dpbuf_recycle(dpbuf_t* self, bool force);
/** 启用/禁用（DPBUF_NORECYCLE）读后 compact。 */
void dpbuf_set_recycle(dpbuf_t* self, bool b);
/** 按 `mode` 中 DPBUF_INIT_R/W 重置游标。 */
void dpbuf_reset(dpbuf_t* self, int mode);
/** 扩容/缩容，保证 `wbeg` 后至少 `s` 字节可写。 */
bool dpbuf_resizew(dpbuf_t* self, int s);

/**@}*/

/** @name 读/写/slack 窗口指针 */
/**@{*/

/** 可读字节 [rbeg, rend)。 */
char* dpbuf_crdata(const dpbuf_t* self);
/** 可写尾部 [wbeg, size)。 */
char* dpbuf_cwdata(const dpbuf_t* self);
/** slack 区 [rend, wbeg)。 */
char* dpbuf_cedata(const dpbuf_t* self);
int dpbuf_crsize(const dpbuf_t* self);
int dpbuf_cwsize(const dpbuf_t* self);
int dpbuf_cesize(const dpbuf_t* self);

/**@}*/

/** @name 比较与扫描（可读窗口） */
/**@{*/

/** @brief 判断可读窗口是否为空。 */
bool dpbuf_cempty(const dpbuf_t* self);

/** @brief 判断可读区是否与 `other` 前 `len` 字节相等。 */
bool dpbuf_cequalc(const dpbuf_t* self, const char* other, int len);

/** 可读区字典序比较（先比长度再 memcmp）。 */
int dpbuf_ccmp(const dpbuf_t* self, const dpbuf_t* other);

/** 从偏移 `left` 查找 `match`；未找到返回负 errno 风格码。 */
int dpbuf_cfind(const dpbuf_t* self, const char* match, int len, int left);

/** @brief 返回可读区内 C 字符串长度（不含尾 `\0`）。 */
int dpbuf_cstrlen(const dpbuf_t* self);

/** @brief 判断可读区是否以 `sub` 开头；`skip_begws` 为 true 时先跳过前导空白。 */
bool dpbuf_cbegwith(const dpbuf_t* self, const char* sub, int len, bool skip_begws);

/**@}*/

/** @name 游标 seek */
/**@{*/

#ifdef SEEK_BEG
#undef SEEK_BEG
#endif
/** seek 原点别名，同 SEEK_SET。 */
#define SEEK_BEG SEEK_SET

/** 移动 `rbeg`，范围 [0, rend]；返回位移量。 */
int dpbuf_rseek(dpbuf_t* self, int offset, int seek);
/** 移动 `wbeg`，范围 [rend, size]。 */
int dpbuf_wseek(dpbuf_t* self, int offset, int seek);
/** 移动 `rend`（划分可读与 slack），范围 [rbeg, wbeg]。 */
int dpbuf_eseek(dpbuf_t* self, int offset, int seek);

/**@}*/

/** @name 追加/写入 */
/**@{*/

/** 追加 `buf` 可读区；`len` ≤ 0 时用 dpbuf_crsize(buf)。 */
int dpbuf_wbuf(dpbuf_t* self, const dpbuf_t* buf, int len);
/** 同 dpbuf_wbuf，并推进 `buf->rbeg`（消费源）。 */
int dpbuf_wbuf_r(dpbuf_t* self, dpbuf_t* buf, int len);
/** @brief 向可写尾部填充 `len` 字节，值为 `v`。
 *  @return 实际写入字节数，失败 < 0。 */
int dpbuf_wfill(dpbuf_t* self, int len, int8_t v);

/** @brief 追加 `data` 共 `len` 字节到可写尾部。
 *  @return 实际写入字节数，失败 < 0。 */
int dpbuf_wdata(dpbuf_t* self, const void* data, int len);

/** @brief printf 风格格式化追加到可写尾部。
 *  @return 实际写入字节数，失败 < 0。 */
int dpbuf_wstrf(dpbuf_t* self, const char* fmt, ...);

/** @brief `dpbuf_wstrf` 的 va_list 版。
 *  @return 实际写入字节数，失败 < 0。 */
int dpbuf_wstrv(dpbuf_t* self, const char* fmt, va_list args);

/**@}*/

/** @name 消费/读取 */
/**@{*/

/** 跳过 slack 前导空白，扩展 rend。 */
int dpbuf_rws(dpbuf_t* self);
/** 从 slack 搬最多 `size` 字节到 `det`；返回写入字节数或 <0。 */
int dpbuf_readto(dpbuf_t* self, dpbuf_t* det, int size);
/** 将 rend 推进 `len` 进入 slack（len < 0 表示全部 slack）。 */
int dpbuf_rdata(dpbuf_t* self, int len);
/** slack 不足 `len` 字节则失败（-ENODATA）。 */
int dpbuf_rmust(dpbuf_t* self, int len);
/** 消费至含 `until` 为止；返回消费长度或 <0。 */
int dpbuf_runtil(dpbuf_t* self, const char* until, int until_sz);
/** 消费 slack 中 C 字符串段。 */
int dpbuf_rcstr(dpbuf_t* self);
/** 将全部 slack 并入可读区。 */
int dpbuf_rall(dpbuf_t* self);

/**@}*/

#ifdef __cplusplus
}
#endif
