add_library(core 
	include/core/core_timing.h
	include/core/core_mem.h
        include/core/ptr.h
	include/core/arm/jit_interface.h
        include/core/core_arm.h
        src/core_timing.cpp
        src/core_mem.cpp
        src/core_arm.cpp)

target_include_directories(core PUBLIC include)
target_link_libraries(core PUBLIC common)
target_link_libraries(core PRIVATE unicorn)
	
