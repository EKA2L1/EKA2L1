add_library(corekernel
	${CORE_INCLUDE_DIR}/kernel/thread.h
	${CORE_INCLUDE_DIR}/kernel/mutex.h
        ${CORE_INCLUDE_DIR}/core_kernel.h
        ../core_kernel.cpp)

target_include_directories(corekernel PUBLIC ${CORE_INCLUDE_DIR})

target_link_libraries(corekernel PUBLIC common)
target_link_libraries(corekernel PRIVATE coremem coretiming)
