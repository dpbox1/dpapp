# dpcpp execute (C++20 coroutine version)

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
        $<BUILD_INTERFACE:${DPAPP_STAGE_DIR}/usr/include>
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

dpapp_install_package(dpcpp TARGETS dpcpp ucoro INCLUDES_DEST usr/include)

file(GLOB DPCPP_HEADERS "${PROJECT_SOURCE_DIR}/lib/dpcpp/*.hh")
dpapp_stage_sync_files(usr/include/dpcpp FILES ${DPCPP_HEADERS})
