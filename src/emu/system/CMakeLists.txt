add_library(epoc
        include/system/installation/firmware.h
        include/system/installation/rpkg.h
        include/system/devices.h
        include/system/epoc.h
        include/system/hal.h
        include/system/software.h
        src/installation/firmware.cpp
        src/installation/rpkg.cpp
        src/devices.cpp
        src/epoc.cpp
        src/hal.cpp
        src/software.cpp)

target_include_directories(epoc PUBLIC include)
target_link_libraries(epoc PRIVATE
        bridge
        cpu
        common
        config
        disasm
        drivers
        epocdispatch
        epocldd
        epockern
        epocio
        epocloader
        epocpkg
        epocservs
        FAT
        gdbstub
        j2me
        miniz
        pugixml)

if (EKA2L1_ENABLE_SCRIPTING_ABILITY)
        target_link_libraries(epoc PUBLIC scripting)
endif()
