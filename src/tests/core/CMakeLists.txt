add_subdirectory(abi)

set (CORE_TESTS_FILE 
        ${CORE_LOADER_TESTS_FILE}
        ${CORE_ABI_TESTS_FILE}
        ${CMAKE_CURRENT_SOURCE_DIR}/core_mem.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/core_timing.cpp)
