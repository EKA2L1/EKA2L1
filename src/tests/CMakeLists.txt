macro(add_test_assets PATH DESTPATH)
    file(GLOB assets RELATIVE ${PATH} ${PATH}/*)

    add_custom_command(
                TARGET ekatests
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/${DESTPATH}/")

    foreach (asset ${assets})
        add_custom_command(
                TARGET ekatests
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PATH}/${asset}" "${CMAKE_CURRENT_BINARY_DIR}/${DESTPATH}/")
    endforeach()
endmacro()

include_directories(${GTEST_INCLUDE_DIR})

# Disable logging for test
add_definitions(-DDISABLE_LOGGING)

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
add_test_assets(${EPOC_LOADER_ASSETS_PATH} "loaderassets")

set(COMMON_ASSETS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/common/assets/")
add_test_assets(${COMMON_ASSETS_PATH} "commonassets")

set(SERVICES_CENTRALREPO_ASSETS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/epoc/services/centralrepo/assets/")
add_test_assets(${SERVICES_CENTRALREPO_ASSETS_PATH} "centralrepoassets")

# Copy the sample RSC file
add_custom_command(
        TARGET ekatests
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/epoc/loader/assets/sample_0xed3e09d5.rsc" "${CMAKE_CURRENT_BINARY_DIR}")