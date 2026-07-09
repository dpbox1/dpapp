# C with coroutine

enable_language(C ASM)

add_library(dpcwc SHARED "")
add_dependencies(dpcwc dpapp dpaco)

target_compile_features(dpcwc PRIVATE c_std_11)

target_include_directories(dpcwc PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib/dpaco>
    $<BUILD_INTERFACE:${DPAPP_STAGE_DIR}/usr/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:usr/include>
)

target_link_libraries(dpcwc
    PUBLIC
        dpapp
        dpaco
    PRIVATE
        pthread
        m
)

target_sources(dpcwc PRIVATE
    dpcwc.c
    dpcwc_aux.c
)

file(GLOB DPCWC_HEADERS "${PROJECT_SOURCE_DIR}/lib/dpcwc/*.h")
dpapp_stage_sync_files(usr/include/dpcwc FILES ${DPCWC_HEADERS})

dpapp_install_package(dpcwc TARGETS dpcwc)
