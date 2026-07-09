#include "dpapp/dpasc.h"
#include "dpapp/dpbuf.h"
#include "dpapp/dpdef.h"
#include "dpapp/dpefd.h"
#include "dpapp/dpevp.h"
#include "dpapp/dpret.h"
#include "dpapp/os/dpevp_pri.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdarg.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/xattr.h>
#include <unistd.h>

#if defined(__linux__)
#include <linux/openat2.h>
#include <linux/stat.h>
#endif

/* epoll 后端 dpasc：EFD/SYC/CTC 元素上的 syscall 封装。
 * - DPASC_GFD_FUNCTION：read/write/splice/tee 等通用 fd I/O（prep+post 循环）
 * - DPASC_EFD_FUNCTION：close/fsync/fallocate 等作用于元素 fd 的控制（prep only）
 * - DPASC_SKT_FUNCTION：send/recv/connect/accept/shutdown 等 socket 专用 API
 * - DPASC_SYC_FUNCTION：open/unlink/xattr 等路径类 syscall（prep+post）
 * QIC/SSL dpasc 在 dpqic.c / dpssl.c，CTC submit/TMR 在 dpevp_epoll.c。 */

typedef struct dpsyc_arg
{
    int dirfd;
    int dirfd2;
    int fd;
    int flags;
    uint64_t len;
    const char* path;

    union
    {
        const char* path2;
        const char* name;
    };
    union
    {
        int mode;
        int mask;
        int advice;
    };
    union
    {
        void* buf;
        void* ptr;
    };
} dpsyc_arg_t;

#define DPASC_SYC_FUNCTION(NAME__, PREP__, POST__, FLAGS__)                         \
    DPASC_FUNCTION(syc, NAME__, PREP__, POST__, (1U << DPELE_TYPE_CTC), 0,          \
        ((POST__) != NULL) ? sizeof(dpsyc_arg_t) : 0, FLAGS__)

static inline dpret_t _dpsyc_ret_fd(int r)
{
    return r >= 0 ? (dpret_t)r : -errno;
}

static inline dpret_t _dpsyc_ret_ok(int r)
{
    return r == 0 ? DPE_OK : -errno;
}

static inline dpsyc_arg_t* _dpsyc_arg(dpasc_out_t* out)
{
    return (dpsyc_arg_t*)out->data;
}

static inline size_t _dpioc_iov_total(const struct iovec* iov, int iovcnt)
{
    if (iov == NULL || iovcnt <= 0)
        return 0;
    size_t total = 0;
    for (int i = 0; i < iovcnt; i++) {
        total += iov[i].iov_len;
    }
    return total;
}

static inline void _dpioc_iov_advance(struct iovec** iov, int* iovcnt, size_t n)
{
    while (*iovcnt > 0 && n > 0) {
        size_t seg = (*iov)[0].iov_len;
        if (n < seg) {
            (*iov)[0].iov_base = (char*)(*iov)[0].iov_base + n;
            (*iov)[0].iov_len -= n;
            return;
        }
        n -= seg;
        (*iov)++;
        (*iovcnt)--;
    }
}

static ssize_t _dpaio_read(int fd, dpaio_arg_t* ioarg, int off)
{
    return read(fd, ioarg->buf + off, ioarg->len - off);
}

static ssize_t _dpaio_write(int fd, dpaio_arg_t* ioarg, int off)
{
    return write(fd, ioarg->buf + off, ioarg->len - off);
}

static ssize_t _dpaio_recv(int fd, dpaio_arg_t* ioarg, int off)
{
    return recv(fd, ioarg->buf + off, ioarg->len - off, ioarg->flag);
}
static ssize_t _dpaio_send(int fd, dpaio_arg_t* ioarg, int off)
{
    return send(fd, ioarg->buf + off, ioarg->len - off, ioarg->flag);
}

typedef ssize_t (*_dpaio_data_f)(int, dpaio_arg_t*, int);
typedef ssize_t (*_dpaio_vec_f)(int, const struct iovec*, int);
typedef ssize_t (*_dpaio_msg_f)(int, struct msghdr*, int);

static dpret_t _dpaio_buf_post(dpele_t* gio, dpasc_out_t* out, _dpaio_data_f iofun)
{
    dpaio_arg_t* ioarg = (dpaio_arg_t*)out->data;
    int fd = dpefd_fd(gio);
    int total = 0;
    ssize_t res = 0;
    for (;;) {
        res = iofun(fd, ioarg, total);
        if (res > 0) {
            total += res;
            if (total == ioarg->len) {
                return total;
            }
        } else if (res == 0) {
            out->inva_events = out->want_events;
            return DPE_EOF;
        } else {
            out->inva_events = out->want_events;
            return total > 0 ? total : (-errno);
        }
    }
}

static dpret_t _dpaio_iov_post(dpele_t* gio, dpasc_out_t* out, _dpaio_vec_f iofun)
{
    dpaio_arg_t* ioarg = (dpaio_arg_t*)out->data;
    int fd = dpefd_fd(gio);

    int iovcnt = ioarg->len;
    struct iovec iov[iovcnt];
    memcpy(iov, ioarg->iov, sizeof(struct iovec) * iovcnt);
    struct iovec* iov_p = iov;

    int total_len = ioarg->total;
    int total = 0;
    ssize_t res = 0;
    for (;;) {
        res = iofun(fd, iov_p, iovcnt);
        if (res > 0) {
            total += (int)res;
            _dpioc_iov_advance(&iov_p, &iovcnt, (size_t)res);
            if (total == total_len) {
                return total;
            }
        } else if (res == 0) {
            out->inva_events = out->want_events;
            return DPE_EOF;
        } else {
            out->inva_events = out->want_events;
            return total > 0 ? total : (-errno);
        }
    }
}

/// msg 版本：适用于 recvmsg/sendmsg 循环读写，自动推进 iov
static dpret_t _dpaio_msg_post(dpele_t* e, dpasc_out_t* out, _dpaio_msg_f iofun)
{
    dpaio_arg_t* io = (dpaio_arg_t*)out->data;
    int fd = dpefd_fd(e);

    int iovcnt = (int)io->msg->msg_iovlen;
    struct iovec iov_work[iovcnt];
    memcpy(iov_work, io->msg->msg_iov, sizeof(struct iovec) * iovcnt);
    struct msghdr msg_work = *io->msg;
    msg_work.msg_iov = iov_work;
    size_t total_len = (size_t)io->total;

    int total = 0;
    ssize_t res = 0;
    dpret_t ret = DPE_OK;
    for (;;) {
        res = iofun(fd, &msg_work, io->flag);
        if (res > 0) {
            total += (int)res;
            _dpioc_iov_advance(&msg_work.msg_iov, &iovcnt, (size_t)res);
            msg_work.msg_iovlen = iovcnt;
            if (total == (int)total_len) {
                ret = total;
                break;
            }
        } else if (res == 0) {
            out->inva_events = out->want_events;
            ret = total > 0 ? total : DPE_EOF;
            break;
        } else {
            out->inva_events = out->want_events;
            ret = total > 0 ? total : -errno;
            break;
        }
    }
    // 同步内核回写的 msghdr 字段到调用者
    io->msg->msg_flags = msg_work.msg_flags;
    io->msg->msg_namelen = msg_work.msg_namelen;
    io->msg->msg_controllen = msg_work.msg_controllen;
    return ret;
}

static dpret_t _dpgfd_read_prep(dpele_t* gio, va_list arg, dpasc_out_t* out)
{
    out->want_events = DPEVT_IN;
    out->inva_events = 0;

    dpaio_arg_t* ioarg = (dpaio_arg_t*)out->data;

    ioarg->buf = va_arg(arg, void*);
    ioarg->len = va_arg(arg, int);
    ioarg->flag = 0;

    if (ioarg->buf == NULL || ioarg->len <= 0) {
        return DPE_INVAL;
    }

    return DPE_CONTINUE;
}

static dpret_t _dpgfd_read_post(dpele_t* gio, dpasc_out_t* out)
{
    return _dpaio_buf_post(gio, out, _dpaio_read);
}

DPASC_GFD_FUNCTION(read, _dpgfd_read_prep, _dpgfd_read_post, sizeof(dpaio_arg_t), 0)

static dpret_t _dpgfd_write_prep(dpele_t* gio, va_list arg, dpasc_out_t* out)
{
    out->want_events = DPEVT_OUT;
    out->inva_events = 0;

    dpaio_arg_t* ioarg = (dpaio_arg_t*)out->data;
    ioarg->buf = va_arg(arg, void*);
    ioarg->len = va_arg(arg, int);
    ioarg->flag = 0;

    if (ioarg->buf == NULL || ioarg->len <= 0) {
        return DPE_INVAL;
    }

    return DPE_CONTINUE;
}

static dpret_t _dpgfd_write_post(dpele_t* gio, dpasc_out_t* out)
{
    return _dpaio_buf_post(gio, out, _dpaio_write);
}

DPASC_GFD_FUNCTION(write, _dpgfd_write_prep, _dpgfd_write_post, sizeof(dpaio_arg_t),
    0)

static dpret_t _dpgfd_readv_prep(dpele_t* gio, va_list arg, dpasc_out_t* out)
{
    out->want_events = DPEVT_IN;
    out->inva_events = 0;

    dpaio_arg_t* ioarg = (dpaio_arg_t*)out->data;
    ioarg->iov = va_arg(arg, struct iovec*);
    ioarg->len = va_arg(arg, int);

    ioarg->total = (int)_dpioc_iov_total(ioarg->iov, ioarg->len);
    if (ioarg->total == 0)
        return DPE_INVAL;
    return DPE_CONTINUE;
}

static dpret_t _dpgfd_readv_post(dpele_t* gio, dpasc_out_t* out)
{
    return _dpaio_iov_post(gio, out, readv);
}

DPASC_GFD_FUNCTION(readv, _dpgfd_readv_prep, _dpgfd_readv_post, sizeof(dpaio_arg_t),
    0)

static dpret_t _dpgfd_writev_prep(dpele_t* gio, va_list arg, dpasc_out_t* out)
{
    out->want_events = DPEVT_OUT;
    out->inva_events = 0;

    dpaio_arg_t* ioarg = (dpaio_arg_t*)out->data;
    ioarg->iov = va_arg(arg, struct iovec*);
    ioarg->len = va_arg(arg, int);

    ioarg->total = (int)_dpioc_iov_total(ioarg->iov, ioarg->len);
    if (ioarg->total == 0)
        return DPE_INVAL;
    return DPE_CONTINUE;
}

static dpret_t _dpgfd_writev_post(dpele_t* gio, dpasc_out_t* out)
{
    return _dpaio_iov_post(gio, out, writev);
}

DPASC_GFD_FUNCTION(writev, _dpgfd_writev_prep, _dpgfd_writev_post,
    sizeof(dpaio_arg_t), 0)

static dpret_t _dpskt_recv_prep(dpele_t* skt, va_list arg, dpasc_out_t* out)
{
    out->want_events = DPEVT_IN;
    out->inva_events = 0;

    dpaio_arg_t* ioarg = (dpaio_arg_t*)out->data;
    ioarg->buf = va_arg(arg, void*);
    ioarg->len = va_arg(arg, int);
    ioarg->flag = va_arg(arg, int);

    if (ioarg->buf == NULL || ioarg->len <= 0) {
        return DPE_INVAL;
    }

    return DPE_CONTINUE;
}

static dpret_t _dpskt_recv_post(dpele_t* skt, dpasc_out_t* out)
{
    return _dpaio_buf_post(skt, out, _dpaio_recv);
}

DPASC_SKT_FUNCTION(recv, _dpskt_recv_prep, _dpskt_recv_post, sizeof(dpaio_arg_t), 0)

/// recv 无 flags 版本，签名 (buf, len) 与 read/qic_recv/ssl_recv 统一
static dpret_t _dpskt_recv2_prep(dpele_t* skt, va_list arg, dpasc_out_t* out)
{
    out->want_events = DPEVT_IN;
    out->inva_events = 0;

    dpaio_arg_t* ioarg = (dpaio_arg_t*)out->data;
    ioarg->buf = va_arg(arg, void*);
    ioarg->len = va_arg(arg, int);
    ioarg->flag = 0;

    if (ioarg->buf == NULL || ioarg->len <= 0)
        return DPE_INVAL;
    return DPE_CONTINUE;
}

DPASC_SKT_FUNCTION(recv2, _dpskt_recv2_prep, _dpskt_recv_post, sizeof(dpaio_arg_t),
    0)

static dpret_t _dpskt_send_prep(dpele_t* skt, va_list arg, dpasc_out_t* out)
{
    out->want_events = DPEVT_OUT;
    out->inva_events = 0;

    dpaio_arg_t* ioarg = (dpaio_arg_t*)out->data;
    ioarg->buf = va_arg(arg, void*);
    ioarg->len = va_arg(arg, int);
    ioarg->flag = va_arg(arg, int);

    if (ioarg->buf == NULL || ioarg->len <= 0) {
        return DPE_INVAL;
    }

    return DPE_CONTINUE;
}

static dpret_t _dpskt_send_post(dpele_t* skt, dpasc_out_t* out)
{
    return _dpaio_buf_post(skt, out, _dpaio_send);
}

DPASC_SKT_FUNCTION(send, _dpskt_send_prep, _dpskt_send_post, sizeof(dpaio_arg_t), 0)

/// send 无 flags 版本，签名 (buf, len) 与 write/qic_send/ssl_send 统一
static dpret_t _dpskt_send2_prep(dpele_t* skt, va_list arg, dpasc_out_t* out)
{
    out->want_events = DPEVT_OUT;
    out->inva_events = 0;

    dpaio_arg_t* ioarg = (dpaio_arg_t*)out->data;
    ioarg->buf = va_arg(arg, void*);
    ioarg->len = va_arg(arg, int);
    ioarg->flag = 0;

    if (ioarg->buf == NULL || ioarg->len <= 0)
        return DPE_INVAL;
    return DPE_CONTINUE;
}

DPASC_SKT_FUNCTION(send2, _dpskt_send2_prep, _dpskt_send_post, sizeof(dpaio_arg_t),
    0)

static dpret_t _dpskt_recvmsg_prep(dpele_t* skt, va_list arg, dpasc_out_t* out)
{
    dpaio_arg_t* io = (dpaio_arg_t*)out->data;
    io->msg = va_arg(arg, struct msghdr*);
    io->flag = va_arg(arg, int);
    out->want_events = DPEVT_IN;
    out->inva_events = 0;
    if (io->msg == NULL)
        return DPE_INVAL;
    io->total = (int)_dpioc_iov_total(io->msg->msg_iov, (int)io->msg->msg_iovlen);
    if (io->total == 0)
        return DPE_INVAL;
    return DPE_CONTINUE;
}

static dpret_t _dpskt_recvmsg_post(dpele_t* skt, dpasc_out_t* out)
{
    return _dpaio_msg_post(skt, out, recvmsg);
}

DPASC_SKT_FUNCTION(recvmsg, _dpskt_recvmsg_prep, _dpskt_recvmsg_post,
    sizeof(dpaio_arg_t), 0)

static dpret_t _dpskt_sendmsg_prep(dpele_t* skt, va_list arg, dpasc_out_t* out)
{
    dpaio_arg_t* io = (dpaio_arg_t*)out->data;
    io->msg = va_arg(arg, struct msghdr*);
    io->flag = va_arg(arg, int);
    out->want_events = DPEVT_OUT;
    out->inva_events = 0;
    if (io->msg == NULL)
        return DPE_INVAL;
    io->total = (int)_dpioc_iov_total(io->msg->msg_iov, (int)io->msg->msg_iovlen);
    if (io->total == 0)
        return DPE_INVAL;
    return DPE_CONTINUE;
}

static dpret_t _dpskt_sendmsg_post(dpele_t* skt, dpasc_out_t* out)
{
    return _dpaio_msg_post(skt, out, (_dpaio_msg_f)sendmsg);
}

DPASC_SKT_FUNCTION(sendmsg, _dpskt_sendmsg_prep, _dpskt_sendmsg_post,
    sizeof(dpaio_arg_t), 0)

typedef struct
{
    dpsockaddr_t* addr;
} _dpskt_acc_t;

static dpret_t _dpskt_accept_prep(dpele_t* skt, va_list arg, dpasc_out_t* out)
{
    _dpskt_acc_t* acc = (_dpskt_acc_t*)out->data;
    acc->addr = va_arg(arg, dpsockaddr_t*);
    out->want_events = DPEVT_IN;
    out->inva_events = 0;
    return DPE_CONTINUE;
}

static dpret_t _dpskt_accept_post(dpele_t* skt, dpasc_out_t* out)
{
    _dpskt_acc_t* acc = (_dpskt_acc_t*)out->data;
    int r = 0;
    if (acc->addr) {
        acc->addr->real = sizeof(acc->addr->addr);
        r = accept(dpefd_fd(skt), (struct sockaddr*)&acc->addr->addr,
            &acc->addr->real);
    } else {
        r = accept(dpefd_fd(skt), NULL, NULL);
    }

    if (r >= 0)
        return r;
    else {
        out->inva_events = DPEVT_IN;
        return -errno;
    }
}

DPASC_SKT_FUNCTION(accept, _dpskt_accept_prep, _dpskt_accept_post,
    sizeof(_dpskt_acc_t), 0)

typedef struct
{
    int in_fd;
    int len;
    int flags;
} _dpsp_tee_t;

static dpret_t _dpgfd_splice_prep(dpele_t* fio, va_list arg, dpasc_out_t* out)
{
    _dpsp_tee_t* io = (_dpsp_tee_t*)out->data;
    io->in_fd = va_arg(arg, int);
    io->len = va_arg(arg, int);
    io->flags = va_arg(arg, int);
    out->want_events = DPEVT_OUT;
    out->inva_events = 0;
    if (io->in_fd < 0 || io->len <= 0)
        return DPE_INVAL;
    return DPE_CONTINUE;
}

static dpret_t _dpgfd_splice_post(dpele_t* fio, dpasc_out_t* out)
{
    _dpsp_tee_t* io = (_dpsp_tee_t*)out->data;
    int total = 0;
    for (;;) {
        int r = splice(io->in_fd, NULL, dpefd_fd(fio), NULL, io->len - total,
            io->flags);
        if (r > 0) {
            total += r;
            if (total == io->len)
                return total;
        } else if (r == 0) {
            return total > 0 ? total : DPE_OK;
        } else {
            out->inva_events = DPEVT_OUT;
            return total > 0 ? total : -errno;
        }
    }
}

DPASC_GFD_FUNCTION(splice, _dpgfd_splice_prep, _dpgfd_splice_post,
    sizeof(_dpsp_tee_t), 0)

dpret_t _dpskt_connect_prep(dpefd_t* skt, va_list arg, dpasc_out_t* out)
{
    const dpsockaddr_t* addr = va_arg(arg, const dpsockaddr_t*);
    if (addr == NULL) {
        return DPE_INVAL;
    }
    int ret = connect(dpefd_fd(skt), (const struct sockaddr*)&addr->addr,
        addr->real);

    if (ret == 0) {
        return DPE_OK;
    } else if (errno == EINPROGRESS) {
        out->inva_events = DPEVT_AIO; // 移除所有事件
        out->want_events = DPEVT_OUT;
        return DPE_CONTINUE;
    } else {
        return -errno;
    }
}

dpret_t _dpskt_connect_post(dpefd_t* skt, dpasc_out_t* out)
{
    (void)skt;
    (void)out;
    return DPE_OK;
}

DPASC_SKT_FUNCTION(connect, _dpskt_connect_prep, _dpskt_connect_post, 0, 0)

static dpret_t _dpgfd_tee_prep(dpele_t* fio, va_list arg, dpasc_out_t* out)
{
    _dpsp_tee_t* io = (_dpsp_tee_t*)out->data;
    io->in_fd = va_arg(arg, int);
    io->len = (int)va_arg(arg, unsigned int);
    io->flags = (int)va_arg(arg, unsigned int);
    out->want_events = DPEVT_OUT;
    out->inva_events = 0;
    if (io->in_fd < 0 || io->len <= 0)
        return DPE_INVAL;
    return DPE_CONTINUE;
}

static dpret_t _dpgfd_tee_post(dpele_t* fio, dpasc_out_t* out)
{
    _dpsp_tee_t* io = (_dpsp_tee_t*)out->data;
    int total = 0;
    for (;;) {
        int r = tee(io->in_fd, dpefd_fd(fio), io->len - total, io->flags);
        if (r > 0) {
            total += r;
            if (total == io->len)
                return total;
        } else if (r == 0) {
            return total > 0 ? total : DPE_OK;
        } else {
            out->inva_events = DPEVT_OUT;
            return total > 0 ? total : -errno;
        }
    }
}

DPASC_GFD_FUNCTION(tee, _dpgfd_tee_prep, _dpgfd_tee_post, sizeof(_dpsp_tee_t), 0)

static dpret_t _dpefd_fsync_prep(dpele_t* efd, va_list arg, dpasc_out_t* out)
{
    unsigned flags = (unsigned)va_arg(arg, int);
    int r = (flags & 1) ? fdatasync(dpefd_fd(efd)) : fsync(dpefd_fd(efd));
    return _dpsyc_ret_ok(r);
}

DPASC_EFD_FUNCTION(fsync, _dpefd_fsync_prep, NULL, 0, 0)

static dpret_t _dpskt_shutdown_prep(dpele_t* skt, va_list arg, dpasc_out_t* out)
{
    int how = va_arg(arg, int);
    return _dpsyc_ret_ok(shutdown(dpefd_fd(skt), how));
}

DPASC_SKT_FUNCTION(shutdown, _dpskt_shutdown_prep, NULL, 0, 0)

static dpret_t _dpefd_fallocate_prep(dpele_t* efd, va_list arg, dpasc_out_t* out)
{
    int mode = va_arg(arg, int);
    uint64_t offset = va_arg(arg, uint64_t);
    uint64_t len = va_arg(arg, uint64_t);
    return _dpsyc_ret_ok(fallocate(dpefd_fd(efd), mode, (off_t)offset, (off_t)len));
}

DPASC_EFD_FUNCTION(fallocate, _dpefd_fallocate_prep, NULL, 0, 0)

static dpret_t _dpefd_fadvise_prep(dpele_t* efd, va_list arg, dpasc_out_t* out)
{
    uint64_t offset = va_arg(arg, uint64_t);
    long len = va_arg(arg, long);
    int advice = va_arg(arg, int);
    return _dpsyc_ret_ok(
        posix_fadvise(dpefd_fd(efd), (off_t)offset, (off_t)len, advice));
}

DPASC_EFD_FUNCTION(fadvise, _dpefd_fadvise_prep, NULL, 0, 0)

static dpret_t _dpefd_sync_file_range_prep(dpele_t* efd, va_list arg,
    dpasc_out_t* out)
{
    uint64_t offset = va_arg(arg, uint64_t);
    unsigned nbytes = (unsigned)va_arg(arg, int);
    int flags = va_arg(arg, int);
    return _dpsyc_ret_ok(
        sync_file_range(dpefd_fd(efd), (off_t)offset, nbytes, flags));
}

DPASC_EFD_FUNCTION(sync_file_range, _dpefd_sync_file_range_prep, NULL, 0, 0)

static dpret_t _dpsyc_madvise_prep(dpele_t* ctc, va_list arg, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    a->ptr = va_arg(arg, void*);
    a->len = (uint64_t)va_arg(arg, long);
    a->advice = va_arg(arg, int);
    if (a->ptr == NULL || a->len == 0) {
        return DPE_INVAL;
    }
    return DPE_CONTINUE;
}

static dpret_t _dpsyc_madvise_post(dpele_t* ctc, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    return _dpsyc_ret_ok(madvise(a->ptr, (size_t)a->len, a->advice));
}

DPASC_SYC_FUNCTION(madvise, _dpsyc_madvise_prep, _dpsyc_madvise_post, 0)

static dpret_t _dpefd_close_prep(dpele_t* efd, va_list arg, dpasc_out_t* out)
{
    (void)arg;
    return _dpsyc_ret_ok(close(dpefd_fd(efd)));
}

DPASC_EFD_FUNCTION(close, _dpefd_close_prep, NULL, 0, 0)

static dpret_t _dpsyc_openat_prep(dpele_t* ctc, va_list arg, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    a->dirfd = va_arg(arg, int);
    a->path = va_arg(arg, const char*);
    a->flags = va_arg(arg, int);
    a->mode = (int)va_arg(arg, unsigned int);
    if (a->path == NULL) {
        return DPE_INVAL;
    }
    return DPE_CONTINUE;
}

static dpret_t _dpsyc_openat_post(dpele_t* ctc, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    return _dpsyc_ret_fd(openat(a->dirfd, a->path, a->flags, (mode_t)a->mode));
}

DPASC_SYC_FUNCTION(openat, _dpsyc_openat_prep, _dpsyc_openat_post, 0)

static dpret_t _dpsyc_open_prep(dpele_t* ctc, va_list arg, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    a->path = va_arg(arg, const char*);
    a->flags = va_arg(arg, int);
    a->mode = (int)va_arg(arg, unsigned int);
    if (a->path == NULL) {
        return DPE_INVAL;
    }
    return DPE_CONTINUE;
}

static dpret_t _dpsyc_open_post(dpele_t* ctc, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    return _dpsyc_ret_fd(openat(AT_FDCWD, a->path, a->flags, (mode_t)a->mode));
}

DPASC_SYC_FUNCTION(open, _dpsyc_open_prep, _dpsyc_open_post, 0)

#if defined(__linux__) && defined(__NR_openat2)
static dpret_t _dpsyc_openat2_prep(dpele_t* ctc, va_list arg, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    a->dirfd = va_arg(arg, int);
    a->path = va_arg(arg, const char*);
    a->ptr = va_arg(arg, struct open_how*);
    if (a->path == NULL || a->ptr == NULL) {
        return DPE_INVAL;
    }
    return DPE_CONTINUE;
}

static dpret_t _dpsyc_openat2_post(dpele_t* ctc, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    return _dpsyc_ret_fd((int)syscall(__NR_openat2, a->dirfd, a->path, a->ptr,
        sizeof(struct open_how)));
}
#else
static dpret_t _dpsyc_openat2_prep(dpele_t* ctc, va_list arg, dpasc_out_t* out)
{
    (void)ctc;
    (void)out;
    (void)va_arg(arg, int);
    (void)va_arg(arg, const char*);
    (void)va_arg(arg, struct open_how*);
    return DPE_UNSUPPORT;
}

static dpret_t _dpsyc_openat2_post(dpele_t* ctc, dpasc_out_t* out)
{
    (void)ctc;
    (void)out;
    return DPE_UNSUPPORT;
}
#endif

DPASC_SYC_FUNCTION(openat2, _dpsyc_openat2_prep, _dpsyc_openat2_post, 0)

#if defined(__linux__) && defined(__NR_statx)
static dpret_t _dpsyc_statx_prep(dpele_t* ctc, va_list arg, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    a->dirfd = va_arg(arg, int);
    a->path = va_arg(arg, const char*);
    a->flags = va_arg(arg, int);
    a->mask = (int)va_arg(arg, unsigned int);
    a->ptr = va_arg(arg, struct statx*);
    if (a->path == NULL || a->ptr == NULL) {
        return DPE_INVAL;
    }
    return DPE_CONTINUE;
}

static dpret_t _dpsyc_statx_post(dpele_t* ctc, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    return _dpsyc_ret_ok((int)syscall(__NR_statx, a->dirfd, a->path, a->flags,
        (unsigned)a->mask, a->ptr));
}
#else
static dpret_t _dpsyc_statx_prep(dpele_t* ctc, va_list arg, dpasc_out_t* out)
{
    (void)ctc;
    (void)out;
    (void)va_arg(arg, int);
    (void)va_arg(arg, const char*);
    (void)va_arg(arg, int);
    (void)va_arg(arg, int);
    (void)va_arg(arg, struct statx*);
    return DPE_UNSUPPORT;
}

static dpret_t _dpsyc_statx_post(dpele_t* ctc, dpasc_out_t* out)
{
    (void)ctc;
    (void)out;
    return DPE_UNSUPPORT;
}
#endif

DPASC_SYC_FUNCTION(statx, _dpsyc_statx_prep, _dpsyc_statx_post, 0)

static dpret_t _dpsyc_unlinkat_prep(dpele_t* ctc, va_list arg, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    a->dirfd = va_arg(arg, int);
    a->path = va_arg(arg, const char*);
    a->flags = va_arg(arg, int);
    if (a->path == NULL) {
        return DPE_INVAL;
    }
    return DPE_CONTINUE;
}

static dpret_t _dpsyc_unlinkat_post(dpele_t* ctc, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    return _dpsyc_ret_ok(unlinkat(a->dirfd, a->path, a->flags));
}

DPASC_SYC_FUNCTION(unlinkat, _dpsyc_unlinkat_prep, _dpsyc_unlinkat_post, 0)

static dpret_t _dpsyc_unlink_prep(dpele_t* ctc, va_list arg, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    a->path = va_arg(arg, const char*);
    a->flags = va_arg(arg, int);
    if (a->path == NULL) {
        return DPE_INVAL;
    }
    return DPE_CONTINUE;
}

static dpret_t _dpsyc_unlink_post(dpele_t* ctc, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    return _dpsyc_ret_ok(unlinkat(AT_FDCWD, a->path, a->flags));
}

DPASC_SYC_FUNCTION(unlink, _dpsyc_unlink_prep, _dpsyc_unlink_post, 0)

static dpret_t _dpsyc_renameat_prep(dpele_t* ctc, va_list arg, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    a->dirfd = va_arg(arg, int);
    a->path = va_arg(arg, const char*);
    a->dirfd2 = va_arg(arg, int);
    a->path2 = va_arg(arg, const char*);
    a->flags = (int)va_arg(arg, unsigned int);
    if (a->path == NULL || a->path2 == NULL) {
        return DPE_INVAL;
    }
    return DPE_CONTINUE;
}

static dpret_t _dpsyc_renameat_post(dpele_t* ctc, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
#if defined(__linux__) && defined(__NR_renameat2)
    return _dpsyc_ret_ok((int)syscall(__NR_renameat2, a->dirfd, a->path, a->dirfd2,
        a->path2, (unsigned)a->flags));
#else
    if (a->flags != 0) {
        return DPE_UNSUPPORT;
    }
    return _dpsyc_ret_ok(renameat(a->dirfd, a->path, a->dirfd2, a->path2));
#endif
}

DPASC_SYC_FUNCTION(renameat, _dpsyc_renameat_prep, _dpsyc_renameat_post, 0)

static dpret_t _dpsyc_rename_prep(dpele_t* ctc, va_list arg, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    a->path = va_arg(arg, const char*);
    a->path2 = va_arg(arg, const char*);
    if (a->path == NULL || a->path2 == NULL) {
        return DPE_INVAL;
    }
    return DPE_CONTINUE;
}

static dpret_t _dpsyc_rename_post(dpele_t* ctc, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
#if defined(__linux__) && defined(__NR_renameat2)
    return _dpsyc_ret_ok(
        (int)syscall(__NR_renameat2, AT_FDCWD, a->path, AT_FDCWD, a->path2, 0));
#else
    return _dpsyc_ret_ok(renameat(AT_FDCWD, a->path, AT_FDCWD, a->path2));
#endif
}

DPASC_SYC_FUNCTION(rename, _dpsyc_rename_prep, _dpsyc_rename_post, 0)

static dpret_t _dpsyc_mkdirat_prep(dpele_t* ctc, va_list arg, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    a->dirfd = va_arg(arg, int);
    a->path = va_arg(arg, const char*);
    a->mode = (int)va_arg(arg, unsigned int);
    if (a->path == NULL) {
        return DPE_INVAL;
    }
    return DPE_CONTINUE;
}

static dpret_t _dpsyc_mkdirat_post(dpele_t* ctc, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    return _dpsyc_ret_ok(mkdirat(a->dirfd, a->path, (mode_t)a->mode));
}

DPASC_SYC_FUNCTION(mkdirat, _dpsyc_mkdirat_prep, _dpsyc_mkdirat_post, 0)

static dpret_t _dpsyc_mkdir_prep(dpele_t* ctc, va_list arg, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    a->path = va_arg(arg, const char*);
    a->mode = (int)va_arg(arg, unsigned int);
    if (a->path == NULL) {
        return DPE_INVAL;
    }
    return DPE_CONTINUE;
}

static dpret_t _dpsyc_mkdir_post(dpele_t* ctc, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    return _dpsyc_ret_ok(mkdirat(AT_FDCWD, a->path, (mode_t)a->mode));
}

DPASC_SYC_FUNCTION(mkdir, _dpsyc_mkdir_prep, _dpsyc_mkdir_post, 0)

static dpret_t _dpsyc_symlinkat_prep(dpele_t* ctc, va_list arg, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    a->name = va_arg(arg, const char*);
    a->dirfd = va_arg(arg, int);
    a->path = va_arg(arg, const char*);
    if (a->name == NULL || a->path == NULL) {
        return DPE_INVAL;
    }
    return DPE_CONTINUE;
}

static dpret_t _dpsyc_symlinkat_post(dpele_t* ctc, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    return _dpsyc_ret_ok(symlinkat(a->name, a->dirfd, a->path));
}

DPASC_SYC_FUNCTION(symlinkat, _dpsyc_symlinkat_prep, _dpsyc_symlinkat_post, 0)

static dpret_t _dpsyc_symlink_prep(dpele_t* ctc, va_list arg, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    a->name = va_arg(arg, const char*);
    a->path = va_arg(arg, const char*);
    if (a->name == NULL || a->path == NULL) {
        return DPE_INVAL;
    }
    return DPE_CONTINUE;
}

static dpret_t _dpsyc_symlink_post(dpele_t* ctc, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    return _dpsyc_ret_ok(symlinkat(a->name, AT_FDCWD, a->path));
}

DPASC_SYC_FUNCTION(symlink, _dpsyc_symlink_prep, _dpsyc_symlink_post, 0)

static dpret_t _dpsyc_linkat_prep(dpele_t* ctc, va_list arg, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    a->dirfd = va_arg(arg, int);
    a->path = va_arg(arg, const char*);
    a->dirfd2 = va_arg(arg, int);
    a->path2 = va_arg(arg, const char*);
    a->flags = va_arg(arg, int);
    if (a->path == NULL || a->path2 == NULL) {
        return DPE_INVAL;
    }
    return DPE_CONTINUE;
}

static dpret_t _dpsyc_linkat_post(dpele_t* ctc, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    return _dpsyc_ret_ok(linkat(a->dirfd, a->path, a->dirfd2, a->path2, a->flags));
}

DPASC_SYC_FUNCTION(linkat, _dpsyc_linkat_prep, _dpsyc_linkat_post, 0)

static dpret_t _dpsyc_link_prep(dpele_t* ctc, va_list arg, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    a->path = va_arg(arg, const char*);
    a->path2 = va_arg(arg, const char*);
    a->flags = va_arg(arg, int);
    if (a->path == NULL || a->path2 == NULL) {
        return DPE_INVAL;
    }
    return DPE_CONTINUE;
}

static dpret_t _dpsyc_link_post(dpele_t* ctc, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    return _dpsyc_ret_ok(linkat(AT_FDCWD, a->path, AT_FDCWD, a->path2, a->flags));
}

DPASC_SYC_FUNCTION(link, _dpsyc_link_prep, _dpsyc_link_post, 0)

static dpret_t _dpsyc_getxattr_prep(dpele_t* ctc, va_list arg, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    a->name = va_arg(arg, const char*);
    a->buf = va_arg(arg, void*);
    a->path = va_arg(arg, const char*);
    a->len = (uint64_t)va_arg(arg, unsigned int);
    if (a->name == NULL || a->path == NULL) {
        return DPE_INVAL;
    }
    return DPE_CONTINUE;
}

static dpret_t _dpsyc_getxattr_post(dpele_t* ctc, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    ssize_t r = getxattr(a->path, a->name, a->buf, (size_t)a->len);
    return r >= 0 ? (dpret_t)r : -errno;
}

DPASC_SYC_FUNCTION(getxattr, _dpsyc_getxattr_prep, _dpsyc_getxattr_post, 0)

static dpret_t _dpsyc_setxattr_prep(dpele_t* ctc, va_list arg, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    a->name = va_arg(arg, const char*);
    a->buf = (void*)va_arg(arg, const void*);
    a->path = va_arg(arg, const char*);
    a->flags = va_arg(arg, int);
    a->len = (uint64_t)va_arg(arg, unsigned int);
    if (a->name == NULL || a->path == NULL) {
        return DPE_INVAL;
    }
    return DPE_CONTINUE;
}

static dpret_t _dpsyc_setxattr_post(dpele_t* ctc, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    return _dpsyc_ret_ok(
        setxattr(a->path, a->name, a->buf, (size_t)a->len, a->flags));
}

DPASC_SYC_FUNCTION(setxattr, _dpsyc_setxattr_prep, _dpsyc_setxattr_post, 0)

static dpret_t _dpsyc_fgetxattr_prep(dpele_t* ctc, va_list arg, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    a->fd = va_arg(arg, int);
    a->name = va_arg(arg, const char*);
    a->buf = va_arg(arg, void*);
    a->len = (uint64_t)va_arg(arg, unsigned int);
    if (a->name == NULL) {
        return DPE_INVAL;
    }
    return DPE_CONTINUE;
}

static dpret_t _dpsyc_fgetxattr_post(dpele_t* ctc, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    ssize_t r = fgetxattr(a->fd, a->name, a->buf, (size_t)a->len);
    return r >= 0 ? (dpret_t)r : -errno;
}

DPASC_SYC_FUNCTION(fgetxattr, _dpsyc_fgetxattr_prep, _dpsyc_fgetxattr_post, 0)

static dpret_t _dpsyc_fsetxattr_prep(dpele_t* ctc, va_list arg, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    a->fd = va_arg(arg, int);
    a->name = va_arg(arg, const char*);
    a->buf = (void*)va_arg(arg, const void*);
    a->flags = va_arg(arg, int);
    a->len = (uint64_t)va_arg(arg, unsigned int);
    if (a->name == NULL) {
        return DPE_INVAL;
    }
    return DPE_CONTINUE;
}

static dpret_t _dpsyc_fsetxattr_post(dpele_t* ctc, dpasc_out_t* out)
{
    dpsyc_arg_t* a = _dpsyc_arg(out);
    return _dpsyc_ret_ok(
        fsetxattr(a->fd, a->name, a->buf, (size_t)a->len, a->flags));
}

DPASC_SYC_FUNCTION(fsetxattr, _dpsyc_fsetxattr_prep, _dpsyc_fsetxattr_post, 0)
