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
    epockern
    epocloader)

add_test(
  NAME ekatests
  COMMAND ekatests
)

# Copy the sample RSC file
add_custom_command(
        TARGET ekatests
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/epoc/loader/sample_0xed3e09d5.rsc" "${CMAKE_CURRENT_BINARY_DIR}")