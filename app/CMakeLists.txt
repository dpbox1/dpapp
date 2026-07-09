# Example app modules (.so): place all artifacts in ${CMAKE_BINARY_DIR}/app
# (flat), not under app/<subdir>/ in the build tree.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

if(DPAPP_WITH_CPP)
    add_subdirectory(cpp)
endif()
if(DPAPP_WITH_CWC)
    add_subdirectory(cwc)
endif()
if(DPAPP_WITH_LUA)
    add_subdirectory(lua)
endif()

install(
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/crt.pem
        ${CMAKE_CURRENT_SOURCE_DIR}/key.pem
    DESTINATION app/example
)
