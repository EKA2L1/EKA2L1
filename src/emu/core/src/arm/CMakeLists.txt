add_library(arm
	${CORE_INCLUDE_DIR}/arm/jit_interface.h
        ${CORE_INCLUDE_DIR}/arm/jit_unicorn.h
        ${CORE_INCLUDE_DIR}/arm/jit_factory.h
        jit_unicorn.cpp
        jit_factory.cpp)

target_include_directories(arm PUBLIC ${CORE_INCLUDE_DIR})

target_link_libraries(arm PUBLIC common)
target_link_libraries(arm 
	PRIVATE 
	disasm 
	coremem 
	coretiming
	unicorn)
