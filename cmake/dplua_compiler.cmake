# dplua_compiler.cmake - Lua 同步/字节码编译与安装（find_package(dplua) 后可用）
#
# dplua_lua_bytecode(
#     TARGET <custom_target>
#     DEST   <相对 CMAKE_INSTALL_PREFIX 的目录，如 lua/myapp>
#     FILES  <.lua 源文件列表>
#     [OUTPUT_DIR <构建期输出目录>]
#     [LUAJIT <luajit 可执行文件>]
#     [BYTECODE ON|OFF]  默认 DPAPP_COMPILE_LUA / DPLUA_COMPILE_LUA / ON
#     [DEPENDS <额外依赖目标>]
#     [INSTALL]  安装到 DEST（外部项目默认开启；写入 stage 时关闭）
# )
function(dplua_lua_bytecode)
    cmake_parse_arguments(_arg "INSTALL" "TARGET;DEST;OUTPUT_DIR;LUAJIT;BYTECODE" "FILES;DEPENDS" ${ARGN})
    if(NOT _arg_TARGET OR NOT _arg_DEST OR NOT _arg_FILES)
        message(FATAL_ERROR "dplua_lua_bytecode: TARGET DEST FILES are required")
    endif()

    if(NOT _arg_BYTECODE)
        if(DEFINED DPAPP_COMPILE_LUA)
            set(_arg_BYTECODE "${DPAPP_COMPILE_LUA}")
        elseif(DEFINED DPLUA_COMPILE_LUA)
            set(_arg_BYTECODE "${DPLUA_COMPILE_LUA}")
        else()
            set(_arg_BYTECODE ON)
        endif()
    endif()
    if(_arg_BYTECODE STREQUAL "OFF" OR _arg_BYTECODE STREQUAL "0" OR _arg_BYTECODE STREQUAL "FALSE")
        set(_use_bytecode OFF)
    else()
        set(_use_bytecode ON)
    endif()

    if(NOT _arg_OUTPUT_DIR)
        if(DEFINED DPAPP_STAGE_DIR)
            set(_arg_OUTPUT_DIR "${DPAPP_STAGE_DIR}/${_arg_DEST}")
        else()
            set(_arg_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/${_arg_TARGET}")
        endif()
    endif()

    if(_use_bytecode AND NOT _arg_LUAJIT)
        if(DEFINED DPLUA_LUAJIT_EXECUTABLE)
            set(_arg_LUAJIT "${DPLUA_LUAJIT_EXECUTABLE}")
        elseif(DEFINED DPAPP_STAGE_DIR)
            set(_arg_LUAJIT "${DPAPP_STAGE_DIR}/usr/bin/luajit")
        else()
            message(FATAL_ERROR "dplua_lua_bytecode: set LUAJIT or find_package(dplua) first")
        endif()
    endif()

    if(_arg_INSTALL)
        set(_do_install ON)
    elseif(DEFINED DPAPP_STAGE_DIR AND _arg_OUTPUT_DIR MATCHES "^${DPAPP_STAGE_DIR}")
        set(_do_install OFF)
    else()
        set(_do_install ON)
    endif()

    set(_outputs "")
    foreach(_f IN LISTS _arg_FILES)
        get_filename_component(_name "${_f}" NAME)
        set(_out "${_arg_OUTPUT_DIR}/${_name}")
        list(APPEND _outputs "${_out}")
        if(_use_bytecode)
            add_custom_command(
                OUTPUT "${_out}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${_arg_OUTPUT_DIR}"
                COMMAND "${_arg_LUAJIT}" -b "${_f}" "${_out}"
                DEPENDS ${_arg_DEPENDS} "${_f}"
                COMMENT "Bytecode ${_name} -> ${_arg_DEST}/"
            )
        else()
            add_custom_command(
                OUTPUT "${_out}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${_arg_OUTPUT_DIR}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_f}" "${_arg_OUTPUT_DIR}/"
                DEPENDS "${_f}"
                COMMENT "Sync ${_name} -> ${_arg_DEST}/"
            )
        endif()
    endforeach()
    add_custom_target(${_arg_TARGET} ALL DEPENDS ${_outputs})

    if(_do_install)
        set(_install_script "${CMAKE_CURRENT_BINARY_DIR}/cmake_${_arg_TARGET}_lua_install.cmake")
        file(WRITE "${_install_script}" "
set(_dplua_lua_files \"${_outputs}\")
set(_dplua_lua_dest \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${_arg_DEST}\")
file(MAKE_DIRECTORY \"\${_dplua_lua_dest}\")
foreach(_f IN LISTS _dplua_lua_files)
    if(NOT EXISTS \"\${_f}\")
        message(FATAL_ERROR \"dplua_lua_bytecode: missing \${_f}, build ${_arg_TARGET} first\")
    endif()
    file(INSTALL \"\${_f}\" DESTINATION \"\${_dplua_lua_dest}\")
endforeach()
")
        install(SCRIPT "${_install_script}")
    endif()

    set(${_arg_TARGET}_OUTPUTS "${_outputs}" PARENT_SCOPE)
endfunction()
