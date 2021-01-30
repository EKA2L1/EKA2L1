add_library(epocldd
        include/ldd/collection.h
        include/ldd/ecomm/consts.h
        include/ldd/ecomm/ecomm.h
        include/ldd/mmcif/consts.h
        include/ldd/mmcif/mmcif.h
        src/collection.cpp
        src/ecomm/ecomm.cpp
        src/mmcif/mmcif.cpp
        )

target_include_directories(epocldd PUBLIC include)

target_link_libraries(epocldd
        PRIVATE
        common
        epoc
        epocmem
        epockern
        )