add_library(corearm
	${CORE_INCLUDE_DIR}/arm/jit_interface.h
	${CORE_INCLUDE_DIR}/arm/jit_unicorn.h
        ${CORE_INCLUDE_DIR}/core_arm.h
        jit_unicorn.cpp
        ../core_arm.cpp)

target_include_directories(corearm PUBLIC ${CORE_INCLUDE_DIR})

target_link_libraries(corearm PUBLIC common)
target_link_libraries(corearm PRIVATE unicorn)
