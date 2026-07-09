enable_language(C ASM)

# dpapp library
add_library(dpapp SHARED "")

set_target_properties(dpapp PROPERTIES OUTPUT_NAME dpapp)

target_compile_features(dpapp PRIVATE c_std_11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -rdynamic -fno-strict-aliasing -Wall -pipe -D_GNU_SOURCE -D_REENTRANT -fPIC -Wno-deprecated")

target_include_directories(dpapp PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
    $<BUILD_INTERFACE:${DPAPP_STAGE_DIR}/usr/include>
    $<INSTALL_INTERFACE:usr/include>
)

target_link_libraries(dpapp PUBLIC
    pthread
    m
)

if(OPENSSL_FOUND)
    # Prefer imported targets if the OpenSSL package provides them.
    if(TARGET OpenSSL::SSL)
        target_link_libraries(dpapp PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    else()
        if(DEFINED OPENSSL_INCLUDE_DIR AND OPENSSL_INCLUDE_DIR)
            target_include_directories(dpapp PRIVATE ${OPENSSL_INCLUDE_DIR})
        endif()
        if(DEFINED OPENSSL_LIBRARIES AND OPENSSL_LIBRARIES)
            target_link_libraries(dpapp PRIVATE ${OPENSSL_LIBRARIES})
        endif()
    endif()
endif()

# Add lsquic support if enabled
if(lsquic_FOUND)
    # Prefer variables first; they might be set to imported targets by the lsquic config.
    if(DEFINED lsquic_LIBRARIES AND lsquic_LIBRARIES)
        target_link_libraries(dpapp PRIVATE ${lsquic_LIBRARIES})
    elseif(TARGET lsquic::lsquic)
        target_link_libraries(dpapp PRIVATE lsquic::lsquic)
    endif()

    if(DEFINED lsquic_INCLUDE_DIR AND lsquic_INCLUDE_DIR)
        target_include_directories(dpapp PRIVATE ${lsquic_INCLUDE_DIR})
    endif()
endif()

target_sources(dpapp PRIVATE
    which.c
    dpapp.c
    dpbuf.c
    dplog.c
    dpret.c
    dpssl.c
    dpasc.c
    os/dpevp_pri.c
    os/dppip.c
    os/dpuds.c
    os/dpspd.c
    os/dpudp.c
    os/dptcp.c
    dpqic.c
    dpefd.c
)

target_include_directories(dpapp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/os)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Building on Linux")

    target_include_directories(dpapp PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/os/linux>)
    target_include_directories(dpapp PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/os/linux)
    target_sources(dpapp PRIVATE
        os/linux/dpevp_epoll.c
        os/linux/dpasc_epoll.c
        os/linux/dpsig.c
        os/linux/dpfsm.c
        os/linux/dpudp_pri.c
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    message(STATUS "Building on ${CMAKE_SYSTEM_NAME} (kqueue backend)")

    target_include_directories(dpapp PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/os/bsd>)
    target_include_directories(dpapp PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/os/bsd)
    target_sources(dpapp PRIVATE
        os/bsd/dpevp_kqueue.c
        os/bsd/dpasc_kqueue.c
        os/bsd/dpsig.c
        os/bsd/dpfsm.c
        os/bsd/dpudp_pri.c
    )
else()
    message(FATAL_ERROR "Building on other OS: ${CMAKE_SYSTEM_NAME}")
endif()

file(GLOB DPAPP_HEADERS "${PROJECT_SOURCE_DIR}/lib/dpapp/*.h")
dpapp_stage_sync_files(usr/include/dpapp FILES ${DPAPP_HEADERS})

dpapp_install_package(dpapp TARGETS dpapp)
