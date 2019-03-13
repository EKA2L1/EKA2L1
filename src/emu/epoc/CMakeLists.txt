# Loader for EPOC image, etc...
add_library(epocloader
    include/epoc/loader/e32img.h
    include/epoc/loader/mbm.h
    include/epoc/loader/romimage.h
    include/epoc/loader/rsc.h
    include/epoc/loader/spi.h
    src/loader/e32img.cpp
    src/loader/mbm.cpp
    src/loader/romimage.cpp
    src/loader/rsc.cpp
    src/loader/spi.cpp
)

# The kernel
add_library(epockern
    include/epoc/kernel/change_notifier.h
    include/epoc/kernel/chunk.h
    include/epoc/kernel/codeseg.h
    include/epoc/kernel/libmanager.h
    include/epoc/kernel/library.h
    include/epoc/kernel/kernel_obj.h
    include/epoc/kernel/mutex.h
    include/epoc/kernel/object_ix.h
    include/epoc/kernel/process.h
    include/epoc/kernel/scheduler.h
    include/epoc/kernel/sema.h
    include/epoc/kernel/thread.h
    include/epoc/kernel/timer.h
    include/epoc/hal.h
    include/epoc/kernel.h
    include/epoc/timing.h
    include/epoc/reg.h
    include/epoc/svc.h
    include/epoc/ptr.h
    src/kernel/change_notifier.cpp
    src/kernel/chunk.cpp
    src/kernel/codeseg.cpp
    src/kernel/libmanager.cpp
    src/kernel/library.cpp
    src/kernel/kernel_obj.cpp
    src/kernel/mutex.cpp
    src/kernel/object_ix.cpp
    src/kernel/process.cpp
    src/kernel/scheduler.cpp
    src/kernel/sema.cpp
    src/kernel/thread.cpp
    src/kernel/timer.cpp
    src/hal.cpp
    src/kernel.cpp
    src/timing.cpp
    src/reg.cpp
    src/svc.cpp
    src/ptr.cpp
)

add_library(epocmem
    include/epoc/mem.h
    include/epoc/page_table.h
    src/mem.cpp
    src/page_table.cpp)

add_library(epocservs 
    include/epoc/services/context.h
    include/epoc/services/framework.h
    include/epoc/services/init.h
    include/epoc/services/session.h
    include/epoc/services/server.h
    include/epoc/services/property.h
    include/epoc/services/applist/op.h
    include/epoc/services/applist/applist.h
    include/epoc/services/backup/backup.h
    include/epoc/services/centralrepo/centralrepo.h
    include/epoc/services/centralrepo/common.h
    include/epoc/services/centralrepo/repo.h
    include/epoc/services/domain/database.h
    include/epoc/services/domain/defs.h
    include/epoc/services/domain/domain.h
    include/epoc/services/drm/helper.h
    include/epoc/services/drm/rights.h
    include/epoc/services/ecom/ecom.h
    include/epoc/services/ecom/plugin.h
    include/epoc/services/fbs/bitmap.h
    include/epoc/services/fbs/fbs.h
    include/epoc/services/fbs/font.h
    include/epoc/services/featmgr/featmgr.h
    include/epoc/services/fs/fs.h
    include/epoc/services/loader/loader.h
    include/epoc/services/install/install.h
    include/epoc/services/loader/op.h
    include/epoc/services/posix/posix.h
    include/epoc/services/posix/op.h
    include/epoc/services/sms/sa/sa.h
    include/epoc/services/ui/eikappui.h
    include/epoc/services/ui/oom_app.h
    include/epoc/services/window/bitmap_cache.h
    include/epoc/services/window/window.h
    include/epoc/ipc.h
    src/services/context.cpp
    src/services/framework.cpp
    src/services/init.cpp
    src/services/property.cpp
    src/services/server.cpp
    src/services/session.cpp
    src/services/applist/applist.cpp
    src/services/backup/backup.cpp
    src/services/centralrepo/centralrepo.cpp
    src/services/centralrepo/cre.cpp
    src/services/centralrepo/repo.cpp
    src/services/domain/domain.cpp
    src/services/drm/helper.cpp
    src/services/drm/rights.cpp
    src/services/ecom/ecom.cpp
    src/services/ecom/plugin.cpp
    src/services/fbs/fbs.cpp
    src/services/fbs/impls/bitmap.cpp
    src/services/fbs/impls/font.cpp
    src/services/featmgr/featmgr.cpp
    src/services/fs/dirs.cpp
    src/services/fs/drives.cpp
    src/services/fs/files.cpp
    src/services/fs/fs.cpp
    src/services/install/install.cpp
    src/services/loader/loader.cpp
    src/services/posix/posix.cpp
    src/services/sms/sa/sa.cpp
    src/services/ui/eikappui.cpp
    src/services/ui/oom_app.cpp
    src/services/window/classes/plugins/animdll.cpp
    src/services/window/classes/plugins/clickdll.cpp
    src/services/window/classes/plugins/sprite.cpp
    src/services/window/classes/gctx.cpp
    src/services/window/classes/scrdvc.cpp
    src/services/window/classes/winbase.cpp
    src/services/window/classes/wingroup.cpp
    src/services/window/classes/winuser.cpp
    src/services/window/classes/wsobj.cpp
    src/services/window/bitmap.cpp
    src/services/window/bitmap_cache.cpp
    src/services/window/common.cpp
    src/services/window/fifo.cpp
    src/services/window/window.cpp
    src/ipc.cpp
)

add_library(epocio
    include/epoc/vfs.h
    include/epoc/loader/rom.h
    src/vfs.cpp
    src/loader/rom.cpp)

add_library(epocutils
    include/epoc/utils/des.h
    include/epoc/utils/handle.h
    include/epoc/utils/panic.h
    include/epoc/utils/dll.h
    include/epoc/utils/reqsts.h
    include/epoc/utils/sec.h
    include/epoc/utils/tl.h
    include/epoc/utils/obj.h
    src/utils/des.cpp
    src/utils/dll.cpp
    src/utils/panic.cpp
    src/utils/reqsts.cpp
    src/utils/sec.cpp
    src/utils/tl.cpp
    src/utils/obj.cpp
)

target_include_directories(epocloader PUBLIC include)
target_include_directories(epocmem PUBLIC include)
target_include_directories(epockern PUBLIC include)
target_include_directories(epocservs PUBLIC include)
target_include_directories(epocutils PUBLIC include)
target_include_directories(epocio PUBLIC include)

target_link_libraries(epocio PUBLIC common)
target_link_libraries(epocio PRIVATE
    epocmem
)

target_link_libraries(epocmem PUBLIC common)
target_link_libraries(epocmem PRIVATE
    arm
    epockern)

target_include_directories(epocloader PUBLIC
    ${YAML_INCLUDE_DIR})
    
target_link_libraries(epocloader
    PRIVATE
    common
    epocio
    miniz
)

target_link_libraries(epocutils PUBLIC common)
target_link_libraries(epocutils PRIVATE 
    epockern
    epocservs
    drivers
    hle
    manager)

target_include_directories(epocservs PUBLIC
    ${EPOC32_INCLUDE_DIR})
    
target_link_libraries(epocservs PRIVATE
    common
    epockern
    epocutils
    drivers
    manager
    stb
    xxHash)
    
# TODO: Make arm dependency private.
target_link_libraries(epockern PUBLIC 
    arm common)

target_link_libraries(epockern PRIVATE
    epocloader
    epocmem
    epocservs
    drivers
    manager
    hle
    tz
)

if (ENABLE_SCRIPTING_ABILITY)
    target_link_libraries(epockern PRIVATE manager)
endif()

add_library(epoc
    include/epoc/epoc.h
    src/epoc.cpp)

target_include_directories(epoc PUBLIC include)
target_link_libraries(epoc PRIVATE
    arm
    common
    disasm
    drivers
    epockern
    epocio
    epocloader
    epocservs
    hle
    gdbstub
    manager)
