include(FileUtils)

add_subdirectory(epoc)
add_subdirectory(common)

add_executable(ekatests 
	tests.cpp
    platform.cpp
    ${COMMON_TEST_FILES}
    ${CORE_TEST_FILES})


target_link_libraries(ekatests PRIVATE
    Catch2
    common
    config
    epoc
    epocio
    epockern
    epocloader
    epocpkg
    epocservs
    epocdispatch
    libtess2
    lunasvg)

# The fixtures are copied next to the executable, and the tests open them by a
# relative path. Multi-config generators put the executable in a per-config
# subdirectory, which is not where CTest would otherwise run it from.
add_test(
  NAME ekatests
  COMMAND ekatests
  WORKING_DIRECTORY $<TARGET_FILE_DIR:ekatests>
)

if (WIN32)
    # SDL2 and the other runtime DLLs are copied next to the frontend, which is
    # not where the test executable is built, so the loader has to be told.
    file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}/bin" EKATESTS_RUNTIME_DLL_DIR)
    set_tests_properties(ekatests PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${EKATESTS_RUNTIME_DLL_DIR}")
endif()

set(EPOC_LOADER_ASSETS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/epoc/loader/assets/")
add_assets(ekatests ${EPOC_LOADER_ASSETS_PATH} "loaderassets")

set(COMMON_ASSETS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/common/assets/")
add_assets(ekatests ${COMMON_ASSETS_PATH} "commonassets")

set(SERVICES_CENTRALREPO_ASSETS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/epoc/services/centralrepo/assets/")
add_assets(ekatests ${SERVICES_CENTRALREPO_ASSETS_PATH} "centralrepoassets")

set(SERVICES_APPLIST_ASSETS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/epoc/services/applist/assets/")
add_assets(ekatests ${SERVICES_APPLIST_ASSETS_PATH} "applistassets")

set(PACKAGE_ASSETS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/epoc/package/assets/")
add_assets(ekatests ${PACKAGE_ASSETS_PATH} "packageassets")
