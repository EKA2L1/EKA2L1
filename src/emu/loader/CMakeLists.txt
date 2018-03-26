add_library(loader
    include/loader/sis.h
    include/loader/sis_script_intepreter.h
    src/sis_script_intepreter.cpp
    src/sis.cpp)

target_include_directories(loader PUBLIC include)
