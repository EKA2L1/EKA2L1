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

#include <epoc/ptr.h>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
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

    using process_ptr = kernel::process*;
    using codeseg_ptr = kernel::codeseg*;

    // Technically, loader is an user application, but here we treat it as kernel

    namespace loader {
        struct e32img;
        struct romimg;

        using e32img_ptr = std::shared_ptr<e32img>;
        using romimg_ptr = std::shared_ptr<romimg>;
    }

    namespace hle {
        struct epoc_import_func {
            std::function<void(system *)> func;
            std::string name;
        };

        using func_map = std::unordered_map<uint32_t, eka2l1::hle::epoc_import_func>;
        using export_table = std::vector<std::uint32_t>;
        using symbols = std::vector<std::string>;

        /*! \brief Manage libraries and HLE functions.
		 * 
		 * HLE functions are stored here. Libraries and images are also cached
		 * and load when needed.
		*/
        class lib_manager {
            io_system *io;
            memory_system *mem;
            kernel_system *kern;
            system *sys;

            std::unordered_map<address, std::string> addr_symbols;
            std::unordered_map<std::string, symbols> lib_symbols;

            bool log_svc{ false };

        protected:
            void load_patch_libraries(const std::string &patch_folder);

        public:
            std::unordered_map<sid, epoc_import_func> svc_funcs;

            lib_manager(){};

            bool register_exports(const std::string &lib_name, export_table table);

            /*! \brief Intialize the library manager. 
			 * \param ver The EPOC version to import HLE functions.
			*/
            void init(system *sys, kernel_system *kern, io_system *ios, memory_system *mems, epocver ver);

            /*! \brief Shutdown the library manager. */
            void shutdown();

            /*! \brief Reset the library manager. */
            void reset();

            /*! \brief Call a HLE system call.
			 * \param svcnum The system call ordinal.
			*/
            bool call_svc(sid svcnum);

            /*! \brief Load a codeseg/library/exe from name
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

            std::optional<std::string> get_symbol(const address addr) {
                auto res = addr_symbols.find(addr);
                if (res != addr_symbols.end()) {
                    return res->second;
                }
                return std::nullopt;
            }

            system *get_sys() {
                return sys;
            }
        };
    }
}
