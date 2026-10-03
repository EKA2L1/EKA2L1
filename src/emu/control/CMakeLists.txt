add_library(control
        include/control/dispatcher.h
        include/control/endpoint.h
        include/control/params.h
        include/control/rpc_server.h
        src/dispatcher.cpp
        src/endpoint.cpp
        src/params.cpp
        src/rpc_server.cpp)

target_include_directories(control PUBLIC include)

target_link_libraries(control PUBLIC
        rapidjson)

target_link_libraries(control PRIVATE
        common
        uvw)
