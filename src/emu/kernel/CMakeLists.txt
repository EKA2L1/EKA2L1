
add_library(epoctiming   
        include/kernel/timing.h
        src/timing.cpp)

# The kernel
add_library(epockern
        include/kernel/legacy/mutex.h
        include/kernel/legacy/sema.h
        include/kernel/legacy/sync_object.h
        include/kernel/smp/avail.h
        include/kernel/smp/balancer.h
        include/kernel/smp/core.h
        include/kernel/smp/scheduler.h
        include/kernel/btrace.h
        include/kernel/codedump_collector.h
        include/kernel/guomen_process.h
        include/kernel/change_notifier.h
        include/kernel/chunk.h
        include/kernel/codeseg.h
        include/kernel/common.h
        include/kernel/condvar.h
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
        include/kernel/undertaker.h
        src/legacy/sync_object.cpp
        src/legacy/mutex.cpp
        src/legacy/sema.cpp
        src/smp/avail.cpp
        src/guomen_process.cpp
        src/btrace.cpp
        src/change_notifier.cpp
        src/chunk.cpp
        src/codeseg.cpp
        src/codedump_collector.cpp
        src/condvar.cpp
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
        src/undertaker.cpp
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
        epocutils
        xxHash)

target_link_libraries(epockern PRIVATE
        epocloader
        epocmem
        epoctiming
        re2::re2
        )