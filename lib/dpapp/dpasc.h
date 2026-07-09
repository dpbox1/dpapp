/** @file dpasc.h
 *  @ingroup dpapp_asc
 *  @brief 异步 prep 描述符：`dpevp_add(ele, prep, ...)` 的第二参数及后续可变参。
 *
 *  每个 `dp*_xxx()` 返回静态 `dpasc_t`，声明 prep/post、适用 `dpele_type_e` 与
 *  `dpaio_type_e`。语言绑定层封装为 `aexec`/`await`；Lua 侧见 `dpasc.lua`。 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "dpapp/dpevp.h"

/** @brief prep/post 阶段向 I/O 后端传递的事件掩码与子步结果。 */
typedef struct dpasc_out
{
    union
    {
        struct
        {
            uint16_t want_events; ///< 期望监听的 epoll 事件掩码
            uint16_t inva_events; ///< 需失效的 epoll 事件掩码
            uint16_t able_events; ///< 后端实际可监听的事件掩码
#if defined(__FreeBSD__) || defined(__APPLE__)
            uint32_t able_bytes; ///< kqueue 可读写字节数提示
            uint32_t kq_fflags;  ///< kqueue filter 标志
#endif
        };
        struct
        {
            dpret_t step_ret; ///< 分步 syscall 的中间返回值
        };
    };

    void* data; ///< prep scratch 或后端私有数据
} dpasc_out_t;

/** @brief 单次异步操作的 prep/post 与适用元素约束。 */
typedef struct dpasc
{
    dpret_t (*prep)(dpele_t*, va_list, dpasc_out_t* out);
    dpret_t (*post)(dpele_t*, dpasc_out_t* out);
    uint32_t types;   ///< 位掩码：`(1U << DPELE_TYPE_*)`
    uint32_t iotypes; ///< 位掩码：`(1U << DPAIO_TYPE_*)`
    uint32_t datasz;  ///< prep scratch 字节数（0 表示无）
    uint32_t flags;   ///< 如 `DPASC_FLAG_ALLOW_DOING`
} dpasc_t;

/** @brief 定义并返回静态 `dpasc_t` 的通用宏。
 *  展开为 `dp{TYPE}_{NAME}()`，填充 prep/post、types、iotypes、datasz、flags。 */
#define DPASC_FUNCTION(TYPE__, NAME__, PREP__, POST__, TS__, IOTS__, DSZ__,         \
    FLAGS__)                                                                        \
    const dpasc_t* dp##TYPE__##_##NAME__()                                          \
    {                                                                               \
        static dpasc_t _asc = (dpasc_t){                                            \
            .prep = PREP__,                                                         \
            .post = POST__,                                                         \
            .types = TS__,                                                          \
            .iotypes = IOTS__,                                                      \
            .datasz = DSZ__,                                                        \
            .flags = FLAGS__,                                                       \
        };                                                                          \
        return &_asc;                                                               \
    }

/** @brief GFD 元素上的通用 fd I/O prep（`DPELE_TYPE_EFD`，GFD+SKT iotype）。 */
#define DPASC_GFD_FUNCTION(NAME__, PREP__, POST__, DSZ__, FLAGS__)                  \
    DPASC_FUNCTION(gfd, NAME__, PREP__, POST__, (1U << DPELE_TYPE_EFD),             \
        (1U << DPAIO_TYPE_GFD) | (1U << DPAIO_TYPE_SKT), DSZ__, FLAGS__)

/** @brief 套接字 I/O prep（`DPELE_TYPE_EFD`，仅 SKT iotype）。 */
#define DPASC_SKT_FUNCTION(NAME__, PREP__, POST__, DSZ__, FLAGS__)                  \
    DPASC_FUNCTION(skt, NAME__, PREP__, POST__, (1U << DPELE_TYPE_EFD),             \
        (1U << DPAIO_TYPE_SKT), DSZ__, FLAGS__)

/** @brief SSL I/O prep（`DPELE_TYPE_EFD`，SSL iotype）。 */
#define DPASC_SSL_FUNCTION(NAME__, PREP__, POST__, DSZ__, FLAGS__)                  \
    DPASC_FUNCTION(ssl, NAME__, PREP__, POST__, (1U << DPELE_TYPE_EFD),             \
        (1U << DPAIO_TYPE_SSL), DSZ__, FLAGS__)

/** @brief QUIC I/O prep（`DPELE_TYPE_USD`，QIC iotype）。 */
#define DPASC_QIC_FUNCTION(NAME__, PREP__, POST__, DSZ__, FLAGS__)                  \
    DPASC_FUNCTION(qic, NAME__, PREP__, POST__, (1U << DPELE_TYPE_USD),             \
        (1U << DPAIO_TYPE_QIC), DSZ__, FLAGS__)

/** @brief EFD 控制类 prep（`DPELE_TYPE_EFD`，无 iotype 约束）。 */
#define DPASC_EFD_FUNCTION(NAME__, PREP__, POST__, DSZ__, FLAGS__)                  \
    DPASC_FUNCTION(efd, NAME__, PREP__, POST__, (1U << DPELE_TYPE_EFD), 0, DSZ__,   \
        FLAGS__)

/** @brief CTC 派发类 prep（如 `dpctc_submit`）。 */
#define DPASC_CTC_FUNCTION(NAME__, PREP__, POST__, DSZ__, FLAGS__)                  \
    DPASC_FUNCTION(ctc, NAME__, PREP__, POST__, (1U << DPELE_TYPE_CTC), 0, DSZ__,   \
        FLAGS__)

/** @brief 定时器 prep（`DPELE_TYPE_TMR`，无 post、无 iotype）。 */
#define DPASC_TMR_FUNCTION(NAME__, PREP__, DSZ__, FLAGS__)                          \
    DPASC_FUNCTION(tmr, NAME__, PREP__, NULL, (1U << DPELE_TYPE_TMR), 0, DSZ__,     \
        FLAGS__)

/** @brief prep 允许在元素已处于 doing 状态时重入（定时器/CTC 等）。 */
#define DPASC_FLAG_ALLOW_DOING (1U << 1)

/** @brief `dpaio_*` prep 内部参数包（用户通常不直接构造）。 */
typedef struct
{
    union
    {
        char* buf;
        struct iovec* iov;
        struct msghdr* msg; ///< recvmsg/sendmsg
        void* ptr;
    };
    int len;
    int flag;
    int total;
    int fd2;
} dpaio_arg_t;

/** @name 调度类 prep */
/**@{*/
/** @brief 跨线程 CTC 派发至目标 worker（`toid` 在元素创建时绑定）。
 *  参数： `(dpv64_t a0, dpv64_t a1)` — 回调数据，存入 asc scratch。 */
const dpasc_t* dpctc_submit();

/** @brief 定时器 sleep / 附带数据槽；到期后 `dpele_ret` 为 `DPE_OK`。
 *  参数： `(double sec, dpv64_t a0, dpv64_t a1)` — 超时秒数与 asc scratch 写入的两个
 *  dpv64（纯 sleep 可传 `DPV64_NULL`）。 */
const dpasc_t* dptmr_timeout();

/** @brief 定时器到期后调用回调。
 *  参数： `(double sec, dptmr_callback_f cb, dpv64_t arg)` */
typedef void (*dptmr_callback_f)(dpele_t*, dpv64_t);
const dpasc_t* dptmr_callback();

/** @brief 等待 EFD fd 的 poll 就绪事件，不执行 read/write 等 I/O。
 *  参数： `(int evs)` — `DPEVT_*` 掩码；至少须与元素 type 的 `events` 有一位交集。
 *  返回：就绪事件位（`dpele_ret` 为 `DPEVT_*` 组合，非 `DPE_OK`）。 */
const dpasc_t* dpefd_poll();

/**@}*/

/** @name 文件与套接字 I/O */
/**@{*/
/** @brief read(2)。
 *  参数： `(void* buf, int len)` */
const dpasc_t* dpgfd_read();
/** @brief write(2)。
 *  参数： `(const void* buf, int len)` */
const dpasc_t* dpgfd_write();
/** @brief readv(2)。
 *  参数： `(const struct iovec* iov, int iovcnt)` */
const dpasc_t* dpgfd_readv();
/** @brief writev(2)。
 *  参数： `(const struct iovec* iov, int iovcnt)` */
const dpasc_t* dpgfd_writev();
/** @brief recv(2)。
 *  参数： `(void* buf, int len, int flags)` */
const dpasc_t* dpskt_recv();
/** @brief recv(2) 无 flags 版本。
 *  参数： `(void* buf, int len)` */
const dpasc_t* dpskt_recv2();
/** @brief send(2)。
 *  参数： `(const void* buf, int len, int flags)` */
const dpasc_t* dpskt_send();
/** @brief send(2) 无 flags 版本。
 *  参数： `(const void* buf, int len)` */
const dpasc_t* dpskt_send2();
/** @brief recvmsg(2)。
 *  参数： `(struct msghdr* msg, int flags)` */
const dpasc_t* dpskt_recvmsg();
/** @brief sendmsg(2)。
 *  参数： `(struct msghdr* msg, int flags)` */
const dpasc_t* dpskt_sendmsg();
/** @brief accept(2)。
 *  参数： `(dpsockaddr_t* addr)` — 出参地址，可为 NULL。 */
const dpasc_t* dpskt_accept();
/** @brief connect(2)。
 *  参数： `(const dpsockaddr_t* addr)` */
const dpasc_t* dpskt_connect();
/** @brief splice(2)。
 *  参数： `(int in_fd, int len, int flags)` */
const dpasc_t* dpgfd_splice();
/** @brief tee(2)。
 *  参数： `(int in_fd, unsigned len, unsigned flags)` */
const dpasc_t* dpgfd_tee();
/** @brief fsync(2)/fdatasync(2)。
 *  参数： `(unsigned flags)` — bit0 表示 fdatasync。 */
const dpasc_t* dpefd_fsync();
/** @brief fallocate(2)。
 *  参数： `(int mode, uint64_t offset, uint64_t len)` */
const dpasc_t* dpefd_fallocate();
/** @brief posix_fadvise(2)。
 *  参数： `(uint64_t offset, long len, int advice)` */
const dpasc_t* dpefd_fadvise();
/** @brief sync_file_range(2)。
 *  参数： `(uint64_t offset, unsigned nbytes, int flags)` */
const dpasc_t* dpefd_sync_file_range();
/**@}*/

/** @name EFD 控制 */
/**@{*/
/** @brief shutdown(2)。
 *  参数： `(int how)` — `SHUT_RD` / `SHUT_WR` / `SHUT_RDWR`。 */
const dpasc_t* dpskt_shutdown();
/** @brief close(2) 元素 fd。无额外参数。 */
const dpasc_t* dpefd_close();
/**@}*/

/** @name 路径与内存 SYC（dpsyc） */
/**@{*/
/** @brief madvise(2)。
 *  参数： `(void* addr, long len, int advice)` */
const dpasc_t* dpsyc_madvise();
/** @brief openat(2)。
 *  参数： `(int dirfd, const char* path, int flags, mode_t mode)` */
const dpasc_t* dpsyc_openat();
/** @brief open(2)（dirfd=AT_FDCWD）。
 *  参数： `(const char* path, int flags, mode_t mode)` */
const dpasc_t* dpsyc_open();
/** @brief openat2(2)。
 *  参数： `(int dirfd, const char* path, struct open_how* how)` */
const dpasc_t* dpsyc_openat2();
/** @brief statx(2)。
 *  参数： `(int dirfd, const char* path, int flags, unsigned mask, struct statx*
 * buf)` */
const dpasc_t* dpsyc_statx();
/** @brief unlinkat(2)。
 *  参数： `(int dirfd, const char* path, int flags)` */
const dpasc_t* dpsyc_unlinkat();
/** @brief unlink(2)。
 *  参数： `(const char* path, int flags)` */
const dpasc_t* dpsyc_unlink();
/** @brief renameat2(2)/renameat(2)。
 *  参数： `(int olddirfd, const char* oldpath, int newdirfd, const char*
 * newpath, unsigned flags)` */
const dpasc_t* dpsyc_renameat();
/** @brief rename(2)。
 *  参数： `(const char* oldpath, const char* newpath)` */
const dpasc_t* dpsyc_rename();
/** @brief mkdirat(2)。
 *  参数： `(int dirfd, const char* path, mode_t mode)` */
const dpasc_t* dpsyc_mkdirat();
/** @brief mkdir(2)。
 *  参数： `(const char* path, mode_t mode)` */
const dpasc_t* dpsyc_mkdir();
/** @brief symlinkat(2)。
 *  参数： `(const char* target, int dirfd, const char* linkpath)` */
const dpasc_t* dpsyc_symlinkat();
/** @brief symlink(2)。
 *  参数： `(const char* target, const char* linkpath)` */
const dpasc_t* dpsyc_symlink();
/** @brief linkat(2)。
 *  参数： `(int olddirfd, const char* oldpath, int newdirfd, const char*
 * newpath, int flags)` */
const dpasc_t* dpsyc_linkat();
/** @brief link(2)。
 *  参数： `(const char* oldpath, const char* newpath, int flags)` */
const dpasc_t* dpsyc_link();
/** @brief getxattr(2)。
 *  参数： `(const char* name, void* value, const char* path, size_t size)` */
const dpasc_t* dpsyc_getxattr();
/** @brief setxattr(2)。
 *  参数： `(const char* name, const void* value, const char* path, int flags,
 * size_t size)` */
const dpasc_t* dpsyc_setxattr();
/** @brief fgetxattr(2)。
 *  参数： `(int fd, const char* name, void* value, size_t size)` */
const dpasc_t* dpsyc_fgetxattr();
/** @brief fsetxattr(2)。
 *  参数： `(int fd, const char* name, const void* value, int flags, size_t size)` */
const dpasc_t* dpsyc_fsetxattr();
/** @brief 空操作，用于占位或测试；元素需为 USD 类型，prep 直接返回
 * `DPE_OK`。无额外参数。 */
const dpasc_t* dpsyc_nop();
/**@}*/

/** @name 流式 I/O（dpbuf） */
/**@{*/
/** @brief 批量读入 dpbuf。
 *  参数： `(dpbuf_t* buf, int max_len)` 最多读取 `max_len` 字节 */
const dpasc_t* dpaio_read_some();
/** @brief 从 dpbuf 尽量写。
 *  参数： `(dpbuf_t* buf, int min_len)` 最少写入 `min_len` 字节 */
const dpasc_t* dpaio_write_some();
/** @brief 读满指定字节到 dpbuf。
 *  参数： `(dpbuf_t* buf, int len)` */
const dpasc_t* dpaio_read_must();
/** @brief 写满 dpbuf 全部可读数据。
 *  参数： `(dpbuf_t* buf, int len)` */
const dpasc_t* dpaio_write_must();
/** @brief 读到分隔串；未找到返回 `DPE_CONTINUE`。
 *  参数： `(const char* until, int until_len, dpbuf_t* buf, int max_len)` */
const dpasc_t* dpaio_read_until();
/** @brief 读满指定字节到裸缓冲区；语义同 `dpaio_read_must`。
 *  参数： `(void* buf, int len)` */
const dpasc_t* dpaio_read_data();
/** @brief 写满裸缓冲区指定字节；语义同 `dpaio_write_must`。
 *  参数： `(const void* buf, int len)` */
const dpasc_t* dpaio_write_data();
/**@}*/

#ifdef __cplusplus
}
#endif
