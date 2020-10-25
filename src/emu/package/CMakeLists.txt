add_library(epocpkg
        include/package/manager.h
        include/package/sis_script_interpreter.h
        include/package/sis_v1_installer.h
        src/manager.cpp
        src/sis_script_interpreter.cpp
        src/sis_v1_installer.cpp)

target_include_directories(epocpkg PUBLIC include)

target_link_libraries(epocpkg PUBLIC common)
target_link_libraries(epocpkg PRIVATE config epocloader epocio yaml-cpp)
