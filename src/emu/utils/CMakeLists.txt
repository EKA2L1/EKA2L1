

add_library(epocutils
        include/utils/bafl.h
        include/utils/des.h
        include/utils/handle.h
        include/utils/panic.h
        include/utils/dll.h
        include/utils/reqsts.h
        include/utils/sec.h
        include/utils/obj.h
        src/bafl.cpp
        src/des.cpp
        src/panic.cpp
        src/reqsts.cpp
        src/sec.cpp
        src/obj.cpp)

target_include_directories(epocutils PUBLIC include)

target_link_libraries(epocutils PUBLIC common epocmem)
target_link_libraries(epocutils PRIVATE
        epocio
        yaml-cpp)
