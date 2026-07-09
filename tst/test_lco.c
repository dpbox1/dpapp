/* 复现 dplua 的 narg 问题：对比 coroutine.wrap 和直接 lua_resume */
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
#include <stdio.h>
#include <stdlib.h>

/* 推 3 个值 + pushthread + xmove + lua_yield(L, narg) */
static int c_yielder(lua_State* L)
{
    lua_pushinteger(L, 10); // host
    lua_pushinteger(L, 20); // port
    lua_pushinteger(L, 30); // cet_ud
    printf("[yielder] pre-yield top=%d\n", lua_gettop(L));

    lua_getfield(L, LUA_REGISTRYINDEX, "mainL");
    lua_State* mainL = (lua_State*)lua_touserdata(L, -1);
    lua_pop(L, 1);
    lua_pushthread(L);
    if (L != mainL)
        lua_xmove(L, mainL, 1);
    printf("[yielder] post-xmove top=%d\n", lua_gettop(L));
    return lua_yield(L, 1); // narg=1
}

/* 递归测试不同参数个数的 lua_resume */
static void test_resume_nargs(int nargs)
{
    lua_State* L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "OOM\n");
        return;
    }
    luaL_openlibs(L);

    lua_pushlightuserdata(L, (void*)L);
    lua_setfield(L, LUA_REGISTRYINDEX, "mainL");
    lua_pushcfunction(L, c_yielder);
    lua_setglobal(L, "c_yielder");

    printf("\n=== lua_resume(co) with %d args ===\n", nargs);

    /* 在 Lua 代码里创建协程、resume、保存引用到 registry */
    char code[512];
    const char* args = nargs == 0 ? "" : nargs == 1 ? "'arg1'" : "'arg1', 'arg2'";
    snprintf(code, sizeof(code),
        "local co = coroutine.create(c_yielder)\n"
        "local ok, r1, r2 = coroutine.resume(co%s%s)\n"
        "print('resume ok=', ok, 'r1=', r1, 'r2=', r2)\n"
        "debug.getregistry().my_co = co\n",
        nargs > 0 ? ", " : "", args);

    if (luaL_loadstring(L, code) != LUA_OK) {
        printf("load error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return;
    }

    int ret = lua_pcall(L, 0, 0, 0);
    if (ret != LUA_OK) {
        printf("pcall error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return;
    }

    /* 取协程 */
    lua_getfield(L, LUA_REGISTRYINDEX, "my_co");
    lua_State* co = lua_tothread(L, -1);
    lua_pop(L, 1);

    if (co) {
        printf("yielded: co top=%d\n", lua_gettop(co));
        for (int i = 1; i <= lua_gettop(co); i++)
            printf("  co[%d] type=%d val=%lld\n", i, lua_type(co, i),
                lua_type(co, i) == LUA_TNUMBER ? (long long)lua_tointeger(co, i)
                                               : -1);

        printf("resume with (999, 0):\n");
        lua_pushinteger(co, 999);
        lua_pushinteger(co, 0);
        ret = lua_resume(co, 2);
        printf("result: %d (LUA_OK=%d)\n", ret, LUA_OK);
        if (ret == LUA_ERRRUN)
            printf("  %s\n", lua_tostring(co, -1));
    } else {
        printf("ERROR: co not found\n");
    }

    lua_close(L);
}

/* 对比：从 C 直接 lua_resume (无中间 Lua boot 函数) */
static void test_direct(void)
{
    lua_State* L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "OOM\n");
        return;
    }
    luaL_openlibs(L);

    lua_pushlightuserdata(L, (void*)L);
    lua_setfield(L, LUA_REGISTRYINDEX, "mainL");
    lua_pushcfunction(L, c_yielder);
    lua_setglobal(L, "c_yielder");

    printf("\n=== direct C: lua_resume(co, 0) ===\n");

    lua_State* co = lua_newthread(L);
    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, "my_co");
    lua_getglobal(co, "c_yielder");

    int ret = lua_resume(co, 0);
    if (ret != LUA_YIELD) {
        printf("ERROR: not yielded\n");
        lua_close(L);
        return;
    }

    printf("yielded: co top=%d\n", lua_gettop(co));
    for (int i = 1; i <= lua_gettop(co); i++)
        printf("  co[%d] type=%d val=%lld\n", i, lua_type(co, i),
            lua_type(co, i) == LUA_TNUMBER ? (long long)lua_tointeger(co, i) : -1);

    printf("resume with (999, 0):\n");
    lua_pushinteger(co, 999);
    lua_pushinteger(co, 0);
    ret = lua_resume(co, 2);
    printf("result: %d\n", ret);

    lua_close(L);
}

int main(void)
{
    test_direct();
    test_resume_nargs(0);
    test_resume_nargs(1);
    test_resume_nargs(2);
    return 0;
}
