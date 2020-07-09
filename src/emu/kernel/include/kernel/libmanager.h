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

#include <common/types.h>
#include <common/container.h>

#include <kernel/common.h>
#include <mem/ptr.h>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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
        using export_table = std::vector<std::uint32_t>;
        using symbols = std::vector<std::string>;

        using codeseg_loaded_callback = std::function<void(const std::string&, kernel::process*, codeseg_ptr)>;

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

            common::identity_container<codeseg_loaded_callback> codeseg_loaded_callback_funcs_;

        protected:
            const std::uint8_t *entry_points_call_routine_;
            const std::uint8_t *thread_entry_routine_;

            drive_number get_drive_rom();

        public:
            std::map<sid, epoc_import_func> svc_funcs_;

            explicit lib_manager(kernel_system *kern, io_system *ios, memory_system *mems);
            ~lib_manager();

            address get_entry_point_call_routine_address() const;
            address get_thread_entry_routine_address() const;

            bool build_eka1_thread_bootstrap_code();

            void run_codeseg_loaded_callback(const std::string &lib_name, kernel::process *attacher, codeseg_ptr target);

            std::size_t register_codeseg_loaded_callback(codeseg_loaded_callback callback);
            bool unregister_codeseg_loaded_callback(const std::size_t handle);

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
            codeseg_ptr load(const std::u16string &name, kernel::process *pr);

            // Search through all drives, which will parse all existing file
            std::pair<std::optional<loader::e32img>, std::optional<loader::romimg>>
            try_search_and_parse(const std::u16string &path);

            codeseg_ptr load_as_e32img(loader::e32img &img, kernel::process *pr, const std::u16string &path = u"");
            codeseg_ptr load_as_romimg(loader::romimg &img, kernel::process *pr, const std::u16string &path = u"");

            void load_patch_libraries(const std::string &patch_folder);

            system *get_sys();
        };
    }

    namespace epoc {
        struct lib_info;

        bool get_image_info(hle::lib_manager *mngr, const std::u16string &name, epoc::lib_info &linfo);
    }
}
