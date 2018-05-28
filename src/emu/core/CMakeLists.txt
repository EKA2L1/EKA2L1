set (CORE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/core)

add_library(coremem
        include/core/core_mem.h
        include/core/ptr.h
        src/core_mem.cpp)

add_library(coretiming
        include/core/core_timing.h
        src/core_timing.cpp)

add_library(coreio
        include/core/vfs.h
        src/vfs.cpp)

add_subdirectory(src/arm)
add_subdirectory(src/loader)
add_subdirectory(src/manager)
add_subdirectory(src/disasm)
add_subdirectory(src/kernel)
#add_subdirectory(src/abi)
add_subdirectory(src/hle)

target_include_directories(coremem PUBLIC include)
target_include_directories(coretiming PUBLIC include)
target_include_directories(coreio PUBLIC include)

target_link_libraries(coremem PUBLIC common)
target_link_libraries(coretiming PUBLIC common)
target_link_libraries(coreio PUBLIC common coreloader)

add_library(core include/core/ptr.h
                 include/core/process.h
                 include/core/core.h
                 src/process.cpp
                 src/core.cpp)

target_link_libraries(core PRIVATE
        simini
        coremem
        corekernel
        coretiming 
        coreio
        arm
        coreloader
        coremanager
		common)

if (UNIX OR APPLE)
        target_link_libraries(core PRIVATE pthread)
endif()

# Disasm on the go in the future
target_link_libraries(core PUBLIC
        disasm
        hle)       
        
target_include_directories(core PUBLIC include)
