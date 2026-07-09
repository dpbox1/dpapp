enable_language(C ASM)

# dpaco library
add_library(dpaco SHARED "")

target_compile_features(dpaco PRIVATE c_std_11)
target_include_directories(dpaco PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
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

install(
    FILES ${PROJECT_SOURCE_DIR}/lib/dpaco/dpaco.h
    DESTINATION usr/include/dpaco
)

install(
    TARGETS dpaco
    EXPORT dpacoTargets
    ARCHIVE DESTINATION usr/lib
    LIBRARY DESTINATION usr/lib
    RUNTIME DESTINATION usr/bin
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/dpacoConfig.cmake.in
    ${PROJECT_BINARY_DIR}/dpacoConfig.cmake
    INSTALL_DESTINATION usr/lib/cmake/dpaco
    PATH_VARS CMAKE_INSTALL_PREFIX
)

write_basic_package_version_file(
    ${PROJECT_BINARY_DIR}/dpacoConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(
    FILES
        ${PROJECT_BINARY_DIR}/dpacoConfig.cmake
        ${PROJECT_BINARY_DIR}/dpacoConfigVersion.cmake
    DESTINATION usr/lib/cmake/dpaco
)

install(
    EXPORT dpacoTargets
    FILE dpacoTargets.cmake
    NAMESPACE dpbox::
    DESTINATION usr/lib/cmake/dpaco
)
