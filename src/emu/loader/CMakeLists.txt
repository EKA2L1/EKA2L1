add_library(loader
    include/loader/sis.h
	src/sis.cpp)

target_include_directories(loader PUBLIC include)