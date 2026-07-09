#include "dpapp/dpasc.h"
#include "dpapp/dpbuf.h"
#include "dpapp/dpevp.h"
#include "dpapp/dpqic.h"
#include "dpapp/dpret.h"
#include "dpapp/dpssl.h"
#include "dpapp/os/dpevp_pri.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static dpret_t _dptmr_timeout_prep(dpele_t* tmr, va_list arg, dpasc_out_t* out)
{
    double sec = va_arg(arg, double);
    dpv64_t* data = (dpv64_t*)out->data;
    data[0] = va_arg(arg, dpv64_t);
    data[1] = va_arg(arg, dpv64_t);
    dpele_set_timeout(tmr, sec);
    return DPE_CONTINUE;
}

DPASC_TMR_FUNCTION(timeout, _dptmr_timeout_prep, sizeof(dpv64_t) * 2,
    DPASC_FLAG_ALLOW_DOING)

static dpret_t _dptmr_callback_prep(dpele_t* tmr, va_list arg, dpasc_out_t* out)
{
    double sec = va_arg(arg, double);
    dpv64_t* data = (dpv64_t*)out->data;
    dptmr_callback_f cb = va_arg(arg, dptmr_callback_f); // 回调函数
    data[1] = va_arg(arg, dpv64_t);                      // 回调参数

    if (cb) {
        data[0].ptr = cb;
    } else if (data[0].ptr == NULL) {
        return DPE_INVAL;
    }

    dpele_set_timeout(tmr, sec);
    return DPE_CONTINUE;
}

static dpret_t _dptmr_callback_post(dpele_t* tmr, dpasc_out_t* out)
{
    dpv64_t* data = (dpv64_t*)out->data;
    dptmr_callback_f cb = (dptmr_callback_f)data[0].ptr;
    cb(tmr, data[1]);
    return DPE_OK;
}

DPASC_FUNCTION(tmr, callback, _dptmr_callback_prep, _dptmr_callback_post,
    (1U << DPELE_TYPE_TMR), 0, sizeof(dpv64_t) * 2, DPASC_FLAG_ALLOW_DOING)

/** poll prep：校验 `evs` 后登记 `want_events`，由 `_dpefd_wake` 挂起或立即 post。 */
static dpret_t _dpefd_poll_prep(dpele_t* efd, va_list arg, dpasc_out_t* out)
{
    int evs = va_arg(arg, int);
    int poll_evs = dpele_type(efd)->events | DPEVT_ALL;
    if ((evs & poll_evs) == 0) {
        return DPE_INVAL;
    }
    out->want_events = evs;
    return DPE_CONTINUE;
}

/** poll post：就绪位已从 `able_events` 匹配，消耗对应事件并返回掩码供用户处理。 */
static dpret_t _dpefd_poll_post(dpele_t* efd, dpasc_out_t* out)
{
    out->inva_events = out->want_events;
    return out->want_events;
}

DPASC_EFD_FUNCTION(poll, _dpefd_poll_prep, _dpefd_poll_post, sizeof(int), 0)

static dpret_t _dpsyc_nop(dpele_t* usd, va_list arg, dpasc_out_t* out)
{
    (void)usd;
    (void)arg;
    (void)out;
    return DPE_OK;
}

DPASC_FUNCTION(syc, nop, _dpsyc_nop, NULL, (1U << DPELE_TYPE_USD), 0, 0, 0)

static inline const dpasc_t* _dpaio_get_asc(dpele_t* ele, int idx)
{
    static const dpasc_t* _IO_ASCS[(DPAIO_TYPE_QIC + 1) * 2] = {};
    static bool _not_init = true;
    if (_not_init) {
        _IO_ASCS[DPAIO_TYPE_GFD * 2 + 0] = dpgfd_read();
        _IO_ASCS[DPAIO_TYPE_GFD * 2 + 1] = dpgfd_write();
        _IO_ASCS[DPAIO_TYPE_SKT * 2 + 0] = dpskt_recv2();
        _IO_ASCS[DPAIO_TYPE_SKT * 2 + 1] = dpskt_send2();
#if DPAPP_HAS_SSL
        _IO_ASCS[DPAIO_TYPE_SSL * 2 + 0] = dpssl_recv();
        _IO_ASCS[DPAIO_TYPE_SSL * 2 + 1] = dpssl_send();
#if DPAPP_HAS_LSQUIC
        _IO_ASCS[DPAIO_TYPE_QIC * 2 + 0] = dpqic_recv();
        _IO_ASCS[DPAIO_TYPE_QIC * 2 + 1] = dpqic_send();
#endif
#endif
        _not_init = false;
    }
    return _IO_ASCS[dpele_type(ele)->iotype * 2 + idx];
}

#define DPAIO_BATCH_SIZE 8192

#define DPAIO_FUNCTION(NAME__)                                                      \
    const dpasc_t* dpaio_##NAME__()                                                 \
    {                                                                               \
        static dpasc_t _asc = {                                                     \
            .prep = _dpaio_##NAME__##_prep,                                         \
            .post = _dpaio_##NAME__##_post,                                         \
            .types = ((1U << DPELE_TYPE_EFD) | (1U << DPELE_TYPE_USD)),             \
            .iotypes = ((1U << DPAIO_TYPE_GFD) | (1U << DPAIO_TYPE_SKT)             \
                | (1U << DPAIO_TYPE_SSL) | (1U << DPAIO_TYPE_QIC)),                 \
            .datasz = sizeof(_dpaio_stm_t),                                         \
        };                                                                          \
        return &_asc;                                                               \
    }

typedef struct
{
    dpaio_arg_t io;
    dpbuf_t* buf;
    int len;
    int until_len;
    const char* until;
} _dpaio_stm_t;

static dpret_t _dpaio_read_some_prep(dpele_t* e, va_list arg, dpasc_out_t* out)
{
    const dpasc_t* sub = _dpaio_get_asc(e, 0);
    dpret_t ret = sub->prep(e, arg, out);
    if (ret != DPE_CONTINUE)
        return ret;

    _dpaio_stm_t* ioarg = out->data;

    // 置换数据，因为基础asc不支持dpbuf,但arg拥有相同的参数类型，可以获取到裸指针和长度
    ioarg->buf = (dpbuf_t*)ioarg->io.ptr;
    ioarg->len = ioarg->io.len; // 最大大小
    ioarg->io.total = 0;

    /* 如果 dpbuf 已有足够数据，直接返回 */
    int esz = dpbuf_eseek(ioarg->buf, 0, SEEK_END);
    if (esz >= ioarg->len) {
        return esz;
    }

    /* 扩容可写区，准备读取剩余字节 */
    int bytes = ioarg->len - esz;
    if (bytes > DPAIO_BATCH_SIZE)
        bytes = DPAIO_BATCH_SIZE;
    if (!dpbuf_resizew(ioarg->buf, bytes))
        return DPE_NOMEM;

    // 填充ioarg->io, 在post中调用子post, 读取到数据
    ioarg->io.buf = (void*)dpbuf_cwdata(ioarg->buf);
    ioarg->io.len = dpbuf_cwsize(ioarg->buf);

    return ret;
}

static dpret_t _dpaio_read_some_post(dpele_t* e, dpasc_out_t* out)
{
    _dpaio_stm_t* stm = (_dpaio_stm_t*)out->data;
    const dpasc_t* sub = _dpaio_get_asc(e, 0);

    // 因为_dpaio_stm_t的头是dpasc_ioarg_t，所以调用post，没有任何影响
    // 且out中事件字段由子post接管，之后不需要修改
    dpret_t ret = sub->post(e, out);

    if (ret <= 0) {
        return (stm->io.total > 0) ? stm->io.total : ret;
    }

    stm->io.total += ret;

    dpbuf_wseek(stm->buf, ret, SEEK_CUR);
    dpbuf_eseek(stm->buf, 0, SEEK_END);

    /* 本轮未满，无更多数据  或 已多于最大大小*/
    if (ret < stm->io.len || stm->io.total >= stm->len)
        return (dpret_t)stm->io.total;

    /* 开始下一轮，扩容 dpbuf */
    int wsz = stm->len - stm->io.total;
    if (wsz > DPAIO_BATCH_SIZE)
        wsz = DPAIO_BATCH_SIZE;

    if (!dpbuf_resizew(stm->buf, wsz))
        return stm->io.total > 0 ? stm->io.total : DPE_NOMEM;

    stm->io.buf = (void*)dpbuf_cwdata(stm->buf);
    stm->io.len = dpbuf_cwsize(stm->buf);
    return DPE_WAIT;
}

static dpret_t _dpaio_write_some_prep(dpele_t* e, va_list arg, dpasc_out_t* out)
{
    const dpasc_t* sub = _dpaio_get_asc(e, 1);
    dpret_t ret = sub->prep(e, arg, out);
    if (ret != DPE_CONTINUE)
        return ret;

    _dpaio_stm_t* ioarg = out->data;

    // 置换数据，因为基础asc不支持dpbuf,但arg拥有相同的参数类型，可以获取到裸指针和长度
    ioarg->buf = (dpbuf_t*)ioarg->io.ptr;
    ioarg->len = ioarg->io.len; // min_len
    ioarg->io.total = 0;

    // 填充ioarg->io, 在post中调用子post, 写入数据
    ioarg->io.buf = (void*)dpbuf_crdata(ioarg->buf);
    ioarg->io.len = dpbuf_crsize(ioarg->buf);

    return ret;
}

static dpret_t _dpaio_write_some_post(dpele_t* e, dpasc_out_t* out)
{
    _dpaio_stm_t* stm = (_dpaio_stm_t*)out->data;
    const dpasc_t* sub = _dpaio_get_asc(e, 1);

    // 因为_dpaio_stm_t的头是dpasc_ioarg_t，所以调用post，没有任何影响
    dpret_t ret = sub->post(e, out);

    if (ret <= 0) {
        if (stm->io.total >= stm->len)
            return (dpret_t)stm->io.total; // 已达标，忽略末尾错误
        return ret;                        // 透传 DPE_WAIT / 错误
    }

    stm->io.total += (int)ret;
    dpbuf_rseek(stm->buf, (int)ret, SEEK_CUR);

    /* 达到 min_len */
    if (stm->io.total >= stm->len)
        return (dpret_t)stm->io.total;

    /* 未达标，推进缓冲区位置后继续等待 */
    stm->io.buf = (char*)stm->io.buf + (int)ret;
    stm->io.len -= (int)ret;
    if (dpele_type(e)->type == DPELE_TYPE_EFD)
        out->want_events = DPEVT_OUT;
    return DPE_WAIT;
}

static dpret_t _dpaio_read_must_prep(dpele_t* e, va_list arg, dpasc_out_t* out)
{
    const dpasc_t* sub = _dpaio_get_asc(e, 0);
    dpret_t ret = sub->prep(e, arg, out);
    if (ret != DPE_CONTINUE)
        return ret;

    _dpaio_stm_t* ioarg = out->data;

    // 置换数据，因为基础asc不支持dpbuf,但arg拥有相同的参数类型，可以获取到裸指针和长度
    ioarg->buf = (dpbuf_t*)ioarg->io.ptr;
    ioarg->len = ioarg->io.len; // 必须读满的字节数
    ioarg->io.total = 0;

    /* 如果 dpbuf 已有足够数据，直接返回 */
    int esz = dpbuf_cesize(ioarg->buf);
    if (esz >= ioarg->len) {
        return dpbuf_eseek(ioarg->buf, ioarg->len, SEEK_CUR);
    }

    /* 扩容可写区，准备读取剩余字节 */
    int bytes = ioarg->len - esz;
    if (!dpbuf_resizew(ioarg->buf, bytes))
        return DPE_NOMEM;

    ioarg->io.buf = dpbuf_cwdata(ioarg->buf);
    ioarg->io.len = bytes;

    return ret;
}

static dpret_t _dpaio_read_must_post(dpele_t* e, dpasc_out_t* out)
{
    _dpaio_stm_t* stm = (_dpaio_stm_t*)out->data;
    const dpasc_t* sub = _dpaio_get_asc(e, 0);

    // 因为_dpaio_stm_t的头是dpasc_ioarg_t，所以调用post，没有任何影响
    dpret_t ret = sub->post(e, out);

    if (ret <= 0) {
        return (stm->io.total > 0) ? stm->io.total : ret;
    }

    stm->io.total += (int)ret;
    dpbuf_wseek(stm->buf, (int)ret, SEEK_CUR);

    /* 读满所需字节 */
    if (stm->io.total >= stm->io.len)
        return (dpret_t)stm->io.total;

    /* 未读满，推进缓冲区位置后继续等待 */
    stm->io.buf = dpbuf_cwdata(stm->buf);
    stm->io.len -= (int)ret;
    if (dpele_type(e)->type == DPELE_TYPE_EFD)
        out->want_events = DPEVT_IN;
    return DPE_WAIT;
}

static dpret_t _dpaio_write_must_prep(dpele_t* e, va_list arg, dpasc_out_t* out)
{
    const dpasc_t* sub = _dpaio_get_asc(e, 1);
    dpret_t ret = sub->prep(e, arg, out);
    if (ret != DPE_CONTINUE)
        return ret;

    _dpaio_stm_t* ioarg = out->data;

    // 置换数据，因为基础asc不支持dpbuf,但arg拥有相同的参数类型，可以获取到裸指针和长度
    ioarg->buf = (dpbuf_t*)ioarg->io.ptr;
    ioarg->len = ioarg->io.len; // 必须写完的字节数
    ioarg->io.total = 0;

    // 填充ioarg->io, 精确写入 len 字节，不能多不能少
    ioarg->io.buf = (void*)dpbuf_crdata(ioarg->buf);
    ioarg->io.len = ioarg->len;

    return ret;
}

static dpret_t _dpaio_write_must_post(dpele_t* e, dpasc_out_t* out)
{
    _dpaio_stm_t* stm = (_dpaio_stm_t*)out->data;
    const dpasc_t* sub = _dpaio_get_asc(e, 1);

    // 因为_dpaio_stm_t的头是dpasc_ioarg_t，所以调用post，没有任何影响
    dpret_t ret = sub->post(e, out);

    if (ret <= 0) {
        return (stm->io.total > 0) ? stm->io.total : ret;
    }

    stm->io.total += (int)ret;
    dpbuf_rseek(stm->buf, (int)ret, SEEK_CUR);

    /* 写完所需字节 */
    if (stm->io.total >= stm->io.len)
        return (dpret_t)stm->io.total;

    /* 未写完，推进缓冲区位置后继续等待 */
    stm->io.buf = (char*)stm->io.buf + (int)ret;
    stm->io.len -= (int)ret;
    if (dpele_type(e)->type == DPELE_TYPE_EFD)
        out->want_events = DPEVT_OUT;
    return DPE_WAIT;
}

static dpret_t _dpaio_read_until_prep(dpele_t* e, va_list arg, dpasc_out_t* out)
{
    _dpaio_stm_t* ioarg = out->data;
    // 解析分隔串参数
    ioarg->until = va_arg(arg, const char*);
    ioarg->until_len = va_arg(arg, int);
    if (ioarg->until == NULL || ioarg->until_len < 1)
        return DPE_INVAL;

    const dpasc_t* sub = _dpaio_get_asc(e, 0);
    dpret_t ret = sub->prep(e, arg, out);
    if (ret != DPE_CONTINUE)
        return ret;

    // 置换数据，因为基础asc不支持dpbuf,但arg拥有相同的参数类型，可以获取到裸指针和长度
    ioarg->buf = (dpbuf_t*)ioarg->io.ptr;
    ioarg->len = ioarg->io.len; // max_len
    ioarg->io.total = 0;

    /* 提交已有数据，检查是否已命中分隔串 */
    int esz = dpbuf_cesize(ioarg->buf);
    if (esz > 0) {
        dpbuf_eseek(ioarg->buf, 0, SEEK_END);
        int idx = dpbuf_cfind(ioarg->buf, ioarg->until, ioarg->until_len, 0);
        if (idx >= 0) {
            dpbuf_eseek(ioarg->buf, idx + ioarg->until_len, SEEK_SET);
            return idx + ioarg->until_len;
        }
        ioarg->io.flag = dpbuf_crsize(ioarg->buf); // 已扫描字节数
    } else {
        ioarg->io.flag = 0;
    }

    /* 扩容可写区，准备读取 */
    int bytes = ioarg->len;
    if (bytes > DPAIO_BATCH_SIZE)
        bytes = DPAIO_BATCH_SIZE;
    if (!dpbuf_resizew(ioarg->buf, bytes))
        return DPE_NOMEM;

    ioarg->io.buf = (void*)dpbuf_cwdata(ioarg->buf);
    ioarg->io.len = dpbuf_cwsize(ioarg->buf);

    return ret;
}

static dpret_t _dpaio_read_until_post(dpele_t* e, dpasc_out_t* out)
{
    _dpaio_stm_t* stm = (_dpaio_stm_t*)out->data;
    const dpasc_t* sub = _dpaio_get_asc(e, 0);

    // 因为_dpaio_stm_t的头是dpasc_ioarg_t，所以调用post，没有任何影响
    dpret_t ret = sub->post(e, out);

    if (ret <= 0) {
        return (stm->io.total > 0) ? stm->io.total : ret;
    }

    stm->io.total += (int)ret;
    dpbuf_wseek(stm->buf, (int)ret, SEEK_CUR);
    dpbuf_eseek(stm->buf, 0, SEEK_END);

    /* 从已扫描位置之后查找分隔串 */
    int left = stm->io.flag - stm->until_len + 1;
    if (left < 0)
        left = 0;
    int idx = dpbuf_cfind(stm->buf, stm->until, stm->until_len, left);
    if (idx >= 0) {
        dpbuf_eseek(stm->buf, idx + stm->until_len, SEEK_SET);
        return idx + stm->until_len;
    }

    /* 未命中，更新已扫描位置 */
    stm->io.flag = stm->io.total;

    /* 超过 max_len 仍未找到，返回 DPE_CONTINUE */
    if (stm->io.total >= stm->len)
        return DPE_CONTINUE;

    /* 扩容下一批 */
    int bytes = stm->len - stm->io.total;
    if (bytes > DPAIO_BATCH_SIZE)
        bytes = DPAIO_BATCH_SIZE;
    if (!dpbuf_resizew(stm->buf, bytes))
        return stm->io.total > 0 ? stm->io.total : DPE_NOMEM;

    stm->io.buf = (void*)dpbuf_cwdata(stm->buf);
    stm->io.len = dpbuf_cwsize(stm->buf);
    if (dpele_type(e)->type == DPELE_TYPE_EFD)
        out->want_events = DPEVT_IN;
    return DPE_WAIT;
}

static dpret_t _dpaio_read_data_prep(dpele_t* e, va_list arg, dpasc_out_t* out)
{
    const dpasc_t* sub = _dpaio_get_asc(e, 0);
    dpret_t ret = sub->prep(e, arg, out);
    if (ret != DPE_CONTINUE)
        return ret;

    _dpaio_stm_t* stm = (_dpaio_stm_t*)out->data;
    stm->buf = NULL;
    stm->len = stm->io.len;
    stm->io.total = 0;
    return ret;
}

static dpret_t _dpaio_read_data_post(dpele_t* e, dpasc_out_t* out)
{
    _dpaio_stm_t* stm = (_dpaio_stm_t*)out->data;
    const dpasc_t* sub = _dpaio_get_asc(e, 0);

    dpret_t ret = sub->post(e, out);

    if (ret <= 0) {
        return (stm->io.total > 0) ? stm->io.total : ret;
    }

    stm->io.total += (int)ret;
    if (stm->io.total >= stm->len)
        return (dpret_t)stm->io.total;

    stm->io.buf = (char*)stm->io.buf + (int)ret;
    stm->io.len -= (int)ret;
    if (dpele_type(e)->type == DPELE_TYPE_EFD)
        out->want_events = DPEVT_IN;
    return DPE_WAIT;
}

static dpret_t _dpaio_write_data_prep(dpele_t* e, va_list arg, dpasc_out_t* out)
{
    const dpasc_t* sub = _dpaio_get_asc(e, 1);
    dpret_t ret = sub->prep(e, arg, out);
    if (ret != DPE_CONTINUE)
        return ret;

    _dpaio_stm_t* stm = (_dpaio_stm_t*)out->data;
    stm->buf = NULL;
    stm->len = stm->io.len;
    stm->io.total = 0;
    return ret;
}

static dpret_t _dpaio_write_data_post(dpele_t* e, dpasc_out_t* out)
{
    _dpaio_stm_t* stm = (_dpaio_stm_t*)out->data;
    const dpasc_t* sub = _dpaio_get_asc(e, 1);

    dpret_t ret = sub->post(e, out);

    if (ret <= 0) {
        return (stm->io.total > 0) ? stm->io.total : ret;
    }

    stm->io.total += (int)ret;
    if (stm->io.total >= stm->len)
        return (dpret_t)stm->io.total;

    stm->io.buf = (char*)stm->io.buf + (int)ret;
    stm->io.len -= (int)ret;
    if (dpele_type(e)->type == DPELE_TYPE_EFD)
        out->want_events = DPEVT_OUT;
    return DPE_WAIT;
}

DPAIO_FUNCTION(read_some)
DPAIO_FUNCTION(read_must)
DPAIO_FUNCTION(read_data)
DPAIO_FUNCTION(read_until)
DPAIO_FUNCTION(write_some)
DPAIO_FUNCTION(write_must)
DPAIO_FUNCTION(write_data)
