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

        if (process_name16->find(u".exe") == std::string::npos) {
            // Add the executable extension
            *process_name16 += u".exe";
        }

        eka2l1::symfile f = ctx.sys->get_io_system()->open_file(*process_name16, READ_MODE | BIN_MODE);

        if (!f) {
            ctx.set_request_status(KErrNotFound);
            return;
        }

        auto temp = loader::parse_eka2img(f, true);

        if (!temp) {
            f->seek(0, eka2l1::file_seek_mode::beg);
            auto romimg = loader::parse_romimg(f, ctx.sys->get_memory_system());

            if (romimg && romimg->header.uid3 == info->uid3) {
                loader::romimg_ptr img_ptr = ctx.sys->get_lib_manager()->load_romimg(*process_name16, false);
                ctx.sys->get_lib_manager()->open_romimg(img_ptr);

                int32_t stack_size_img = img_ptr->header.stack_size;
                img_ptr->header.stack_size = std::min(img_ptr->header.stack_size, info->min_stack_size);

                /* Create process through kernel system. */
                uint32_t handle = ctx.sys->get_kernel_system()->create_process(info->uid3, name_process,
                    *process_name16, *process_args, img_ptr, static_cast<kernel::process_priority>(img_ptr->header.priority),
                    static_cast<kernel::owner_type>(info->owner_type));

                img_ptr->header.stack_size = stack_size_img;

                f->close();

                LOG_TRACE("Spawned process: {}, entry point: 0x{:x}", name_process, img_ptr->header.entry_point);

                info->handle = handle;
                ctx.write_arg_pkg(0, *info);

                ctx.set_request_status(KErrNone);
                return;
            }

            f->close();

            ctx.set_request_status(KErrUnknown);
            return;
        }

        auto res2 = ctx.sys->get_lib_manager()->load_e32img(*process_name16);

        if (!res2) {
            ctx.set_request_status(KErrUnknown);
            f->close();

            return;
        }

        ctx.sys->get_lib_manager()->open_e32img(res2);
        ctx.sys->get_lib_manager()->patch_hle();

        /* Create process through kernel system */
        int32_t stack_size_img = res2->header.stack_size;
        res2->header.stack_size = std::min(res2->header.stack_size, (uint32_t)info->min_stack_size);

        /* Create process through kernel system. */
        uint32_t handle = ctx.sys->get_kernel_system()->create_process(info->uid3, name_process,
            *process_name16, *process_args, res2, static_cast<kernel::process_priority>(res2->header.priority),
            static_cast<kernel::owner_type>(info->owner_type));

        res2->header.stack_size = stack_size_img;

        f->close();

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

    loader_server::loader_server(system *sys)
        : service::server(sys, "!Loader") {
        REGISTER_IPC(loader_server, load_process, ELoadProcess, "Loader::LoadProcess");
        REGISTER_IPC(loader_server, load_library, ELoadLibrary, "Loader::LoadLibrary");
    }
}