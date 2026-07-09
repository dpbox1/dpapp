add_library(cwc_echo_svr SHARED echo_svr.c)
set_target_properties(cwc_echo_svr PROPERTIES OUTPUT_NAME "cwc_echo_svr")
set_target_properties(cwc_echo_svr PROPERTIES PREFIX "")

add_dependencies(cwc_echo_svr dpcwc)
target_link_libraries(cwc_echo_svr PRIVATE dpcwc)
target_compile_features(cwc_echo_svr PRIVATE c_std_11)

target_include_directories(cwc_echo_svr PUBLIC
    ${PROJECT_SOURCE_DIR}/lib
    ${PROJECT_SOURCE_DIR}/lib/dpaco
    ${DPAPP_STAGE_DIR}/usr/include
    ${PROJECT_SOURCE_DIR}/bin
    ${PROJECT_SOURCE_DIR}/app/cwc
)

add_library(cwc_echo_cet SHARED echo_cet.c)
set_target_properties(cwc_echo_cet PROPERTIES OUTPUT_NAME "cwc_echo_cet")
set_target_properties(cwc_echo_cet PROPERTIES PREFIX "")

add_dependencies(cwc_echo_cet dpcwc)
target_link_libraries(cwc_echo_cet PRIVATE dpcwc)
target_compile_features(cwc_echo_cet PRIVATE c_std_11)

target_include_directories(cwc_echo_cet PUBLIC
    ${PROJECT_SOURCE_DIR}/lib
    ${PROJECT_SOURCE_DIR}/lib/dpaco
    ${DPAPP_STAGE_DIR}/usr/include
    ${PROJECT_SOURCE_DIR}/bin
    ${PROJECT_SOURCE_DIR}/app/cwc
)

add_library(cwc_echo_cet_bm SHARED echo_cet_bm.c)
set_target_properties(cwc_echo_cet_bm PROPERTIES OUTPUT_NAME "cwc_echo_cet_bm")
set_target_properties(cwc_echo_cet_bm PROPERTIES PREFIX "")

add_dependencies(cwc_echo_cet_bm dpcwc)
target_link_libraries(cwc_echo_cet_bm PRIVATE dpcwc)
target_compile_features(cwc_echo_cet_bm PRIVATE c_std_11)

target_include_directories(cwc_echo_cet_bm PUBLIC
    ${PROJECT_SOURCE_DIR}/lib
    ${PROJECT_SOURCE_DIR}/lib/dpaco
    ${DPAPP_STAGE_DIR}/usr/include
    ${PROJECT_SOURCE_DIR}/bin
    ${PROJECT_SOURCE_DIR}/app/cwc
)

add_library(cwc_http_svr SHARED http_svr.c)
set_target_properties(cwc_http_svr PROPERTIES OUTPUT_NAME "cwc_http_svr")
set_target_properties(cwc_http_svr PROPERTIES PREFIX "")
add_dependencies(cwc_http_svr dpcwc)
target_link_libraries(cwc_http_svr PRIVATE dpcwc)
target_compile_features(cwc_http_svr PRIVATE c_std_11)
target_compile_features(cwc_http_svr PRIVATE cxx_std_11)
target_include_directories(cwc_http_svr PUBLIC
    ${PROJECT_SOURCE_DIR}/lib
    ${PROJECT_SOURCE_DIR}/lib/dpaco
    ${DPAPP_STAGE_DIR}/usr/include
    ${PROJECT_SOURCE_DIR}/bin
    ${PROJECT_SOURCE_DIR}/app/cwc
)
