#pragma once

#include "dpapp/dpbuf.h"
#include "dpapp/dpdef.h"
#include "dpapp/dpevp.h"
#include "dpapp/dpret.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

struct dpele;

// dpevp 内部接口
typedef struct _dpevp dpevp_t;

typedef struct
{
    DPEVP_SET_MEMBER
    dpevp_t** evps;
} _dpevp_set_t;

extern _dpevp_set_t _gevps;

dpevp_t* dpevp_new(int type, int id);
void dpevp_del(dpevp_t* self);
void dpevp_start(dpevp_t* self, void (*start_fun)(void*), void* start_arg);

dpele_t* dpele_get_by_uptr(void* uptr);

// 仅供其他语言绑定使用
void _dpele_set_cop(dpele_t* self, dpv64_t cop);

const dpasc_t* _dpele_asc_type(dpele_t* ele);

void _dpele_del(dpele_t* self);

/** @brief 解析 CTC 目标 worker：`toid>=0` 为 id，负数为 type 索引；失败返回负
 * `dpret_t`。
 */
int _dpctc_resolve_toid(const dpele_t* key, int toid);

#if defined(__FreeBSD__) || defined(__APPLE__)
dpret_t _dpevp_kq_sig_add(dpele_t* ele, int signo);
dpret_t _dpevp_kq_sig_del(int signo);
void _dpsig_kq_notify(dpele_t* ele, int signo);

dpret_t _dpevp_kq_vnode_add(int vnode_fd, void* udata, uint32_t fflags);
dpret_t _dpevp_kq_vnode_del(int vnode_fd);
void _dpfsm_kq_notify(void* udata, uint32_t note_fflags);
#endif

#ifdef __cplusplus
}
#endif
