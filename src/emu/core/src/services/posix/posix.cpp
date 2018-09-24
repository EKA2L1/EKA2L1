/*
* Copyright (c) 2018 EKA2L1 Team
*
* This file is part of EKA2L1 project
* (see bentokun.github.com/EKA2L1).
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <core/services/posix/op.h>
#include <core/services/posix/posix.h>

#include <common/cvt.h>
#include <common/e32inc.h>
#include <common/path.h>

#include <core/core.h>
#include <e32err.h>

#include <experimental/filesystem>

/* These are in SDKS for ucrt on Windows SDK */
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace fs = std::experimental::filesystem;

#define POSIX_REQUEST_FINISH_WITH_ERR(ctx, err)          \
    params->ret = err;                                   \
    ctx.write_arg_pkg<int>(0, err);                      \
    ctx.write_arg_pkg<eka2l1::posix_params>(1, *params); \
    ctx.set_request_status(KErrNone);                    \
    return

#define POSIX_REQUEST_FINISH(ctx)                        \
    params->ret = 0;                                     \
    ctx.write_arg_pkg<int>(0, 0);                        \
    ctx.write_arg_pkg<eka2l1::posix_params>(1, *params); \
    ctx.set_request_status(KErrNone);                    \
    return

#define POSIX_REQUEST_INIT(ctx)                                \
    auto params = ctx.get_arg_packed<eka2l1::posix_params>(1); \
    if (!params) {                                             \
        POSIX_REQUEST_FINISH_WITH_ERR(ctx, EINVAL);            \
    }

namespace eka2l1 {
    void posix_server::chdir(service::ipc_context ctx) {
        POSIX_REQUEST_INIT(ctx);

        working_dir = std::move(fs::absolute(
            params->cwptr[0].get(ctx.sys->get_memory_system()), working_dir)
                                    .u16string());

        POSIX_REQUEST_FINISH(ctx);
    }

    void posix_server::mkdir(service::ipc_context ctx) {
        POSIX_REQUEST_INIT(ctx);

        // Get the absolute path
        const std::u16string full_new_path = std::move(fs::absolute(
            params->cwptr[0].get(ctx.sys->get_memory_system()), working_dir)
                                                           .u16string());

        const std::string path_utf8 = ctx.sys->get_io_system()->get(
            common::ucs2_to_utf8(full_new_path));

        // Take advantage of the standard filesystem, which give back the error code
        std::error_code code;
        fs::create_directory(path_utf8, code);

        POSIX_REQUEST_FINISH_WITH_ERR(ctx, code.value());
    }

    posix_server::posix_server(eka2l1::system *sys, process_ptr &associated_process)
        : service::server(sys, std::string("Posix-") + common::to_string(associated_process->unique_id()), true) {
        working_dir = common::utf8_to_ucs2(eka2l1::root_dir(common::ucs2_to_utf8(associated_process->get_exe_path()), true));

        REGISTER_IPC(posix_server, chdir, PMchdir, "Posix::Chdir");
        REGISTER_IPC(posix_server, mkdir, PMmkdir, "Posix::Mkdir");
    }
}