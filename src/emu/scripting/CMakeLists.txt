include(FileUtils)

add_library(scripting
        include/scripting/codeseg.h
        include/scripting/cpu.h
        include/scripting/instance.h
        include/scripting/process.h
        include/scripting/thread.h
        include/scripting/manager.h
        include/scripting/mem.h
        include/scripting/message.h
        include/scripting/server.h
        include/scripting/session.h
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

target_include_directories(scripting PUBLIC include ${EPOC32_INCLUDE_DIR})
target_link_libraries(scripting PRIVATE common 
        epoc
        epockern
        epocio
        epocmem
        epocpkg
        epocutils
        epocservs)

if (EKA2L1_ENABLE_SCRIPTING_ABILITY)
    target_link_libraries(scripting PUBLIC liblua)
endif (EKA2L1_ENABLE_SCRIPTING_ABILITY)

if (UNIX OR APPLE)
    target_link_libraries(scripting PRIVATE epocutils)
endif ()

# Move the syemu to script folder
set_target_properties(scripting PROPERTIES OUTPUT_NAME scripting
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>"
        LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/$<0:>"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/$<0:>"
        LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin/$<0:>")

add_assets(scripting ${CMAKE_CURRENT_SOURCE_DIR}/lua/eka2l1 scripts/eka2l1)