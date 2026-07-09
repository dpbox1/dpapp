# Example app modules (.so): artifacts in ${DPAPP_STAGE_DIR}/app/example (install layout).
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${DPAPP_STAGE_DIR}/app/example")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${DPAPP_STAGE_DIR}/app/example")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${DPAPP_STAGE_DIR}/app/example")

if(DPAPP_WITH_CPP)
    add_subdirectory(cpp)
endif()
if(DPAPP_WITH_CWC)
    add_subdirectory(cwc)
endif()
if(DPAPP_WITH_LUA)
    add_subdirectory(lua)
endif()

dpapp_stage_sync_files(app/example
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/crt.pem
        ${CMAKE_CURRENT_SOURCE_DIR}/key.pem
)
