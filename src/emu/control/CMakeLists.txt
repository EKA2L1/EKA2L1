add_library(control
        include/control/dispatcher.h
        include/control/endpoint.h
        include/control/frontend.h
        include/control/keys.h
        include/control/new_file.h
        include/control/params.h
        include/control/rpc_server.h
        include/control/server.h
        src/context.cpp
        src/context.h
        src/dispatcher.cpp
        src/endpoint.cpp
        src/keys.cpp
        src/methods_apps.cpp
        src/methods_emulator.cpp
        src/methods_events.cpp
        src/methods_input.cpp
        src/methods_screen.cpp
        src/new_file.cpp
        src/params.cpp
        src/rpc_server.cpp
        src/server.cpp)

target_include_directories(control PUBLIC include)

target_link_libraries(control PUBLIC
        rapidjson)

target_link_libraries(control PRIVATE
        common
        config
        drivers
        epoc
        epockern
        epocpkg
        epocservs
        epocutils
        stb
        uvw)
