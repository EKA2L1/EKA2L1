
add_library(epocio
        include/vfs/vfs.h
        src/vfs.cpp)

target_include_directories(epocio PUBLIC include)

target_link_libraries(epocio PUBLIC common)
target_link_libraries(epocio PRIVATE
        epocmem epocloader)
