add_library(io 
    include/io/vfs.h
	src/vfs.cpp)

target_include_directories(io PUBLIC include)