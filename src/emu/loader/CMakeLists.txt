add_library(loader
    include/loader/sis.h
    include/loader/sis_script_intepreter.h
    include/loader/sis_fields.h
    src/sis_script_intepreter.cpp
    src/sis_fields.cpp
    src/sis.cpp)

target_include_directories(loader PUBLIC include)
target_link_libraries(loader miniz common)
