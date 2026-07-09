/** @file dpcwc_asc.h
 *  @ingroup dpcwc_asc
 *  @brief `dpasc` 异步 prep 的 `dpcwc_aexec` 内联包装（对齐 `dpasc.h` /
 * `dpasc.lua`）。
 *
 *  `dpcwc_<name>(ele, …)` ≡ `dpcwc_aexec(ele, dp<name>(), …)`。
 *  宏仅展开参数类型；形参在宏内命名为 `p0`…`pN`。
 *  `aio_*` 带默认长度/flags 的包装在本文件单独实现。 */
#pragma once

#include "dpapp/dpasc.h"
#include "dpapp/dpbuf.h"
#include "dpapp/dpefd.h"
#include "dpapp/dpqic.h"
#include "dpapp/dpssl.h"
#include "dpcwc/dpcwc.h"

#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/uio.h>

#if defined(__linux__)
#include <linux/openat2.h>
#include <linux/stat.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#define _DPCWC_ASC0(name)                                                           \
    static inline dpret_t dpcwc_##name(dpele_t* ele)                                \
    {                                                                               \
        return dpcwc_aexec(ele, dp##name());                                        \
    }

#define _DPCWC_ASC1(name, t0)                                                       \
    static inline dpret_t dpcwc_##name(dpele_t* ele, t0 p0)                         \
    {                                                                               \
        return dpcwc_aexec(ele, dp##name(), p0);                                    \
    }

#define _DPCWC_ASC2(name, t0, t1)                                                   \
    static inline dpret_t dpcwc_##name(dpele_t* ele, t0 p0, t1 p1)                  \
    {                                                                               \
        return dpcwc_aexec(ele, dp##name(), p0, p1);                                \
    }

#define _DPCWC_ASC3(name, t0, t1, t2)                                               \
    static inline dpret_t dpcwc_##name(dpele_t* ele, t0 p0, t1 p1, t2 p2)           \
    {                                                                               \
        return dpcwc_aexec(ele, dp##name(), p0, p1, p2);                            \
    }

#define _DPCWC_ASC4(name, t0, t1, t2, t3)                                           \
    static inline dpret_t dpcwc_##name(dpele_t* ele, t0 p0, t1 p1, t2 p2, t3 p3)    \
    {                                                                               \
        return dpcwc_aexec(ele, dp##name(), p0, p1, p2, p3);                        \
    }

#define _DPCWC_ASC5(name, t0, t1, t2, t3, t4)                                       \
    static inline dpret_t dpcwc_##name(dpele_t* ele, t0 p0, t1 p1, t2 p2, t3 p3,    \
        t4 p4)                                                                      \
    {                                                                               \
        return dpcwc_aexec(ele, dp##name(), p0, p1, p2, p3, p4);                    \
    }

#define _DPCWC_ASC6(name, t0, t1, t2, t3, t4, t5)                                   \
    static inline dpret_t dpcwc_##name(dpele_t* ele, t0 p0, t1 p1, t2 p2, t3 p3,    \
        t4 p4, t5 p5)                                                               \
    {                                                                               \
        return dpcwc_aexec(ele, dp##name(), p0, p1, p2, p3, p4, p5);                \
    }

/** @name 调度 */
/**@{*/
/** @brief 跨线程 CTC 派发，包装 `dpctc_submit()`。 */
_DPCWC_ASC2(ctc_submit, dpv64_t, dpv64_t)
/** @brief 定时器 sleep，包装 `dptmr_timeout()`。 */
_DPCWC_ASC3(tmr_timeout, double, dpv64_t, dpv64_t)
/** @brief 定时器到期回调，包装 `dptmr_callback()`。 */
_DPCWC_ASC3(tmr_callback, double, dptmr_callback_f, dpv64_t)
/** @brief 等待 EFD poll 就绪，包装 `dpefd_poll()`。 */
_DPCWC_ASC1(efd_poll, int)
/**@}*/

/** @name GFD */
/**@{*/
/** @brief read(2)，包装 `dpgfd_read()`。 */
_DPCWC_ASC2(gfd_read, void*, int)
/** @brief write(2)，包装 `dpgfd_write()`。 */
_DPCWC_ASC2(gfd_write, const void*, int)
/** @brief readv(2)，包装 `dpgfd_readv()`。 */
_DPCWC_ASC2(gfd_readv, const struct iovec*, int)
/** @brief writev(2)，包装 `dpgfd_writev()`。 */
_DPCWC_ASC2(gfd_writev, const struct iovec*, int)
/** @brief splice(2)，包装 `dpgfd_splice()`。 */
_DPCWC_ASC3(gfd_splice, int, int, int)
/** @brief tee(2)，包装 `dpgfd_tee()`。 */
_DPCWC_ASC3(gfd_tee, int, unsigned, unsigned)
/**@}*/

/** @name SKT */
/**@{*/
/** @brief recv(2)，包装 `dpskt_recv()`。 */
_DPCWC_ASC3(skt_recv, void*, int, int)
/** @brief send(2)，包装 `dpskt_send()`。 */
_DPCWC_ASC3(skt_send, const void*, int, int)
/** @brief recv(2) 无 flags，包装 `dpskt_recv2()`。 */
_DPCWC_ASC2(skt_recv2, void*, int)
/** @brief send(2) 无 flags，包装 `dpskt_send2()`。 */
_DPCWC_ASC2(skt_send2, const void*, int)
/** @brief recvmsg(2)，包装 `dpskt_recvmsg()`。 */
_DPCWC_ASC2(skt_recvmsg, struct msghdr*, int)
/** @brief sendmsg(2)，包装 `dpskt_sendmsg()`。 */
_DPCWC_ASC2(skt_sendmsg, struct msghdr*, int)

/** @brief accept(2)，包装 `dpskt_accept()`。
 *  @param addr 出参地址；NULL 时用栈上临时变量。
 *  @return `dpret_t` */
static inline dpret_t dpcwc_skt_accept(dpele_t* ele, dpsockaddr_t* addr)
{
    dpsockaddr_t stack_addr;
    if (addr == NULL) {
        addr = &stack_addr;
    }
    return dpcwc_aexec(ele, dpskt_accept(), addr);
}

/** @brief connect(2)，包装 `dpskt_connect()`。
 *  @param addr 目标地址；NULL 时从 `dpele_aux_data(ele)` 读取。
 *  @return `dpret_t` */
static inline dpret_t dpcwc_skt_connect(dpele_t* ele, const dpsockaddr_t* addr)
{
    if (addr == NULL) {
        addr = (const dpsockaddr_t*)dpele_aux_data(ele);
    }
    if (addr == NULL) {
        return DPE_INVAL;
    }
    return dpcwc_aexec(ele, dpskt_connect(), addr);
}

/** @brief shutdown(2)，包装 `dpskt_shutdown()`。 */
_DPCWC_ASC1(skt_shutdown, int)
/**@}*/

/** @name EFD */
/**@{*/
/** @brief fsync/fdatasync，包装 `dpefd_fsync()`。 */
_DPCWC_ASC1(efd_fsync, unsigned)
/** @brief fallocate(2)，包装 `dpefd_fallocate()`。 */
_DPCWC_ASC3(efd_fallocate, int, uint64_t, uint64_t)
/** @brief posix_fadvise(2)，包装 `dpefd_fadvise()`。 */
_DPCWC_ASC3(efd_fadvise, uint64_t, long, int)
/** @brief sync_file_range(2)，包装 `dpefd_sync_file_range()`。 */
_DPCWC_ASC3(efd_sync_file_range, uint64_t, unsigned, int)
/** @brief close(2) 元素 fd，包装 `dpefd_close()`。 */
_DPCWC_ASC0(efd_close)
/**@}*/

/** @name SYC */
/**@{*/
/** @brief madvise(2)，包装 `dpsyc_madvise()`。 */
_DPCWC_ASC3(syc_madvise, void*, long, int)
/** @brief openat(2)，包装 `dpsyc_openat()`。 */
_DPCWC_ASC4(syc_openat, int, const char*, int, mode_t)
/** @brief open(2)，包装 `dpsyc_open()`。 */
_DPCWC_ASC3(syc_open, const char*, int, mode_t)
#if defined(__linux__)
/** @brief openat2(2)，包装 `dpsyc_openat2()`。 */
_DPCWC_ASC3(syc_openat2, int, const char*, struct open_how*)
/** @brief statx(2)，包装 `dpsyc_statx()`。 */
_DPCWC_ASC5(syc_statx, int, const char*, int, unsigned, struct statx*)
#endif
/** @brief unlinkat(2)，包装 `dpsyc_unlinkat()`。 */
_DPCWC_ASC3(syc_unlinkat, int, const char*, int)
/** @brief unlink(2)，包装 `dpsyc_unlink()`。 */
_DPCWC_ASC2(syc_unlink, const char*, int)
/** @brief renameat2/renameat(2)，包装 `dpsyc_renameat()`。 */
_DPCWC_ASC5(syc_renameat, int, const char*, int, const char*, unsigned)
/** @brief rename(2)，包装 `dpsyc_rename()`。 */
_DPCWC_ASC2(syc_rename, const char*, const char*)
/** @brief mkdirat(2)，包装 `dpsyc_mkdirat()`。 */
_DPCWC_ASC3(syc_mkdirat, int, const char*, mode_t)
/** @brief mkdir(2)，包装 `dpsyc_mkdir()`。 */
_DPCWC_ASC2(syc_mkdir, const char*, mode_t)
/** @brief symlinkat(2)，包装 `dpsyc_symlinkat()`。 */
_DPCWC_ASC3(syc_symlinkat, const char*, int, const char*)
/** @brief symlink(2)，包装 `dpsyc_symlink()`。 */
_DPCWC_ASC2(syc_symlink, const char*, const char*)
/** @brief linkat(2)，包装 `dpsyc_linkat()`。 */
_DPCWC_ASC5(syc_linkat, int, const char*, int, const char*, int)
/** @brief link(2)，包装 `dpsyc_link()`。 */
_DPCWC_ASC3(syc_link, const char*, const char*, int)
/** @brief getxattr(2)，包装 `dpsyc_getxattr()`。 */
_DPCWC_ASC4(syc_getxattr, const char*, void*, const char*, size_t)
/** @brief setxattr(2)，包装 `dpsyc_setxattr()`。 */
_DPCWC_ASC5(syc_setxattr, const char*, const void*, const char*, int, size_t)
/** @brief fgetxattr(2)，包装 `dpsyc_fgetxattr()`。 */
_DPCWC_ASC4(syc_fgetxattr, int, const char*, void*, size_t)
/** @brief fsetxattr(2)，包装 `dpsyc_fsetxattr()`。 */
_DPCWC_ASC5(syc_fsetxattr, int, const char*, const void*, int, size_t)
/** @brief 空操作占位，包装 `dpsyc_nop()`。 */
_DPCWC_ASC0(syc_nop)
/**@}*/

/** @name AIO（流式读写） */
/**@{*/
/** @brief 批量读入 dpbuf，包装 `dpaio_read_some()`。
 *  @param max_len ≤0 时用 `DPBUF_X_SIZE`。
 *  @return `dpret_t` */
static inline dpret_t dpcwc_aio_read_some(dpele_t* ele, dpbuf_t* buf, int max_len)
{
    if (buf == NULL) {
        return DPE_INVAL;
    }
    if (max_len <= 0) {
        max_len = DPBUF_X_SIZE;
    }
    return dpcwc_aexec(ele, dpaio_read_some(), buf, max_len);
}

/** @brief 从 dpbuf 尽量写出，包装 `dpaio_write_some()`。
 *  @param min_len ≤0 时默认为 1。
 *  @return `dpret_t` */
static inline dpret_t dpcwc_aio_write_some(dpele_t* ele, dpbuf_t* buf, int min_len)
{
    if (buf == NULL) {
        return DPE_INVAL;
    }
    if (min_len <= 0) {
        min_len = 1;
    }
    return dpcwc_aexec(ele, dpaio_write_some(), buf, min_len);
}

/** @brief 读满指定字节到 dpbuf，包装 `dpaio_read_must()`。
 *  @param len ≤0 时用 `dpbuf_cwsize(buf)`。
 *  @return `dpret_t` */
static inline dpret_t dpcwc_aio_read_must(dpele_t* ele, dpbuf_t* buf, int len)
{
    if (buf == NULL) {
        return DPE_INVAL;
    }
    if (len <= 0) {
        len = dpbuf_cwsize(buf);
    }
    if (len <= 0) {
        return DPE_INVAL;
    }
    return dpcwc_aexec(ele, dpaio_read_must(), buf, len);
}

/** @brief 写满 dpbuf 可读区，包装 `dpaio_write_must()`。
 *  @param len ≤0 时用 `dpbuf_crsize(buf)`。
 *  @return `dpret_t` */
static inline dpret_t dpcwc_aio_write_must(dpele_t* ele, dpbuf_t* buf, int len)
{
    if (buf == NULL) {
        return DPE_INVAL;
    }
    if (len <= 0) {
        len = dpbuf_crsize(buf);
    }
    if (len <= 0) {
        return len;
    }
    return dpcwc_aexec(ele, dpaio_write_must(), buf, len);
}

/** @brief 读满指定字节到裸缓冲区，包装 `dpaio_read_data()`。 */
_DPCWC_ASC2(aio_read_data, void*, int)
/** @brief 写满裸缓冲区指定字节，包装 `dpaio_write_data()`。 */
_DPCWC_ASC2(aio_write_data, const void*, int)

/** @brief 读到分隔串，包装 `dpaio_read_until()`。
 *  @param until_len ≤0 时用 `strlen(until)`；`max_len` ≤0 时用 `DPBUF_X_SIZE`。
 *  @return `dpret_t` */
static inline dpret_t dpcwc_aio_read_until(dpele_t* ele, const char* until,
    int until_len, dpbuf_t* buf, int max_len)
{
    if (until == NULL || buf == NULL) {
        return DPE_INVAL;
    }
    if (until_len <= 0) {
        until_len = (int)strlen(until);
    }
    if (until_len <= 0) {
        return DPE_INVAL;
    }
    if (max_len <= 0) {
        max_len = DPBUF_X_SIZE;
    }
    return dpcwc_aexec(ele, dpaio_read_until(), until, until_len, buf, max_len);
}
/**@}*/

#if DPAPP_HAS_SSL
/** @name SSL */
/**@{*/
/** @brief SSL 握手，包装 `dpssl_handshake()`。 */
_DPCWC_ASC0(ssl_handshake)
/** @brief SSL 关闭，包装 `dpssl_shutdown()`。 */
_DPCWC_ASC0(ssl_shutdown)
/** @brief SSL recv，包装 `dpssl_recv()`。 */
_DPCWC_ASC2(ssl_recv, void*, int)
/** @brief SSL send，包装 `dpssl_send()`。 */
_DPCWC_ASC2(ssl_send, const void*, int)
/**@}*/
#endif

#if DPAPP_HAS_LSQUIC
/** @name QIC */
/**@{*/
/** @brief QUIC connect，包装 `dpqic_connect()`。 */
_DPCWC_ASC3(qic_connect, const char*, const char*, dpele_t**)
/** @brief QUIC accept，包装 `dpqic_accept()`。 */
_DPCWC_ASC1(qic_accept, dpele_t**)
/** @brief 打开 QUIC stream，包装 `dpqic_stream()`。 */
_DPCWC_ASC2(qic_stream, dpele_t**, bool)
/** @brief QUIC recv，包装 `dpqic_recv()`。 */
_DPCWC_ASC2(qic_recv, void*, int)
/** @brief QUIC send，包装 `dpqic_send()`。 */
_DPCWC_ASC2(qic_send, const void*, int)
/** @brief QUIC recvv，包装 `dpqic_recvv()`。 */
_DPCWC_ASC2(qic_recvv, struct iovec*, int)
/** @brief QUIC sendv，包装 `dpqic_sendv()`。 */
_DPCWC_ASC2(qic_sendv, struct iovec*, int)
/** @brief 接收 HTTP 头集，包装 `dpqic_recv_hdrset()`。 */
_DPCWC_ASC1(qic_recv_hdrset, dpqic_hdrset_t**)
/** @brief 发送 HTTP 头集，包装 `dpqic_send_hdrset()`。 */
_DPCWC_ASC1(qic_send_hdrset, const dpqic_hdrset_t*)
/**@}*/
#endif

#undef _DPCWC_ASC0
#undef _DPCWC_ASC1
#undef _DPCWC_ASC2
#undef _DPCWC_ASC3
#undef _DPCWC_ASC4
#undef _DPCWC_ASC5
#undef _DPCWC_ASC6

#ifdef __cplusplus
}
#endif
