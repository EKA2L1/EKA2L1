if (WIN32)
    add_definitions(-DNOMINMAX)
    message("NOMINMAX")
endif()

add_subdirectory(common)
add_subdirectory(drivers)
add_subdirectory(core)

if (ENABLE_SCRIPTING_ABILITY)
    add_subdirectory(scripting)
endif()

add_subdirectory(debugger)
add_subdirectory(console)