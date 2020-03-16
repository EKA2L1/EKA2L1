# Loader for EPOC image, etc...
add_library(epocloader
    include/epoc/loader/e32img.h
    include/epoc/loader/mbm.h
    include/epoc/loader/mif.h
    include/epoc/loader/romimage.h
    include/epoc/loader/rsc.h
    include/epoc/loader/spi.h
    src/loader/e32img.cpp
    src/loader/mbm.cpp
    src/loader/mif.cpp
    src/loader/romimage.cpp
    src/loader/rsc.cpp
    src/loader/spi.cpp
)

# The kernel
add_library(epockern
    include/epoc/kernel/smp/avail.h
    include/epoc/kernel/smp/balancer.h
    include/epoc/kernel/smp/core.h
    include/epoc/kernel/smp/scheduler.h
    include/epoc/kernel/change_notifier.h
    include/epoc/kernel/chunk.h
    include/epoc/kernel/codeseg.h
    include/epoc/kernel/libmanager.h
    include/epoc/kernel/library.h
    include/epoc/kernel/kernel_obj.h
    include/epoc/kernel/msgqueue.h
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
    src/kernel/smp/avail.cpp
    src/kernel/change_notifier.cpp
    src/kernel/chunk.cpp
    src/kernel/codeseg.cpp
    src/kernel/libmanager.cpp
    src/kernel/library.cpp
    src/kernel/kernel_obj.cpp
    src/kernel/msgqueue.cpp
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
    include/epoc/mem/allocator/std_page_allocator.h
    include/epoc/mem/model/multiple/chunk.h
    include/epoc/mem/model/multiple/mmu.h
    include/epoc/mem/model/multiple/process.h
    include/epoc/mem/model/multiple/section.h
    include/epoc/mem/chunk.h
    include/epoc/mem/common.h
    include/epoc/mem/mmu.h
    include/epoc/mem/page.h
    include/epoc/mem/process.h
    src/mem.cpp
    src/mem/allocator/std_page_allocator.cpp
    src/mem/model/multiple/chunk.cpp
    src/mem/model/multiple/mmu.cpp
    src/mem/model/multiple/process.cpp
    src/mem/chunk.cpp
    src/mem/mmu.cpp
    src/mem/page.cpp
    src/mem/process.cpp)

add_library(epocservs 
    include/epoc/services/context.h
    include/epoc/services/faker.h
    include/epoc/services/framework.h
    include/epoc/services/init.h
    include/epoc/services/session.h
    include/epoc/services/server.h
    include/epoc/services/property.h
    include/epoc/services/akn/icon/icon.h
    include/epoc/services/akn/skin/chunk_maintainer.h
    include/epoc/services/akn/skin/icon_cfg.h
    include/epoc/services/akn/skin/ops.h
    include/epoc/services/akn/skin/server.h
    include/epoc/services/akn/skin/settings.h
    include/epoc/services/akn/skin/skn.h
    include/epoc/services/akn/skin/bitmap_store.h
    include/epoc/services/akn/skin/utils.h
    include/epoc/services/applist/op.h
    include/epoc/services/applist/applist.h
    include/epoc/services/audio/keysound/context.h
    include/epoc/services/audio/keysound/keysound.h
    include/epoc/services/audio/keysound/ops.h
    include/epoc/services/audio/keysound/ringtab.h
    include/epoc/services/backup/backup.h
    include/epoc/services/cdl/cdl.h
    include/epoc/services/cdl/common.h
    include/epoc/services/cdl/ops.h
    include/epoc/services/cdl/watcher.h
    include/epoc/services/centralrepo/centralrepo.h
    include/epoc/services/centralrepo/common.h
    include/epoc/services/centralrepo/repo.h
    include/epoc/services/domain/database.h
    include/epoc/services/domain/defs.h
    include/epoc/services/domain/domain.h
    include/epoc/services/drm/helper.h
    include/epoc/services/drm/rights.h
    include/epoc/services/ecom/ecom.h
    include/epoc/services/ecom/hleutils.h
    include/epoc/services/ecom/plugin.h
    include/epoc/services/fbs/adapter/font_adapter.h
    include/epoc/services/fbs/adapter/stb_font_adapter.h
    include/epoc/services/fbs/bitmap.h
    include/epoc/services/fbs/compress_queue.h
    include/epoc/services/fbs/fbs.h
    include/epoc/services/fbs/font.h
    include/epoc/services/fbs/font_atlas.h
    include/epoc/services/fbs/font_store.h
    include/epoc/services/fbs/palette.h
    include/epoc/services/featmgr/featmgr.h
    include/epoc/services/fs/fs.h
    include/epoc/services/hwrm/def.h
    include/epoc/services/hwrm/hwrm.h
    include/epoc/services/hwrm/op.h
    include/epoc/services/hwrm/light/light_data.h
    include/epoc/services/hwrm/light/light_def.h
    include/epoc/services/hwrm/light/light.h
    include/epoc/services/hwrm/vibration/vibration.h
    include/epoc/services/loader/loader.h
    include/epoc/services/install/install.h
    include/epoc/services/loader/op.h
    include/epoc/services/posix/posix.h
    include/epoc/services/posix/op.h
    include/epoc/services/remcon/common.h
    include/epoc/services/remcon/controller.h
    include/epoc/services/remcon/remcon.h
    include/epoc/services/remcon/target.h
    include/epoc/services/sms/sa/sa.h
    include/epoc/services/ui/cap/eiksrv.h
    include/epoc/services/ui/cap/oom_app.h
    include/epoc/services/ui/cap/sgc.h
    include/epoc/services/ui/eikappui.h
    include/epoc/services/ui/view/common.h
    include/epoc/services/ui/view/queue.h
    include/epoc/services/ui/view/view.h
    include/epoc/services/window/bitmap_cache.h
    include/epoc/services/window/scheduler.h
    include/epoc/services/window/screen.h
    include/epoc/services/window/window.h
    include/epoc/ipc.h
    src/services/context.cpp
    src/services/faker.cpp
    src/services/framework.cpp
    src/services/init.cpp
    src/services/property.cpp
    src/services/server.cpp
    src/services/session.cpp
    src/services/akn/icon/icon.cpp
    src/services/akn/icon/init.cpp
    src/services/akn/skin/chunk_maintainer.cpp
    src/services/akn/skin/icon_cfg.cpp
    src/services/akn/skin/ops.cpp
    src/services/akn/skin/server.cpp
    src/services/akn/skin/settings.cpp
    src/services/akn/skin/skn.cpp
    src/services/akn/skin/bitmap_store.cpp
    src/services/akn/skin/utils.cpp
    src/services/applist/applist.cpp
    src/services/applist/registeration.cpp
    src/services/audio/keysound/context.cpp
    src/services/audio/keysound/keysound.cpp
    src/services/backup/backup.cpp
    src/services/cdl/cdl.cpp
    src/services/cdl/common.cpp
    src/services/cdl/observer.cpp
    src/services/cdl/watcher.cpp
    src/services/centralrepo/centralrepo.cpp
    src/services/centralrepo/cre.cpp
    src/services/centralrepo/repo.cpp
    src/services/domain/domain.cpp
    src/services/drm/helper.cpp
    src/services/drm/rights.cpp
    src/services/ecom/ecom.cpp
    src/services/ecom/hleutils.cpp
    src/services/ecom/instantiate.cpp
    src/services/ecom/plugin.cpp
    src/services/fbs/adapter/font_adapter.cpp
    src/services/fbs/adapter/stb_font_adapter.cpp
    src/services/fbs/compress_queue.cpp
    src/services/fbs/fbs.cpp
    src/services/fbs/font_atlas.cpp
    src/services/fbs/impls/bitmap.cpp
    src/services/fbs/impls/font.cpp
    src/services/fbs/impls/font_store.cpp
    src/services/featmgr/featmgr.cpp
    src/services/fs/dirs.cpp
    src/services/fs/drives.cpp
    src/services/fs/files.cpp
    src/services/fs/fs.cpp
    src/services/hwrm/hwrm.cpp
    src/services/hwrm/light/light_data.cpp
    src/services/hwrm/light/light.cpp
    src/services/hwrm/vibration/vibration.cpp
    src/services/install/install.cpp
    src/services/loader/loader.cpp
    src/services/posix/posix.cpp
    src/services/remcon/common.cpp
    src/services/remcon/controller.cpp
    src/services/remcon/remcon.cpp
    src/services/remcon/target.cpp
    src/services/sms/sa/sa.cpp
    src/services/ui/cap/eiksrv.cpp
    src/services/ui/cap/oom_app.cpp
    src/services/ui/cap/sgc.cpp
    src/services/ui/eikappui.cpp
    src/services/ui/view/queue.cpp
    src/services/ui/view/view.cpp
    src/services/window/classes/plugins/animdll.cpp
    src/services/window/classes/plugins/clickdll.cpp
    src/services/window/classes/plugins/sprite.cpp
    src/services/window/classes/dsa.cpp
    src/services/window/classes/gctx.cpp
    src/services/window/classes/scrdvc.cpp
    src/services/window/classes/winbase.cpp
    src/services/window/classes/wingroup.cpp
    src/services/window/classes/winuser.cpp
    src/services/window/classes/wsobj.cpp
    src/services/window/bitmap_cache.cpp
    src/services/window/common.cpp
    src/services/window/fifo.cpp
    src/services/window/scheduler.cpp
    src/services/window/screen.cpp
    src/services/window/window.cpp
    src/ipc.cpp
)

add_library(epocio
    include/epoc/vfs.h
    include/epoc/loader/rom.h
    src/vfs.cpp
    src/loader/rom.cpp)

add_library(epocutils
    include/epoc/utils/bafl.h
    include/epoc/utils/des.h
    include/epoc/utils/handle.h
    include/epoc/utils/panic.h
    include/epoc/utils/dll.h
    include/epoc/utils/reqsts.h
    include/epoc/utils/sec.h
    include/epoc/utils/tl.h
    include/epoc/utils/obj.h
    src/utils/bafl.cpp
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
target_link_libraries(epocmem PRIVATE arm)

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
