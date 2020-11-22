add_library(epocldd
        include/ldd/collection.h
        include/ldd/mmcif/consts.h
        include/ldd/mmcif/mmcif.h
        src/collection.cpp
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