# Loader for EPOC image, etc...
add_library(epocloader
        include/loader/e32img.h
        include/loader/mbm.h
        include/loader/mif.h
        include/loader/rom.h
        include/loader/romimage.h
        include/loader/rsc.h
        include/loader/spi.h
        src/e32img.cpp
        src/mbm.cpp
        src/mif.cpp
        src/rom.cpp
        src/romimage.cpp
        src/rsc.cpp
        src/spi.cpp
        )

target_include_directories(epocloader PUBLIC include)

target_include_directories(epocloader PUBLIC
        ${YAML_INCLUDE_DIR})

target_link_libraries(epocloader
        PRIVATE
        common
        epocmem
        miniz
        )