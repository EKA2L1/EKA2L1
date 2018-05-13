add_library(abi 
    ${CORE_INCLUDE_DIR}/abi/eabi.h
	 ${CORE_INCLUDE_DIR}/abi/vtable.h
    eabi.cpp
    mangler.cpp
	vtable.cpp)

target_include_directories(abi PUBLIC ${CORE_INCLUDE_DIR})
target_link_libraries(abi PRIVATE common)

if (MSVC)
    target_link_libraries(abi PRIVATE libcxxabi)
endif()