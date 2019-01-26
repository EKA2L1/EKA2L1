if (BUILD_JIT_LLVM)
    set (LLVM_JIT_FILES
        include/arm/arm_llvm_recompiler.h
        src/arm_llvm_recompiler.cpp)
endif()

add_library(arm
    include/arm/arm_analyser.h
    include/arm/arm_analyser_capstone.h
    include/arm/arm_dynarmic.h
    include/arm/arm_factory.h
	include/arm/arm_interface.h
    include/arm/arm_unicorn.h
    include/arm/arm_utils.h
    src/arm_analyser_capstone.cpp
    src/arm_dynarmic.cpp
    src/arm_factory.cpp
    src/arm_unicorn.cpp
    src/arm_utils.cpp
    ${LLVM_JIT_FILES})

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

if (BUILD_JIT_LLVM)
    target_link_libraries(arm PRIVATE external::llvm)
endif()