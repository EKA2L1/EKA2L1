add_library(package
        include/package/manager.h
        include/package/sis_script_interpreter.h
        include/package/sis_v1_installer.h
        src/manager.cpp
        src/sis_script_interpreter.cpp
        src/sis_v1_installer.cpp)

target_include_directories(package PUBLIC include)

target_link_libraries(package PUBLIC common)
target_link_libraries(package PRIVATE config epocloader epocio yaml-cpp)
