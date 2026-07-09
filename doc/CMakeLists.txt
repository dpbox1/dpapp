find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(DOXYGEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/doxygen")
    set(DPAPP_DOXYGEN_STAGE_DIR "${DPAPP_STAGE_DIR}/usr/share/dpapp/doxygen")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in"
        "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
        @ONLY
    )
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/stage_layout")
    set(_doxygen_stamp "${CMAKE_BINARY_DIR}/stage_layout/doxygen.stamp")
    add_custom_command(
        OUTPUT "${_doxygen_stamp}"
        COMMAND "${DOXYGEN_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${DPAPP_DOXYGEN_STAGE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DPAPP_DOXYGEN_STAGE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${DOXYGEN_OUTPUT_DIR}/html"
            "${DPAPP_DOXYGEN_STAGE_DIR}/html"
        COMMAND ${CMAKE_COMMAND} -E touch "${_doxygen_stamp}"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
            "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in"
            "${CMAKE_CURRENT_SOURCE_DIR}/dpdoxy_groups.h"
        COMMENT "Doxygen -> stage/usr/share/dpapp/doxygen/html"
        VERBATIM
    )
    add_custom_target(doxygen_doc ALL DEPENDS "${_doxygen_stamp}")
    message(STATUS "Doxygen: stage ${DPAPP_DOXYGEN_STAGE_DIR}/html (target doxygen_doc, ALL)")
else()
    message(
        STATUS
        "Doxygen not found; documentation disabled (install doxygen or use -DDPAPP_WITH_DOCS=OFF)"
    )
endif()
