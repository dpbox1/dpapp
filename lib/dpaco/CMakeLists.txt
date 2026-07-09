enable_language(C ASM)

# dpaco library
add_library(dpaco SHARED "")

target_compile_features(dpaco PRIVATE c_std_11)
target_include_directories(dpaco PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
    $<BUILD_INTERFACE:${DPAPP_STAGE_DIR}/usr/include>
    $<INSTALL_INTERFACE:usr/include>
)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    target_sources(dpaco PRIVATE dpaco_arm64.S)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
    target_sources(dpaco PRIVATE dpaco_amd64.S)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64")
    target_sources(dpaco PRIVATE dpaco_riscv.S)
else()
    message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

target_sources(dpaco PRIVATE
    dpaco_arch.c
    dpaco.c
)

target_link_libraries(dpaco PRIVATE pthread m)

dpapp_stage_sync_files(usr/include/dpaco FILES ${PROJECT_SOURCE_DIR}/lib/dpaco/dpaco.h)

dpapp_install_package(dpaco TARGETS dpaco)
