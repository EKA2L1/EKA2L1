add_library(disasm
	${CORE_INCLUDE_DIR}/disasm/disasm.h
        disasm.cpp)

target_include_directories(disasm PUBLIC ${CORE_OUTER_INCLUDE_DIR})
target_include_directories(disasm PUBLIC ${capstone_INCLUDE_DIRS})

target_link_libraries(disasm PRIVATE capstone-static common)
