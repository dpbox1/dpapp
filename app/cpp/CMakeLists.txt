add_library(cpp_echo_svr SHARED echo_svr.cc)
set_target_properties(cpp_echo_svr PROPERTIES OUTPUT_NAME "cpp_echo_svr")
set_target_properties(cpp_echo_svr PROPERTIES PREFIX "")

add_dependencies(cpp_echo_svr dpcpp)
target_compile_features(cpp_echo_svr PRIVATE cxx_std_20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")

target_include_directories(cpp_echo_svr PUBLIC
    ${PROJECT_SOURCE_DIR}/bin
    ${DPNET_PREFIX}/usr/include
)
target_link_libraries(cpp_echo_svr PUBLIC dpcpp)

add_library(cpp_echo_cet SHARED echo_cet.cc)
set_target_properties(cpp_echo_cet PROPERTIES OUTPUT_NAME "cpp_echo_cet")
set_target_properties(cpp_echo_cet PROPERTIES PREFIX "")

add_dependencies(cpp_echo_cet dpcpp)
target_compile_features(cpp_echo_cet PRIVATE cxx_std_20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")

target_include_directories(cpp_echo_cet PUBLIC
    ${PROJECT_SOURCE_DIR}/bin
    ${DPNET_PREFIX}/usr/include
)
target_link_libraries(cpp_echo_cet PUBLIC dpcpp)

install(
    TARGETS cpp_echo_svr cpp_echo_cet
    EXPORT dpapp-example-targets
    ARCHIVE DESTINATION app/example
    LIBRARY DESTINATION app/example
    RUNTIME DESTINATION usr/bin
)
