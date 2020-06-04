add_library(cpu
        include/cpu/arm_analyser.h
        include/cpu/arm_analyser_capstone.h
        include/cpu/arm_dynarmic.h
        include/cpu/arm_factory.h
        include/cpu/arm_interface.h
        include/cpu/arm_utils.h
        src/arm_analyser_capstone.cpp
        src/arm_analyser.cpp
        src/arm_dynarmic.cpp
        src/arm_factory.cpp
        src/arm_utils.cpp)

target_include_directories(cpu PUBLIC include)
target_include_directories(cpu PRIVATE ${capstone_INCLUDE_DIRS})

target_link_libraries(cpu PUBLIC common)
target_link_libraries(cpu
        PRIVATE
        capstone-static
        dynarmic)