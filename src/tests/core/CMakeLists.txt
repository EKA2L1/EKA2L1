add_subdirectory(loader)

set(CORE_TEST_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/core_mem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core_timing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vfs.cpp
    ${CORE_LOADER_TEST_FILES}
    PARENT_SCOPE)