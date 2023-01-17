# Loader for EPOC image, etc...
add_library(epocloader
        include/loader/e32img.h
        include/loader/fpsx.h
        include/loader/gdr.h
        include/loader/mbm.h
        include/loader/mif.h
        include/loader/nvg.h
        include/loader/rofs.h
        include/loader/rom.h
        include/loader/romimage.h
        include/loader/rsc.h
        include/loader/sis_common.h
        include/loader/sis_fields.h
        include/loader/sis_old.h
        include/loader/sis.h
        include/loader/spi.h
        include/loader/svgb.h
        src/e32img.cpp
        src/fpsx.cpp
        src/gdr.cpp
        src/mbm.cpp
        src/mif.cpp
        src/nvg.cpp
        src/rofs.cpp
        src/rom.cpp
        src/romimage.cpp
        src/rsc.cpp
        src/sis_fields.cpp
        src/sis_old.cpp
        src/sis.cpp
        src/spi.cpp
        src/svgb.cpp
        )

target_include_directories(epocloader PUBLIC include)

target_include_directories(epocloader PUBLIC
        ${YAML_INCLUDE_DIR})

target_link_libraries(epocloader
        PRIVATE
        common
        epocmem
        epocutils
        miniz
        glm
        )
