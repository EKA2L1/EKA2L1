add_library(abi 
    ${CORE_INCLUDE_DIR}/abi/eabi.h
    eabi.cpp)

target_include_directories(abi PUBLIC ${CORE_INCLUDE_DIR}/abi)
target_link_libraries(abi PRIVATE common)