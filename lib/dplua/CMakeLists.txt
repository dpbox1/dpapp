set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -rdynamic -fPIC -D_GNU_SOURCE -D_REENTRANT")

set(DPLUA_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(DPLUA_VERSION_MINOR ${PROJECT_VERSION_MINOR})

# FFI Lua 运行时（lib/dplua/ 下的 *.lua，安装到 ${PREFIX}/lua/dplua/）
set(DPLUA_LUA_SRC "${CMAKE_CURRENT_SOURCE_DIR}")
if(NOT EXISTS "${DPLUA_LUA_SRC}/dplua.lua")
    message(FATAL_ERROR "FFI Lua sources not found in ${DPLUA_LUA_SRC}")
endif()

file(GLOB _dplua_lua_files "${DPLUA_LUA_SRC}/dp*.lua")

# luajit.h 由 make 生成，configure 阶段应检查源码树内恒存在的文件。
if(NOT luajit_SOURCE_DIR OR NOT EXISTS "${luajit_SOURCE_DIR}/src/luajit.c")
    message(FATAL_ERROR "DPAPP_WITH_LUA=ON requires luajit source from FetchContent")
endif()

# LuaJIT RELVER 固定为 ROLLING → libluajit-5.1.so.2.1.ROLLING
set(DPLUA_LUAJIT_RELVER "ROLLING")
set(LUAJIT_SONAME "libluajit-5.1.so.2.1.${DPLUA_LUAJIT_RELVER}")

add_custom_command(
    WORKING_DIRECTORY ${luajit_SOURCE_DIR}
    OUTPUT ${DPAPP_STAGE_DIR}/usr/lib/libluajit-5.1.so
    DEPENDS
        ${luajit_SOURCE_DIR}/src/lj_state.c
        ${luajit_SOURCE_DIR}/src/luajit.c
    COMMAND ${CMAKE_COMMAND} -E echo "${DPLUA_LUAJIT_RELVER}" > ${luajit_SOURCE_DIR}/.relver
    COMMAND make -j$(nproc)
        BUILDMODE=dynamic
        RELVER=${DPLUA_LUAJIT_RELVER}
        XCFLAGS='-msse4.2 -fPIC'
        PREFIX=${DPAPP_STAGE_DIR}/usr/
    COMMAND make install
        BUILDMODE=dynamic
        RELVER=${DPLUA_LUAJIT_RELVER}
        XCFLAGS='-msse4.2 -fPIC'
        PREFIX=${DPAPP_STAGE_DIR}/usr/
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${DPAPP_STAGE_DIR}/usr/lib/lua
    COMMAND ${CMAKE_COMMAND} -E remove -f ${DPAPP_STAGE_DIR}/usr/lib/libluajit-5.1.a
)

add_custom_target(
    compile_luajit ALL
    DEPENDS ${DPAPP_STAGE_DIR}/usr/lib/libluajit-5.1.so
)

set(_dplua_lua_depends "")
if(DPAPP_COMPILE_LUA)
    list(APPEND _dplua_lua_depends compile_luajit)
endif()
dplua_lua_bytecode(
    TARGET dplua_lua_sync
    DEST lua/dplua
    FILES ${_dplua_lua_files}
    DEPENDS ${_dplua_lua_depends}
)

set(LUAJIT_INC_DIR ${DPAPP_STAGE_DIR}/usr/include/luajit-2.1 CACHE INTERNAL "")
set(LUAJIT_LIB ${DPAPP_STAGE_DIR}/usr/lib/libluajit-5.1.so CACHE INTERNAL "")

add_library(dplua SHARED "")
add_dependencies(dplua dpapp compile_luajit dplua_lua_sync)

target_compile_features(dplua PRIVATE c_std_11)
target_compile_options(dplua PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-Wno-format-security>
)

target_include_directories(dplua PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/bin>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${LUAJIT_INC_DIR}>
    $<INSTALL_INTERFACE:usr/include>
    PRIVATE
    $<BUILD_INTERFACE:${luajit_SOURCE_DIR}/src>
)

target_link_libraries(dplua PUBLIC
    ${LUAJIT_LIB}
    dpapp
)

target_sources(dplua PRIVATE
    dplua.c
    dplua_ext.c
)

file(GLOB DPLUA_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
dpapp_stage_sync_files(usr/include/dplua FILES ${DPLUA_HEADERS})

dpapp_install_package(dplua TARGETS dplua)

dpapp_stage_sync_files(usr/lib/cmake/dplua
    FILES ${PROJECT_SOURCE_DIR}/cmake/dplua_compiler.cmake
)
