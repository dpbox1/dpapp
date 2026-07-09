/** @file dpcpp_asc.hh
 *  @ingroup dpcpp_asc
 *  @brief `dpasc` prep 的 `dpcpp::aexec` 内联包装（对齐 `dpasc.h` / `dpasc.lua`）。
 *
 *  `dpcpp::<name>(ele, …)` ≡ `co_await aexec(ele, dp<name>(), …)`。
 *  带 `flags` 默认值的 socket/syc 操作用 `*F`/`*FU`/`*F1` 宏族单独展开。 */
#pragma once

#include "dpapp/dpasc.h"
#include "dpapp/dpbuf.h"
#include "dpapp/dpefd.h"
#include "dpapp/dpqic.h"
#include "dpapp/dpssl.h"
#include "dpcpp/dpcpp.hh"

#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>

#if defined(__linux__)
#include <linux/openat2.h>
#include <linux/stat.h>
#endif

namespace dpcpp
{

#define _DPCPP_ASC0(name)                                                           \
    inline aco_ret name(dpele_t* ele)                                               \
    {                                                                               \
        co_return co_await aexec(ele, dp##name());                                  \
    }

#define _DPCPP_ASC1(name, t0)                                                       \
    inline aco_ret name(dpele_t* ele, t0 p0)                                        \
    {                                                                               \
        co_return co_await aexec(ele, dp##name(), p0);                              \
    }

#define _DPCPP_ASC2(name, t0, t1)                                                   \
    inline aco_ret name(dpele_t* ele, t0 p0, t1 p1)                                 \
    {                                                                               \
        co_return co_await aexec(ele, dp##name(), p0, p1);                          \
    }

#define _DPCPP_ASC3(name, t0, t1, t2)                                               \
    inline aco_ret name(dpele_t* ele, t0 p0, t1 p1, t2 p2)                          \
    {                                                                               \
        co_return co_await aexec(ele, dp##name(), p0, p1, p2);                      \
    }

#define _DPCPP_ASC4(name, t0, t1, t2, t3)                                           \
    inline aco_ret name(dpele_t* ele, t0 p0, t1 p1, t2 p2, t3 p3)                   \
    {                                                                               \
        co_return co_await aexec(ele, dp##name(), p0, p1, p2, p3);                  \
    }

#define _DPCPP_ASC5(name, t0, t1, t2, t3, t4)                                       \
    inline aco_ret name(dpele_t* ele, t0 p0, t1 p1, t2 p2, t3 p3, t4 p4)            \
    {                                                                               \
        co_return co_await aexec(ele, dp##name(), p0, p1, p2, p3, p4);              \
    }

#define _DPCPP_ASC6(name, t0, t1, t2, t3, t4, t5)                                   \
    inline aco_ret name(dpele_t* ele, t0 p0, t1 p1, t2 p2, t3 p3, t4 p4, t5 p5)     \
    {                                                                               \
        co_return co_await aexec(ele, dp##name(), p0, p1, p2, p3, p4, p5);          \
    }

#define _DPCPP_ASC0F(name)                                                          \
    inline aco_ret name(dpele_t* ele, int flags = 0)                                \
    {                                                                               \
        co_return co_await aexec(ele, dp##name(), flags);                           \
    }

#define _DPCPP_ASC1F(name, t0)                                                      \
    inline aco_ret name(dpele_t* ele, t0 p0, int flags = 0)                         \
    {                                                                               \
        co_return co_await aexec(ele, dp##name(), p0, flags);                       \
    }

#define _DPCPP_ASC2F(name, t0, t1)                                                  \
    inline aco_ret name(dpele_t* ele, t0 p0, t1 p1, int flags = 0)                  \
    {                                                                               \
        co_return co_await aexec(ele, dp##name(), p0, p1, flags);                   \
    }

#define _DPCPP_ASC3F(name, t0, t1, t2)                                              \
    inline aco_ret name(dpele_t* ele, t0 p0, t1 p1, t2 p2, int flags = 0)           \
    {                                                                               \
        co_return co_await aexec(ele, dp##name(), p0, p1, p2, flags);               \
    }

#define _DPCPP_ASC4F(name, t0, t1, t2, t3)                                          \
    inline aco_ret name(dpele_t* ele, t0 p0, t1 p1, t2 p2, t3 p3, int flags = 0)    \
    {                                                                               \
        co_return co_await aexec(ele, dp##name(), p0, p1, p2, p3, flags);           \
    }

#define _DPCPP_ASC0FU(name)                                                         \
    inline aco_ret name(dpele_t* ele, unsigned flags = 0)                           \
    {                                                                               \
        co_return co_await aexec(ele, dp##name(), flags);                           \
    }

#define _DPCPP_ASC2FU(name, t0, t1)                                                 \
    inline aco_ret name(dpele_t* ele, t0 p0, t1 p1, unsigned flags = 0)             \
    {                                                                               \
        co_return co_await aexec(ele, dp##name(), p0, p1, flags);                   \
    }

#define _DPCPP_ASC4FU(name, t0, t1, t2, t3)                                         \
    inline aco_ret name(dpele_t* ele, t0 p0, t1 p1, t2 p2, t3 p3,                   \
        unsigned flags = 0)                                                         \
    {                                                                               \
        co_return co_await aexec(ele, dp##name(), p0, p1, p2, p3, flags);           \
    }

#define _DPCPP_ASC2F1(name, t0, t1, t2)                                             \
    inline aco_ret name(dpele_t* ele, t0 p0, t1 p1, t2 p2, int flags = 0)           \
    {                                                                               \
        co_return co_await aexec(ele, dp##name(), p0, p1, flags, p2);               \
    }

#define _DPCPP_ASC4F1(name, t0, t1, t2, t3)                                         \
    inline aco_ret name(dpele_t* ele, t0 p0, t1 p1, t2 p2, t3 p3, int flags = 0)    \
    {                                                                               \
        co_return co_await aexec(ele, dp##name(), p0, p1, p2, flags, p3);           \
    }

/** @name 调度 */
/**@{*/
/** @brief 跨线程 CTC 派发，包装 `dpctc_submit()`。 */
_DPCPP_ASC2(ctc_submit, dpv64_t, dpv64_t)
/** @brief 定时器 sleep，包装 `dptmr_timeout()`。 */
_DPCPP_ASC3(tmr_timeout, double, dpv64_t, dpv64_t)
/** @brief 定时器到期回调，包装 `dptmr_callback()`。 */
_DPCPP_ASC3(tmr_callback, double, dptmr_callback_f, dpv64_t)
/** @brief 等待 EFD poll 就绪，包装 `dpefd_poll()`。 */
_DPCPP_ASC1(efd_poll, int)
/**@}*/

/** @name GFD */
/**@{*/
/** @brief read(2)，包装 `dpgfd_read()`。 */
_DPCPP_ASC2(gfd_read, void*, int)
/** @brief write(2)，包装 `dpgfd_write()`。 */
_DPCPP_ASC2(gfd_write, const void*, int)
/** @brief readv(2)，包装 `dpgfd_readv()`。 */
_DPCPP_ASC2(gfd_readv, const struct iovec*, int)
/** @brief writev(2)，包装 `dpgfd_writev()`。 */
_DPCPP_ASC2(gfd_writev, const struct iovec*, int)
/** @brief splice(2)，包装 `dpgfd_splice()`。 */
_DPCPP_ASC2F(gfd_splice, int, int)
/** @brief tee(2)，包装 `dpgfd_tee()`。 */
_DPCPP_ASC2FU(gfd_tee, int, unsigned)
/**@}*/

/** @name SKT */
/**@{*/
/** @brief recv(2)，包装 `dpskt_recv()`。 */
_DPCPP_ASC2F(skt_recv, void*, int)
/** @brief send(2)，包装 `dpskt_send()`。 */
_DPCPP_ASC2F(skt_send, const void*, int)
/** @brief recv(2) 无 flags，包装 `dpskt_recv2()`。 */
_DPCPP_ASC2(skt_recv2, void*, int)
/** @brief send(2) 无 flags，包装 `dpskt_send2()`。 */
_DPCPP_ASC2(skt_send2, const void*, int)
/** @brief recvmsg(2)，包装 `dpskt_recvmsg()`。 */
_DPCPP_ASC1F(skt_recvmsg, struct msghdr*)
/** @brief sendmsg(2)，包装 `dpskt_sendmsg()`。 */
_DPCPP_ASC1F(skt_sendmsg, struct msghdr*)

/** @brief accept(2)，包装 `dpskt_accept()`。
 *  @param addr 出参地址；nullptr 时用栈上临时变量。 */
inline aco_ret skt_accept(dpele_t* ele, dpsockaddr_t* addr = nullptr)
{
    dpsockaddr_t stack_addr{};
    if (addr == nullptr) {
        addr = &stack_addr;
    }
    co_return co_await aexec(ele, dpskt_accept(), addr);
}

/** @brief connect(2)，包装 `dpskt_connect()`。
 *  @param addr 目标地址；nullptr 时从 `dpele_aux_data(ele)` 读取。 */
inline aco_ret skt_connect(dpele_t* ele, const dpsockaddr_t* addr = nullptr)
{
    if (addr == nullptr) {
        addr = static_cast<const dpsockaddr_t*>(dpele_aux_data(ele));
    }
    if (addr == nullptr) {
        co_return DPE_INVAL;
    }
    co_return co_await aexec(ele, dpskt_connect(), addr);
}

/** @brief shutdown(2)，包装 `dpskt_shutdown()`。 */
_DPCPP_ASC1(skt_shutdown, int)
/**@}*/

/** @name EFD */
/**@{*/
/** @brief fsync/fdatasync，包装 `dpefd_fsync()`。 */
_DPCPP_ASC0FU(efd_fsync)
/** @brief fallocate(2)，包装 `dpefd_fallocate()`。 */
_DPCPP_ASC3(efd_fallocate, int, uint64_t, uint64_t)
/** @brief posix_fadvise(2)，包装 `dpefd_fadvise()`。 */
_DPCPP_ASC3(efd_fadvise, uint64_t, long, int)
/** @brief sync_file_range(2)，包装 `dpefd_sync_file_range()`。 */
_DPCPP_ASC2F(efd_sync_file_range, uint64_t, unsigned)
/** @brief close(2) 元素 fd，包装 `dpefd_close()`。 */
_DPCPP_ASC0(efd_close)
/**@}*/

/** @name SYC */
/**@{*/
/** @brief madvise(2)，包装 `dpsyc_madvise()`。 */
_DPCPP_ASC3(syc_madvise, void*, long, int)
/** @brief openat(2)，包装 `dpsyc_openat()`。 */
_DPCPP_ASC4(syc_openat, int, const char*, int, mode_t)
/** @brief open(2)，包装 `dpsyc_open()`。 */
_DPCPP_ASC3(syc_open, const char*, int, mode_t)
#if defined(__linux__)
/** @brief openat2(2)，包装 `dpsyc_openat2()`。 */
_DPCPP_ASC3(syc_openat2, int, const char*, struct open_how*)
/** @brief statx(2)，包装 `dpsyc_statx()`。 */
_DPCPP_ASC5(syc_statx, int, const char*, int, unsigned, struct statx*)
#endif
/** @brief unlinkat(2)，包装 `dpsyc_unlinkat()`。 */
_DPCPP_ASC2F(syc_unlinkat, int, const char*)
/** @brief unlink(2)，包装 `dpsyc_unlink()`。 */
_DPCPP_ASC1F(syc_unlink, const char*)
/** @brief renameat2/renameat(2)，包装 `dpsyc_renameat()`。 */
_DPCPP_ASC4FU(syc_renameat, int, const char*, int, const char*)
/** @brief rename(2)，包装 `dpsyc_rename()`。 */
_DPCPP_ASC2(syc_rename, const char*, const char*)
/** @brief mkdirat(2)，包装 `dpsyc_mkdirat()`。 */
_DPCPP_ASC3(syc_mkdirat, int, const char*, mode_t)
/** @brief mkdir(2)，包装 `dpsyc_mkdir()`。 */
_DPCPP_ASC2(syc_mkdir, const char*, mode_t)
/** @brief symlinkat(2)，包装 `dpsyc_symlinkat()`。 */
_DPCPP_ASC3(syc_symlinkat, const char*, int, const char*)
/** @brief symlink(2)，包装 `dpsyc_symlink()`。 */
_DPCPP_ASC2(syc_symlink, const char*, const char*)
/** @brief linkat(2)，包装 `dpsyc_linkat()`。 */
_DPCPP_ASC4F(syc_linkat, int, const char*, int, const char*)
/** @brief link(2)，包装 `dpsyc_link()`。 */
_DPCPP_ASC2F(syc_link, const char*, const char*)
/** @brief getxattr(2)，包装 `dpsyc_getxattr()`。 */
_DPCPP_ASC4(syc_getxattr, const char*, void*, const char*, size_t)
/** @brief setxattr(2)，包装 `dpsyc_setxattr()`。 */
_DPCPP_ASC4F1(syc_setxattr, const char*, const void*, const char*, size_t)
/** @brief fgetxattr(2)，包装 `dpsyc_fgetxattr()`。 */
_DPCPP_ASC4(syc_fgetxattr, int, const char*, void*, size_t)
/** @brief fsetxattr(2)，包装 `dpsyc_fsetxattr()`。 */
_DPCPP_ASC5(syc_fsetxattr, int, const char*, const void*, int, size_t)
/** @brief 空操作占位，包装 `dpsyc_nop()`。 */
_DPCPP_ASC0(syc_nop)
/**@}*/

/** @name AIO（流式读写） */
/**@{*/
/** @brief 批量读入 dpbuf，包装 `dpaio_read_some()`。 */
inline aco_ret aio_read_some(dpele_t* ele, dpbuf_t* buf, int max_len = DPBUF_X_SIZE)
{
    if (buf == nullptr) {
        co_return DPE_INVAL;
    }
    if (max_len <= 0) {
        max_len = DPBUF_X_SIZE;
    }
    co_return co_await aexec(ele, dpaio_read_some(), buf, max_len);
}

/** @brief 从 dpbuf 尽量写出，包装 `dpaio_write_some()`。 */
inline aco_ret aio_write_some(dpele_t* ele, dpbuf_t* buf, int min_len = 1)
{
    if (buf == nullptr) {
        co_return DPE_INVAL;
    }
    if (min_len <= 0) {
        min_len = 1;
    }
    co_return co_await aexec(ele, dpaio_write_some(), buf, min_len);
}

/** @brief 读满指定字节到 dpbuf，包装 `dpaio_read_must()`。 */
inline aco_ret aio_read_must(dpele_t* ele, dpbuf_t* buf, int len = 0)
{
    if (buf == nullptr) {
        co_return DPE_INVAL;
    }
    if (len <= 0) {
        len = dpbuf_cwsize(buf);
    }
    if (len <= 0) {
        co_return DPE_INVAL;
    }
    co_return co_await aexec(ele, dpaio_read_must(), buf, len);
}

/** @brief 写满 dpbuf 可读区，包装 `dpaio_write_must()`。 */
inline aco_ret aio_write_must(dpele_t* ele, dpbuf_t* buf, int len = 0)
{
    if (buf == nullptr) {
        co_return DPE_INVAL;
    }
    if (len <= 0) {
        len = dpbuf_crsize(buf);
    }
    if (len <= 0) {
        co_return len;
    }
    co_return co_await aexec(ele, dpaio_write_must(), buf, len);
}

/** @brief 读满指定字节到裸缓冲区，包装 `dpaio_read_data()`。 */
_DPCPP_ASC2(aio_read_data, void*, int)
/** @brief 写满裸缓冲区指定字节，包装 `dpaio_write_data()`。 */
_DPCPP_ASC2(aio_write_data, const void*, int)

/** @brief 读到分隔串（`strlen(until)`），包装 `dpaio_read_until()`。 */
inline aco_ret aio_read_until(dpele_t* ele, const char* until, dpbuf_t* buf,
    int max_len = DPBUF_X_SIZE)
{
    if (until == nullptr || buf == nullptr) {
        co_return DPE_INVAL;
    }
    int until_len = static_cast<int>(std::strlen(until));
    if (until_len <= 0) {
        co_return DPE_INVAL;
    }
    if (max_len <= 0) {
        max_len = DPBUF_X_SIZE;
    }
    co_return co_await aexec(ele, dpaio_read_until(), until, until_len, buf,
        max_len);
}

/** @brief 读到分隔串（显式 `until_len`），包装 `dpaio_read_until()`。 */
inline aco_ret aio_read_until(dpele_t* ele, const char* until, int until_len,
    dpbuf_t* buf, int max_len = DPBUF_X_SIZE)
{
    if (until == nullptr || buf == nullptr) {
        co_return DPE_INVAL;
    }
    if (until_len <= 0) {
        until_len = static_cast<int>(std::strlen(until));
    }
    if (until_len <= 0) {
        co_return DPE_INVAL;
    }
    if (max_len <= 0) {
        max_len = DPBUF_X_SIZE;
    }
    co_return co_await aexec(ele, dpaio_read_until(), until, until_len, buf,
        max_len);
}
/**@}*/

#if DPAPP_HAS_SSL
/** @name SSL */
/**@{*/
/** @brief SSL 握手，包装 `dpssl_handshake()`。 */
_DPCPP_ASC0(ssl_handshake)
/** @brief SSL 关闭，包装 `dpssl_shutdown()`。 */
_DPCPP_ASC0(ssl_shutdown)
/** @brief SSL recv，包装 `dpssl_recv()`。 */
_DPCPP_ASC2(ssl_recv, void*, int)
/** @brief SSL send，包装 `dpssl_send()`。 */
_DPCPP_ASC2(ssl_send, const void*, int)
/**@}*/
#endif

#if DPAPP_HAS_LSQUIC
/** @name QIC */
/**@{*/
/** @brief QUIC connect，包装 `dpqic_connect()`。 */
_DPCPP_ASC3(qic_connect, const char*, const char*, dpele_t**)
/** @brief QUIC accept，包装 `dpqic_accept()`。 */
_DPCPP_ASC1(qic_accept, dpele_t**)
/** @brief 打开 QUIC stream，包装 `dpqic_stream()`。 */
_DPCPP_ASC2(qic_stream, dpele_t**, bool)
/** @brief QUIC recv，包装 `dpqic_recv()`。 */
_DPCPP_ASC2(qic_recv, void*, int)
/** @brief QUIC send，包装 `dpqic_send()`。 */
_DPCPP_ASC2(qic_send, const void*, int)
/** @brief QUIC recvv，包装 `dpqic_recvv()`。 */
_DPCPP_ASC2(qic_recvv, struct iovec*, int)
/** @brief QUIC sendv，包装 `dpqic_sendv()`。 */
_DPCPP_ASC2(qic_sendv, struct iovec*, int)
/** @brief 接收 HTTP 头集，包装 `dpqic_recv_hdrset()`。 */
_DPCPP_ASC1(qic_recv_hdrset, dpqic_hdrset_t**)
/** @brief 发送 HTTP 头集，包装 `dpqic_send_hdrset()`。 */
_DPCPP_ASC1(qic_send_hdrset, const dpqic_hdrset_t*)
/**@}*/
#endif

#undef _DPCPP_ASC0
#undef _DPCPP_ASC1
#undef _DPCPP_ASC2
#undef _DPCPP_ASC3
#undef _DPCPP_ASC4
#undef _DPCPP_ASC5
#undef _DPCPP_ASC6
#undef _DPCPP_ASC0F
#undef _DPCPP_ASC1F
#undef _DPCPP_ASC2F
#undef _DPCPP_ASC3F
#undef _DPCPP_ASC4F
#undef _DPCPP_ASC0FU
#undef _DPCPP_ASC2FU
#undef _DPCPP_ASC4FU
#undef _DPCPP_ASC2F1
#undef _DPCPP_ASC4F1

} // namespace dpcpp
