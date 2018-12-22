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
#include <common/path.h>
#include <common/random.h>

#include <epoc/reg.h>
#include <epoc/kernel/libmanager.h>

#include <epoc/configure.h>

#ifdef ENABLE_SCRIPTING
#include <manager/manager.h>
#include <manager/script_manager.h>
#endif

#include <epoc/loader/e32img.h>
#include <epoc/loader/romimage.h>
#include <epoc/vfs.h>

#include <epoc/epoc.h>
#include <epoc/configure.h>
#include <epoc/kernel.h>

namespace eka2l1 {
    namespace hle {
        // Write simple relocation
        // Symbian only used this, as found on IDA
        bool write(uint32_t *data, uint32_t sym) {
            *data = sym;
            return true;
        }

        bool relocate(uint32_t *dest_ptr, loader::relocation_type type, uint32_t code_delta, 
            uint32_t data_delta) {
            if (type == loader::relocation_type::reserved) {
                return true;
            }

            // What is in it ?? :))
            uint32_t reloc_offset = *dest_ptr;

            switch (type) {
            case loader::relocation_type::text:
                write(dest_ptr, reloc_offset + code_delta);
                break;

            case loader::relocation_type::data:
                // This is relocation for dynamic data.                
                write(dest_ptr, reloc_offset + data_delta);
                break;

            case loader::relocation_type::inffered:
            default:
                LOG_WARN("Relocation not properly handle: {}", (int)type);
                break;
            }

            return true;
        }

        // Given relocation entries, relocate the code and data
        bool relocate(std::vector<loader::e32_reloc_entry> entries,
            uint8_t *dest_addr,
            uint32_t code_delta,
            uint32_t data_delta) {
            for (uint32_t i = 0; i < entries.size(); i++) {
                auto entry = entries[i];

                for (auto &rel_info : entry.rels_info) {
                    // Get the lower 12 bit for virtual_address
                    uint32_t virtual_addr = entry.base + (rel_info & 0x0FFF);
                    uint8_t *dest_ptr = virtual_addr + dest_addr;

                    loader::relocation_type rel_type = (loader::relocation_type)(rel_info & 0xF000);

                    // LOG_TRACE("{}", virtual_addr + 0x8000);

                    if (!relocate(reinterpret_cast<uint32_t *>(dest_ptr), rel_type, code_delta, data_delta)) {
                        LOG_WARN("Relocate fail at page: {}", i);
                    }
                }
            }

            return true;
        }

        std::string get_real_dll_name(std::string dll_name) {
            size_t dll_name_end_pos = dll_name.find_first_of("{");

            if (FOUND_STR(dll_name_end_pos)) {
                dll_name = dll_name.substr(0, dll_name_end_pos);
            } else {
                dll_name_end_pos = dll_name.find_last_of(".");

                if (FOUND_STR(dll_name_end_pos)) {
                    dll_name = dll_name.substr(0, dll_name_end_pos);
                }
            }

            return dll_name;
        }

        bool pe_fix_up_iat(memory_system *mem, hle::lib_manager &mngr, loader::e32img &me, 
            loader::e32img_import_block &import_block, loader::e32img_iat &iat, uint32_t &crr_idx) {
            const std::string dll_name8 = get_real_dll_name(import_block.dll_name);
            const std::u16string dll_name = common::utf8_to_ucs2(dll_name8);

            loader::e32img_ptr img = mngr.load_e32img(dll_name);
            loader::romimg_ptr rimg;

            if (!img) {
                rimg = mngr.load_romimg(dll_name);
            }
            
            uint32_t *imdir = &(import_block.ordinals[0]);
            uint32_t *expdir;

            std::vector<std::uint32_t> relocated_export_addresses;

            if (!img && !rimg) {
                return false;
            } else {
                if (img) {
                    mngr.open_e32img(img);

                    const std::uint32_t data_start = img->header.data_offset;
                    const std::uint32_t data_end = data_start + img->header.data_size + img->header.bss_size;

                    const std::uint32_t code_delta = img->rt_code_addr - img->header.code_base;
                    const std::uint32_t data_delta = img->rt_data_addr - img->header.data_base;

                    relocated_export_addresses.assign(img->ed.syms.begin(), img->ed.syms.end());
                    expdir = relocated_export_addresses.data();

                    for (auto &exp : img->ed.syms) {
                        // Add with the relative section delta.
                        if (exp > data_start && exp < data_end) {
                            exp += data_delta;
                        } else {
                            exp += code_delta;
                        }
                    }
                } else {
                    mngr.open_romimg(rimg);
                    expdir = rimg->exports.data();
                }
            }

            for (uint32_t i = crr_idx, j = 0; 
                i < crr_idx + import_block.ordinals.size() && j < import_block.ordinals.size();
                i++, j++) {
                uint32_t iat_off = img->header.code_offset + img->header.code_size;
                img->data[iat_off + i * 4] = expdir[import_block.ordinals[j] - 1];
            }

            crr_idx += static_cast<uint32_t>(import_block.ordinals.size());

            return true;
        }

        bool elf_fix_up_import_dir(memory_system *mem, hle::lib_manager &mngr, loader::e32img &me,
            loader::e32img_import_block &import_block) {
            LOG_INFO("Fixup for: {}", import_block.dll_name);

            const std::string dll_name8 = get_real_dll_name(import_block.dll_name);
            const std::u16string dll_name = common::utf8_to_ucs2(dll_name8);

            loader::e32img_ptr img = mngr.load_e32img(dll_name);
            loader::romimg_ptr rimg;

            if (!img) {
                rimg = mngr.load_romimg(dll_name);
            }

            std::uint32_t *imdir = &(import_block.ordinals[0]);
            std::uint32_t *expdir;

            std::vector<std::uint32_t> relocated_export_addresses;

            if (!img && !rimg) {
                LOG_WARN("Can't find image or rom image for: {}", dll_name8);
                return false;
            } else {
                if (img) {
                    mngr.open_e32img(img);

                    const std::uint32_t data_start = img->header.data_offset;
                    const std::uint32_t data_end = data_start + img->header.data_size + img->header.bss_size;

                    const std::uint32_t code_delta = img->rt_code_addr - img->header.code_base;
                    const std::uint32_t data_delta = img->rt_data_addr - img->header.data_base;

                    relocated_export_addresses.assign(img->ed.syms.begin(), img->ed.syms.end());
                    expdir = relocated_export_addresses.data();

                    for (auto &exp : relocated_export_addresses) {
                        // Add with the relative section delta.
                        if (exp > data_start && exp < data_end) {
                            exp += data_delta;
                        } else {
                            exp += code_delta;
                        }
                    }
                } else {
                    mngr.open_romimg(rimg);
                    expdir = rimg->exports.data();
                }
            }

            for (uint32_t i = 0; i < import_block.ordinals.size(); i++) {
                uint32_t off = imdir[i];
                uint32_t *code_ptr = ptr<uint32_t>(me.rt_code_addr + off).get(mem);

                uint32_t import_inf = *code_ptr;
                uint32_t ord = import_inf & 0xffff;
                uint32_t adj = import_inf >> 16;

                uint32_t export_addr;
                uint32_t val = 0;

                if (ord > 0) {
                    export_addr = expdir[ord - 1];

                    auto sid = mngr.get_sid(export_addr);

                    if (sid) {
                        if (mngr.get_hle(*sid)) {
                            uint32_t impaddr = mngr.get_stub(*sid).ptr_address();
                            write(code_ptr, impaddr);

                            continue;
                        }
                    }

                    // The export address provided is already added with relative code/data
                    // delta, so add this directly to the adjustment address
                    val = export_addr + adj;
                }

                // LOG_TRACE("Writing 0x{:x} to 0x{:x}", val, me.header.code_base + off);
                write(code_ptr, val);
            }

            return true;
        }

        bool import_e32img(loader::e32img *img, memory_system *mem, kernel_system *kern, hle::lib_manager &mngr) {
            std::uint32_t data_seg_size = img->header.data_size + img->header.bss_size;
            
            img->code_chunk = kern->create_chunk("", 0, common::align(img->header.code_size, mem->get_page_size()), common::align(img->header.code_size, mem->get_page_size()),
                prot::read_write_exec, kernel::chunk_type::normal, kernel::chunk_access::code, kernel::chunk_attrib::none, kernel::owner_type::kernel);

            img->data_chunk = kern->create_chunk("", 0, common::align(data_seg_size, mem->get_page_size()), common::align(data_seg_size, mem->get_page_size()), 
                prot::read_write, kernel::chunk_type::normal, kernel::chunk_access::code, kernel::chunk_attrib::none, kernel::owner_type::kernel);

            chunk_ptr code_chunk_ptr = std::reinterpret_pointer_cast<kernel::chunk>(kern->get_kernel_obj(img->code_chunk));
            chunk_ptr data_chunk_ptr = std::reinterpret_pointer_cast<kernel::chunk>(kern->get_kernel_obj(img->data_chunk));

            uint32_t rtcode_addr = code_chunk_ptr->base().ptr_address();
            uint32_t rtdata_addr = data_chunk_ptr ? data_chunk_ptr->base().ptr_address() : 0;

            LOG_INFO("Runtime code: 0x{:x}", rtcode_addr);

            img->rt_code_addr = rtcode_addr;
            img->rt_data_addr = rtdata_addr;

            // Ram code address is the run time address of the code
            uint32_t code_delta = rtcode_addr - img->header.code_base;
            uint32_t data_delta = rtdata_addr - img->header.data_base;

            relocate(img->code_reloc_section.entries, reinterpret_cast<uint8_t *>(img->data.data() + img->header.code_offset), code_delta, data_delta);

            if (img->header.data_size)
                relocate(img->data_reloc_section.entries, reinterpret_cast<uint8_t *>(img->data.data() + img->header.data_offset), code_delta, data_delta);

            if (img->epoc_ver == epocver::epoc6) {
                uint32_t track = 0;

                for (auto &ib : img->import_section.imports) {
                    pe_fix_up_iat(mem, mngr, *img, ib, img->iat, track);
                }
            }

            memcpy(ptr<void>(rtcode_addr).get(mem), img->data.data() + img->header.code_offset, img->header.code_size);
            std::uint8_t *dt_ptr = ptr<std::uint8_t>(rtdata_addr).get(mem); 

            if (img->header.data_size) {
                // If there is initialized data, copy that
                memcpy(dt_ptr + img->header.bss_size, img->data.data() + img->header.data_offset, 
                    img->header.data_size);
            }

            // Definitely, there maybe bss, fill that with zero
            // I usually depends on this to get my integer zero
            // Filling zero from beginning of code segment, with size of bss size - 1
            std::fill(dt_ptr, dt_ptr + img->header.bss_size, 0);

            if (img->epoc_ver == epocver::epoc9) {
                for (auto &ib : img->import_section.imports) {
                    elf_fix_up_import_dir(mem, mngr, *img, ib);
                }
            }

            // Hack: open the descriptor address and relocate the type info
            // Somehow Symbian doesn't do this
            // TODO: Find out why and fix
            if (img->has_extended_header && img->header_extended.exception_des & 1) {
                // Let's fix this
                address exception_des_addr = img->header_extended.exception_des - 1 + img->rt_code_addr;
                address exception_struct_addr = *ptr<address>(exception_des_addr).get(mem);
                address typeinfo_ptr_addr = exception_struct_addr - 0x10;

                // The typeinfo should be relocated
                auto do_relocate_typeinfo = [&](address addr) {
                    address *typeinfo_relocate_val = ptr<address>(addr).get(mem);

                    if (typeinfo_relocate_val == nullptr) {
                        LOG_ERROR("Can't relocate exception typeinfo!");
                    }

                    *typeinfo_relocate_val = *typeinfo_relocate_val - img->header.code_base + img->rt_code_addr;
                };

                do_relocate_typeinfo(typeinfo_ptr_addr);
                do_relocate_typeinfo(typeinfo_ptr_addr - 0x54);
                do_relocate_typeinfo(typeinfo_ptr_addr - 0x54 - 0x78);
            }
            
            LOG_INFO("Load e32img success");

            return true;
        }

        void lib_manager::init(system *syss, kernel_system *kerns, io_system *ios, memory_system *mems, epocver ver) {
            sys = syss;
            io = ios;
            mem = mems;
            kern = kerns;

            // TODO (pent0): Implement external id loading
            
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
           //  #include <hle/epoc6_n.def>
            } else {
            // #include <hle/epoc9_n.def>
            }

            #undef LIB
            #undef EXPORT
            #undef ENLIB

            if (ver == epocver::epoc9) {
                epoc::register_epocv94(*this);
            } else if (ver == epocver::epoc93) {
                epoc::register_epocv93(*this);
            }

            stub = kern->create_chunk("", 0, 0x5000, 0x5000, prot::read_write, kernel::chunk_type::disconnected,
                kernel::chunk_access::code, kernel::chunk_attrib::none, kernel::owner_type::kernel);

            custom_stub = kern->create_chunk("", 0, 0x5000, 0x5000, prot::read_write, kernel::chunk_type::disconnected,
                kernel::chunk_access::code, kernel::chunk_attrib::none, kernel::owner_type::kernel);

            chunk_ptr stub_chunk_obj = std::reinterpret_pointer_cast<kernel::chunk>(kern->get_kernel_obj(stub));
            chunk_ptr custom_stub_chunk_obj = std::reinterpret_pointer_cast<kernel::chunk>(kern->get_kernel_obj(custom_stub));

            stub_ptr = stub_chunk_obj->base().cast<uint32_t>();
            custom_stub_ptr = custom_stub_chunk_obj->base().cast<uint32_t>();

            LOG_INFO("Lib manager initialized, total implemented HLE functions: {}", import_funcs.size());
        }

        ptr<uint32_t> lib_manager::get_stub(uint32_t id) {
            auto res = stubbed.find(id);

            if (res == stubbed.end()) {
                uint32_t *stub_ptr_real = stub_ptr.get(mem);
                stub_ptr_real[0] = 0xef900000; // svc #0, never used
                stub_ptr_real[1] = 0xe1a0f00e; // mov pc, lr
                stub_ptr_real[2] = id;

                stub_ptr += 12;

                return ptr<uint32_t>(stub_ptr.ptr_address() - 12);
            }

            return res->second;
        }

        ptr<uint32_t> lib_manager::do_custom_stub(uint32_t addr) {
            auto res = custom_stubbed.find(addr);

            if (res == custom_stubbed.end()) {
                uint32_t *cstub_ptr_real = custom_stub_ptr.get(mem);
                cstub_ptr_real[0] = 0xef900001; // svc #1, never used
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
                kern->close(img.second.img->code_chunk);
                kern->close(img.second.img->data_chunk);
            }
        }

        void lib_manager::reset() {
            func_names.clear();
            ids.clear();
            svc_funcs.clear();
            custom_funcs.clear();
            import_funcs.clear();
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

                for (uint32_t i = 0; i < common::min(addrs.size(), libids.size()); i++) {
                    addr_map.insert(std::make_pair(addrs[i], libids[i]));

#ifdef ENABLE_SCRIPTING
                    sys->get_manager_system()->get_script_manager()->patch_sid_breakpoints(libids[i], addrs[i]);
#endif

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
                if (log_exports) {
                    LOG_WARN("Can't find SID database for: {}", common::ucs2_to_utf8(lib_name));
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
            symfile img;

            // It's a full path
            if (img_name.find(u":") == 1) {
                bool should_append_ext = (eka2l1::path_extension(img_name) == u"");

                img = io->open_file(img_name + (should_append_ext ? u".dll" : u""), READ_MODE | BIN_MODE);

                if (!img && should_append_ext) {
                    img = io->open_file(img_name + (should_append_ext ? u".exe" : u""), READ_MODE | BIN_MODE);
                }

                if (!img) {
                    return nullptr;
                }
            }

            bool xip = false;
            bool is_rom = false;
            std::u16string path;

            const std::map<std::u16string, std::u16string> patterns = {
                { u"Z:\\sys\\bin\\", u".dll" },
                { u"Z:\\sys\\bin\\", u".exe" },
                { u"C:\\sys\\bin\\", u".dll" },
                { u"C:\\sys\\bin\\", u".exe" },
                { u"E:\\sys\\bin\\", u".dll" },
                { u"E:\\sys\\bin\\", u".exe" }
            };

            std::optional<loader::e32img> res;

            if (img) {
                res = loader::parse_e32img(img);

                if (!res) {
                    img->close();
                    return nullptr;
                }
            } else {
                bool should_append_ext = (eka2l1::path_extension(img_name) == u"");

                for (const auto &pattern : patterns) {
                    std::u16string full = pattern.first + img_name + (should_append_ext ? pattern.second : u"");

                    if (io->exist(full)) {
                        img = io->open_file(full, READ_MODE | BIN_MODE);
                        res = loader::parse_e32img(img);

                        if (res) {
                            break;
                        }
                    }
                }
            }

            if (!res) {
                return loader::e32img_ptr(nullptr);
            }

            if (res->ed.syms.size() > 0) {
                register_exports(img_name, res->ed.syms, sys->get_bool_config("log_exports"));
            }

            loader::e32img_ptr pimg = std::make_shared<loader::e32img>(res.value());

            if (e32imgs_cache.find(pimg->header.check) != e32imgs_cache.end()) {
                return e32imgs_cache[pimg->header.check].img;
            }

            e32img_inf info;

            info.img = pimg;
            info.is_xip = xip;
            info.is_rom = is_rom;
            info.full_path = std::move(img->file_name());
            
            img->close();

            uint32_t check = info.img->header.check;

            e32imgs_cache.insert(std::make_pair(info.img->header.check, std::move(info)));

            return e32imgs_cache[check].img;
        }

        loader::romimg_ptr lib_manager::load_romimg(const std::u16string &rom_name, bool log_exports) {
            symfile romimgf = nullptr;

            if (rom_name.find(u":") == 1) {
                bool should_append_ext = (eka2l1::path_extension(rom_name) == u"");

                // Normally by default, Symbian should append the extension itself if no extension provided.
                romimgf = io->open_file(rom_name + (should_append_ext ? u".dll" : u""), READ_MODE | BIN_MODE);

                if (!romimgf && should_append_ext) {
                    romimgf = io->open_file(rom_name + (should_append_ext ? u".exe" : u""), READ_MODE | BIN_MODE);
                }
                
                if (!romimgf) {
                    return nullptr;
                }
            }

            if (!romimgf) {
                bool should_append_ext = (eka2l1::path_extension(rom_name) == u"");

                romimgf = io->open_file(u"Z:\\sys\\bin\\" + rom_name + (should_append_ext ? u".dll" : u""),
                    READ_MODE | BIN_MODE);

                if (!romimgf) {
                    romimgf = io->open_file(u"Z:\\sys\\bin\\" + rom_name + (should_append_ext ? u".exe" : u""),
                        READ_MODE | BIN_MODE);

                    if (!romimgf) {
                        return loader::romimg_ptr(nullptr);
                    }
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
            info.full_path = std::move(romimgf->file_name());

            romimgf->close();

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
                res->second.is_xip = true;
                import_e32img(img.get(), mem, kern, *this);
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

            //LOG_INFO("Calling {}", *get_func_name(id));

            auto imp = eimp.value();
            imp.func(sys);

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

            if (sys->get_bool_config("log_svc_passed")) {
                LOG_TRACE("Calling SVC 0x{:x} {}", svcnum, func.name);
            }

#ifdef ENABLE_SCRIPTING
            sys->get_manager_system()->get_script_manager()->call_svcs(svcnum);
#endif

            func.func(sys);

            return true;
        }

        bool lib_manager::call_custom_hle(address addr) {
            auto res = custom_funcs.find(addr);

            if (res == custom_funcs.end()) {
                return false;
            }

            epoc_import_func func = res->second;
            func.func(sys);

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

                    //LOG_INFO("Write interrupt of {} at: 0x{:x} {}", *get_func_name(func.first), addr - addr % 2, thumb ? "(thumb)" : "");

                    *eka2l1::ptr<uint32_t>(addr - addr % 2).get(mem) = thumb ? 0xDF02 : 0xEF000002;
                }
            }
        }
    }
}
