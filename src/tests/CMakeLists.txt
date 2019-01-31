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

# Disable logging for test
add_definitions(-DDISABLE_LOGGING)

add_subdirectory(epoc)
add_subdirectory(common)

add_executable(ekatests 
	tests.cpp
    ${COMMON_TEST_FILES}
    ${CORE_TEST_FILES})

target_link_libraries(ekatests PRIVATE
    Catch2
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
