#include "dplua_ext.h"
#include "dpapp/dpapp.h"
#include "dpapp/dpevp.h"
#include "dpapp/dplog.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void _to_base62(uint64_t value, char* buffer)
{
    static const char base62_chars
        [] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    memset(buffer, '0', 11);
    for (int i = 10; value > 0 && i >= 0; i--) {
        buffer[i] = base62_chars[value % 62];
        value /= 62;
    }
}

static uint64_t _to_uint64(const char* b62)
{
    if (b62 == NULL) {
        return 0;
    }

    int l = (int)strlen(b62);
    if (l > 11) {
        l = 11;
    }

    uint64_t v = 0;
    for (int i = 0; i < l; i++) {
        int c = b62[i];
        if (c >= '0' && c <= '9') {
            c = c - '0';
        } else if (c >= 'A' && c <= 'Z') {
            c = 10 + (c - 'A');
        } else if (c >= 'a' && c <= 'z') {
            c = 36 + (c - 'a');
        } else {
            return 0;
        }
        v = v * 62 + (uint64_t)c;
    }
    return v;
}

struct dpid_t
{
    uint64_t sequence   : 10;
    uint64_t thread_id  : 8;
    uint64_t machine_id : 5;
    uint64_t timestamp  : 41;
};

struct dpid_ctx_t
{
    int sequence;
    uint64_t timestamp;
};

static __thread struct dpid_ctx_t _g_idctx = {0, 0};

uint64_t dpid_next(char* out_str)
{
    static int64_t _base_ts = 1735660800000;
    static int _max_sequence = (1 << 10) - 1;
    struct dpid_ctx_t* ctx = &_g_idctx;

    struct dpid_t id;
    id.machine_id = dpapp_info()->machine_id;
    id.thread_id = (uint64_t)dpevp_id();
    id.sequence = (uint64_t)ctx->sequence++;
    id.timestamp = (uint64_t)(dplog_millis() - _base_ts);

    if (id.sequence == (uint64_t)_max_sequence) {
        ctx->sequence = 0;
        while (id.timestamp == ctx->timestamp) {
            usleep(5000);
            id.timestamp = (uint64_t)(dplog_millis() - _base_ts);
        }
    }
    ctx->timestamp = id.timestamp;

    uint64_t ret = *(uint64_t*)&id;
    if (out_str) {
        _to_base62(ret, out_str);
    }
    return ret;
}

uint64_t dpid_2u64(const char* str)
{
    return _to_uint64(str);
}

void dpid_2str(uint64_t id, char* out_str)
{
    _to_base62(id, out_str);
}

dptask_comspec_t* dptask_comspec_new(const char* info, const char* body,
    const char* args, int reto_node, int reto_name)
{
    dptask_comspec_t* comspec = (dptask_comspec_t*)calloc(1,
        sizeof(dptask_comspec_t));
    if (comspec == NULL) {
        return NULL;
    }

    if (info) {
        comspec->info = strdup(info);
    }
    if (body) {
        comspec->body = strdup(body);
    }
    if (args) {
        comspec->args = strdup(args);
    }
    comspec->reto_node = reto_node;
    comspec->reto_name = reto_name;
    return comspec;
}

void dptask_comspec_set_result(dptask_comspec_t* comspec, int ok, const char* result)
{
    if (comspec == NULL) {
        return;
    }

    comspec->ok = ok;
    if (comspec->result) {
        free(comspec->result);
        comspec->result = NULL;
    }
    if (result) {
        comspec->result = strdup(result);
    }
}

void dptask_comspec_del(dptask_comspec_t* comspec)
{
    if (comspec == NULL) {
        return;
    }

    if (comspec->info) {
        free(comspec->info);
    }
    if (comspec->body) {
        free(comspec->body);
    }
    if (comspec->args) {
        free(comspec->args);
    }
    if (comspec->result) {
        free(comspec->result);
    }
    memset(comspec, 0, sizeof(dptask_comspec_t));
}

void dptask_comspec_del2(void* comspec)
{
    dptask_comspec_del((dptask_comspec_t*)comspec);
}
