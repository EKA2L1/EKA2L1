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
#include <common/random.h>

#include <hle/epoc9/register.h>
#include <hle/libmanager.h>

#include <loader/eka2img.h>
#include <loader/romimage.h>
#include <vfs.h>

#include <core_kernel.h>

namespace eka2l1 {
    namespace hle {
        void lib_manager::init(system *syss, kernel_system *kerns, io_system *ios, memory_system *mems, epocver ver) {
            sys = syss;
            io = ios;
            mem = mems;
            kern = kerns;

            load_all_sids(ver);

            if (ver == epocver::epoc9) {
                register_epoc9(*this);
            }

            stub = kern->create_chunk("", 0, 0x5000, 0x5000, prot::read_write, kernel::chunk_type::disconnected,
                kernel::chunk_access::code, kernel::chunk_attrib::none, kernel::owner_type::kernel);

            custom_stub = kern->create_chunk("", 0, 0x5000, 0x5000, prot::read_write, kernel::chunk_type::disconnected,
                kernel::chunk_access::code, kernel::chunk_attrib::none, kernel::owner_type::kernel);

            stub_ptr = stub->base().cast<uint32_t>();
            custom_stub_ptr = custom_stub->base().cast<uint32_t>();

            LOG_INFO("Lib manager initialized, total implemented HLE functions: {}", import_funcs.size());
        }

        ptr<uint32_t> lib_manager::get_stub(uint32_t id) {
            auto &res = stubbed.find(id);

            if (res == stubbed.end()) {
                uint32_t *stub_ptr_real = stub_ptr.get(mem);
                stub_ptr_real[0] = 0xef000000; // svc #0, never used
                stub_ptr_real[1] = 0xe1a0f00e; // mov pc, lr
                stub_ptr_real[2] = id;

                stub_ptr += 12;

                return ptr<uint32_t>(stub_ptr.ptr_address() - 12);
            }

            return res->second;
        }

        ptr<uint32_t> lib_manager::do_custom_stub(uint32_t addr) {
            auto &res = custom_stubbed.find(addr);

            if (res == custom_stubbed.end()) {
                uint32_t *cstub_ptr_real = custom_stub_ptr.get(mem);
                cstub_ptr_real[0] = 0xef000001; // svc #1, never used
                cstub_ptr_real[1] = 0xe1a0f00e; // mov pc, lr
                cstub_ptr_real[2] = addr;

                *ptr<uint32_t>(addr).get(mem) = custom_stub_ptr.ptr_address();

                custom_stub_ptr += 12;

                return ptr<uint32_t>(custom_stub_ptr.ptr_address() - 12);
            }

            ptr<uint32_t> pt = res->second;
            *ptr<uint32_t>(addr).get(mem) = pt.ptr_address();

            return pt;
        }

        void lib_manager::register_custom_func(std::pair<address, epoc_import_func> func) {
            custom_funcs.insert(func);
            do_custom_stub(func.first);
        }

        void lib_manager::shutdown() {
            for (auto &img : e32imgs_cache) {
                kern->close_chunk(img.second.img->code_chunk->unique_id());
                kern->close_chunk(img.second.img->data_chunk->unique_id());
            }
        }

        void lib_manager::reset() {
            func_names.clear();
            ids.clear();
            svc_funcs.clear();
            custom_funcs.clear();
            import_funcs.clear();
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
#include <hle/epoc6_n.def>
            } else {
#include <hle/epoc9_n.def>
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

            return res->second + 8;
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
                        LOG_INFO("{} [address: 0x{:x}, sid: 0x{:x}, ord = {}]", func_names[libids[i]], addrs[i], libids[i], i);
                    }
                }
            } else {
                LOG_WARN("Can't find SID database for: {}", common::ucs2_to_utf8(lib_name));
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
            symfile img;

            // It's a full path
            if (img_name.find(u":") == 1) {
                img = io->open_file(img_name, READ_MODE | BIN_MODE);

                if (!img) {
                    return nullptr;
                }
            }

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
                        } else {
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

            if (romimgs_cache.find(res->header.entry_point) != romimgs_cache.end()) {
                romimgf->close();
                return romimgs_cache[res->header.entry_point].img;
            }

            //loader::stub_romimg()
            romimg_inf info;
            info.img = std::make_shared<loader::romimg>(res.value());

            romimgs_cache.emplace(res->header.entry_point, std::move(info));
            return romimgs_cache[res->header.entry_point].img;
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
                return false;
            }

            LOG_INFO("Calling {}", *get_func_name(id));

            auto imp = eimp.value();
            imp(sys);

            if (sys->get_kernel_system()->crr_thread() == nullptr) {
                return false;
            }

            return true;
        }

        address lib_manager::get_export_addr(sid id) {
            for (const auto & [ addr, sidk ] : addr_map) {
                if (sidk == id) {
                    return addr;
                }
            }

            return 0;
        }

        void lib_manager::open_romimg(loader::romimg_ptr &img) {
            auto res = romimgs_cache.find(img->header.entry_point);

            if (res == romimgs_cache.end()) {
                return;
            }

            res->second.loader.push_back(kern->crr_process());
        }

        void lib_manager::close_romimg(loader::romimg_ptr &img) {
            auto res = romimgs_cache.find(img->header.entry_point);

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

        bool lib_manager::call_svc(sid svcnum) {
            auto res = svc_funcs.find(svcnum);

            if (res == svc_funcs.end()) {
                return false;
            }

            epoc_import_func func = res->second;
            func(sys);

            if (sys->get_kernel_system()->crr_thread() == nullptr) {
                return false;
            }

            return true;
        }

        bool lib_manager::call_custom_hle(address addr) {
            auto res = custom_funcs.find(addr);

            if (res == custom_funcs.end()) {
                return false;
            }

            epoc_import_func func = res->second;
            func(sys);

            if (sys->get_kernel_system()->crr_thread() == nullptr) {
                return false;
            }

            return true;
        }

        void lib_manager::patch_hle() {
            // This is mostly based on assumption that: even a function: thumb or ARM, should be large
            // enough to contains an svc call (This hold true).
            for (const auto &func : import_funcs) {
                uint32_t addr = get_export_addr(func.first);

                if (addr) {
                    bool thumb = (addr % 2 != 0); //?

                    LOG_INFO("Write interrupt of {} at: 0x{:x} {}", *get_func_name(func.first), addr - addr % 2, thumb ? "(thumb)" : "");

                    *eka2l1::ptr<uint32_t>(addr - addr % 2).get(mem) = thumb ? 0xDF02 : 0xEF000002;
                }
            }
        }
    }
}
