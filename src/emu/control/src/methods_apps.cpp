/*
 * Copyright (c) 2026 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
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

#include "context.h"

#include <control/dispatcher.h>
#include <control/frontend.h>
#include <control/params.h>

#include <common/cvt.h>
#include <common/fileutils.h>
#include <dispatch/dispatcher.h>
#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <package/manager.h>
#include <services/applist/applist.h>
#include <system/epoc.h>
#include <utils/apacmd.h>

#include <fmt/format.h>

#include <mutex>
#include <optional>
#include <set>
#include <vector>

namespace eka2l1::control {
    // Bounds the kill loop should a process refuse to die.
    static constexpr int MAX_PROCESSES_KILLED = 256;

    static std::string uid_text(const std::uint32_t uid) {
        return fmt::format("0x{:08X}", uid);
    }

    // The caller holds the kernel lock: the UI thread starts processes too.
    static kernel::process *find_running(kernel_system &kern, const std::uint32_t uid) {
        for (kernel_obj_unq_ptr &obj : kern.get_process_list()) {
            kernel::process *pr = reinterpret_cast<kernel::process *>(obj.get());

            if (pr && (pr->get_uid() == uid) && (pr->get_exit_type() == kernel::entity_exit_type::pending)) {
                return pr;
            }
        }

        return nullptr;
    }

    static std::string utf8_of(const std::u16string &text) {
        return common::ucs2_to_utf8(text);
    }

    // Copies, taken under the list lock: the frontend's UI thread may rescan the list meanwhile.
    static std::vector<apa_app_registry> copy_registrations(applist_server &applist) {
        std::vector<apa_app_registry> &live = applist.get_registerations();

        const std::lock_guard<std::mutex> guard(applist.list_access_mut_);
        return live;
    }

    static std::optional<apa_app_registry> copy_registration(applist_server &applist, const std::uint32_t uid) {
        std::vector<apa_app_registry> &live = applist.get_registerations();

        const std::lock_guard<std::mutex> guard(applist.list_access_mut_);

        for (const apa_app_registry &reg : live) {
            if (reg.mandatory_info.uid == uid) {
                return reg;
            }
        }

        return std::nullopt;
    }

    static void list_apps(system &sys, rapidjson::Value &apps, json_allocator &allocator) {
        kernel_system &kern = require_kernel(sys);
        applist_server &applist = require_applist(sys);

        // Never both locks at once: take the list's, then the kernel's.
        std::vector<apa_app_registry> registrations = copy_registrations(applist);
        std::set<std::uint32_t> running;

        {
            const std::lock_guard<kernel_system> guard(kern);

            for (const apa_app_registry &reg : registrations) {
                if (find_running(kern, reg.mandatory_info.uid)) {
                    running.insert(reg.mandatory_info.uid);
                }
            }
        }

        for (apa_app_registry &reg : registrations) {
            const std::string name = utf8_of(reg.mandatory_info.long_caption.to_std_string(nullptr));
            const std::string short_name = utf8_of(reg.mandatory_info.short_caption.to_std_string(nullptr));
            const std::string executable = utf8_of(reg.mandatory_info.app_path.to_std_string(nullptr));
            const char drive[2] = { static_cast<char>(drive_to_char16(reg.land_drive)), '\0' };

            rapidjson::Value app(rapidjson::kObjectType);
            app.AddMember("uid", reg.mandatory_info.uid, allocator);
            app.AddMember("name", rapidjson::Value(name.c_str(), allocator), allocator);
            app.AddMember("short_name", rapidjson::Value(short_name.c_str(), allocator), allocator);
            app.AddMember("executable", rapidjson::Value(executable.c_str(), allocator), allocator);
            app.AddMember("drive", rapidjson::Value(drive, allocator), allocator);
            app.AddMember("hidden", reg.caps.is_hidden != 0, allocator);
            app.AddMember("running", running.count(reg.mandatory_info.uid) != 0, allocator);

            apps.PushBack(app, allocator);
        }
    }

    static drive_number install_drive(system &sys, const std::optional<std::string> &letter) {
        if (!letter) {
            // The drive the Qt frontend installs to.
            return sys.is_s80_device_active() ? drive_d : drive_e;
        }

        if ((letter->size() != 1) || !std::isalpha(static_cast<unsigned char>((*letter)[0]))) {
            throw rpc_error(error_invalid_params, "Parameter 'drive' must be a drive letter such as \"E\"");
        }

        const drive_number drive = char16_to_drive(static_cast<char16_t>((*letter)[0]));

        if ((drive == drive_z) || !sys.get_io_system()->get_drive_entry(drive)) {
            throw rpc_error(error_invalid_params, "Drive " + *letter + " is not a writable drive of this device");
        }

        return drive;
    }

    void add_app_methods(dispatcher &rpc, context &ctx) {
        rpc.add("apps.list", [&ctx](session &, const rapidjson::Value &, rapidjson::Value &result, json_allocator &allocator) {
            rapidjson::Value apps(rapidjson::kArrayType);

            ctx.run_in_guest([&](system &sys) {
                list_apps(sys, apps, allocator);
            });

            result.AddMember("apps", apps, allocator);
        });

        rpc.add("app.launch", [&ctx](session &, const rapidjson::Value &params, rapidjson::Value &result, json_allocator &allocator) {
            const std::uint32_t uid = require_uid(params, "uid");
            const std::optional<std::string> document = optional_string(params, "document");
            const std::optional<std::string> args = optional_string(params, "args");

            // Let the frontend finish reacting to an earlier exit (the Qt frontend reboots the
            // device) before starting something new.
            ctx.host.settle(ctx.stopping);

            kernel::uid pid = 0;

            ctx.run_in_guest([&](system &sys) {
                kernel_system &kern = require_kernel(sys);
                applist_server &applist = require_applist(sys);
                std::optional<apa_app_registry> reg = copy_registration(applist, uid);

                if (!reg) {
                    throw rpc_error(error_not_found, "No app with UID " + uid_text(uid) + " is installed");
                }

                epoc::apa::command_line cmdline;
                cmdline.launch_cmd_ = document ? epoc::apa::command_open : epoc::apa::command_create;

                if (document) {
                    cmdline.document_name_ = common::utf8_to_ucs2(*document);
                }

                if (args) {
                    cmdline.tail_end_ = *args;
                }

                kernel::uid thread_id = 0;
                kernel::process *app = nullptr;

                {
                    // The same guard the services use when they start processes.
                    const std::lock_guard<kernel_system> guard(kern);

                    if (applist.launch_app(*reg, cmdline, &thread_id, ctx.host.app_exit_callback())) {
                        kernel::thread *main_thread = kern.get_by_id<kernel::thread>(thread_id);
                        app = main_thread ? main_thread->owning_process() : nullptr;
                    }

                    kern.stop_cores_idling();
                }

                if (!app) {
                    throw rpc_error(error_failed, "The app " + uid_text(uid) + " could not be started; the emulator log says why");
                }

                pid = app->unique_id();
                ctx.host.on_app_launched(app);
            });

            result.AddMember("pid", static_cast<std::uint64_t>(pid), allocator);
        });

        rpc.add("app.kill", [&ctx](session &, const rapidjson::Value &params, rapidjson::Value &result, json_allocator &allocator) {
            const std::uint32_t uid = require_uid(params, "uid");
            int killed = 0;

            ctx.run_in_guest([&](system &sys) {
                kernel_system &kern = require_kernel(sys);

                {
                    const std::lock_guard<kernel_system> guard(kern);

                    // Killing one process can free others, so look the next one up afresh each time.
                    while (killed < MAX_PROCESSES_KILLED) {
                        kernel::process *target = find_running(kern, uid);

                        if (!target) {
                            break;
                        }

                        target->kill(kernel::entity_exit_type::kill, u"Kill", 0);
                        killed++;
                    }

                    kern.stop_cores_idling();
                }

                // Destroy what the killed processes owned now, outside the kernel lock, as the
                // emulation loop would: it may be parked by the frontend for a long time.
                if (dispatch::dispatcher *dispatcher = sys.get_dispatcher()) {
                    dispatcher->flush_pending_teardown();
                }
            });

            if (killed == 0) {
                throw rpc_error(error_not_found, "No app with UID " + uid_text(uid) + " is running");
            }

            result.AddMember("killed", killed, allocator);
        });

        rpc.add("package.install", [&ctx](session &, const rapidjson::Value &params, rapidjson::Value &, json_allocator &) {
            const std::string path = require_string(params, "path");
            const std::optional<std::string> drive_letter = optional_string(params, "drive");

            if (!common::exists(path) || common::is_dir(path)) {
                throw rpc_error(error_not_found, "There is no package file at " + path);
            }

            package::installation_result outcome = package::installation_result_invalid;

            ctx.run_in_guest([&](system &sys) {
                manager::packages *packages = sys.get_packages();
                require_kernel(sys);

                if (!packages) {
                    throw rpc_error(error_not_ready, "No device has been booted");
                }

                const drive_number drive = install_drive(sys, drive_letter);

                // Silent: pick defaults instead of asking anyone a question.
                outcome = packages->install_package(common::utf8_to_ucs2(path), drive, nullptr, nullptr, true);

                if (outcome == package::installation_result_success) {
                    require_applist(sys).rescan_registries(sys.get_io_system());
                }
            });

            switch (outcome) {
            case package::installation_result_success:
                ctx.host.on_packages_changed();
                return;

            case package::installation_result_aborted:
                throw rpc_error(error_failed, "The installation of " + path + " was aborted; the emulator log says why");

            default:
                throw rpc_error(error_failed, path + " is not a SIS/SISX package the emulator can install");
            }
        });

        rpc.add("package.remove", [&ctx](session &, const rapidjson::Value &params, rapidjson::Value &, json_allocator &) {
            const std::uint32_t uid = require_uid(params, "uid");

            ctx.run_in_guest([&](system &sys) {
                manager::packages *packages = sys.get_packages();
                require_kernel(sys);

                package::object *installed = packages ? packages->package(uid) : nullptr;

                if (!installed) {
                    throw rpc_error(error_not_found, "No package with UID " + uid_text(uid) + " is installed");
                }

                if (!packages->uninstall_package(*installed)) {
                    throw rpc_error(error_failed, "The package " + uid_text(uid) + " could not be removed; the emulator log says why");
                }

                require_applist(sys).rescan_registries(sys.get_io_system());
            });

            ctx.host.on_packages_changed();
        });
    }
}
