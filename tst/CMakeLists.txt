enable_language(C CXX ASM)

get_filename_component(PROJECT_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/.. ABSOLUTE)

message(STATUS "PROJECT_ROOT_DIR: ${PROJECT_ROOT_DIR}")

# echo 集成：tst/test_echo.sh case <binding> <proto> <short|large> ...
function(dpapp_add_echo_test suffix binding proto size timeout)
    set(_test_name "integration.echo.${suffix}")
    add_test(
        NAME ${_test_name}
        COMMAND /usr/bin/env bash ${PROJECT_ROOT_DIR}/tst/test_echo.sh
            case ${binding} ${proto} ${size}
            $<TARGET_FILE:dpapp_bin> ${PROJECT_BINARY_DIR}
    )
    set(_props TIMEOUT ${timeout} WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
    set_tests_properties(${_test_name} PROPERTIES ${_props})
endfunction()

# HTTP 集成：tst/test_http.sh case <binding> <http|https|http3> ...
function(dpapp_add_http_test suffix binding proto timeout)
    set(_test_name "integration.http.${suffix}")
    add_test(
        NAME ${_test_name}
        COMMAND /usr/bin/env bash ${PROJECT_ROOT_DIR}/tst/test_http.sh
            case ${binding} ${proto}
            $<TARGET_FILE:dpapp_bin> ${PROJECT_BINARY_DIR}
    )
    set(_props TIMEOUT ${timeout} WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
    if(proto STREQUAL "http3")
        list(APPEND _props SKIP_RETURN_CODE 77)
    endif()
    set_tests_properties(${_test_name} PROPERTIES ${_props})
endfunction()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(COROUTINE_ASM_FILE "${PROJECT_ROOT_DIR}/lib/dpaco/dpaco_arm64.S")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
    set(COROUTINE_ASM_FILE "${PROJECT_ROOT_DIR}/lib/dpaco/dpaco_amd64.S")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64")
    set(COROUTINE_ASM_FILE "${PROJECT_ROOT_DIR}/lib/dpaco/dpaco_riscv.S")
else()
    message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

add_executable(test_coroutine
    test_coroutine.c
    ${PROJECT_ROOT_DIR}/lib/dpaco/dpaco_arch.c
    ${PROJECT_ROOT_DIR}/lib/dpaco/dpaco.c
    ${COROUTINE_ASM_FILE}
)

target_compile_features(test_coroutine PRIVATE c_std_11)
target_include_directories(test_coroutine PUBLIC
    ${PROJECT_ROOT_DIR}/lib
    ${PROJECT_ROOT_DIR}/lib/dpaco
    ${PROJECT_BINARY_DIR}
)
target_link_libraries(test_coroutine pthread m)

add_test(NAME unit.coroutine.basic COMMAND test_coroutine)
set_tests_properties(unit.coroutine.basic PROPERTIES TIMEOUT 20)

add_executable(test_coroutine_perf
    test_coroutine_perf.c
    ${PROJECT_ROOT_DIR}/lib/dpaco/dpaco_arch.c
    ${PROJECT_ROOT_DIR}/lib/dpaco/dpaco.c
    ${COROUTINE_ASM_FILE}
)

target_compile_features(test_coroutine_perf PRIVATE c_std_11)
target_include_directories(test_coroutine_perf PUBLIC
    ${PROJECT_ROOT_DIR}/lib
    ${PROJECT_ROOT_DIR}/lib/dpaco
    ${PROJECT_BINARY_DIR}
)
target_link_libraries(test_coroutine_perf pthread m)

add_test(NAME perf.coroutine COMMAND test_coroutine_perf)
set_tests_properties(perf.coroutine PROPERTIES TIMEOUT 120)

add_executable(test_basic test_basic.c)
target_compile_features(test_basic PRIVATE c_std_11)
target_include_directories(test_basic PUBLIC
    ${PROJECT_ROOT_DIR}/lib
    ${PROJECT_BINARY_DIR}
)
add_dependencies(test_basic dpapp)
target_link_libraries(test_basic dpapp)
add_test(NAME unit.basic COMMAND test_basic)
set_tests_properties(unit.basic PROPERTIES TIMEOUT 30)

add_test(
    NAME integration.dpapp.help
    COMMAND $<TARGET_FILE:dpapp_bin> -h
)
set_tests_properties(
    integration.dpapp.help
    PROPERTIES
        PASS_REGULAR_EXPRESSION "Produce help message"
        TIMEOUT 10
)

add_test(
    NAME integration.dpapp.invalid_module
    COMMAND $<TARGET_FILE:dpapp_bin> /definitely/not/found/module.so
)
set_tests_properties(
    integration.dpapp.invalid_module
    PROPERTIES
        WILL_FAIL TRUE
        TIMEOUT 10
)

add_test(
    NAME integration.dpapp.invalid_root_dir
    COMMAND $<TARGET_FILE:dpapp_bin> -d /definitely/not/found echo/echo_svr.so
)
set_tests_properties(
    integration.dpapp.invalid_root_dir
    PROPERTIES
        WILL_FAIL TRUE
        TIMEOUT 10
)

if(DPAPP_WITH_CWC)
    set(_ECHO_PROTOS tcp ssl)
    if(lsquic_FOUND)
        list(APPEND _ECHO_PROTOS qic)
    endif()
    foreach(_proto IN LISTS _ECHO_PROTOS)
        dpapp_add_echo_test("cwc.${_proto}.short" cwc ${_proto} short 15)
        dpapp_add_echo_test("cwc.${_proto}.large" cwc ${_proto} large 12)
    endforeach()

    dpapp_add_http_test("cwc.http" cwc http 15)
    dpapp_add_http_test("cwc.https" cwc https 15)
    if(lsquic_FOUND)
        dpapp_add_http_test("cwc.http3" cwc http3 15)
    endif()

    add_library(test_ctc_perf SHARED test_ctc_perf.c)
    set_target_properties(test_ctc_perf PROPERTIES OUTPUT_NAME "test_ctc_perf")
    set_target_properties(test_ctc_perf PROPERTIES PREFIX "")

    add_dependencies(test_ctc_perf dpcwc)
    target_link_libraries(test_ctc_perf PRIVATE dpcwc)
    target_compile_features(test_ctc_perf PRIVATE c_std_11)

    target_include_directories(test_ctc_perf PUBLIC
        ${PROJECT_ROOT_DIR}/lib
        ${PROJECT_ROOT_DIR}/lib/dpaco
        ${PROJECT_BINARY_DIR}
        ${PROJECT_ROOT_DIR}/bin
    )

    add_library(test_cwc SHARED test_cwc.c)
    set_target_properties(test_cwc PROPERTIES OUTPUT_NAME "test_cwc")
    set_target_properties(test_cwc PROPERTIES PREFIX "")

    add_dependencies(test_cwc dpcwc)
    target_link_libraries(test_cwc PRIVATE dpcwc)
    target_compile_features(test_cwc PRIVATE c_std_11)

    target_include_directories(test_cwc PUBLIC
        ${PROJECT_ROOT_DIR}/lib
        ${PROJECT_ROOT_DIR}/lib/dpaco
        ${PROJECT_BINARY_DIR}
        ${PROJECT_ROOT_DIR}/bin
    )

    add_test(
        NAME integration.cwc.ctc
        COMMAND /usr/bin/env bash ${PROJECT_ROOT_DIR}/tst/test_cwc.sh
            $<TARGET_FILE:dpapp_bin>
            ${PROJECT_BINARY_DIR}
            $<TARGET_FILE:test_cwc>
    )
    set_tests_properties(
        integration.cwc.ctc
        PROPERTIES
            TIMEOUT 15
            WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    )

    add_library(test_fsm_sig_pip SHARED test_fsm_sig_pip.c)
    set_target_properties(test_fsm_sig_pip PROPERTIES OUTPUT_NAME "test_fsm_sig_pip")
    set_target_properties(test_fsm_sig_pip PROPERTIES PREFIX "")

    add_dependencies(test_fsm_sig_pip dpcwc)
    target_link_libraries(test_fsm_sig_pip PRIVATE dpcwc)
    target_compile_features(test_fsm_sig_pip PRIVATE c_std_11)

    target_include_directories(test_fsm_sig_pip PUBLIC
        ${PROJECT_ROOT_DIR}/lib
        ${PROJECT_ROOT_DIR}/lib/dpaco
        ${PROJECT_BINARY_DIR}
        ${PROJECT_ROOT_DIR}/bin
    )

    add_test(
        NAME integration.cwc.fsm_sig_pip
        COMMAND /usr/bin/env bash ${PROJECT_ROOT_DIR}/tst/test_fsm_sig_pip.sh
            $<TARGET_FILE:dpapp_bin>
            ${PROJECT_BINARY_DIR}
            $<TARGET_FILE:test_fsm_sig_pip>
    )
    set_tests_properties(
        integration.cwc.fsm_sig_pip
        PROPERTIES
            TIMEOUT 15
            WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    )
endif()

if(lsquic_FOUND)
    add_executable(test_dpqic_hdrset
        test_qic_hdrset.c
    )

    target_compile_features(test_dpqic_hdrset PRIVATE c_std_11)
    target_include_directories(test_dpqic_hdrset PUBLIC
        ${PROJECT_ROOT_DIR}/lib
        ${PROJECT_ROOT_DIR}/lib/dpaco
        ${PROJECT_BINARY_DIR}
        ${DPBOX_PREFIX}/include
    )

    add_dependencies(test_dpqic_hdrset dpapp)
    target_link_libraries(test_dpqic_hdrset dpapp)
    target_include_directories(test_dpqic_hdrset PRIVATE ${lsquic_INCLUDE_DIR})
    target_link_directories(test_dpqic_hdrset PRIVATE ${lsquic_LIBRARY_DIR})
    target_link_libraries(test_dpqic_hdrset ${lsquic_LIBRARIES})

    add_test(NAME unit.dpqic.hdrset COMMAND test_dpqic_hdrset)
    set_tests_properties(unit.dpqic.hdrset PROPERTIES TIMEOUT 30)

endif()

if(DPAPP_WITH_CPP)
    set(_ECHO_PROTOS tcp ssl)
    if(lsquic_FOUND)
        list(APPEND _ECHO_PROTOS qic)
    endif()
    foreach(_proto IN LISTS _ECHO_PROTOS)
        dpapp_add_echo_test("cpp.${_proto}.short" cpp ${_proto} short 15)
        dpapp_add_echo_test("cpp.${_proto}.large" cpp ${_proto} large 12)
    endforeach()

    add_library(test_cpp_fsm_sig_pip SHARED test_fsm_sig_pip.cc)
    set_target_properties(test_cpp_fsm_sig_pip PROPERTIES OUTPUT_NAME "test_cpp_fsm_sig_pip")
    set_target_properties(test_cpp_fsm_sig_pip PROPERTIES PREFIX "")

    add_dependencies(test_cpp_fsm_sig_pip dpcpp)
    target_link_libraries(test_cpp_fsm_sig_pip PRIVATE dpcpp)
    target_compile_features(test_cpp_fsm_sig_pip PRIVATE cxx_std_20)

    target_include_directories(test_cpp_fsm_sig_pip PUBLIC
        ${PROJECT_ROOT_DIR}/lib
        ${PROJECT_BINARY_DIR}
        ${PROJECT_ROOT_DIR}/bin
    )

    add_test(
        NAME integration.cpp.fsm_sig_pip
        COMMAND /usr/bin/env bash ${PROJECT_ROOT_DIR}/tst/test_fsm_sig_pip.sh
            $<TARGET_FILE:dpapp_bin>
            ${PROJECT_BINARY_DIR}
            $<TARGET_FILE:test_cpp_fsm_sig_pip>
    )
    set_tests_properties(
        integration.cpp.fsm_sig_pip
        PROPERTIES
            TIMEOUT 15
            WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    )

    if(DPAPP_WITH_LUA)
        add_test(
            NAME integration.dplua.basic
            COMMAND $<TARGET_FILE:dpapp_bin>
                -n1 1
                -d ${PROJECT_BINARY_DIR}
                ${PROJECT_ROOT_DIR}/tst/test_lua.lua
        )
        set_tests_properties(
            integration.dplua.basic
            PROPERTIES
                TIMEOUT 30
                WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
        )

        add_test(
            NAME integration.dplua.fsm_sig_pip
            COMMAND /usr/bin/env bash ${PROJECT_ROOT_DIR}/tst/test_fsm_sig_pip.sh
                $<TARGET_FILE:dpapp_bin>
                ${PROJECT_BINARY_DIR}
                ${PROJECT_ROOT_DIR}/tst/test_fsm_sig_pip.lua
        )
        set_tests_properties(
            integration.dplua.fsm_sig_pip
            PROPERTIES
                TIMEOUT 30
                WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
        )

        foreach(_proto IN LISTS _ECHO_PROTOS)
            dpapp_add_echo_test("lua.${_proto}.short" lua ${_proto} short 15)
            dpapp_add_echo_test("lua.${_proto}.large" lua ${_proto} large 12)
        endforeach()

        dpapp_add_http_test("lua.http" lua http 15)
        dpapp_add_http_test("lua.https" lua https 15)
        if(lsquic_FOUND)
            dpapp_add_http_test("lua.http3" lua http3 15)
        endif()
    endif()
endif()