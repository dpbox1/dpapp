set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -rdynamic -fPIC -D_GNU_SOURCE -D_REENTRANT")

set(DPLUA_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(DPLUA_VERSION_MINOR ${PROJECT_VERSION_MINOR})

# FFI Lua 运行时（lib/dplua/ 下的 *.lua，安装到 ${PREFIX}/lua/dplua/）
set(DPLUA_LUA_SRC "${CMAKE_CURRENT_SOURCE_DIR}")
if(NOT EXISTS "${DPLUA_LUA_SRC}/dplua.lua")
    message(FATAL_ERROR "FFI Lua sources not found in ${DPLUA_LUA_SRC}")
endif()

set(DPLUA_LUA_STAGING "${PROJECT_BINARY_DIR}/lua/dplua")
file(MAKE_DIRECTORY "${DPLUA_LUA_STAGING}")
file(GLOB _dplua_lua_files "${DPLUA_LUA_SRC}/dp*.lua")
set(_dplua_lua_outputs "")
foreach(_f IN LISTS _dplua_lua_files)
    get_filename_component(_name "${_f}" NAME)
    set(_out "${DPLUA_LUA_STAGING}/${_name}")
    list(APPEND _dplua_lua_outputs "${_out}")
    add_custom_command(
        OUTPUT "${_out}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_f}" "${DPLUA_LUA_STAGING}/"
        DEPENDS "${_f}"
        COMMENT "Sync ${_name} -> lua/dplua/"
    )
endforeach()
add_custom_target(dplua_lua_sync ALL DEPENDS ${_dplua_lua_outputs})

# luajit.h 由 make 生成，configure 阶段应检查源码树内恒存在的文件。
if(NOT luajit_SOURCE_DIR OR NOT EXISTS "${luajit_SOURCE_DIR}/src/luajit.c")
    message(FATAL_ERROR "DPAPP_WITH_LUA=ON requires luajit source from FetchContent")
endif()

# LuaJIT RELVER 固定为 ROLLING → libluajit-5.1.so.2.1.ROLLING
set(DPLUA_LUAJIT_RELVER "ROLLING")
set(LUAJIT_SONAME "libluajit-5.1.so.2.1.${DPLUA_LUAJIT_RELVER}")

add_custom_command(
    WORKING_DIRECTORY ${luajit_SOURCE_DIR}
    OUTPUT ${PROJECT_BINARY_DIR}/lib/libluajit-5.1.so
    DEPENDS
        ${luajit_SOURCE_DIR}/src/lj_state.c
        ${luajit_SOURCE_DIR}/src/luajit.c
    COMMAND ${CMAKE_COMMAND} -E echo "${DPLUA_LUAJIT_RELVER}" > ${luajit_SOURCE_DIR}/.relver
    COMMAND make -j$(nproc)
        BUILDMODE=dynamic
        RELVER=${DPLUA_LUAJIT_RELVER}
        XCFLAGS='-msse4.2 -fPIC'
        PREFIX=${PROJECT_BINARY_DIR}/
        INSTALL_SHARE_=${PROJECT_BINARY_DIR}/lua
        INSTALL_SHARE=${PROJECT_BINARY_DIR}/lua
        INSTALL_LJLIBD=${PROJECT_BINARY_DIR}/lua/luajit-2.1
    COMMAND make install
        BUILDMODE=dynamic
        RELVER=${DPLUA_LUAJIT_RELVER}
        XCFLAGS='-msse4.2 -fPIC'
        PREFIX=${PROJECT_BINARY_DIR}/
        INSTALL_SHARE_=${PROJECT_BINARY_DIR}/lua
        INSTALL_SHARE=${PROJECT_BINARY_DIR}/lua
        INSTALL_LJLIBD=${PROJECT_BINARY_DIR}/lua/luajit-2.1
    COMMAND ${CMAKE_COMMAND} -E remove -f ${PROJECT_BINARY_DIR}/lib/libluajit-5.1.a
)

add_custom_target(
    compile_luajit ALL
    DEPENDS ${PROJECT_BINARY_DIR}/lib/libluajit-5.1.so
)

set(LUAJIT_INC_DIR ${PROJECT_BINARY_DIR}/include/luajit-2.1 CACHE INTERNAL "")
set(LUAJIT_LIB ${PROJECT_BINARY_DIR}/lib/libluajit-5.1.so CACHE INTERNAL "")

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

install(DIRECTORY "${PROJECT_BINARY_DIR}/lua/"
    DESTINATION lua
    FILES_MATCHING
    PATTERN "*"
)

file(GLOB _luajit_bins ${PROJECT_BINARY_DIR}/bin/luajit-*)
install(FILES ${PROJECT_BINARY_DIR}/bin/luajit ${_luajit_bins}
    DESTINATION usr/bin
    PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
)

install(DIRECTORY "${PROJECT_BINARY_DIR}/lib/"
    DESTINATION ${DPBOX_USR_PREFIX}/lib
    FILES_MATCHING
    PATTERN "libluajit-5.1.so*"
)

install(DIRECTORY "${PROJECT_BINARY_DIR}/include/"
    DESTINATION ${DPBOX_USR_PREFIX}/include
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN "*.hpp"
)

file(GLOB DPLUA_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
install(
    FILES ${DPLUA_HEADERS}
    DESTINATION ${DPBOX_USR_PREFIX}/include/dplua
)

install(
    TARGETS dplua
    EXPORT dpluaTargets
    ARCHIVE DESTINATION ${DPBOX_USR_PREFIX}/lib
    LIBRARY DESTINATION ${DPBOX_USR_PREFIX}/lib
    RUNTIME DESTINATION ${DPBOX_USR_PREFIX}/bin
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/dpluaConfig.cmake.in
    ${PROJECT_BINARY_DIR}/dpluaConfig.cmake
    INSTALL_DESTINATION usr/lib/cmake/dplua
    PATH_VARS CMAKE_INSTALL_PREFIX
)

write_basic_package_version_file(
    ${PROJECT_BINARY_DIR}/dpluaConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(
    FILES
        ${PROJECT_BINARY_DIR}/dpluaConfig.cmake
        ${PROJECT_BINARY_DIR}/dpluaConfigVersion.cmake
    DESTINATION usr/lib/cmake/dplua
)

install(
    EXPORT dpluaTargets
    FILE dpluaTargets.cmake
    NAMESPACE dpbox::
    DESTINATION usr/lib/cmake/dplua
)
