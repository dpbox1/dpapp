# Lua 示例同步到 stage/app/example/（与 .so 示例同目录）
file(GLOB _app_lua_files "${CMAKE_CURRENT_SOURCE_DIR}/*.lua")
set(_app_lua_depends "")
if(DPAPP_COMPILE_LUA)
    list(APPEND _app_lua_depends compile_luajit)
endif()
dplua_lua_bytecode(
    TARGET app_lua_example_sync
    DEST app/example
    FILES ${_app_lua_files}
    DEPENDS ${_app_lua_depends}
)
