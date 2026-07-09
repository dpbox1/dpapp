#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "dpapp/dpapp.h"
#include "dpapp/dpasc.h"
#include "dpapp/dpbuf.h"
#include "dpapp/dpdef.h"
#include "dpapp/dpevp.h"
#include "dpapp/dplog.h"
#include "dpapp/dpret.h"
#include "dpapp/os/dpevp_pri.h"
#include "dplua.h"

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

typedef struct dplua_thread_context
{
    lua_State* mainL;
} dplua_thread_context;

static _Thread_local dplua_thread_context _g_ctx;

typedef struct dplua_arg
{
    int argc;
    char** argv;
} dplua_arg_t;

typedef struct
{
    dpapp_arg_t* arg;
    dplua_arg_t* larg;
    dplua_config_t* config;
} dplua_thread_arg_t;

static dpret_t _encode_value(lua_State* L, int idx, dpbuf_t* buf);
static dpret_t _decode_value(lua_State* L, dpbuf_t* buf);
static dplua_config_t* _load_config(dpapp_arg_t* sarg, dplua_arg_t* larg);
static void _free_config(dplua_config_t* cfg);

static int l_timestamp(lua_State* L)
{
    const int ta = (int)luaL_optinteger(L, 1, DPLOG_TA_MILLIS);
    const int64_t ts = dplog_nowts((dplog_tsacc_e)ta);
    lua_pushinteger(L, (lua_Integer)ts);
    return 1;
}

static int l_now(lua_State* L)
{
    const struct tm* stm = dplog_nowtm();
    if (stm == NULL) {
        lua_pushnil(L);
        return 1;
    }
    if (lua_gettop(L) >= 1 && lua_type(L, 1) == LUA_TSTRING) {
        const char* fmt = lua_tostring(L, 1);
        if (strcmp(fmt, "*t") == 0) {
            lua_createtable(L, 0, 9);
            lua_pushinteger(L, stm->tm_sec);
            lua_setfield(L, -2, "sec");
            lua_pushinteger(L, stm->tm_min);
            lua_setfield(L, -2, "min");
            lua_pushinteger(L, stm->tm_hour);
            lua_setfield(L, -2, "hour");
            lua_pushinteger(L, stm->tm_mday);
            lua_setfield(L, -2, "day");
            lua_pushinteger(L, stm->tm_mon + 1);
            lua_setfield(L, -2, "month");
            lua_pushinteger(L, stm->tm_year + 1900);
            lua_setfield(L, -2, "year");
            lua_pushinteger(L, stm->tm_wday + 1);
            lua_setfield(L, -2, "wday");
            lua_pushinteger(L, stm->tm_yday + 1);
            lua_setfield(L, -2, "yday");
            lua_pushboolean(L, stm->tm_isdst == 1);
            lua_setfield(L, -2, "isdst");
            return 1;
        }
        char buf[1024];
        memset(buf, 0, sizeof buf);
        const size_t tsz = strftime(buf, sizeof buf, fmt, stm);
        if (tsz > 0)
            lua_pushlstring(L, buf, tsz);
        else
            lua_pushnil(L);
        return 1;
    }
    char buf[1024];
    memset(buf, 0, sizeof buf);
    const size_t tsz = strftime(buf, sizeof buf, "%c", stm);
    if (tsz > 0)
        lua_pushlstring(L, buf, tsz);
    else
        lua_pushnil(L);
    return 1;
}

static void _inject_dplua_globals(lua_State* L, const char* root_dir)
{
    const dpapp_info_t* info = dpapp_info();
    if (root_dir == NULL && info)
        root_dir = info->root_dir;
    if (root_dir == NULL)
        root_dir = "";

    lua_newtable(L);
    const int dplua_tbl = lua_gettop(L);

    lua_pushstring(L, root_dir);
    lua_setfield(L, dplua_tbl, "rootdir");

    lua_pushcfunction(L, l_timestamp);
    lua_setfield(L, dplua_tbl, "timestamp");

    lua_pushinteger(L, sysconf(_SC_NPROCESSORS_ONLN));
    lua_setfield(L, dplua_tbl, "cpucount");

    lua_pushcfunction(L, l_now);
    lua_setfield(L, dplua_tbl, "now");

    lua_pushinteger(L, dpevp_id());
    lua_setfield(L, dplua_tbl, "id");

    lua_pushinteger(L, dpevp_type());
    lua_setfield(L, dplua_tbl, "type_id");

    lua_pushinteger(L, DPROLE_CLIENT);
    lua_setfield(L, dplua_tbl, "ROLE_CLIENT");
    lua_pushinteger(L, DPROLE_SERVER);
    lua_setfield(L, dplua_tbl, "ROLE_SERVER");
    lua_pushinteger(L, DPROLE_UNSURE);
    lua_setfield(L, dplua_tbl, "ROLE_UNSURE");

    lua_pushinteger(L, DPEVT_NAN);
    lua_setfield(L, dplua_tbl, "EVT_NAN");
    lua_pushinteger(L, DPEVT_IN);
    lua_setfield(L, dplua_tbl, "EVT_IN");
    lua_pushinteger(L, DPEVT_PRI);
    lua_setfield(L, dplua_tbl, "EVT_PRI");
    lua_pushinteger(L, DPEVT_OUT);
    lua_setfield(L, dplua_tbl, "EVT_OUT");
    lua_pushinteger(L, DPEVT_ERR);
    lua_setfield(L, dplua_tbl, "EVT_ERR");
    lua_pushinteger(L, DPEVT_HUP);
    lua_setfield(L, dplua_tbl, "EVT_HUP");
    lua_pushinteger(L, DPEVT_AIN);
    lua_setfield(L, dplua_tbl, "EVT_AIN");
    lua_pushinteger(L, DPEVT_ALL);
    lua_setfield(L, dplua_tbl, "EVT_ALL");

    lua_newtable(L);
    const int each_ids = lua_gettop(L);
    if (info) {
        for (int i = 0; i < info->type_count; i++) {
            lua_newtable(L);
            const int tids = lua_gettop(L);
            for (int n = 0; n < info->each_count[i]; n++) {
                lua_pushinteger(L, info->each_ids[i][n]);
                lua_rawseti(L, tids, n + 1);
            }
            lua_rawseti(L, each_ids, i);
        }
    }
    lua_setfield(L, dplua_tbl, "each_ids");

    lua_pushinteger(L, info ? info->machine_id : 0);
    lua_setfield(L, dplua_tbl, "machine_id");

    lua_pushinteger(L, DPAPP_VERSION_MAJOR);
    lua_setfield(L, dplua_tbl, "app_version_major");
    lua_pushinteger(L, DPAPP_VERSION_MINOR);
    lua_setfield(L, dplua_tbl, "app_version_minor");
    lua_pushstring(L, DPAPP_VERSION);
    lua_setfield(L, dplua_tbl, "app_version");

    lua_pushstring(L, "epoll");
    lua_setfield(L, dplua_tbl, "app_poller");

    lua_pushvalue(L, dplua_tbl);
    lua_setfield(L, LUA_GLOBALSINDEX, "dplua");
    lua_pop(L, 1);
}

static void _push_runtime_args(lua_State* L, dpapp_arg_t* arg, dplua_arg_t* larg)
{
    lua_newtable(L);
    lua_pushstring(L, arg->bin_file ? arg->bin_file : "");
    lua_setfield(L, -2, "-1");
    lua_pushstring(L, larg->argv[0]);
    lua_rawseti(L, -2, 0);
    for (int i = 1; i < larg->argc; i++) {
        lua_pushstring(L, larg->argv[i]);
        lua_rawseti(L, -2, i);
    }
}

static void _push_runtime_hdr(lua_State* L, dplua_config_t* config)
{
    lua_newtable(L);
    const int tid = dpevp_type();
    if (config && tid >= 0 && tid < config->type_count) {
        dplua_hdr_t* hdr = &config->hdrs[tid];
        if (hdr->init_name[0]) {
            lua_pushstring(L, hdr->init_name);
            lua_setfield(L, -2, "init");
            dpbuf_rseek(&hdr->arg_buf, 0, SEEK_SET);
            if (dpbuf_crsize(&hdr->arg_buf) > 0)
                _decode_value(L, &hdr->arg_buf);
            else
                lua_pushnil(L);
            lua_setfield(L, -2, "args");
        }
    }
}

static void _setfenv_with_runtime(lua_State* L, int func_idx, int args_idx,
    int hdr_idx, bool is_main)
{
    lua_newtable(L);
    const int env = lua_gettop(L);
    lua_newtable(L);
    lua_pushvalue(L, LUA_GLOBALSINDEX);
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, env);
    lua_pushvalue(L, args_idx);
    lua_setfield(L, env, "__ARGS__");
    lua_pushvalue(L, hdr_idx);
    lua_setfield(L, env, "__HDR__");
    lua_pushboolean(L, is_main);
    lua_setfield(L, env, "__DPLUA_MAIN__");
    lua_getfield(L, LUA_GLOBALSINDEX, "dplua");
    lua_setfield(L, env, "dplua");
    lua_setfenv(L, func_idx);
}

static bool _run_dplua_lua(lua_State* L, dpapp_arg_t* arg, dplua_arg_t* larg,
    dplua_config_t* config, bool is_main, int* out_type_count)
{
    char path[PATH_MAX];
    snprintf(path, sizeof path, "%s/lua/dplua/dplua.lua", arg->root_dir);
    if (luaL_loadfile(L, path) != 0) {
        dplog_error("dplua", "load %s: %s", path, lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    const int func = lua_gettop(L);
    _push_runtime_args(L, arg, larg);
    const int args_idx = lua_gettop(L);
    if (is_main)
        lua_newtable(L);
    else
        _push_runtime_hdr(L, config);
    const int hdr_idx = lua_gettop(L);
    if (is_main) {
        lua_pushvalue(L, hdr_idx);
        lua_setfield(L, LUA_REGISTRYINDEX, "_dplua_hdrs");
    }
    _setfenv_with_runtime(L, func, args_idx, hdr_idx, is_main);
    lua_pop(L, 2);
    const int nresults = is_main ? 1 : 0;
    if (lua_pcall(L, 0, nresults, 0) != 0) {
        dplog_error("dplua", is_main ? "dplua __main__: %s" : "dplua.lua: %s",
            lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    if (is_main && out_type_count) {
        *out_type_count = (int)lua_tointeger(L, -1);
        lua_pop(L, 1);
    }
    return true;
}

static bool _start_dplua_runtime(lua_State* L, dpapp_arg_t* arg, dplua_arg_t* larg,
    dplua_config_t* config)
{
    if (!_run_dplua_lua(L, arg, larg, config, false, NULL)) {
        dplog_alert("dplua", "dplua runtime failed");
        return false;
    }
    return true;
}

static int _dplua_arg_push(dplua_arg_t* a, const char* s)
{
    char** new_argv = (char**)realloc(a->argv, sizeof(char*) * (a->argc + 1));
    if (!new_argv) {
        dplog_alert("dplua", "Alloc args memory failure");
        return -1;
    }
    a->argv = new_argv;
    a->argv[a->argc] = strdup(s);
    if (!a->argv[a->argc])
        return -1;
    a->argc++;
    return 0;
}

static void _dplua_arg_free(dplua_arg_t* a)
{
    if (a) {
        for (int i = 0; i < a->argc; i++)
            free(a->argv[i]);
        free(a->argv);
        a->argc = 0;
        a->argv = NULL;
    }
}

static void _dplua_ctx_set_package(const char* key, const char* value,
    size_t value_len)
{
    lua_State* L = _g_ctx.mainL;
    lua_getfield(L, LUA_GLOBALSINDEX, "package");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    lua_pushlstring(L, value, value_len);
    lua_setfield(L, -2, key);
    lua_pop(L, 1);
}

static char* _dplua_ctx_get_package_dup(const char* key)
{
    lua_State* L = _g_ctx.mainL;
    lua_getfield(L, LUA_GLOBALSINDEX, "package");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return strdup("");
    }
    lua_getfield(L, -1, key);
    const char* s = lua_tostring(L, -1);
    char* out = strdup(s ? s : "");
    lua_pop(L, 2);
    return out ? out : strdup("");
}

static void _init_package(dpapp_arg_t* arg, dplua_arg_t* larg, lua_State* L)
{
    dpbuf_t* buf = dpbuf_new(4096);
    if (buf == NULL) {
        dplog_alert("dplua", "dpbuf_new failed");
        lua_close(L);
        _g_ctx.mainL = NULL;
        return;
    }

    const char* root_dir = arg->root_dir;
    char mod_dir[4096] = {0};
    bool has_mod_dir = false;

    if (larg->argc > 0) {
        const char* entry = larg->argv[0];
        const char* slash = strrchr(entry, '/');
        if (slash) {
            size_t plen = (size_t)(slash - entry);
            if (plen >= sizeof mod_dir) {
                dpbuf_del(buf);
                dplog_alert("dplua", "entry path too long");
                lua_close(L);
                _g_ctx.mainL = NULL;
                return;
            }
            memcpy(mod_dir, entry, plen);
            mod_dir[plen] = '\0';
        } else {
            mod_dir[0] = '.';
            mod_dir[1] = '\0';
        }
        has_mod_dir = true;
    }

    if (has_mod_dir) {
        dpbuf_wstrf(buf, "%s/?.lua;", mod_dir);
        dpbuf_wstrf(buf, "%s/?/init.lua;", mod_dir);
        dpbuf_wstrf(buf, "%s/?/?/init.lua;", mod_dir);

        dpbuf_wstrf(buf, "%s/../?.lua;", mod_dir);
        dpbuf_wstrf(buf, "%s/../?/init.lua;", mod_dir);
        dpbuf_wstrf(buf, "%s/../?/?/init.lua;", mod_dir);
    }

    dpbuf_wstrf(buf, "%s/lua/luajit-2.1/jit/?.lua;", root_dir);
    dpbuf_wstrf(buf, "%s/lua/?.lua;", root_dir);
    dpbuf_wstrf(buf, "%s/lua/dplua/?.lua;", root_dir);
    dpbuf_wstrf(buf, "%s/lua/?/init.lua;", root_dir);
    dpbuf_wstrf(buf, "%s/lua/?/?/init.lua;", root_dir);

    dpbuf_wstrf(buf, "%s/app/?.lua;", root_dir);
    dpbuf_wstrf(buf, "%s/app/?/init.lua;", root_dir);
    dpbuf_wstrf(buf, "%s/app/?/?/init.lua;", root_dir);

    char* lua_path_old = _dplua_ctx_get_package_dup("path");
    dpbuf_wstrf(buf, "%s", lua_path_old);
    free(lua_path_old);

    dpbuf_eseek(buf, 0, 2);
    _dplua_ctx_set_package("path", dpbuf_crdata(buf), dpbuf_crsize(buf));

    dpbuf_reset(buf, 0);

    char* lua_cpath_old = _dplua_ctx_get_package_dup("cpath");
    if (has_mod_dir) {
        dpbuf_wstrf(buf, "%s/?.so;", mod_dir);
        dpbuf_wstrf(buf, "%s/?/?.so;", mod_dir);

        dpbuf_wstrf(buf, "%s/../?.so;", mod_dir);
        dpbuf_wstrf(buf, "%s/../?/?.so;", mod_dir);
    }
    dpbuf_wstrf(buf, "%s/lua/?.so;", root_dir);
    dpbuf_wstrf(buf, "%s/lua/?/?.so;", root_dir);
    dpbuf_wstrf(buf, "%s", lua_cpath_old);
    free(lua_cpath_old);

    dpbuf_eseek(buf, 0, 2);
    _dplua_ctx_set_package("cpath", dpbuf_crdata(buf), dpbuf_crsize(buf));

    dpbuf_del(buf);
}

/* ── string.buffer 编码/解码 ─────────────────────────────────────────── */

#define DPLUA_SB_KEY "_dplua_sb"

static void _ensure_sb(lua_State* L)
{
    lua_getfield(L, LUA_REGISTRYINDEX, DPLUA_SB_KEY);
    if (lua_istable(L, -1))
        return;
    lua_pop(L, 1);
    lua_getglobal(L, "require");
    lua_pushstring(L, "string.buffer");
    if (lua_pcall(L, 1, 1, 0) != 0) {
        lua_pop(L, 1);
        lua_pushnil(L);
        return;
    }
    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, DPLUA_SB_KEY);
}

static dpret_t _encode_value(lua_State* L, int idx, dpbuf_t* buf)
{
    int abs_val = idx >= 0 ? idx : lua_gettop(L) + idx + 1;
    if (lua_type(L, abs_val) == LUA_TNIL)
        return DPE_INVAL;
    _ensure_sb(L);
    if (!lua_istable(L, -1))
        return DPE_INVAL;
    lua_getfield(L, -1, "encode");
    lua_pushvalue(L, abs_val);
    if (lua_pcall(L, 1, 1, 0) != 0) {
        lua_pop(L, 1);
        return DPE_INVAL;
    }
    size_t len;
    const char* data = lua_tolstring(L, -1, &len);
    dpret_t ret = DPE_INVAL;
    if (data && len > 0)
        ret = dpbuf_wdata(buf, data, (int)len);
    lua_pop(L, 2);
    return ret;
}

static dpret_t _decode_value(lua_State* L, dpbuf_t* buf)
{
    int crsize = dpbuf_crsize(buf);
    if (crsize <= 0) {
        lua_pushnil(L);
        return DPE_INVAL;
    }
    _ensure_sb(L);
    if (!lua_istable(L, -1)) {
        lua_pushnil(L);
        return DPE_INVAL;
    }

    lua_getfield(L, -1, "new");
    if (lua_pcall(L, 0, 1, 0) != 0) {
        lua_pop(L, 1);
        lua_pushnil(L);
        return DPE_INVAL;
    }
    lua_getfield(L, -1, "set");
    lua_pushvalue(L, -2);
    lua_pushlstring(L, dpbuf_crdata(buf), (size_t)crsize);
    if (lua_pcall(L, 2, 0, 0) != 0) {
        lua_pop(L, 2);
        lua_pushnil(L);
        return DPE_INVAL;
    }
    lua_pushvalue(L, -1);
    size_t len = lua_objlen(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, -1, "decode");
    lua_pushvalue(L, -2);
    if (lua_pcall(L, 1, 1, 0) != 0) {
        lua_pop(L, 2);
        lua_pushnil(L);
        return DPE_INVAL;
    }
    lua_pushvalue(L, -2);
    size_t rem = lua_objlen(L, -1);
    lua_pop(L, 1);
    int consumed = (int)(len - rem);
    lua_remove(L, -2);
    lua_remove(L, -2);
    return consumed;
}

#define DPLUA_FMT_REGKEY "dplua_strfmt_fmt"

static const char* _strfmt(lua_State* L, int fmt_idx, int* msg_len)
{
    const int top = lua_gettop(L);
    if (top == fmt_idx) {
        size_t len = 0;
        const char* msg = luaL_checklstring(L, fmt_idx, &len);
        *msg_len = (int)len;
        return msg;
    }

    lua_getfield(L, LUA_REGISTRYINDEX, DPLUA_FMT_REGKEY);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_getglobal(L, "string");
        lua_getfield(L, -1, "format");
        lua_remove(L, -2);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, DPLUA_FMT_REGKEY);
    }

    lua_pushvalue(L, fmt_idx);
    for (int i = fmt_idx + 1; i <= top; i++)
        lua_pushvalue(L, i);

    if (lua_pcall(L, top - fmt_idx + 1, 1, 0) != 0) {
        dplog_error("dplua", "string.format: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
        *msg_len = 0;
        return "";
    }

    size_t len;
    const char* result = lua_tolstring(L, -1, &len);
    *msg_len = (int)len;
    return result;
}

/* ── hdrs 配置 ───────────────────────────────────────────────────────── */

/** @brief 加载 dplua.lua 运行 __main__，解析 hdrs 配置。 */
static dplua_config_t* _load_config(dpapp_arg_t* sarg, dplua_arg_t* larg)
{
    lua_State* L = luaL_newstate();
    if (!L) {
        dplog_error("dplua", "dplua_load_config: luaL_newstate failed");
        return NULL;
    }
    lua_State* saved_mainL = _g_ctx.mainL;
    _g_ctx.mainL = L;

    luaL_openlibs(L);
    _inject_dplua_globals(L, sarg->root_dir);
    _init_package(sarg, larg, L);

    int type_count = 0;
    if (!_run_dplua_lua(L, sarg, larg, NULL, true, &type_count)) {
        _g_ctx.mainL = saved_mainL;
        lua_close(L);
        return NULL;
    }

    if (type_count < 1 || type_count > DPAPP_TYPE_MAX) {
        dplog_error("dplua", "dplua_load_config: invalid type_count=%d", type_count);
        _g_ctx.mainL = saved_mainL;
        lua_close(L);
        return NULL;
    }

    dplua_config_t* config = (dplua_config_t*)calloc(1, sizeof(dplua_config_t));
    if (!config) {
        _g_ctx.mainL = saved_mainL;
        lua_close(L);
        return NULL;
    }
    config->type_count = type_count;

    lua_getfield(L, LUA_REGISTRYINDEX, "_dplua_hdrs");
    lua_pushnil(L);
    lua_setfield(L, LUA_REGISTRYINDEX, "_dplua_hdrs");

    lua_pushnil(L);
    while (lua_next(L, -2)) {
        if (lua_type(L, -2) == LUA_TNUMBER) {
            int t = (int)lua_tointeger(L, -2);
            if (t >= 0 && t < DPAPP_TYPE_MAX) {
                lua_getfield(L, -1, "init");
                if (lua_isstring(L, -1))
                    snprintf(config->hdrs[t].init_name, DPLUA_HDR_NAME_MAX, "%s",
                        lua_tostring(L, -1));
                lua_pop(L, 1);
                lua_getfield(L, -1, "args");
                if (dpbuf_init(&config->hdrs[t].arg_buf, 256)) {
                    _encode_value(L, -1, &config->hdrs[t].arg_buf);
                    dpbuf_eseek(&config->hdrs[t].arg_buf, 0, SEEK_END);
                }
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    _g_ctx.mainL = saved_mainL;
    lua_close(L);
    return config;
}

static void _free_config(dplua_config_t* cfg)
{
    if (cfg) {
        for (int i = 0; i < cfg->type_count && i < DPAPP_TYPE_MAX; i++)
            dpbuf_fini(&cfg->hdrs[i].arg_buf);
        free(cfg);
    }
}

/* ── 线程主函数 ──────────────────────────────────────────────────────── */
// clang-format off
static void _thread_main(void* user_data)
{
    dplua_thread_arg_t* ta = (dplua_thread_arg_t*)user_data;
    dpapp_arg_t* arg = ta->arg;
    dplua_arg_t* larg = ta->larg;
    dplua_config_t* config = ta->config;

    dpret_t ret = dpssl_thrd_init();
    if (ret != DPE_OK && ret != DPE_UNSUPPORT) {
        dplog_alert("dplua", "Failed to initialize the ssl environment: %d", ret);
        return;
    }

    ret = dpqic_thrd_init();
    if (ret != DPE_OK && ret != DPE_UNSUPPORT) {
        dplog_alert("dplua", "Failed to initialize the quic environment: %d", ret);
        return;
    }

    dplog_info("dplua", "Thread %d (type %d) started", dpevp_id(), dpevp_type());

    lua_State* L = luaL_newstate();
    if (!L) {
        dplog_alert("dplua", "luaL_newstate failed");
        return;
    }
    _g_ctx.mainL = L;
    luaL_openlibs(L);
    _inject_dplua_globals(L, arg->root_dir);
    _init_package(arg, larg, L);
    if (_g_ctx.mainL == NULL)
        return;

    if (!_start_dplua_runtime(L, arg, larg, config)) {
        lua_close(L);
        _g_ctx.mainL = NULL;
        return;
    }

    dplog_info("dplua", "Thread %d (type %d) finished", dpevp_id(), dpevp_type());

    dpqic_thrd_exit();
    dpssl_thrd_exit();

    lua_close(L);
    _g_ctx.mainL = NULL;
}

static char* _find_start_file(const char* name, const char* root_dir)
{
    char absbuf[PATH_MAX];
    const char* abs_name = name;
    if (realpath(name, absbuf) != NULL)
        abs_name = absbuf;

    char* cands[4] = {NULL};
    int cands_sz[4] = {0};
    cands_sz[0] = asprintf(&cands[0], "%s", abs_name);
    cands_sz[1] = asprintf(&cands[1], "%s/init.lua", abs_name);
    cands_sz[2] = asprintf(&cands[2], "%s/app/%s", root_dir, name);
    cands_sz[3] = asprintf(&cands[3], "%s/app/%s/init.lua", root_dir, name);

    char* out = NULL;
    for (int i = 0; i < 4; i++) {
        if (out == NULL && cands_sz[i] > 0 && is_file(cands[i])) {
            out = cands[i];
        } else {
            if (cands[i]) {
                free(cands[i]);
                cands[i] = NULL;
            }
        }
    }
    return out;
}

static dpret_t _preload_file(const char* chunk, const char* root_dir,
    dplua_arg_t* lua_arg)
{
    char* lua_file = _find_start_file(chunk, root_dir);
    if (lua_file == NULL) {
        dplog_error("dplua", "Not find valid start file for %s", chunk);
        return DPE_NOTEXISTS;
    }

    if (lua_arg->argc == 0) {
        if (_dplua_arg_push(lua_arg, lua_file) != 0) {
            free(lua_file);
            return DPE_NOMEM;
        }
    }
    free(lua_file);
    return 1;
}

static dpret_t _preload_debug(const char* chunk, const char* root_dir,
    dplua_arg_t* lua_arg)
{
    lua_State* L = luaL_newstate();
    if (!L)
        return DPE_NOMEM;
    luaL_openlibs(L);

    lua_newtable(L);
    lua_pushlstring(L, chunk, strlen(chunk));
    lua_rawseti(L, -2, 0);
    lua_setglobal(L, "args");

    static const char
        kWrapper[] = "local b, e, _ = string.find(args[0],\n"
                     "    [[require%(['']+lldebugger['']+%)%.runFile]])\n"
                     "if b and b > 0 then\n"
                     "    string.gsub(args[0], '(%b{})', function(s)\n"
                     "        local debugargs = "
                     "loadstring(string.format('return %s', s))()\n"
                     "        for i, a in pairs(debugargs) do\n"
                     "            args[i] = a\n"
                     "        end\n"
                     "    end, 1)\n"
                     "end\n";

    if (luaL_loadstring(L, kWrapper) != 0) {
        dplog_error("dplua", "preloadDebug loadstring: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
        lua_close(L);
        return DPE_UNKNOWN;
    }
    if (lua_pcall(L, 0, 0, 0) != 0) {
        dplog_error("dplua", "preloadDebug chunk: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
        lua_close(L);
        return DPE_UNKNOWN;
    }

    lua_getglobal(L, "args");
    const int tidx = lua_gettop(L);
    for (int i = 0;; i++) {
        lua_rawgeti(L, tidx, i);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            break;
        }
        if (lua_type(L, -1) == LUA_TSTRING) {
            const char* s = lua_tostring(L, -1);
            if (s && _dplua_arg_push(lua_arg, s) != 0) {
                lua_pop(L, 1);
                lua_pop(L, 1);
                lua_close(L);
                return DPE_NOMEM;
            }
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    lua_close(L);

    if (lua_arg->argc > 0) {
        return _preload_file(lua_arg->argv[0], root_dir, lua_arg);
    } else {
        return DPE_INVAL;
    }
}

static dpret_t _preload_chunk(const char* chunk, const char* root_dir,
    dplua_arg_t* lua_arg)
{
    size_t chunk_len = strlen(chunk);

    char var_dir[4096];
    snprintf(var_dir, sizeof var_dir, "%s/var/dplua/cache", root_dir);
    if (!mkdir_p(var_dir)) {
        dplog_error("dplua", "Create var directory failed: %s", var_dir);
        return DPE_IO;
    }

    unsigned char rnd[8];
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0 || read(fd, rnd, sizeof rnd) != (ssize_t)sizeof rnd) {
        if (fd >= 0)
            close(fd);
        return DPE_IO;
    }
    if (fd >= 0)
        close(fd);
    uint64_t u = 0;
    memcpy(&u, rnd, sizeof u);

    char* chunk_path = NULL;
    if (asprintf(&chunk_path, "%s/dplua_chunk_%" PRIx64 ".lua", var_dir, u) < 0) {
        return DPE_NOMEM;
    }

    FILE* ofs = fopen(chunk_path, "wb");
    if (!ofs) {
        dplog_error("dplua", "Create temporary chunk file failed: %s", chunk_path);
        free(chunk_path);
        return DPE_OPEN;
    }

    static const char pfx[] = "-- execute dplua chunk\n"
                              "local M = {}\n"
                              "function M.__init00(arg)\n";
    static const char sfx[] = "\nend\nfunction M.__main__(args, hdrs)\n"
                              "  hdrs[0] = { init = '__init00' }\n"
                              "  return 1\n"
                              "end\nreturn M\n";
    if (fwrite(pfx, 1, sizeof pfx - 1, ofs) != sizeof pfx - 1
        || fwrite(chunk, 1, chunk_len, ofs) != chunk_len
        || fwrite(sfx, 1, sizeof sfx - 1, ofs) != sizeof sfx - 1) {
        fclose(ofs);
        return DPE_IO;
    }
    fclose(ofs);

    dpret_t ret = _dplua_arg_push(lua_arg, chunk_path);
    free(chunk_path);
    return dpret_isok(ret) ? 1 : ret;
}

/** LuaJIT 可变参无法可靠传递 dpv64_t；经固定参数包装后写入 asc scratch。 */
dpret_t dplua_add_ctc_submit(dpele_t* ctc, int64_t topic_id, dpv64_t arg)
{
    if (ctc == NULL) {
        return DPE_INVAL;
    }
    dpv64_t v0 = {.s64 = topic_id};
    return dpevp_add(ctc, dpctc_submit(), v0, arg);
}

/** LuaJIT 可变参无法可靠传递小整数；经固定参数包装创建 CTC。 */
dpele_t* dplua_new_ctc(int toid, int detach)
{
    return dpele_new(dpctc_init_type(), toid, detach);
}

dpret_t dplua_add_tmr_timeout(dpele_t* tmr, double sec, int64_t cache_id, dpv64_t arg)
{
    if (tmr == NULL) {
        return DPE_INVAL;
    }
    dpv64_t v0 = {.s64 = cache_id};
    return dpevp_add(tmr, dptmr_timeout(), sec, v0, arg);
}

dpret_t dplua_start(dpapp_arg_t* args)
{
    if (args == NULL || args->argv == NULL || args->root_dir == NULL) {
        return DPE_INVAL;
    }

    dplog_init(args->log_file, args->log_level, args->log_tsacc);

    bool emode = false;
    int i = 0;
    for (; i < args->argc; i++) {
        const char* arg = args->argv[i];
        if (strcmp(arg, "-e") == 0) {
            emode = true;
        } else {
            break;
        }
    }

    if (i >= args->argc) {
        dplog_error("dplua", "No Lua module specified");
        return DPE_INVAL;
    }

    dplua_arg_t Larg = {0, NULL};
    dpret_t ret = 0;
    if (emode) {
        const char* chunk = args->argv[i];
        size_t chunk_len = strlen(chunk);
        static const char dbgneedle[] = "require('lldebugger').runFile";
        if (chunk_len >= sizeof dbgneedle - 1
            && memmem(chunk, chunk_len, dbgneedle, sizeof dbgneedle - 1)) {
            ret = _preload_debug(chunk, args->root_dir, &Larg);
        } else {
            ret = _preload_chunk(chunk, args->root_dir, &Larg);
        }
    } else {
        ret = _preload_file(args->argv[i], args->root_dir, &Larg);
    }

    if (ret < 1) {
        dplog_error("dplua", "Failed to preload module '%s' (%d)", args->argv[i], ret);
        _dplua_arg_free(&Larg);
        return DPE_INVAL;
    }

    for (int j = i + 1; j < args->argc; j++) {
        if (_dplua_arg_push(&Larg, args->argv[j]) != 0) {
            dplog_error("dplua", "Failed to push argv[%d]", j);
            _dplua_arg_free(&Larg);
            return DPE_NOMEM;
        }
    }

    // 配置阶段：加载模块，调用 __main__，解析 hdrs
    dplua_config_t* config = _load_config(args, &Larg);
    if (!config) {
        _dplua_arg_free(&Larg);
        return DPE_INVAL;
    }
    args->type_count = DP_MAX(args->type_count, config->type_count);
    dplog_info("dplua", "Module loaded: entry=%s types=%d", Larg.argv[0],
        config->type_count);

    dplua_thread_arg_t thread_arg;
    thread_arg.arg = args;
    thread_arg.larg = &Larg;
    thread_arg.config = config;

    ret = dpapp_start(args, _thread_main, (void*)&thread_arg);

    _free_config(config);
    _dplua_arg_free(&Larg);

    return ret;
}
