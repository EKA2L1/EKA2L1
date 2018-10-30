add_library(arm
    ${CORE_INCLUDE_DIR}/arm/jit_dynarmic.h
    ${CORE_INCLUDE_DIR}/arm/jit_factory.h
	${CORE_INCLUDE_DIR}/arm/jit_interface.h
    ${CORE_INCLUDE_DIR}/arm/jit_unicorn.h
    ${CORE_INCLUDE_DIR}/arm/jit_utils.h
    jit_dynarmic.cpp
    jit_factory.cpp
    jit_unicorn.cpp
    jit_utils.cpp)

target_include_directories(arm PUBLIC ${CORE_OUTER_INCLUDE_DIR})

target_link_libraries(arm PUBLIC common)
target_link_libraries(arm 
	PRIVATE 
	coremem 
    coretiming
    corekernel
    debugger
	disasm
    dynarmic
    gdbstub
    unicorn
    pybind11::embed)
