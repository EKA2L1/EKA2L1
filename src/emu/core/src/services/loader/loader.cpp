#include <core/services/loader/loader.h>
#include <core/services/loader/op.h>

#include <core/epoc/des.h>

#include <core/core.h>
#include <core/vfs.h>

#include <core/loader/romimage.h>

#include <common/cvt.h>
#include <common/e32inc.h>
#include <common/log.h>
#include <common/path.h>

#include <e32err.h>

namespace eka2l1 {
    void loader_server::load_process(eka2l1::service::ipc_context ctx) {
        std::optional<epoc::ldr_info> info = ctx.get_arg_packed<epoc::ldr_info>(0);

        if (!info) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        std::optional<std::u16string> process_name16 = ctx.get_arg<std::u16string>(1);
        std::optional<std::u16string> process_args = ctx.get_arg<std::u16string>(2);

        if (!process_name16 || !process_args) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        std::string name_process = eka2l1::filename(common::ucs2_to_utf8(*process_name16));
        auto eimg = ctx.sys->get_lib_manager()->load_e32img(*process_name16);

        if (!eimg) {
            loader::romimg_ptr img_ptr = ctx.sys->get_lib_manager()->load_romimg(*process_name16, false);

            if (img_ptr && img_ptr->header.uid3 == info->uid3) {
                ctx.sys->get_lib_manager()->open_romimg(img_ptr);

                int32_t stack_size_img = img_ptr->header.stack_size;
                img_ptr->header.stack_size = std::min(img_ptr->header.stack_size, info->min_stack_size);

                /* Create process through kernel system. */
                uint32_t handle = ctx.sys->get_kernel_system()->create_process(info->uid3, name_process,
                    *process_name16, *process_args, img_ptr, static_cast<kernel::process_priority>(img_ptr->header.priority),
                    static_cast<kernel::owner_type>(info->owner_type));

                img_ptr->header.stack_size = stack_size_img;
                LOG_TRACE("Spawned process: {}, entry point: 0x{:x}", name_process, img_ptr->header.entry_point);

                info->handle = handle;
                ctx.write_arg_pkg(0, *info);

                ctx.set_request_status(KErrNone);
                return;
            } 

            ctx.set_request_status(KErrUnknown);
            return;
        }

        ctx.sys->get_lib_manager()->open_e32img(eimg);
        ctx.sys->get_lib_manager()->patch_hle();

        /* Create process through kernel system */
        int32_t stack_size_img = eimg->header.stack_size;
        eimg->header.stack_size = std::min(eimg->header.stack_size, (uint32_t)info->min_stack_size);

        /* Create process through kernel system. */
        uint32_t handle = ctx.sys->get_kernel_system()->create_process(info->uid3, name_process,
            *process_name16, *process_args, eimg, static_cast<kernel::process_priority>(eimg->header.priority),
            static_cast<kernel::owner_type>(info->owner_type));

        eimg->header.stack_size = stack_size_img;

        LOG_TRACE("Spawned process: {}", name_process);

        info->handle = handle;
        ctx.write_arg_pkg(0, *info);

        ctx.set_request_status(KErrNone);
    }

    void loader_server::load_library(service::ipc_context ctx) {
        std::optional<epoc::ldr_info> info = ctx.get_arg_packed<epoc::ldr_info>(0);

        if (!info) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        std::optional<std::u16string> lib_path = ctx.get_arg<std::u16string>(1);

        if (!lib_path) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        std::string lib_name = eka2l1::filename(common::ucs2_to_utf8(*lib_path));

        loader::romimg_ptr img_ptr = ctx.sys->get_lib_manager()->load_romimg(*lib_path, false);

        if (!img_ptr) {
            loader::e32img_ptr e32img_ptr = ctx.sys->get_lib_manager()->load_e32img(*lib_path);

            if (!e32img_ptr) {
                LOG_TRACE("Invalid library provided {}", lib_name);
                ctx.set_request_status(KErrArgument);

                return;
            }

            /* Create process through kernel system. */
            uint32_t handle = ctx.sys->get_kernel_system()->create_library(lib_name,
                e32img_ptr, static_cast<kernel::owner_type>(info->owner_type));

            LOG_TRACE("Loaded library: {}", lib_name);

            info->handle = handle;

            ctx.sys->get_kernel_system()->crr_process()->signal_dll_lock();

            ctx.write_arg_pkg(0, *info);
            ctx.set_request_status(KErrNone);
        } else {
            ctx.sys->get_lib_manager()->open_romimg(img_ptr);

            /* Create process through kernel system. */
            uint32_t handle = ctx.sys->get_kernel_system()->create_library(
                lib_name, img_ptr, static_cast<kernel::owner_type>(info->owner_type));

            LOG_TRACE("Loaded library: {}", lib_name);

            info->handle = handle;

            ctx.write_arg_pkg(0, *info);
            ctx.set_request_status(KErrNone);

            ctx.sys->get_kernel_system()->crr_process()->signal_dll_lock();

            return;
        }
    }

    void loader_server::get_info(service::ipc_context ctx) {
        std::optional<utf16_str> lib_name = ctx.get_arg<utf16_str>(1);
        epoc::TDes8 *buffer = reinterpret_cast<decltype(buffer)>(ctx.msg->own_thr->owning_process()->get_ptr_on_addr_space(ctx.msg->args.args[2]));

        if (!lib_name || !buffer) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        // Check the buffer size
        if (buffer->iMaxLength < sizeof(epoc::lib_info)) {
            ctx.set_request_status(KErrNoMemory);
            return;
        }

        epoc::ldr_info load_info;
        loader::e32img_ptr eimg = ctx.sys->get_lib_manager()->load_e32img(*lib_name);

        if (!eimg) {
            loader::romimg_ptr rimg = ctx.sys->get_lib_manager()->load_romimg(*lib_name);

            if (!rimg) {
                ctx.set_request_status(KErrNotFound);
                return;
            }

            load_info.uid1 = rimg->header.uid1;
            load_info.uid2 = rimg->header.uid2;
            load_info.uid3 = rimg->header.uid3;
            load_info.min_stack_size = rimg->header.heap_minimum_size;
            load_info.secure_id = rimg->header.sec_info.secure_id;

            epoc::lib_info linfo;
            linfo.uid1 = rimg->header.uid1;
            linfo.uid2 = rimg->header.uid2;
            linfo.uid3 = rimg->header.uid3;
            linfo.secure_id = rimg->header.sec_info.secure_id;
            linfo.caps[0] = rimg->header.sec_info.cap1;
            linfo.caps[1] = rimg->header.sec_info.cap2;
            linfo.vendor_id = rimg->header.sec_info.vendor_id;
            linfo.major = rimg->header.major;
            linfo.minor = rimg->header.minor;

            if (buffer->iMaxLength >= sizeof(epoc::lib_info2)) {
                epoc::lib_info2 linfo2;
                memcpy(&linfo2, &linfo, sizeof(epoc::lib_info));

                linfo2.debug_attrib = 1;
                linfo2.hfp = 0;

                ctx.write_arg_pkg(2, reinterpret_cast<uint8_t *>(&linfo2), sizeof(epoc::lib_info2));
            } else {
                ctx.write_arg_pkg(2, reinterpret_cast<uint8_t *>(&linfo), sizeof(epoc::lib_info2));
            }

            ctx.write_arg_pkg<epoc::ldr_info>(0, load_info);
            ctx.set_request_status(KErrNone);

            return;
        }

        memcpy(&load_info.uid1, &eimg->header.uid1, 12);

        load_info.min_stack_size = eimg->header.heap_size_min;
        load_info.secure_id = eimg->header_extended.info.secure_id;

        epoc::lib_info linfo;
        memcpy(&linfo.uid1, &eimg->header.uid1, 12);

        linfo.secure_id = eimg->header_extended.info.secure_id;
        linfo.caps[0] = eimg->header_extended.info.cap1;
        linfo.caps[1] = eimg->header_extended.info.cap2;
        linfo.vendor_id = eimg->header_extended.info.vendor_id;
        linfo.major = eimg->header.major;
        linfo.minor = eimg->header.minor;

        if (buffer->iMaxLength >= sizeof(epoc::lib_info2)) {
            epoc::lib_info2 linfo2;
            memcpy(&linfo2, &linfo, sizeof(epoc::lib_info));

            linfo2.debug_attrib = 1;
            linfo2.hfp = 0;

            ctx.write_arg_pkg(2, reinterpret_cast<uint8_t *>(&linfo2), sizeof(epoc::lib_info2));
        } else {
            ctx.write_arg_pkg(2, reinterpret_cast<uint8_t *>(&linfo), sizeof(epoc::lib_info2));
        } 

        ctx.write_arg_pkg<epoc::ldr_info>(0, load_info);
        ctx.set_request_status(KErrNone);
    }

    loader_server::loader_server(system *sys)
        : service::server(sys, "!Loader", true) {
        REGISTER_IPC(loader_server, load_process, ELoadProcess, "Loader::LoadProcess");
        REGISTER_IPC(loader_server, load_library, ELoadLibrary, "Loader::LoadLibrary");
        REGISTER_IPC(loader_server, get_info, EGetInfo, "Loader::GetInfo");
    }
}