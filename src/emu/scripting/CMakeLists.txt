include(FileUtils)

add_library(symemu
        include/scripting/codeseg.h
        include/scripting/cpu.h
        include/scripting/emulog.h
        include/scripting/instance.h
        include/scripting/process.h
        include/scripting/thread.h
        include/scripting/manager.h
        include/scripting/mem.h
        include/scripting/message.h
        include/scripting/hook.h
        include/scripting/server.h
        include/scripting/session.h
        include/scripting/symemu.inl
        src/codeseg.cpp
        src/cpu.cpp
        src/emulog.cpp
        src/instance.cpp
        src/manager.cpp
        src/message.cpp
        src/mem.cpp
        src/hook.cpp
        src/process.cpp
        src/thread.cpp
        src/server.cpp
        src/session.cpp)

target_include_directories(symemu PUBLIC include ${EPOC32_INCLUDE_DIR})
target_link_libraries(symemu PRIVATE common 
        epoc
        epockern
        epocio
        epocmem
        epocutils
        epocservs)

if (EKA2L1_ENABLE_SCRIPTING_ABILITY)
    if (NOT ANDROID)
        target_link_libraries(symemu PUBLIC pybind11 pybind11::embed)
    endif()

    target_link_libraries(symemu PUBLIC liblua)
endif (EKA2L1_ENABLE_SCRIPTING_ABILITY)

if (UNIX OR APPLE)
    target_link_libraries(symemu PRIVATE epocutils)
endif ()

# Move the syemu to script folder
set_target_properties(symemu PROPERTIES OUTPUT_NAME symemu
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>"
        LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/$<0:>"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/$<0:>"
        LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin/$<0:>")

add_assets(symemu ${CMAKE_CURRENT_SOURCE_DIR}/python/example scripts/example)
add_assets(symemu ${CMAKE_CURRENT_SOURCE_DIR}/lua/symemu scripts/symemu)

if (NOT ANDROID)
    pybind11_add_module(symemu_common
            include/scripting/common/compression.h
            include/scripting/common/loader.h
            src/common/compression.cpp
            src/common/loader.cpp
            src/common/register.cpp)

    target_include_directories(symemu_common PUBLIC include)

    target_link_libraries(symemu_common PRIVATE common epocio epocservs pybind11)

    if (UNIX OR APPLE)
        target_link_libraries(symemu_common PRIVATE epocutils)
    endif ()

    set_target_properties(symemu_common PROPERTIES OUTPUT_NAME common
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/scripts/symemu2"
            LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/scripts/symemu2"
            LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/scripts/symemu2"
            LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin/scripts/symemu2")

    add_assets(symemu_common ${CMAKE_CURRENT_SOURCE_DIR}/python/symemu2 .)
endif()