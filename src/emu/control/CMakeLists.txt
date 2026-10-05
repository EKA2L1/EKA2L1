add_library(control
        include/control/dispatcher.h
        include/control/endpoint.h
        include/control/frontend.h
        include/control/params.h
        include/control/rpc_server.h
        include/control/server.h
        src/context.cpp
        src/context.h
        src/dispatcher.cpp
        src/endpoint.cpp
        src/methods_apps.cpp
        src/methods_emulator.cpp
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
        uvw)
