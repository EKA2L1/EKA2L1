add_library(abi 
    ${CORE_INCLUDE_DIR}/abi/eabi.h
    eabi.cpp
    mangler.cpp)

target_include_directories(abi PUBLIC ${CORE_OUTER_INCLUDE_DIR})
target_link_libraries(abi PRIVATE common)

if (MSVC)
   target_link_libraries(abi PRIVATE libdemangle)
endif()