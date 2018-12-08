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

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <unordered_map>

#include <epoc/ptr.h>

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
    }

    using chunk_ptr = std::shared_ptr<kernel::chunk>;
    using process_ptr = std::shared_ptr<kernel::process>;

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

        struct e32img_inf {
            loader::e32img_ptr img;
            bool is_xip;
            bool is_rom;
            std::u16string full_path;
            std::vector<process_ptr> loader;
        };

        struct romimg_inf {
            loader::romimg_ptr img;
            std::vector<process_ptr> loader;
            std::u16string full_path;
        };

        /*! \brief Manage libraries and HLE functions.
		 * 
		 * HLE functions are stored here. Libraries and images are also cached
		 * and load when needed.
		*/
        class lib_manager {
            std::map<std::u16string, sids> ids;
            std::map<sid, std::string> func_names;

            std::map<std::u16string, exportaddrs> exports;
            std::map<address, sid> addr_map;

            std::map<std::string, address> vtable_addrs;

            // Caches the image
            std::map<uint32_t, e32img_inf> e32imgs_cache;
            std::map<uint32_t, romimg_inf> romimgs_cache;

            io_system *io;
            memory_system *mem;
            kernel_system *kern;
            system *sys;

            uint32_t custom_stub;
            uint32_t stub;

            ptr<uint32_t> stub_ptr;
            ptr<uint32_t> custom_stub_ptr;

            std::map<uint32_t, ptr<uint32_t>> stubbed;
            std::map<uint32_t, ptr<uint32_t>> custom_stubbed;

        public:
            std::map<address, epoc_import_func> import_funcs;
            std::map<sid, epoc_import_func> svc_funcs;
            std::map<address, epoc_import_func> custom_funcs;

            lib_manager(){};

            ptr<uint32_t> get_stub(uint32_t id);
            ptr<uint32_t> do_custom_stub(uint32_t addr);

            void patch_hle();

            void register_custom_func(std::pair<address, epoc_import_func> func);

            /*! \brief Intialize the library manager. 
			 * \param ver The EPOC version to import HLE functions.
			*/
            void init(system *sys, kernel_system *kern, io_system *ios, memory_system *mems, epocver ver);

            /*! \brief Shutdown the library manager. */
            void shutdown();

            /*! \brief Reset the library manager. */
            void reset();

            /*! \brief Get the SIDS of a library. */
            std::optional<sids> get_sids(const std::u16string &lib_name);

            /*! \brief Get the export addresses of a library. */
            std::optional<exportaddrs> get_export_addrs(const std::u16string &lib_name);

            void register_hle(sid id, epoc_import_func func);

            /*! \brief Get the HLE function from an SID */
            std::optional<epoc_import_func> get_hle(sid id);

            /*! \brief Call HLE function with an ID */
            bool call_hle(sid id);

            /*! \brief Call a HLE function registered at a specific address */
            bool call_custom_hle(address addr);

            /*! \brief Call a HLE system call.
			 * \param svcnum The system call ordinal.
			*/
            bool call_svc(sid svcnum);

            /*! \brief Load an E32Image. */
            loader::e32img_ptr load_e32img(const std::u16string &img_name);

            /*! \brief Load an ROM image. */
            loader::romimg_ptr load_romimg(const std::u16string &rom_name, bool log_export = false);

            /*! \brief Open the image code segment */
            void open_e32img(loader::e32img_ptr &img);

            /*! \brief Close the image code segment. Means that the image will be unloaded, XIP turns to false. */
            void close_e32img(loader::e32img_ptr &img);

            void open_romimg(loader::romimg_ptr &img);
            void close_romimg(loader::romimg_ptr &img);

            /*! \brief Register export addresses for desired HLE library
             *
			 * This will also map the export address with the correspond SID.
             * Note that these export addresses are unique, since they are the address in
             * the memory.
			*/
            bool register_exports(const std::u16string &lib_name, exportaddrs &addrs, bool log_export = false);
            std::optional<sid> get_sid(exportaddr addr);

            /*! \brief Given an SID, get the function name. */
            std::optional<std::string> get_func_name(const sid id);

            std::map<uint32_t, e32img_inf> &get_e32imgs_cache() {
                return e32imgs_cache;
            }

            std::map<uint32_t, romimg_inf> &get_romimgs_cache() {
                return romimgs_cache;
            }

            /*! \brief Given a class name, return the vtable address.
			 * \returns 0 if the class doesn't exists in the cache, else returns the vtable address of that class.
			*/
            address get_vtable_address(const std::string class_name);

            address get_vtable_entry_addr(const std::string class_name, uint32_t idx) {
                return get_vtable_address(class_name) + idx * 4;
            }

            address get_export_addr(sid id);

            system *get_sys() {
                return sys;
            }
        };
    }
}
