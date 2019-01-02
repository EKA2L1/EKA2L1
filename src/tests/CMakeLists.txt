include_directories(${GTEST_INCLUDE_DIR})

add_subdirectory(epoc)
add_subdirectory(common)

add_executable(ekatests 
	tests.cpp
    ${COMMON_TEST_FILES}
    ${CORE_TEST_FILES})

target_link_libraries(ekatests PRIVATE 
    gmock 
    gtest
    common
    epocio
    epockern)

add_test(
  NAME ekatests
  COMMAND ekatests
)