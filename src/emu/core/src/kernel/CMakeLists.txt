add_library(corekernel
	${CORE_INCLUDE_DIR}/kernel/thread.h
	${CORE_INCLUDE_DIR}/kernel/mutex.h
        ${CORE_INCLUDE_DIR}/kernel/kernel_obj.h
        ${CORE_INCLUDE_DIR}/core_kernel.h
        thread.cpp
        mutex.cpp
        kernel_obj.cpp
        ../core_kernel.cpp)

target_include_directories(corekernel PUBLIC ${CORE_INCLUDE_DIR})

target_link_libraries(corekernel PUBLIC common)
target_link_libraries(corekernel PRIVATE coremem coretiming arm)
