/*
 * Copyright (c) 2018 EKA2L1 Team.
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

#include <common/container.h>
#include <common/types.h>

#include <kernel/common.h>
#include <mem/ptr.h>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace YAML {
    class Node;
}

namespace eka2l1 {
    class io_system;
    class memory_system;
    class kernel_system;
    class system;

    using sid = uint32_t;
    using sids = std::vector<uint32_t>;
    using exportaddr = uint32_t;
    using exportaddrs = sids;

    typedef uint32_t address;

    namespace common {
        class ro_stream;
    }

    namespace kernel {
        class chunk;
        class process;
        class codeseg;
    }

    using process_ptr = kernel::process *;
    using codeseg_ptr = kernel::codeseg *;

    // Technically, loader is an user application, but here we treat it as kernel

    namespace loader {
        struct e32img;
        struct romimg;

        using e32img_ptr = std::shared_ptr<e32img>;
        using romimg_ptr = std::shared_ptr<romimg>;
    }

    namespace hle {
        static std::uint16_t THUMB_TRAMPOLINE_ASM[] = {
            0xB540, // 0: push { r6, lr }
            0x4E01, // 2: ldr r6, [pc, #4]
            0x47B0, // 4: blx r6
            0xBD40, // 6: pop { r6, pc }
            0x0000, // 8: nop
            // constant here, offset 10: makes that total of 14 bytes
            // Hope some function are big enough!!!
        };

        static std::uint32_t ARM_TRAMPOLINE_ASM[] = {
            0xE51FF004, // 0: ldr pc, [pc, #-4]
            // constant here
            // 8 bytes in total
        };

        using export_table = std::vector<std::uint32_t>;
        using symbols = std::vector<std::string>;
        using patch_route_info = std::pair<std::uint32_t, std::uint32_t>;

        struct patch_info {
            std::string name_;
            std::uint32_t req_uid2_;
            std::uint32_t req_uid3_;
            bool need_dest_rom_ = false;

            codeseg_ptr patch_;
            std::vector<patch_route_info> routes_;
        };

        enum load_patch_infos_state {
            PATCH_IDLE = 0,
            PATCH_IMPORT_IMAGES = 1,
            PATCH_DEPENDENCIES = 2
        };

        struct patch_pending_entry {
            codeseg_ptr dest_;
            std::size_t info_index_;
        };

        /**
         * \brief Manage libraries and HLE functions.
		 * 
		 * HLE functions are stored here. Libraries and images are also cached
		 * and load when needed.
		*/
        class lib_manager {
        private:
            io_system *io_;
            memory_system *mem_;
            kernel_system *kern_;
            system *sys_;

            drive_number rom_drv_;

            kernel::chunk *bootstrap_chunk_;
            bool log_svc{ false };

            std::vector<patch_info> patches_;
            std::vector<patch_pending_entry> patch_pendings_;
            std::map<address, address> trampoline_lookup_;

        protected:
            const std::uint8_t *entry_points_call_routine_;
            const std::uint8_t *thread_entry_routine_;

            std::uint32_t additional_mode_;

            drive_number get_drive_rom();

            void apply_pending_patches();
            void apply_trick_or_treat_algo();
            void jump_trampoline_through_svc();

        public:
            std::unordered_map<sid, epoc_import_func> svc_funcs_;
            std::vector<std::u16string> search_paths;

            explicit lib_manager(kernel_system *kern, io_system *ios, memory_system *mems);
            ~lib_manager();

            address get_entry_point_call_routine_address() const;
            address get_thread_entry_routine_address() const;

            bool build_eka1_thread_bootstrap_code();

            /**
             * \brief Call a HLE system call.
			 * \param svcnum The system call ordinal.
			*/
            bool call_svc(sid svcnum);

            /**
             * \brief Load a codeseg/library/exe from name
             *
             * If the manager detects we are loading a library and a HLE module is available,
             * it will returns a HLE codeseg contains HLE export
             * 
             * Else it will just load E32 Image or a ROM image and returns a codeseg
            */
            codeseg_ptr load(const std::u16string &name);

            // Search through all drives, which will parse all existing file
            std::pair<std::optional<loader::e32img>, std::optional<loader::romimg>>
            try_search_and_parse(const std::u16string &path, std::u16string *full_path = nullptr);

            codeseg_ptr load_as_e32img(loader::e32img &img, const std::u16string &path = u"");
            codeseg_ptr load_as_romimg(loader::romimg &img, const std::u16string &path = u"", const bool only_shell = false);

            void load_patch_libraries(const std::string &patch_folder);
            bool try_apply_patch(codeseg_ptr original);

            system *get_sys();
        };
    }

    namespace epoc {
        struct lib_info;

        bool get_image_info(hle::lib_manager *mngr, const std::u16string &name, epoc::lib_info &linfo);
        std::int32_t get_image_info_from_stream(common::ro_stream *stream, epoc::lib_info &linfo);
    }
}
