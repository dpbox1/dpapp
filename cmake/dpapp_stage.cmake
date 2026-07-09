include(dplua_compiler)

# stage 下 cmake 包路径（与安装布局 usr/lib/cmake/<pkg> 对齐）
function(dpapp_stage_cmake_dir pkg out_var)
    set(${out_var} "${DPAPP_STAGE_DIR}/usr/lib/cmake/${pkg}" PARENT_SCOPE)
endfunction()

# 登记待同步文件；构建期由 dpapp_stage_sync_target 复制
function(dpapp_stage_sync_files dest_dir)
    cmake_parse_arguments(_arg "" "" "FILES;OUT_NAMES" ${ARGN})
    if(NOT _arg_FILES)
        return()
    endif()
    list(LENGTH _arg_FILES _file_count)
    list(LENGTH _arg_OUT_NAMES _name_count)
    if(_name_count GREATER 0 AND NOT _name_count EQUAL _file_count)
        message(FATAL_ERROR "dpapp_stage_sync_files: OUT_NAMES count must match FILES")
    endif()
    math(EXPR _last_idx "${_file_count} - 1")
    foreach(_i RANGE 0 ${_last_idx})
        list(GET _arg_FILES ${_i} _f)
        if(_name_count GREATER 0)
            list(GET _arg_OUT_NAMES ${_i} _out_name)
        else()
            get_filename_component(_out_name "${_f}" NAME)
        endif()
        set_property(GLOBAL APPEND PROPERTY DPAPP_STAGE_SYNC_ENTRIES
            "${dest_dir}|${_f}|${_out_name}")
    endforeach()
endfunction()

# 登记待同步目录；构建期 copy_directory 写入 stage
function(dpapp_stage_sync_directory dest_dir src_dir)
    if(NOT dest_dir OR NOT src_dir)
        message(FATAL_ERROR "dpapp_stage_sync_directory: dest_dir and src_dir are required")
    endif()
    set_property(GLOBAL APPEND PROPERTY DPAPP_STAGE_SYNC_DIRS "${dest_dir}|${src_dir}")
endfunction()

function(dpapp_stage_sync_target)
    get_property(_entries GLOBAL PROPERTY DPAPP_STAGE_SYNC_ENTRIES)
    get_property(_dirs GLOBAL PROPERTY DPAPP_STAGE_SYNC_DIRS)

    set(_outputs "")

    if(_entries)
        foreach(_entry IN LISTS _entries)
            string(REPLACE "|" ";" _parts "${_entry}")
            list(GET _parts 0 _dest)
            list(GET _parts 1 _src)
            list(LENGTH _parts _part_count)
            if(_part_count GREATER 2)
                list(GET _parts 2 _name)
            else()
                get_filename_component(_name "${_src}" NAME)
            endif()
            set(_out "${DPAPP_STAGE_DIR}/${_dest}/${_name}")
            list(APPEND _outputs "${_out}")
            add_custom_command(
                OUTPUT "${_out}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${DPAPP_STAGE_DIR}/${_dest}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_src}" "${_out}"
                DEPENDS "${_src}"
                COMMENT "Sync ${_name} -> ${_dest}/"
            )
        endforeach()
    endif()

    if(_dirs)
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/stage_layout")
        foreach(_entry IN LISTS _dirs)
            string(REPLACE "|" ";" _parts "${_entry}")
            list(GET _parts 0 _dest)
            list(GET _parts 1 _src)
            string(REPLACE "/" "_" _dest_id "${_dest}")
            set(_stamp "${CMAKE_BINARY_DIR}/stage_layout/sync_${_dest_id}.stamp")
            list(APPEND _outputs "${_stamp}")
            file(GLOB_RECURSE _dir_deps LIST_DIRECTORIES false "${_src}/*")
            add_custom_command(
                OUTPUT "${_stamp}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${DPAPP_STAGE_DIR}/${_dest}"
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${_src}" "${DPAPP_STAGE_DIR}/${_dest}"
                COMMAND ${CMAKE_COMMAND} -E touch "${_stamp}"
                DEPENDS ${_dir_deps}
                COMMENT "Sync ${_dest}/ <- ${_src}"
            )
        endforeach()
    endif()

    # 与安装布局一致：var/、bin -> usr/bin（stamp 在 build/，不写入 stage）
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/stage_layout")
    set(_var_stamp "${CMAKE_BINARY_DIR}/stage_layout/var.stamp")
    set(_bin_stamp "${CMAKE_BINARY_DIR}/stage_layout/bin.stamp")
    list(APPEND _outputs "${_var_stamp}" "${_bin_stamp}")

    add_custom_command(
        OUTPUT "${_var_stamp}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DPAPP_STAGE_DIR}/var"
        COMMAND ${CMAKE_COMMAND} -E touch "${_var_stamp}"
        COMMENT "Stage layout: var/"
    )
    add_custom_command(
        OUTPUT "${_bin_stamp}"
        COMMAND ${CMAKE_COMMAND} -E rm -f "${DPAPP_STAGE_DIR}/bin"
        COMMAND ${CMAKE_COMMAND} -E create_symlink usr/bin "${DPAPP_STAGE_DIR}/bin"
        COMMAND ${CMAKE_COMMAND} -E touch "${_bin_stamp}"
        COMMENT "Stage layout: bin -> usr/bin"
    )

    if(DPAPP_WITH_LUA)
        set(_lua_stamp "${CMAKE_BINARY_DIR}/stage_layout/lua.stamp")
        list(APPEND _outputs "${_lua_stamp}")
        add_custom_command(
            OUTPUT "${_lua_stamp}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${DPAPP_STAGE_DIR}/lua/clib"
            COMMAND ${CMAKE_COMMAND} -E rm -f "${DPAPP_STAGE_DIR}/lua/llib"
            COMMAND ${CMAKE_COMMAND} -E create_symlink
                ../usr/share/lua/5.1 "${DPAPP_STAGE_DIR}/lua/llib"
            COMMAND ${CMAKE_COMMAND} -E rm -f "${DPAPP_STAGE_DIR}/lua/luajit-2.1"
            COMMAND ${CMAKE_COMMAND} -E create_symlink
                ../usr/share/luajit-2.1 "${DPAPP_STAGE_DIR}/lua/luajit-2.1"
            COMMAND ${CMAKE_COMMAND} -E touch "${_lua_stamp}"
            DEPENDS compile_luajit
            COMMENT "Stage layout: lua/clib, llib, luajit-2.1"
        )
    endif()

    add_custom_target(dpapp_stage_sync ALL DEPENDS ${_outputs})
endfunction()
