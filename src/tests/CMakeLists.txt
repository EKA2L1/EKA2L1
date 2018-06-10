include_directories(${GTEST_INCLUDE_DIR})
set (CMAKE_CXX_STANDARD 14)

add_subdirectory(core)

add_executable(ekatests 
	tests.cpp 
	${CORE_TESTS_FILE})

target_link_libraries(ekatests PRIVATE core common coreloader gmock gtest)
