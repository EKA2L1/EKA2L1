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

set(EPOC_LOADER_ASSETS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/epoc/loader/assets/")
file(GLOB assets RELATIVE ${EPOC_LOADER_ASSETS_PATH} ${EPOC_LOADER_ASSETS_PATH}/*)

foreach (asset ${assets})
    add_custom_command(
            TARGET ekatests
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${EPOC_LOADER_ASSETS_PATH}/${asset}" "${CMAKE_CURRENT_BINARY_DIR}")
endforeach()

# Copy the sample RSC file
add_custom_command(
        TARGET ekatests
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/epoc/loader/assets/sample_0xed3e09d5.rsc" "${CMAKE_CURRENT_BINARY_DIR}")