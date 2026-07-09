add_executable(dpapp_bin)
set_target_properties(dpapp_bin PROPERTIES
    OUTPUT_NAME "dpapp"
    RUNTIME_OUTPUT_DIRECTORY "${DPAPP_STAGE_DIR}/usr/bin"
)
target_include_directories(dpapp_bin PRIVATE
    ${PROJECT_SOURCE_DIR}/lib
    ${DPAPP_STAGE_DIR}/usr/include
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

dpapp_install_package(dpapp_bin NO_EXPORT)
