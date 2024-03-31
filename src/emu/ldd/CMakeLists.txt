add_library(epocldd
        include/ldd/collection.h
        include/ldd/ecomm/consts.h
        include/ldd/ecomm/ecomm.h
        include/ldd/ekeyb/ekeyb.h
        include/ldd/hal/hal.h
        include/ldd/mmcif/consts.h
        include/ldd/mmcif/mmcif.h
        include/ldd/oldcamera/oldcamera.h
        include/ldd/videodriver/videodriver.h
        src/collection.cpp
        src/ecomm/ecomm.cpp
        src/ekeyb/ekeyb.cpp
        src/hal/hal.cpp
        src/mmcif/mmcif.cpp
        src/oldcamera/oldcamera.cpp
        src/videodriver/videodriver.cpp
)

target_include_directories(epocldd PUBLIC include)

target_link_libraries(epocldd
        PRIVATE
        common
        epoc
        epocmem
        epockern
        epocpkg
        epocservs
        )