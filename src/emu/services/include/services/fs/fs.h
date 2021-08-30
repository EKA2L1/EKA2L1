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

#pragma once

#include <kernel/server.h>
#include <services/context.h>
#include <services/framework.h>
#include <utils/des.h>

#include <mem/ptr.h>

#include <atomic>
#include <clocale>
#include <memory>
#include <regex>
#include <unordered_map>

namespace eka2l1::kernel {
    using uid = std::uint64_t;
}

namespace eka2l1::service {
    class property;
}

namespace eka2l1 {
    namespace epoc {
        struct security_policy;
    }

    namespace epoc::fs {
        std::string get_server_name_through_epocver(const epocver ver);
    }

    struct entry_info;

    static constexpr std::uint32_t FS_UID = 0x100039E3;
    static constexpr std::uint32_t SYSTEM_DRIVE_KEY = 0x10283049;
    static constexpr std::uint32_t LEX_COMPONENTS = 0x4;
    static constexpr std::uint32_t DEFAULT_DRIVE_NUM = 0x7FFFFFFF;

    class io_system;

    struct file;
    using symfile = std::unique_ptr<file>;

    struct io_component;
    using io_component_ptr = std::unique_ptr<io_component>;

    enum class fs_file_attrib_flag {
        exclusive = 1 << 0,
        share_read = 1 << 1,
        share_write = 1 << 2,
        share_optional = 1 << 3
    };

    struct file_attrib;

    struct fs_node : public epoc::ref_count_object {
        io_component_ptr vfs_node;
        file_attrib *attrib;

        int mix_mode;
        int open_mode;
        bool temporary = false;

        kernel::uid process{ 0 };

        void deref() override;
    };

    struct fs_path_case_insensitive_hasher {
        size_t operator()(const utf16_str &key) const;
    };

    struct fs_path_case_insensitive_comparer {
        bool operator()(const utf16_str &x, const utf16_str &y) const;
    };

    std::u16string get_full_symbian_path(const std::u16string &session_path, const std::u16string &target_path);
    bool check_path_capabilities_pass(const std::u16string &path, kernel::process *pr, epoc::security_policy &private_policy, epoc::security_policy &sys_policy, epoc::security_policy &resource_policy);

    struct fs_server_client : public service::typical_session {
        std::u16string ss_path;

        fs_node *get_file_node(const int handle) {
            return obj_table_.get<fs_node>(handle);
        }

        explicit fs_server_client(service::typical_server *srv, kernel::uid suid, epoc::version client_version, service::ipc_context *ctx);
        void fetch(service::ipc_context *ctx) override;

        void generic_close(service::ipc_context *ctx);

        void file_open(service::ipc_context *ctx);
        void file_create(service::ipc_context *ctx);
        void file_replace(service::ipc_context *ctx);
        void file_temp(service::ipc_context *ctx);
        void file_flush(service::ipc_context *ctx);
        void file_close(service::ipc_context *ctx);
        void file_duplicate(service::ipc_context *ctx);
        void file_adopt(service::ipc_context *ctx);
        void file_drive(service::ipc_context *ctx);
        void file_name(service::ipc_context *ctx);
        void file_full_name(service::ipc_context *ctx);
        void file_att(service::ipc_context *ctx);
        void file_set_att(service::ipc_context *ctx);

        enum exist_check_mode {
            exist_mode_dont_care = 0,
            exist_mode_neccessary = 1,
            exist_mode_must_not = 2
        };

        void new_file_subsession(service::ipc_context *ctx, const exist_check_mode existence,
            bool overwrite = false, bool temporary = false);

        void file_size(service::ipc_context *ctx);
        void file_set_size(service::ipc_context *ctx);
        void file_modified(service::ipc_context *ctx);
        void file_set_modified(service::ipc_context *ctx);

        void file_seek(service::ipc_context *ctx);
        void file_read(service::ipc_context *ctx);
        void file_write(service::ipc_context *ctx);

        void file_rename(service::ipc_context *ctx);

        void open_dir(service::ipc_context *ctx);
        void read_dir_packed(service::ipc_context *ctx);
        void read_dir(service::ipc_context *ctx);
        void close_dir(service::ipc_context *ctx);

        void session_path(service::ipc_context *ctx);
        void set_session_path(service::ipc_context *ctx);
        void set_session_to_private(service::ipc_context *ctx);
        void create_private_path(service::ipc_context *ctx);

        int new_node(io_system *io, kernel::thread *sender, std::u16string name, int org_mode,
            bool overwrite = false, bool temporary = false);

        void entry(service::ipc_context *ctx);
        void is_file_in_rom(service::ipc_context *ctx);
        void is_valid_name(service::ipc_context *ctx);

        void notify_change_ex(service::ipc_context *ctx);
        void notify_change(service::ipc_context *ctx);
        void notify_change_cancel_ex(service::ipc_context *ctx);
        void notify_change_cancel(service::ipc_context *ctx);

        void mkdir(service::ipc_context *ctx);
        void rmdir(service::ipc_context *ctx);
        void rename(service::ipc_context *ctx);
        void replace(service::ipc_context *ctx);
        void parse(service::ipc_context *ctx);

        void delete_entry(service::ipc_context *ctx);
        void set_entry(service::ipc_context *ctx);
        void set_should_notify_failure(service::ipc_context *ctx);

        void read_file_section(service::ipc_context *ctx);

        void query_drive_info_ext(service::ipc_context *ctx);
        void drive_list(service::ipc_context *ctx);
        void drive(service::ipc_context *ctx);
        void volume(service::ipc_context *ctx);
        void is_file_opened(service::ipc_context *ctx);

        bool is_file_opened_here(const std::u16string &path);

        enum class notify_type {
            entry = 1,
            all = 2,
            file = 4,
            dir = 8,
            attrib = 0x10,
            write = 0x20,
            disk = 0x40
        };

        struct notify_entry {
            std::regex match_pattern;
            notify_type type;
            epoc::notify_info info;
        };

        std::vector<notify_entry> notify_entries;

        void notify(const utf16_str &entry, const notify_type type);
        bool should_notify_failures;
    };

    enum file_wild {
        file_wild_name = 0x01,
        file_wild_ext = 0x02,
        file_wild_either = 0x04,
        file_wild_is_root = 0x08,
        file_wild_is_kmatch_one = 0x10,
        file_wild_is_kmatch_any = 0x20
    };

    struct file_sfield {
        std::uint8_t pos;
        std::uint8_t len;
        std::uint8_t present;
        std::uint8_t filler;
    };

    struct file_parse {
        std::int16_t mod;
        std::int16_t wild;
        file_sfield fields[LEX_COMPONENTS];
        eka2l1::ptr<void> vtable;
        epoc::filename name_buf;
    };

    struct file_attrib {
        std::uint32_t flags{ 0 };
        kernel::uid owner{ 0 };
        std::uint32_t use_count{ 0 };

        bool is_exlusive() const {
            return flags & static_cast<std::uint32_t>(fs_file_attrib_flag::exclusive);
        }

        bool is_readable() const {
            return flags & static_cast<std::uint32_t>(fs_file_attrib_flag::share_read);
        }

        bool is_writeable() const {
            return flags & static_cast<std::uint32_t>(fs_file_attrib_flag::share_write);
        }

        bool is_readonly() const {
            return is_readable() && !is_writeable();
        }

        bool is_readable_and_writeable() const {
            return is_readable() && is_writeable();
        }

        bool is_optional() const {
            return flags & static_cast<std::uint32_t>(fs_file_attrib_flag::share_optional);
        }

        /**
         * \brief    Claim the exclusive for this file, to given process.
         * 
         * If the file attrib is not currently being claimed, the given process will claim it. If the file
         * is claimed by other process, this failed. Else, if the given process already claimed the file,
         * nothing happens. You can only unclaimed when use of this file for the process claimed it reached 0.
         * 
         * \returns  False, if a process already claimed this file.
         */
        bool claim_exclusive(const kernel::uid pr_uid);

        /**
         * \brief Increment usage of the attrib.
         * 
         * This only work with process UID that has already claimed this file attribute, or if this file attribute
         * has not been claimed.
         * 
         * \param pr_uid        UID of the process to increment use. 
         */
        void increment_use(const kernel::uid pr_uid);

        /**
         * \brief    Decrement use count of the given process.
         * 
         * If the given process UID currently exclusively decrement use of this attrib, when it reachs zero,
         * the claim will be freed. Any flags will also be cleared along.
         * 
         * This is used for situation where multiple file handles in a process all claimed for exclusive.
         * 
         * \param pr_uid The UID of target process.
         */
        void decrement_use(const kernel::uid pr_uid);
    };

    class fs_server : public service::typical_server {
        friend struct fs_server_client;

        std::unordered_map<std::u16string, file_attrib, fs_path_case_insensitive_hasher> attribs;
        service::property *system_drive_prop;
        std::u16string default_sys_path;

        void connect(service::ipc_context &ctx) override;
        void disconnect(service::ipc_context &ctx) override;

        void synchronize_driver(service::ipc_context *ctx);
        void private_path(service::ipc_context *ctx);
        void set_default_system_path(service::ipc_context *ctx);
        void get_default_system_path(service::ipc_context *ctx);

        enum {
            FLAG_INITED = 0
        };

        std::uint32_t flags;
        void init();

    public:
        explicit fs_server(system *sys);

        service::uid get_owner_secure_uid() const override {
            return 0x100039E3;
        }

        service::uid get_owner_vendor_uid() const override {
            return 0x100039E3;
        }

        file *get_file(const kernel::uid session_uid, const std::uint32_t handle);
        bool is_file_opened(const std::u16string &path);
    };
}
