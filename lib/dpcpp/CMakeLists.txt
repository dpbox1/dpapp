# dpcpp execute (C++20 coroutine version)

cmake_minimum_required(VERSION 3.22.0)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable coroutines
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")

add_library(dpcpp SHARED "")
target_compile_features(dpcpp PRIVATE cxx_std_20)

add_dependencies(dpcpp dpapp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -rdynamic -fno-strict-aliasing -Wall -pipe -D_GNU_SOURCE -D_REENTRANT -fPIC -Wno-deprecated")

target_include_directories(dpcpp
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:usr/include>
)

target_link_libraries(dpcpp
    PUBLIC
        dpapp
        ucoro
    PRIVATE
        pthread
        m
        dl
)
target_sources(dpcpp PRIVATE
    dpcpp_aco.cc
    dpcpp.cc
    dpcpp_aux.cc
)

install(
    TARGETS dpcpp
    EXPORT dpcppTargets
    ARCHIVE DESTINATION usr/lib
    LIBRARY DESTINATION usr/lib
    RUNTIME DESTINATION usr/bin
)

file(GLOB DPCPP_HEADERS "${PROJECT_SOURCE_DIR}/lib/dpcpp/*.hh")
install(
    FILES ${DPCPP_HEADERS}
    DESTINATION usr/include/dpcpp
)

install(
    DIRECTORY "${ucoro_SOURCE_DIR}/include/ucoro"
    DESTINATION usr/include
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/dpcppConfig.cmake.in
    ${PROJECT_BINARY_DIR}/dpcppConfig.cmake
    INSTALL_DESTINATION usr/lib/cmake/dpcpp
    PATH_VARS CMAKE_INSTALL_PREFIX
)

write_basic_package_version_file(
    ${PROJECT_BINARY_DIR}/dpcppConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(
    FILES
        ${PROJECT_BINARY_DIR}/dpcppConfig.cmake
        ${PROJECT_BINARY_DIR}/dpcppConfigVersion.cmake
    DESTINATION usr/lib/cmake/dpcpp
)

install(
    EXPORT dpcppTargets
    FILE dpcppTargets.cmake
    NAMESPACE dpbox::
    DESTINATION usr/lib/cmake/dpcpp
)
