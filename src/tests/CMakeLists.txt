include(TestUtils)

add_subdirectory(epoc)
add_subdirectory(common)
add_subdirectory(drivers)

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
add_test_assets(ekatests ${EPOC_LOADER_ASSETS_PATH} "loaderassets")

set(COMMON_ASSETS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/common/assets/")
add_test_assets(ekatests ${COMMON_ASSETS_PATH} "commonassets")

set(SERVICES_CENTRALREPO_ASSETS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/epoc/services/centralrepo/assets/")
add_test_assets(ekatests ${SERVICES_CENTRALREPO_ASSETS_PATH} "centralrepoassets")
