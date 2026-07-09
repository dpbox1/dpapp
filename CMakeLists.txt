cmake_minimum_required(VERSION 3.22.0)
project(dpapp VERSION 1.2)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/opt/dpbox" CACHE PATH "Install prefix" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(dpapp_stage)
include(dpapp_package)
set_property(GLOBAL PROPERTY DPAPP_STAGE_SYNC_ENTRIES "")
set_property(GLOBAL PROPERTY DPAPP_STAGE_SYNC_DIRS "")

option(DPAPP_WITH_LSQUIC "Support lsquic" OFF)
option(DPAPP_WITH_SSL "Enable OpenSSL support" ON)
option(DPAPP_WITH_TESTS "Build test cases" ON)
option(DPAPP_WITH_CWC "Build CWC components" ON)
option(DPAPP_WITH_CPP "Build C++ components" ON)
option(DPAPP_WITH_LUA "Build Lua components" ON)
include(CMakeDependentOption)
cmake_dependent_option(DPAPP_COMPILE_LUA "Compile Lua scripts to bytecode (luajit -b)" ON "DPAPP_WITH_LUA" OFF)
option(DPAPP_WITH_DOCS "Build Doxygen HTML documentation on every build (ALL)" OFF)

# -----------------------------------------------------------------------------
# Install prefixes / rpath
# -----------------------------------------------------------------------------
set(DPBOX_PREFIX ${CMAKE_INSTALL_PREFIX} CACHE PATH "Path to DPAPP installation")
set(DPBOX_USR_PREFIX ${DPBOX_PREFIX}/usr)

set(CMAKE_INSTALL_RPATH "${DPBOX_USR_PREFIX}/lib;${DPBOX_USR_PREFIX}/lib64")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

message(STATUS "Install prefix: ${DPBOX_PREFIX}")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDPAPP_DEBUG)
    message(STATUS "Debug mode: DPAPP_DEBUG enabled (shorter timeouts)")
endif()

# Prefer dpbox prefix when finding dependencies.
list(PREPEND CMAKE_PREFIX_PATH "${DPBOX_USR_PREFIX}")

# -----------------------------------------------------------------------------
# Feature flags
# -----------------------------------------------------------------------------
set(DPAPP_CWC_ENABLE 0)
set(DPAPP_CPP_ENABLE 0)
set(DPAPP_LUA_ENABLE 0)
if(DPAPP_WITH_CWC)
    set(DPAPP_CWC_ENABLE 1)
endif()
if(DPAPP_WITH_CPP)
    set(DPAPP_CPP_ENABLE 1)
endif()
if(DPAPP_WITH_LUA)
    set(DPAPP_LUA_ENABLE 1)
endif()

# Defaults for generated config header.
set(DPAPP_SSL_ENABLE 0)
set(DPAPP_LSQUIC_ENABLE 0)
# -----------------------------------------------------------------------------
# Dependency discovery
# -----------------------------------------------------------------------------
if(DPAPP_WITH_LSQUIC AND NOT DPAPP_WITH_SSL)
    message(STATUS "DPAPP_WITH_LSQUIC=ON implies DPAPP_WITH_SSL=ON (overriding)")
    set(DPAPP_WITH_SSL ON CACHE BOOL "Enable OpenSSL support" FORCE)
endif()

if(DPAPP_WITH_SSL)
    find_package(OpenSSL QUIET)
    if(NOT OPENSSL_FOUND)
        message(FATAL_ERROR "OpenSSL not found, SSL support disabled")
    endif()
    set(DPAPP_SSL_ENABLE 1)
    message(STATUS "OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
    message(STATUS "OpenSSL libraries: ${OPENSSL_LIBRARIES}")
else()
    message(STATUS "DPAPP_WITH_SSL=OFF, skip OpenSSL detection")
endif()

if(DPAPP_WITH_LSQUIC AND OPENSSL_FOUND)
    find_package(lsquic QUIET CONFIG)
    # lsquic-config.cmake may only expose lsquic::lsquic.
    if(lsquic_FOUND AND (NOT DEFINED lsquic_LIBRARIES OR lsquic_LIBRARIES STREQUAL ""))
        if(TARGET lsquic::lsquic)
            set(lsquic_LIBRARIES lsquic::lsquic)
        endif()
    endif()
    if(NOT lsquic_FOUND)
        message(FATAL_ERROR "LSQUIC not found. Install OpenSSL and lsquic under ${DPBOX_USR_PREFIX} or system-wide.")
    endif()
    set(DPAPP_LSQUIC_ENABLE 1)
    message(STATUS "LSQUIC include dir: ${lsquic_INCLUDE_DIR}")
    message(STATUS "LSQUIC libraries: ${lsquic_LIBRARIES}")
else()
    message(STATUS "LSQUIC support disabled")
endif()

# -----------------------------------------------------------------------------
# Build stage (install-layout artifacts, separate from CMake intermediates)
# -----------------------------------------------------------------------------
set(DPAPP_STAGE_DIR "${CMAKE_BINARY_DIR}/stage" CACHE INTERNAL "")
set(DPAPP_DEV_ROOT "${DPAPP_STAGE_DIR}" CACHE INTERNAL "")

# 重新 configure 时清空 stage，避免陈旧产物与布局冲突
if(EXISTS "${DPAPP_STAGE_DIR}")
    file(REMOVE_RECURSE "${DPAPP_STAGE_DIR}")
endif()
if(EXISTS "${CMAKE_BINARY_DIR}/stage_layout")
    file(REMOVE_RECURSE "${CMAKE_BINARY_DIR}/stage_layout")
endif()

# 安装时先同步 stage，再由 install(TARGETS/EXPORT) 覆盖库与 cmake Targets
# 分项 DESTINATION 避免 DESTINATION . 产生 ${PREFIX}/./ 路径
# lua/ 仅安装子目录；符号链接从 stage 复制（见下方 install(CODE)）
foreach(_dpapp_stage_top usr app var)
    install(DIRECTORY "${DPAPP_STAGE_DIR}/${_dpapp_stage_top}/"
        DESTINATION "${_dpapp_stage_top}"
        USE_SOURCE_PERMISSIONS
    )
endforeach()
if(DPAPP_WITH_LUA)
    install(DIRECTORY "${DPAPP_STAGE_DIR}/lua/dplua/"
        DESTINATION lua/dplua
        USE_SOURCE_PERMISSIONS
    )
    install(DIRECTORY "${DPAPP_STAGE_DIR}/lua/clib/"
        DESTINATION lua/clib
        USE_SOURCE_PERMISSIONS
    )
endif()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${DPAPP_STAGE_DIR}/usr/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${DPAPP_STAGE_DIR}/usr/lib")

set(DPAPP_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(DPAPP_VERSION_MINOR "${PROJECT_VERSION_MINOR}")

set(DPAPP_CONFIG_HEADER "${DPAPP_STAGE_DIR}/usr/include/dpapp_config.h")
configure_file(
    "${PROJECT_SOURCE_DIR}/lib/dpapp_config.h.in"
    "${DPAPP_CONFIG_HEADER}"
    @ONLY
)

# -----------------------------------------------------------------------------
# FetchContent dependencies (cpp / lua)
# -----------------------------------------------------------------------------
if(DPAPP_WITH_CPP OR DPAPP_WITH_LUA)
    include(FetchContent)
endif()

if(DPAPP_WITH_LUA)
    macro(_dpapp_fetch_only name repo tag)
        FetchContent_Declare(
            ${name}
            GIT_REPOSITORY ${repo}
            GIT_TAG ${tag}
            GIT_SHALLOW TRUE
            GIT_SUBMODULES ""
            SOURCE_SUBDIR  __does_not_exist__
        )
        FetchContent_MakeAvailable(${name})
    endmacro()

    _dpapp_fetch_only(luajit https://gitcode.com/dppub/luajit.git v2.1-agentzh)
    # Patch: child coroutines must NOT inherit parent exdata/exdata2.
    # dplua uses exdata2 for registry reference counting — inheritance
    # would cause child coroutines to carry stale ref/flag values.
    FetchContent_GetProperties(luajit)
    if(luajit_POPULATED)
        set(_ljsrc "${luajit_SOURCE_DIR}/src/lj_state.c")
        if(EXISTS "${_ljsrc}")
            file(READ "${_ljsrc}" _ljstate_content)
            string(REPLACE "L1->exdata = L->exdata;" "L1->exdata = NULL;" _ljstate_content "${_ljstate_content}")
            string(REPLACE "L1->exdata2 = L->exdata2;" "L1->exdata2 = NULL;" _ljstate_content "${_ljstate_content}")
            file(WRITE "${_ljsrc}" "${_ljstate_content}")
        endif()
    endif()
endif()

if(DPAPP_WITH_CPP)
    set(UCORO_BUILD_TESTING OFF CACHE BOOL "Disable ucoro tests in dpapp build" FORCE)
    set(CMAKE_INSTALL_INCLUDEDIR usr/include CACHE STRING "" FORCE)
    FetchContent_Declare(
        ucoro
        GIT_REPOSITORY https://gitcode.com/dppub/ucoro.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        GIT_SUBMODULES ""
        PATCH_COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/cmake/patch_ucoro_stage.sh"
            CMakeLists.txt
    )
    FetchContent_MakeAvailable(ucoro)

    dpapp_stage_sync_directory(usr/include/ucoro "${ucoro_SOURCE_DIR}/include/ucoro")
    dpapp_stage_sync_files(usr/lib/cmake/ucoro
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ucoro-config.stage.cmake.in"
        OUT_NAMES ucoro-config.cmake
    )
endif()

# -----------------------------------------------------------------------------
# Project targets
# -----------------------------------------------------------------------------
include_directories(lib)
include_directories("${DPAPP_STAGE_DIR}/usr/include")

add_subdirectory(lib/dpapp)

if(DPAPP_WITH_CWC)
    add_subdirectory(lib/dpaco)
    add_subdirectory(lib/dpcwc)
endif()

if(DPAPP_WITH_CPP)
    add_subdirectory(lib/dpcpp)
endif()

if(DPAPP_WITH_LUA)
    add_subdirectory(lib/dplua)
endif()

if(DPAPP_WITH_DOCS)
    add_subdirectory(doc)
endif()

add_subdirectory(bin)
add_subdirectory(app)

dpapp_stage_sync_target()

# -----------------------------------------------------------------------------
# Install helpers
# -----------------------------------------------------------------------------
if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        @ONLY
    )
    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
        COMMENT "Uninstall dpapp from ${CMAKE_INSTALL_PREFIX}"
    )
endif()

set(_dpapp_link_install "
set(_r \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}\")
execute_process(COMMAND \"${CMAKE_COMMAND}\" -E rm -rf \"\${_r}/bin\")
file(INSTALL \"${DPAPP_STAGE_DIR}/bin\" DESTINATION \"\${_r}\")
")
if(DPAPP_WITH_LUA)
    string(APPEND _dpapp_link_install "
file(MAKE_DIRECTORY \"\${_r}/lua\")
execute_process(COMMAND \"${CMAKE_COMMAND}\" -E rm -rf
    \"\${_r}/lua/llib\" \"\${_r}/lua/luajit-2.1\")
file(INSTALL
    \"${DPAPP_STAGE_DIR}/lua/llib\"
    \"${DPAPP_STAGE_DIR}/lua/luajit-2.1\"
    DESTINATION \"\${_r}/lua\")
")
endif()
install(CODE "${_dpapp_link_install}")

# -----------------------------------------------------------------------------
# Tests
# -----------------------------------------------------------------------------
if(DPAPP_WITH_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tst)
    message(STATUS "Building test cases")
else()
    message(STATUS "Test cases disabled (use -DDPAPP_WITH_TESTS=ON to enable)")
endif()
