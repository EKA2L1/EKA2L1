add_library(arm
        include/arm/arm_analyser.h
        include/arm/arm_analyser_capstone.h
        include/arm/arm_dynarmic.h
        include/arm/arm_factory.h
        include/arm/arm_interface.h
        include/arm/arm_interface_extended.h
        include/arm/arm_unicorn.h
        include/arm/arm_utils.h
        src/arm_analyser_capstone.cpp
        src/arm_analyser.cpp
        src/arm_dynarmic.cpp
        src/arm_factory.cpp
        src/arm_interface_extended.cpp
        src/arm_unicorn.cpp
        src/arm_utils.cpp)

target_include_directories(arm PUBLIC include)

target_link_libraries(arm PUBLIC common)
target_link_libraries(arm
        PRIVATE
        epocmem
        epockern
        debugger
        disasm
        dynarmic
        gdbstub
        unicorn
        manager)