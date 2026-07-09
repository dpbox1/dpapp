add_executable(dpapp_bin)
set_target_properties(dpapp_bin PROPERTIES OUTPUT_NAME "dpapp")
target_include_directories(dpapp_bin PRIVATE
    ${PROJECT_SOURCE_DIR}/lib
    ${PROJECT_BINARY_DIR}
)

add_dependencies(dpapp_bin dpapp)
target_link_libraries(dpapp_bin
    dpapp
    dl
)

if(DPAPP_WITH_CPP)
    add_dependencies(dpapp_bin dpcpp)
    target_sources(dpapp_bin PRIVATE
        dpapp_all.cc
    )
    target_compile_features(dpapp_bin PRIVATE cxx_std_20)
    target_link_libraries(dpapp_bin
        dpcpp
    )

    if(DPAPP_WITH_LUA)
        add_dependencies(dpapp_bin dplua)
        target_link_libraries(dpapp_bin
            dplua
        )
    endif()

    if(DPAPP_WITH_CWC)
        target_compile_features(dpapp_bin PRIVATE c_std_11)
        add_dependencies(dpapp_bin dpcwc dpaco)
        target_link_libraries(dpapp_bin
            dpcwc
            dpaco
        )
    endif()
elseif(DPAPP_WITH_CWC)
    add_dependencies(dpapp_bin dpcwc dpaco)
    target_sources(dpapp_bin PRIVATE
        dpapp_cwc.c
    )
    target_compile_features(dpapp_bin PRIVATE c_std_11)
    target_link_libraries(dpapp_bin
        dpcwc
        dpaco
    )
endif()

install(
    TARGETS dpapp_bin
    RUNTIME DESTINATION usr/bin
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/dpapp_binConfig.cmake.in
    ${PROJECT_BINARY_DIR}/dpapp_binConfig.cmake
    INSTALL_DESTINATION usr/lib/cmake/dpapp_bin
    PATH_VARS CMAKE_INSTALL_PREFIX
)

write_basic_package_version_file(
    ${PROJECT_BINARY_DIR}/dpapp_binConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(
    FILES
        ${PROJECT_BINARY_DIR}/dpapp_binConfig.cmake
        ${PROJECT_BINARY_DIR}/dpapp_binConfigVersion.cmake
    DESTINATION usr/lib/cmake/dpapp_bin
)
