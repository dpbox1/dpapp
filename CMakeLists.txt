cmake_minimum_required(VERSION 3.16.0)
project(dpapp VERSION 1.2)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

option(DPAPP_WITH_LSQUIC "Support lsquic" OFF)
option(DPAPP_WITH_SSL "Enable OpenSSL support" ON)
option(DPAPP_WITH_TESTS "Build test cases" ON)
option(DPAPP_WITH_CWC "Build CWC components" ON)
option(DPAPP_WITH_CPP "Build C++ components" ON)
option(DPAPP_WITH_LUA "Build Lua components" ON)
option(DPAPP_WITH_DOCS "Build Doxygen HTML documentation on every build (ALL)" ON)

# -----------------------------------------------------------------------------
# Install prefixes / rpath
# -----------------------------------------------------------------------------
if(NOT DEFINED DPBOX_PREFIX)
    set(DPBOX_PREFIX ${CMAKE_INSTALL_PREFIX} CACHE PATH "Path to DPAPP installation")
endif()
if(NOT DEFINED DPBOX_USR_PREFIX)
    set(DPBOX_USR_PREFIX ${DPBOX_PREFIX}/usr)
endif()

set(CMAKE_INSTALL_RPATH "${DPBOX_USR_PREFIX}/lib;${DPBOX_USR_PREFIX}/lib64")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

if(EXISTS "${DPBOX_USR_PREFIX}")
    message(STATUS "DPBOX_USR_PREFIX: ${DPBOX_USR_PREFIX}")
else()
    message(STATUS "DPBOX_USR_PREFIX does not exist: ${DPBOX_USR_PREFIX} (will fallback to system)")
endif()

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
    # 与 dpapp 安装布局对齐：头文件与 cmake 包均在 prefix/usr/ 下。
    set(CMAKE_INSTALL_INCLUDEDIR usr/include CACHE STRING "" FORCE)
    FetchContent_Declare(
        ucoro
        GIT_REPOSITORY https://gitcode.com/dppub/ucoro.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        GIT_SUBMODULES ""
        PATCH_COMMAND bash -c
            "sed -i 's#share/ucoro#usr/lib/cmake/ucoro#' CMakeLists.txt"
    )
    FetchContent_MakeAvailable(ucoro)
endif()

# -----------------------------------------------------------------------------
# Generated config header
# -----------------------------------------------------------------------------
set(DPAPP_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(DPAPP_VERSION_MINOR "${PROJECT_VERSION_MINOR}")

set(DPAPP_CONFIG_HEADER "${PROJECT_BINARY_DIR}/lib/dpapp_config.h")
configure_file(
    "${PROJECT_SOURCE_DIR}/lib/dpapp_config.h.in"
    "${DPAPP_CONFIG_HEADER}"
    @ONLY
)

install(
    FILES ${DPAPP_CONFIG_HEADER}
    DESTINATION usr/include
    RENAME dpapp_config.h
)

# -----------------------------------------------------------------------------
# Project targets
# -----------------------------------------------------------------------------
include_directories(lib)
include_directories("${PROJECT_BINARY_DIR}/lib")

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

# -----------------------------------------------------------------------------
# Install helpers
# -----------------------------------------------------------------------------
install(DIRECTORY DESTINATION var)

# Under install prefix (same as DPBOX_PREFIX when CMAKE_INSTALL_PREFIX matches):
# bin -> usr/bin. Bracket form keeps ${CMAKE_INSTALL_PREFIX} for install-time
# (supports cmake --install --prefix).
install(
    CODE [[
    set(_dpapp_root "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}")
    if(EXISTS "${_dpapp_root}/bin" AND NOT IS_SYMLINK "${_dpapp_root}/bin")
        message(WARNING "Skipping bin -> usr/bin: ${_dpapp_root}/bin exists and is not a symlink")
    else()
        if(EXISTS "${_dpapp_root}/bin")
            file(REMOVE "${_dpapp_root}/bin")
        endif()
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E create_symlink usr/bin bin
            WORKING_DIRECTORY "${_dpapp_root}"
            RESULT_VARIABLE _dpapp_symlink_r
        )
        if(_dpapp_symlink_r)
            message(WARNING "Could not create ${_dpapp_root}/bin -> usr/bin (code ${_dpapp_symlink_r})")
        endif()
    endif()
    ]]
)

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
