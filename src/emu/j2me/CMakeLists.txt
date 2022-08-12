add_library(j2me
    include/j2me/applist.h
    include/j2me/common.h
    include/j2me/interface.h
    include/j2me/kmidrun.h
    src/applist.cpp
    src/common.cpp
    src/interface.cpp
    src/kmidrun.cpp)

target_include_directories(j2me PUBLIC include)
target_link_libraries(j2me PRIVATE common config epocio miniz sqlite3 epoc)