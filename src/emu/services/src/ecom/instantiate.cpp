/*
 * Copyright (c) 2019 EKA2L1 Team
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

#include <epoc/epoc.h>
#include <kernel/libmanager.h>
#include <kernel/process.h>
#include <services/ecom/common.h>
#include <services/ecom/ecom.h>
#include <services/fs/std.h>
#include <utils/dll.h>
#include <common/uid.h>
#include <vfs/vfs.h>

#include <utils/err.h>

namespace eka2l1 {
    bool ecom_server::get_implementation_dll_info(kernel::thread *requester, const epoc::uid interface_uid,
        const epoc::uid implementation_uid, epoc::fs::entry &dll_entry, epoc::uid &dtor_key, std::int32_t *err, const bool check_cap_comp) {
        if (implementation_uid == 0) {
            return false;
        }

        dtor_key = implementation_uid;

        // Find the implementation
        ecom_implementation_info_ptr impl_info = nullptr;

        if (interface_uid == 0) {
            // Find the implementation uid in the list
            auto result = std::lower_bound(implementations.begin(), implementations.end(), implementation_uid,
                [=](const ecom_implementation_info_ptr &impl, const epoc::uid &rhs) { return impl->uid < rhs; });

            if (result == implementations.end() || (*result)->uid != implementation_uid) {
                if (err) {
                    *err = epoc::ecom_no_registration_identified;
                }

                return false;
            }

            impl_info = *result;
        } else {
            auto interface = get_interface(interface_uid);

            if (!interface) {
                if (err) {
                    *err = epoc::ecom_no_interface_identified;
                }

                return false;
            }

            auto result = std::lower_bound(interface->implementations.begin(), interface->implementations.end(), implementation_uid,
                [](const ecom_implementation_info_ptr &impl, const epoc::uid &rhs) { return impl->uid < rhs; });

            if (result == interface->implementations.end() || (*result)->uid != implementation_uid) {
                if (err) {
                    *err = epoc::ecom_no_registration_identified;
                }

                return false;
            }

            impl_info = *result;
        }

        std::u16string name = std::u16string(1, drive_to_char16(impl_info->drv)) + u":\\sys\\bin\\" + impl_info->original_name + u".dll";

        if (!(impl_info->flags & ecom_implementation_info::FLAG_IMPL_CREATE_INFO_CACHED)) {
            if (!epoc::get_image_info(sys->get_lib_manager(), name, impl_info->plugin_dll_info)) {
                if (err) {
                    *err = epoc::error_not_found;
                }

                return false;
            }

            // Cache the info
            impl_info->flags |= ecom_implementation_info::FLAG_IMPL_CREATE_INFO_CACHED;
        }

        // Must satisfy the capabilities
        if (check_cap_comp && !requester->owning_process()->has(*reinterpret_cast<epoc::capability_set *>(impl_info->plugin_dll_info.caps))) {
            if (err) {
                *err = epoc::error_permission_denied;
            }

            return false;
        }

        dll_entry.uid1 = impl_info->plugin_dll_info.uid1;
        dll_entry.uid2 = impl_info->plugin_dll_info.uid2;
        dll_entry.uid3 = impl_info->plugin_dll_info.uid3;
        dll_entry.name = name;

        dll_entry.modified = epoc::time{ sys->get_io_system()->get_entry_info(name)->last_write };

        return true;
    }

    void ecom_session::do_get_resolved_impl_creation_method(service::ipc_context *ctx) {
        std::optional<epoc::uid_type> uids = ctx->get_argument_data_from_descriptor<epoc::uid_type>(0);

        if (!uids) {
            ctx->complete(epoc::error_argument);
            return;
        }

        std::string match_str;
        std::vector<std::uint32_t> given_extended_interfaces;

        {
            std::string arg2_data;

            if (auto arg2_data_op = ctx->get_argument_value<std::string>(1)) {
                arg2_data = std::move(arg2_data_op.value());
            } else {
                ctx->complete(epoc::error_argument);
                return;
            }

            if (!unpack_match_str_and_extended_interfaces(arg2_data, match_str, given_extended_interfaces)) {
                ctx->complete(epoc::error_argument);
                return;
            }
        }

        epoc::fs::entry lib_entry{};
        epoc::uid dtor_key{ 0 };

        switch (ctx->msg->function) {
        case ecom_get_implementation_creation_method: {
            std::int32_t error_code = 0;

            if (!server<ecom_server>()->get_implementation_dll_info(ctx->msg->own_thr, 0, (*uids)[epoc::ecom_impl_uid_index],
                    lib_entry, dtor_key, &error_code, false)) {
                ctx->complete(error_code);
                return;
            }

            break;
        }

        default: {
            LOG_ERROR("Unimplemented get creation method op: 0x{:X}", ctx->msg->function);
            return;
        }
        }

        // Write entry and more infos
        ctx->write_data_to_descriptor_argument<epoc::fs::entry>(3, lib_entry);

        epoc::uid_type dtor_uids{ 0, dtor_key, 0 };
        ctx->write_data_to_descriptor_argument<epoc::uid_type>(0, dtor_uids);

        ctx->complete(epoc::error_none);
    }
}
