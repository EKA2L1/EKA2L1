#include <core/services/loader/loader.h>
#include <core/services/loader/op.h>

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
                    *process_name16, *process_args, img_ptr, kernel::process_priority::foreground, 
                    static_cast<kernel::owner_type>(info->owner_type));

                img_ptr->header.stack_size = stack_size_img;

                f->close();

                LOG_TRACE("Spawned process: {}, entry point: 0x{:x}", name_process, img_ptr->header.entry_point);

                /* Set request status and returns*/
                ctx.set_request_status(handle);
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
            *process_name16, *process_args, res2, kernel::process_priority::foreground,
            static_cast<kernel::owner_type>(info->owner_type));

        res2->header.stack_size = stack_size_img;

        f->close();

        LOG_TRACE("Spawned process: {}", name_process);
        ctx.set_request_status(handle);
        

        //ctx.set_request_status(24324);
    }

    loader_server::loader_server(system *sys)
        : service::server(sys, "!Loader") {
        REGISTER_IPC(loader_server, load_process, ELoadProcess, "Loader::LoadProcess");
    }
}