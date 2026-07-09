#include "dpapp/os/dpevp_pri.h"
#include "dpapp/dpevp.h"
#include <stdarg.h>
#include <stdbool.h>

static dpret_t _dpctc_type_init(void* udata, va_list vlist)
{
    dpele_t* ele = dpele_get_by_uptr(udata);
    int toid = va_arg(vlist, int);
    int detach = va_arg(vlist, int);
    dpret_t r = dpctc_set_toid(ele, toid);
    if (dpret_iserr(r)) {
        return r;
    }
    return dpele_set_detach(ele, detach != 0);
}

const dpele_type_t* dptmr_init_type()
{
    static dpele_type_t _type = {
        .name = "tmr",
        .type = DPELE_TYPE_TMR,
        .size = 0,
        .iotype = DPAIO_TYPE_NAN,
    };
    return &_type;
}

const dpele_type_t* dpctc_init_type()
{
    static dpele_type_t _type = {
        .name = "ctc",
        .type = DPELE_TYPE_CTC,
        .size = 0,
        .iotype = DPAIO_TYPE_NAN,
        .init = _dpctc_type_init,
    };
    return &_type;
}

static uint32_t _dpctc_hash_ptr(const void* ptr, uint32_t range)
{
    dpv64_t b = DPV64_PTR(ptr);
    b.ptr = (void*)ptr;
    uint32_t v = 0;
    for (int i = 0; i < 8; i++) {
        v = v * 31 + b.bytes[i];
    }
    return v % range;
}

int _dpctc_resolve_toid(const dpele_t* key, int toid)
{
    if (toid >= 0 && toid < _gevps.count) {
        return toid;
    }

    if (toid < 0) {
        if (-toid >= _gevps.type_count || _gevps.each_count[-toid] == 0) {
            return DPE_INVAL;
        }
        int idx = (int)_dpctc_hash_ptr(key, (uint32_t)_gevps.each_count[-toid]);
        return _gevps.each_ids[-toid][idx];
    }

    return DPE_INVAL;
}
