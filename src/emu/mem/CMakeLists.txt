add_library(epocmem
        include/mem/mem.h
        include/mem/allocator/std_page_allocator.h
        include/mem/model/flexible/addrspace.h
        include/mem/model/flexible/chnkmngr.h
        include/mem/model/flexible/chunk.h
        include/mem/model/flexible/dirmngr.h
        include/mem/model/flexible/mapping.h
        include/mem/model/flexible/memobj.h
        include/mem/model/flexible/process.h
        include/mem/model/multiple/chunk.h
        include/mem/model/multiple/mmu.h
        include/mem/model/multiple/process.h
        include/mem/model/section.h
        include/mem/chunk.h
        include/mem/common.h
        include/mem/mmu.h
        include/mem/page.h
        include/mem/process.h
        include/mem/ptr.h
        src/mem.cpp
        src/allocator/std_page_allocator.cpp
        src/model/flexible/addrspace.cpp
        src/model/flexible/chunk.cpp
        src/model/flexible/chnkmngr.cpp
        src/model/flexible/dirmngr.cpp
        src/model/flexible/mapping.cpp
        src/model/flexible/memobj.cpp
        src/model/flexible/process.cpp
        src/model/flexible/mmu.cpp
        src/model/multiple/chunk.cpp
        src/model/multiple/mmu.cpp
        src/model/multiple/process.cpp
        src/chunk.cpp
        src/mmu.cpp
        src/page.cpp
        src/process.cpp
        )

target_include_directories(epocmem PUBLIC include)

target_link_libraries(epocmem PUBLIC common)
target_link_libraries(epocmem PRIVATE cpu config)
