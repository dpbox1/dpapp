# C with coroutine

enable_language(C ASM)

add_library(dpcwc SHARED "")
add_dependencies(dpcwc dpapp dpaco)

target_compile_features(dpcwc PRIVATE c_std_11)

target_include_directories(dpcwc PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib/dpaco>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
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
install(
    FILES ${DPCWC_HEADERS}
    DESTINATION usr/include/dpcwc
)

install(
    TARGETS dpcwc
    EXPORT dpcwcTargets
    ARCHIVE DESTINATION usr/lib
    LIBRARY DESTINATION usr/lib
    RUNTIME DESTINATION usr/bin
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/dpcwcConfig.cmake.in
    ${PROJECT_BINARY_DIR}/dpcwcConfig.cmake
    INSTALL_DESTINATION usr/lib/cmake/dpcwc
    PATH_VARS CMAKE_INSTALL_PREFIX
)

write_basic_package_version_file(
    ${PROJECT_BINARY_DIR}/dpcwcConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(
    FILES
        ${PROJECT_BINARY_DIR}/dpcwcConfig.cmake
        ${PROJECT_BINARY_DIR}/dpcwcConfigVersion.cmake
    DESTINATION usr/lib/cmake/dpcwc
)

install(
    EXPORT dpcwcTargets
    FILE dpcwcTargets.cmake
    NAMESPACE dpbox::
    DESTINATION usr/lib/cmake/dpcwc
)
