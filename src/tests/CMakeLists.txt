include_directories(${GTEST_INCLUDE_DIR})

add_subdirectory(core)
add_subdirectory(common)

add_executable(ekatests 
	tests.cpp
    ${COMMON_TEST_FILES}
    ${CORE_TEST_FILES})

target_link_libraries(ekatests PRIVATE 
    gmock 
    gtest
    common
    epocloader
    epoc)

add_test(
  NAME ekatests
  COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/ekatests
)