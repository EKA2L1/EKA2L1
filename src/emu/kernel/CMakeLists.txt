
add_library(epoctiming   
        include/kernel/timing.h
        src/timing.cpp)

# The kernel
add_library(epockern
        include/kernel/smp/avail.h
        include/kernel/smp/balancer.h
        include/kernel/smp/core.h
        include/kernel/smp/scheduler.h
        include/kernel/btrace.h
        include/kernel/change_notifier.h
        include/kernel/chunk.h
        include/kernel/codeseg.h
        include/kernel/common.h
        include/kernel/ipc.h
        include/kernel/ldd.h
        include/kernel/libmanager.h
        include/kernel/library.h
        include/kernel/kernel_obj.h
        include/kernel/msgqueue.h
        include/kernel/mutex.h
        include/kernel/object_ix.h
        include/kernel/process.h
        include/kernel/property.h
        include/kernel/scheduler.h
        include/kernel/sema.h
        include/kernel/session.h
        include/kernel/server.h
        include/kernel/thread.h
        include/kernel/timer.h
        include/kernel/kernel.h
        include/kernel/reg.h
        include/kernel/svc.h
        src/smp/avail.cpp
        src/btrace.cpp
        src/change_notifier.cpp
        src/chunk.cpp
        src/codeseg.cpp
        src/ldd.cpp
        src/libmanager.cpp
        src/library.cpp
        src/ipc.cpp
        src/kernel_obj.cpp
        src/msgqueue.cpp
        src/mutex.cpp
        src/object_ix.cpp
        src/process.cpp
        src/scheduler.cpp
        src/sema.cpp
        src/thread.cpp
        src/timer.cpp
        src/kernel.cpp
        src/property.cpp
        src/reg.cpp
        src/server.cpp
        src/session.cpp
        src/svc.cpp
        )

target_include_directories(epoctiming PUBLIC include)
target_include_directories(epockern PUBLIC include)

target_link_libraries(epoctiming PUBLIC
        common)

target_link_libraries(epockern PUBLIC
        bridge
        cpu 
        common
        config
        disasm
        drivers
        epocio
        epocutils)

target_link_libraries(epockern PRIVATE
        epocloader
        epocmem
        epoctiming
        )