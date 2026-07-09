# CMake 包配置与安装（Config/Export 写入 stage，install(EXPORT) 修正 Targets 路径）
function(dpapp_install_package pkg)
    cmake_parse_arguments(_arg "NO_EXPORT" "EXPORT;CONFIG_IN;INCLUDES_DEST" "TARGETS" ${ARGN})
    if(NOT _arg_CONFIG_IN)
        set(_arg_CONFIG_IN "${PROJECT_SOURCE_DIR}/cmake/${pkg}Config.cmake.in")
    endif()
    if(NOT _arg_EXPORT)
        set(_arg_EXPORT "${pkg}Targets")
    endif()

    dpapp_stage_cmake_dir(${pkg} _pkg_cmake)

    if(_arg_TARGETS AND NOT _arg_NO_EXPORT)
        if(_arg_INCLUDES_DEST)
            install(
                TARGETS ${_arg_TARGETS}
                EXPORT ${_arg_EXPORT}
                ARCHIVE DESTINATION usr/lib
                LIBRARY DESTINATION usr/lib
                RUNTIME DESTINATION usr/bin
                INCLUDES DESTINATION ${_arg_INCLUDES_DEST}
            )
        else()
            install(
                TARGETS ${_arg_TARGETS}
                EXPORT ${_arg_EXPORT}
                ARCHIVE DESTINATION usr/lib
                LIBRARY DESTINATION usr/lib
                RUNTIME DESTINATION usr/bin
            )
        endif()
    endif()

    include(CMakePackageConfigHelpers)

    configure_package_config_file(
        ${_arg_CONFIG_IN}
        ${_pkg_cmake}/${pkg}Config.cmake
        INSTALL_DESTINATION usr/lib/cmake/${pkg}
        PATH_VARS CMAKE_INSTALL_PREFIX
    )

    write_basic_package_version_file(
        ${_pkg_cmake}/${pkg}ConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY AnyNewerVersion
    )

    if(_arg_TARGETS AND NOT _arg_NO_EXPORT)
        install(
            EXPORT ${_arg_EXPORT}
            FILE ${_arg_EXPORT}.cmake
            NAMESPACE dpbox::
            DESTINATION usr/lib/cmake/${pkg}
        )
        export(EXPORT ${_arg_EXPORT}
            FILE "${_pkg_cmake}/${_arg_EXPORT}.cmake"
            NAMESPACE dpbox::
        )
    endif()
endfunction()
