

add_library(epocutils
        include/utils/guest/actsched.h
        include/utils/apacmd.h
        include/utils/bafl.h
        include/utils/cardinality.h
        include/utils/cppabi.h
        include/utils/des.h
        include/utils/handle.h
        include/utils/uchar.h
        include/utils/obj.h
        include/utils/panic.h
        include/utils/dll.h
        include/utils/reqsts.h
        include/utils/sec.h
        src/guest/actsched.cpp
        src/apacmd.cpp
        src/bafl.cpp
        src/cardinality.cpp
        src/cppabi.cpp
        src/des.cpp
        src/obj.cpp
        src/panic.cpp
        src/reqsts.cpp
        src/sec.cpp
        src/uchar.cpp)

target_include_directories(epocutils PUBLIC include)

target_link_libraries(epocutils PUBLIC common epocmem)
target_link_libraries(epocutils PRIVATE
        epocio
        yaml-cpp)
