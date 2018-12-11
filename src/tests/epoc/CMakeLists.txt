add_subdirectory(loader)

set(CORE_TEST_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/mem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/timing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vfs.cpp
    ${CORE_LOADER_TEST_FILES}
    PARENT_SCOPE)