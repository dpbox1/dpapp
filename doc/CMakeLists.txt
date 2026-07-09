find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(DOXYGEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/doxygen")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in"
        "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
        @ONLY
    )
    add_custom_target(
        doxygen_doc ALL
        COMMAND "${DOXYGEN_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
    message(STATUS "Doxygen: output ${DOXYGEN_OUTPUT_DIR}/html (target doxygen_doc, ALL)")
else()
    message(
        STATUS
        "Doxygen not found; documentation disabled (install doxygen or use -DDPAPP_WITH_DOCS=OFF)"
    )
endif()
