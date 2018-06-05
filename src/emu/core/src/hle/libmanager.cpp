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
#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/log.h>

#include <hle/epoc9/register.h>
#include <hle/libmanager.h>

#include <loader/eka2img.h>
#include <loader/romimage.h>
#include <vfs.h>

#include <core_kernel.h>

namespace eka2l1 {
    namespace hle {
        void lib_manager::init(system* syss, kernel_system *kerns, io_system *ios, memory_system *mems, epocver ver) {
            sys = syss;
            io = ios;
            mem = mems;
            kern = kerns;

            load_all_sids(ver);
            
            if (ver == epocver::epoc9) {
                register_epoc9(*this);
            }

            LOG_INFO("Lib manager initialized, total implemented HLE functions: {}", import_funcs.size());
        }

        void lib_manager::load_all_sids(const epocver ver) {
            std::vector<sid> tids;
            std::string lib_name;

#define LIB(x) lib_name = #x;
#define EXPORT(x, y)   \
    tids.push_back(y); \
    func_names.insert(std::make_pair(y, x));
#define ENDLIB()                                                                        \
    ids.insert(std::make_pair(std::u16string(lib_name.begin(), lib_name.end()), tids)); \
    tids.clear();

            if (ver == epocver::epoc6) {
#include <hle/epoc6_n.h>
            }
            else {
#include <hle/epoc9_n.h>
            }

#undef LIB
#undef EXPORT
#undef ENLIB
        }

        std::optional<sids> lib_manager::get_sids(const std::u16string &lib_name) {
            auto res = ids.find(lib_name);

            if (res == ids.end()) {
                return std::optional<sids>{};
            }

            return res->second;
        }

        std::optional<exportaddrs> lib_manager::get_export_addrs(const std::u16string &lib_name) {
            auto res = exports.find(lib_name);

            if (res == exports.end()) {
                return std::optional<exportaddrs>{};
            }

            return res->second;
        }

        address lib_manager::get_vtable_address(const std::string class_name) {
            auto res = vtable_addrs.find(class_name);

            if (res == vtable_addrs.end()) {
                return 0;
            }

            return res->second;
        }

        bool lib_manager::register_exports(const std::u16string &lib_name, exportaddrs &addrs, bool log_exports) {
            if (exports.find(lib_name) != exports.end()) {
                LOG_WARN("Exports already register, not really dangerous");
                return true;
            }

            exports.insert(std::make_pair(lib_name, addrs));

            auto libidsop = get_sids(lib_name);

            if (libidsop) {
                sids libids = libidsop.value();

                if (addrs.size() > libids.size()) {
                    LOG_WARN("Export size is bigger than total symbol size provided, please update the symbol database for: {}",
                        common::ucs2_to_utf8(lib_name));
                }

                for (uint32_t i = 0; i < common::min(addrs.size(), libids.size()); i++) {
                    addr_map.insert(std::make_pair(addrs[i], libids[i]));
                    std::string name_imp = get_func_name(libids[i]).value();
                    size_t vtab_start = name_imp.find("vtable for ");

                    if (FOUND_STR(vtab_start)) {
                        vtable_addrs.emplace(name_imp.substr(11), addrs[i]);
                    }

                    if (log_exports) {
                        LOG_INFO("{} [address: 0x{:x}, sid: 0x{:x}]", func_names[libids[i]], addrs[i], libids[i]);
                    }
                }
            }

            return true;
        }

        std::optional<sid> lib_manager::get_sid(exportaddr addr) {
            auto res = addr_map.find(addr);

            if (res == addr_map.end()) {
                return std::optional<sid>{};
            }

            return res->second;
        }

        // Images are searched in
        // C:\\sys\bin, E:\\sys\\bin and Z:\\sys\\bin
        loader::e32img_ptr lib_manager::load_e32img(const std::u16string &img_name) {
            symfile img = io->open_file(img_name, READ_MODE | BIN_MODE);
            bool xip = false;
            bool is_rom = false;
            std::u16string path;

            // I'm so sorry
            if (!img) {
                img = io->open_file(u"C:\\sys\\bin\\" + img_name + u".dll", READ_MODE | BIN_MODE);

                if (!img) {
                    img = io->open_file(u"E:\\sys\\bin\\" + img_name + u".dll", READ_MODE | BIN_MODE);

                    if (!img) {
                        img = io->open_file(u"Z:\\sys\\bin\\" + img_name + u".dll", READ_MODE | BIN_MODE);

                        if (!img) {
                            return loader::e32img_ptr(nullptr);
                        }
                        else {
                            xip = true;
                            is_rom = true;
                        }
                    }
                }
            }

            auto res = loader::parse_eka2img(img);

            if (!res) {
                return loader::e32img_ptr(nullptr);
            }

            if (res->ed.syms.size() > 0) {
                register_exports(img_name, res->ed.syms);
            }

            loader::e32img_ptr pimg = std::make_shared<loader::eka2img>(res.value());

            if (e32imgs_cache.find(pimg->header.check) != e32imgs_cache.end()) {
                img->close();
                return e32imgs_cache[pimg->header.check].img;
            }

            e32img_inf info;

            info.img = pimg;
            info.is_xip = xip;
            info.is_rom = is_rom;

            uint32_t check = info.img->header.check;

            e32imgs_cache.insert(std::make_pair(info.img->header.check, std::move(info)));
            img->close();

            return e32imgs_cache[check].img;
        }

        loader::romimg_ptr lib_manager::load_romimg(const std::u16string &rom_name, bool log_exports) {
            symfile romimgf = io->open_file(u"Z:\\sys\\bin\\" + rom_name + u".dll", READ_MODE | BIN_MODE);

            if (!romimgf) {
                romimgf = io->open_file(rom_name, READ_MODE | BIN_MODE);

                if (!romimgf) {
                    return loader::romimg_ptr(nullptr);
                }
            }

            auto res = loader::parse_romimg(romimgf, mem);

            if (!res) {
                return loader::romimg_ptr(nullptr);
            }

            register_exports(rom_name, res->exports, log_exports);

            if (romimgs_cache.find(res->header.code_checksum) != romimgs_cache.end()) {
                romimgf->close();
                return romimgs_cache[res->header.code_checksum].img;
            }

            //loader::stub_romimg()
            romimg_inf info;
            info.img = std::make_shared<loader::romimg>(res.value());

            romimgs_cache.emplace(res->header.code_checksum, std::move(info));
            return romimgs_cache[res->header.code_checksum].img;
        }

        // Open the image code segment
        void lib_manager::open_e32img(loader::e32img_ptr &img) {
            auto res = e32imgs_cache.find(img->header.check);

            if (res == e32imgs_cache.end()) {
                LOG_ERROR("Image not loaded, checksum: {}", img->header.check);
                return;
            }

            // If the image is not XIP, means that it's unloaded or not loaded
            if (!res->second.is_xip) {
                loader::load_eka2img(*img, mem, kern, *this);
                res->second.is_xip = true;
            }

            res->second.loader.push_back(kern->crr_process());
        }

        // Close the image code segment. Means that the image will be unloaded, XIP turns to false
        void lib_manager::close_e32img(loader::e32img_ptr &img) {
            auto res = e32imgs_cache.find(img->header.check);

            if (res == e32imgs_cache.end()) {
                LOG_ERROR("Image not loaded, checksum: {}", img->header.check);
                return;
            }

            if (!res->second.is_rom) {
                kern->close_chunk(img->code_chunk->unique_id());
                res->second.is_xip = false;
            }

            auto res2 = std::find(res->second.loader.begin(), res->second.loader.end(), kern->crr_process());

            if (res2 == res->second.loader.end()) {
                LOG_ERROR("Image never opened by this process");
                return;
            }

            res->second.loader.erase(res2);
        }

        std::optional<std::string> lib_manager::get_func_name(const sid id) {
            auto res = func_names.find(id);

            if (res == func_names.end()) {
                return std::optional<std::string>{};
            }

            return res->second;
        }

        void lib_manager::register_hle(sid id, epoc_import_func func) {
            import_funcs.emplace(id, func);
        }

        std::optional<epoc_import_func> lib_manager::get_hle(sid id) {
            auto res = import_funcs.find(id);

            if (res != import_funcs.end()) {
                return res->second;
            }

            return std::optional<epoc_import_func>{};
        }

        bool lib_manager::call_hle(sid id) {
            auto eimp = get_hle(id);

            if (!eimp) {
                LOG_WARN("Function unimplemented!");
                return false;
            }

            auto imp = eimp.value();
            imp(sys);

            return true;
        }

        address lib_manager::get_export_addr(sid id) {
            for (const auto &[addr, sidk] : addr_map) {
                if (sidk == id) {
                    return addr;
                }
            }

            return 0;
        }

        void lib_manager::open_romimg(loader::romimg_ptr &img) {
            auto res = romimgs_cache.find(img->header.code_checksum);

            if (res == romimgs_cache.end()) {
                return;
            }

            res->second.loader.push_back(kern->crr_process());
        }

        void lib_manager::close_romimg(loader::romimg_ptr &img) {
            auto res = romimgs_cache.find(img->header.code_checksum);

            if (res == romimgs_cache.end()) {
                return;
            }

            auto res2 = std::find(res->second.loader.begin(), res->second.loader.end(), kern->crr_process());

            if (res2 == res->second.loader.end()) {
                LOG_ERROR("Image never opened by this process");
                return;
            }

            res->second.loader.erase(res2);
        }
    }
}
