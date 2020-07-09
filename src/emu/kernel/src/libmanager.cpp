/*
 * Copyright (c) 2018 EKA2L1 Team.
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

#include <common/algorithm.h>
#include <common/armemitter.h>
#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/ini.h>
#include <common/log.h>
#include <common/path.h>
#include <common/random.h>

#include <kernel/common.h>
#include <kernel/libmanager.h>
#include <kernel/reg.h>

#include <common/configure.h>
#include <config/config.h>

#include <loader/e32img.h>
#include <loader/romimage.h>
#include <mem/page.h>
#include <vfs/vfs.h>
#include <utils/dll.h>

#include <kernel/kernel.h>
#include <kernel/codeseg.h>

#include <cctype>

namespace eka2l1::hle {
    // Write simple relocation
    static bool write(uint32_t *data, uint32_t sym) {
        *data = sym;
        return true;
    }

    static bool relocate(uint32_t *dest_ptr, loader::relocation_type type, uint32_t code_delta, uint32_t data_delta) {
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

        default:
            LOG_WARN("Relocation not properly handle: {}", (int)type);
            break;
        }

        return true;
    }

    // Given relocation entries, relocate the code and data
    static bool relocate(std::vector<loader::e32_reloc_entry> entries,
        std::uint8_t *dest_addr,
        std::uint32_t code_size,
        std::uint32_t code_delta,
        std::uint32_t data_delta) {
        for (uint32_t i = 0; i < entries.size(); i++) {
            auto entry = entries[i];

            for (auto &rel_info : entry.rels_info) {
                // Get the lower 12 bit for virtual_address
                uint32_t virtual_addr = entry.base + (rel_info & 0x0FFF);
                uint8_t *dest_ptr = virtual_addr + dest_addr;

                loader::relocation_type rel_type = (loader::relocation_type)(rel_info & 0xF000);

                // Compare with petran in case there are problems
                // LOG_TRACE("{:x}", virtual_addr);

                // This is relocation type named: Who bother to put my identity in PETRAN
                // figure out who I am yourself
                if (rel_type == loader::relocation_type::inferred) {
                    rel_type = (virtual_addr < code_size) ? loader::relocation_type::text :
                        loader::relocation_type::data;
                }

                if (!relocate(reinterpret_cast<uint32_t *>(dest_ptr), rel_type, code_delta, data_delta)) {
                    LOG_WARN("Relocate fail at page: {}", i);
                }
            }
        }

        return true;
    }

    static std::string get_real_dll_name(std::string dll_name) {
        size_t dll_name_end_pos = dll_name.find_first_of("{");

        if (FOUND_STR(dll_name_end_pos)) {
            dll_name = dll_name.substr(0, dll_name_end_pos);
        } else {
            dll_name_end_pos = dll_name.find_last_of("[");

            if (FOUND_STR(dll_name_end_pos)) {
                dll_name = dll_name.substr(0, dll_name_end_pos);
            } else {
                return dll_name;
            }
        }

        return dll_name + ".dll";
    }

    static bool pe_fix_up_iat(memory_system *mem, hle::lib_manager &mngr, std::uint32_t *iat_pointer,
        kernel::process *pr, loader::e32img_import_block &import_block, uint32_t &crr_idx, codeseg_ptr &parent_codeseg) {
        const std::string dll_name8 = get_real_dll_name(import_block.dll_name);
        const std::u16string dll_name = common::utf8_to_ucs2(dll_name8);

        codeseg_ptr cs = mngr.load(dll_name, pr);

        if (!cs) {
            LOG_TRACE("Can't find {}", dll_name8);
            return false;
        }

        // Add dependency for the codeseg
        if (!parent_codeseg->add_dependency(cs)) {
            LOG_ERROR("Fail to add a codeseg as dependency!");
            return false;
        }

        uint32_t *imdir = &(import_block.ordinals[0]);

        for (uint32_t i = crr_idx, j = 0;
             i < crr_idx + import_block.ordinals.size() && j < import_block.ordinals.size();
             i++, j++) {
            iat_pointer[crr_idx++] = cs->lookup(pr, import_block.ordinals[j]);
        }

        return true;
    }

    static bool elf_fix_up_import_dir(memory_system *mem, hle::lib_manager &mngr, std::uint8_t *code_addr,
        kernel::process *pr, loader::e32img_import_block &import_block, codeseg_ptr &parent_cs) {
        // LOG_INFO("Fixup for: {}", import_block.dll_name);

        const std::string dll_name8 = get_real_dll_name(import_block.dll_name);
        const std::u16string dll_name = common::utf8_to_ucs2(dll_name8);

        codeseg_ptr cs = mngr.load(dll_name, pr);

        if (!cs) {
            LOG_TRACE("Can't find {}", dll_name8);
            return false;
        }

        // Add that codeseg as our dependency
        if (parent_cs)
            parent_cs->add_dependency(cs);

        std::uint32_t *imdir = &(import_block.ordinals[0]);

        for (uint32_t i = 0; i < import_block.ordinals.size(); i++) {
            uint32_t off = imdir[i];
            uint32_t *code_ptr = reinterpret_cast<std::uint32_t *>(code_addr + off);

            uint32_t import_inf = *code_ptr;
            uint32_t ord = import_inf & 0xffff;
            uint32_t adj = import_inf >> 16;

            uint32_t export_addr;
            uint32_t val = 0;

            if (ord > 0) {
                export_addr = cs->lookup(pr, ord);
                assert(export_addr != 0);

                // The export address provided is already added with relative code/data
                // delta, so add this directly to the adjustment address
                val = export_addr + adj;
            }

            // LOG_TRACE("Writing 0x{:x} to 0x{:x}", val, me.header.code_base + off);
            write(code_ptr, val);
        }

        return true;
    }

    static void relocate_e32img(loader::e32img *img, kernel::process *pr, memory_system *mem, hle::lib_manager &mngr, codeseg_ptr cs) {
        std::uint8_t *code_base = nullptr;
        std::uint8_t *data_base = nullptr;

        uint32_t rtcode_addr = cs->get_code_run_addr(pr, &code_base);
        uint32_t rtdata_addr = cs->get_data_run_addr(pr, &data_base);

        LOG_INFO("{} runtime code: 0x{:x}", cs->name(), rtcode_addr);
        LOG_INFO("{} runtime data: 0x{:x}", cs->name(), rtdata_addr);

        img->rt_code_addr = rtcode_addr;

        // Ram code address is the run time address of the code
        uint32_t code_delta = rtcode_addr - img->header.code_base;
        uint32_t data_delta = rtdata_addr - img->header.data_base;

        relocate(img->code_reloc_section.entries, code_base, img->header.code_size, code_delta, data_delta);

        // Either .data or .bss exists, we need to relocate. Sometimes .data needs relocate too!
        if ((img->header.bss_size) || (img->header.data_size)) {
            img->rt_data_addr = rtdata_addr;
            relocate(img->data_reloc_section.entries, data_base, img->header.code_size, code_delta, data_delta);
        }

        if (img->epoc_ver == epocver::epoc6) {
            std::uint32_t track = 0;
            std::uint32_t *iat_replace_start = reinterpret_cast<std::uint32_t *>(code_base + img->header.text_size);

            for (auto &ib : img->import_section.imports) {
                pe_fix_up_iat(mem, mngr, iat_replace_start, pr, ib, track, cs);
            }
        }

        if (static_cast<int>(img->epoc_ver) >= static_cast<int>(epocver::epoc93)) {
            for (auto &ib : img->import_section.imports) {
                elf_fix_up_import_dir(mem, mngr, code_base, pr, ib, cs);
            }
        }
    }

    static codeseg_ptr import_e32img(loader::e32img *img, memory_system *mem, kernel_system *kern, hle::lib_manager &mngr, kernel::process *pr,
        const std::u16string &path = u"", const address force_code_addr = 0) {
        std::uint32_t data_seg_size = img->header.data_size + img->header.bss_size;
        kernel::codeseg_create_info info;

        info.full_path = path;
        info.uids[0] = static_cast<std::uint32_t>(img->header.uid1);
        info.uids[1] = img->header.uid2;
        info.uids[2] = img->header.uid3;
        info.code_base = img->header.code_base;
        info.data_base = img->header.data_base;
        info.code_size = img->header.code_size;
        info.data_size = img->header.data_size;
        info.text_size = img->header.text_size;
        info.bss_size = img->header.bss_size;
        info.entry_point = img->header.entry_point;
        info.export_table = img->ed.syms;

        if (img->has_extended_header) {
            info.sinfo.caps_u[0] = img->header_extended.info.cap1;
            info.sinfo.caps_u[1] = img->header_extended.info.cap2;
            info.sinfo.vendor_id = img->header_extended.info.vendor_id;
            info.sinfo.secure_id = img->header_extended.info.secure_id;

            if (img->has_extended_header && (img->header_extended.exception_des & 1)) {
                info.exception_descriptor = img->header_extended.exception_des - 1;
            } else {
                info.exception_descriptor = 0;
            }
        }

        info.constant_data = reinterpret_cast<std::uint8_t *>(&img->data[img->header.data_offset]);
        info.code_data = reinterpret_cast<std::uint8_t *>(&img->data[img->header.code_offset]);

        if (force_code_addr != 0) {
            info.code_load_addr = force_code_addr;
        }

        codeseg_ptr cs = kern->create<kernel::codeseg>(common::ucs2_to_utf8(eka2l1::filename(path)), info);
        if (!cs) {
            LOG_ERROR("E32 image loading failed!");
            return nullptr;
        }

        cs->attach(pr);

        mngr.run_codeseg_loaded_callback(common::ucs2_to_utf8(eka2l1::replace_extension(eka2l1::filename(path),
            u"")), pr, cs);

        relocate_e32img(img, pr, mem, mngr, cs);
        return cs;
    }

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

    static void patch_rom_export(memory_system *mem, codeseg_ptr source_seg,
        codeseg_ptr dest_seg, const std::uint32_t source_export,
        const std::uint32_t dest_export) {
        const address source_ptr = source_seg->lookup(nullptr, source_export);
        const address dest_ptr = dest_seg->lookup(nullptr, dest_export);

        std::uint8_t *source_ptr_host = reinterpret_cast<std::uint8_t *>(mem->get_real_pointer(source_ptr & ~1));

        if (!source_ptr_host) {
            LOG_WARN("Unable to patch export {} of {} due to export not exist", source_export, source_seg->name());
            return;
        }

        if (source_ptr & 1) {
            std::memcpy(source_ptr_host, THUMB_TRAMPOLINE_ASM, sizeof(THUMB_TRAMPOLINE_ASM));

            // It's thumb
            // Hope it's big enough
            if (((source_ptr & ~1) & 3) == 0) {
                source_ptr_host -= 2;
            }

            *reinterpret_cast<std::uint32_t *>(source_ptr_host + sizeof(THUMB_TRAMPOLINE_ASM)) = dest_ptr;
        } else {
            // ARM!!!!!!!!!
            std::memcpy(source_ptr_host, ARM_TRAMPOLINE_ASM, sizeof(ARM_TRAMPOLINE_ASM));
            *reinterpret_cast<std::uint32_t *>(source_ptr_host + sizeof(ARM_TRAMPOLINE_ASM)) = dest_ptr;
        }
    }

    static void patch_original_codeseg(common::ini_section &section, memory_system *mem, codeseg_ptr source_seg,
        codeseg_ptr dest_seg) {
        for (auto &pair_node : section) {
            common::ini_pair *pair = pair_node->get_as<common::ini_pair>();
            const std::uint32_t source_export = pair->key_as<std::uint32_t>();

            std::uint32_t dest_export = 0;
            pair->get(&dest_export, 1, 0);

            if (source_seg->is_rom()) {
                patch_rom_export(mem, source_seg, dest_seg, source_export, dest_export);
            }

            source_seg->set_export(source_export, dest_seg->lookup(nullptr, dest_export));
        }
    }

    void lib_manager::load_patch_libraries(const std::string &patch_folder) {
        common::dir_iterator iterator(patch_folder);
        common::dir_entry entry;

        while (iterator.next_entry(entry) == 0) {
            if (common::lowercase_string(eka2l1::path_extension(entry.name)) == ".dll") {
                // Try loading original ROM segment
                codeseg_ptr original_sec = load(common::utf8_to_ucs2(eka2l1::filename(entry.name)),
                    nullptr);

                if (!original_sec) {
                    LOG_ERROR("Unable to find original code segment for {}", eka2l1::filename(entry.name));
                    continue;
                }

                const std::string patch_dll_path = eka2l1::add_path(patch_folder, entry.name);

                // We want to patch ROM image though. Do it.
                auto e32imgfile = eka2l1::physical_file_proxy(patch_dll_path, READ_MODE | BIN_MODE);
                eka2l1::ro_file_stream image_data_stream(e32imgfile.get());

                // Try to load them to ROM section
                auto e32img = loader::parse_e32img(reinterpret_cast<common::ro_stream *>(&image_data_stream));

                if (!e32img) {
                    // Ignore.
                    continue;
                }

                // Create the code chunk in ROM
                kernel::chunk *code_chunk = kern_->create<kernel::chunk>(kern_->get_memory_system(), nullptr, "",
                    0, static_cast<eka2l1::address>(e32img->header.code_size), e32img->header.code_size, prot::read_write_exec,
                    kernel::chunk_type::normal, kernel::chunk_access::rom, kernel::chunk_attrib::anonymous);

                if (!code_chunk) {
                    continue;
                }

                // Relocate imports (yes we want to make codeseg object think this is a ROM image)
                const std::uint32_t code_delta = code_chunk->base(nullptr).ptr_address() - e32img->header.code_base;

                for (auto &export_entry : e32img->ed.syms) {
                    export_entry += code_delta;
                }

                memory_system *mem = kern_->get_memory_system();

                // Relocate! Import
                std::memcpy(code_chunk->host_base(), e32img->data.data() + e32img->header.code_offset, e32img->header.code_size);
                codeseg_ptr patch_seg = import_e32img(&e32img.value(), mem, kern_, *this, nullptr, u"", code_chunk->base(nullptr).ptr_address());

                if (!patch_seg) {
                    continue;
                }

                // Look for map file. These describes the export maps.
                // This function will replace original ROM subroutines with route to these functions.
                const std::string map_path = eka2l1::replace_extension(patch_dll_path, ".map");
                common::ini_file map_file_parser;
                map_file_parser.load(map_path.c_str());

                // Patch out the shared segment first
                common::ini_section *shared_section = map_file_parser.find("shared")->get_as<common::ini_section>();

                if (shared_section) {
                    patch_original_codeseg(*shared_section, mem, original_sec, patch_seg);
                } else {
                    LOG_TRACE("Shared section not found for patch DLL {}", entry.name);
                }

                const char *alone_section_name = nullptr;

                switch (kern_->get_epoc_version()) {
                case epocver::epoc94:
                    alone_section_name = "epoc9v4";
                    break;

                case epocver::epoc6:
                    alone_section_name = "epoc6";
                    break;

                case epocver::epoc93:
                    alone_section_name = "epoc9v3";
                    break;

                case epocver::epoc10:
                    alone_section_name = "epoc10";
                    break;

                default:
                    break;
                }

                if (alone_section_name) {
                    common::ini_section *indi_section = map_file_parser.find(alone_section_name)->get_as<common::ini_section>();

                    if (indi_section) {
                        patch_original_codeseg(*indi_section, mem, original_sec, patch_seg);
                    } else {
                        LOG_TRACE("Seperate section not found for epoc version {} of patch DLL {}", static_cast<int>(kern_->get_epoc_version()),
                            entry.name);
                    }
                }
            }
        }
    }


    codeseg_ptr lib_manager::load_as_e32img(loader::e32img &img, kernel::process *pr, const std::u16string &path) {
        if (auto seg = kern_->pull_codeseg_by_uids(static_cast<std::uint32_t>(img.header.uid1),
                img.header.uid2, img.header.uid3)) {
            if (seg->attach(pr)) {
                relocate_e32img(&img, pr, kern_->get_memory_system(), *this, seg);
                run_codeseg_loaded_callback(seg->name(), pr, seg);
            }

            return seg;
        }

        return import_e32img(&img, mem_, kern_, *this, pr, path);
    }

    codeseg_ptr lib_manager::load_as_romimg(loader::romimg &romimg, kernel::process *pr, const std::u16string &path) {
        if (auto seg = kern_->pull_codeseg_by_ep(romimg.header.entry_point)) {
            seg->attach(pr);
            return seg;
        }

        kernel::codeseg_create_info info;

        info.full_path = path;
        info.uids[0] = romimg.header.uid1;
        info.uids[1] = romimg.header.uid2;
        info.uids[2] = romimg.header.uid3;
        info.code_base = romimg.header.code_address;
        info.data_base = romimg.header.data_bss_linear_base_address;
        info.code_load_addr = romimg.header.code_address;
        info.data_load_addr = romimg.header.data_address;
        info.code_size = romimg.header.code_size;
        info.data_size = romimg.header.data_size;
        info.text_size = romimg.header.text_size;
        info.entry_point = romimg.header.entry_point;
        info.bss_size = romimg.header.bss_size;
        info.export_table = romimg.exports;
        info.sinfo.caps_u[0] = romimg.header.sec_info.cap1;
        info.sinfo.caps_u[1] = romimg.header.sec_info.cap2;
        info.sinfo.vendor_id = romimg.header.sec_info.vendor_id;
        info.sinfo.secure_id = romimg.header.sec_info.secure_id;
        info.exception_descriptor = romimg.header.exception_des;
        info.constant_data = reinterpret_cast<std::uint8_t *>(mem_->get_real_pointer(romimg.header.data_address));

        auto cs = kern_->create<kernel::codeseg>("codeseg", info);
        cs->attach(pr);

        run_codeseg_loaded_callback(common::ucs2_to_utf8(eka2l1::replace_extension(eka2l1::filename(path), u"")),
            pr, cs);

        struct dll_ref_table {
            std::uint16_t flags;
            std::uint16_t num_entries;
            std::uint32_t rom_img_headers_ref[25];
        };

        // Find dependencies
        std::function<void(loader::rom_image_header *, codeseg_ptr)> dig_dependencies;
        dig_dependencies = [&](loader::rom_image_header *header, codeseg_ptr acs) {
            if (header->dll_ref_table_address != 0) {
                dll_ref_table *ref_table = eka2l1::ptr<dll_ref_table>(header->dll_ref_table_address).get(mem_);

                for (std::uint16_t i = 0; i < ref_table->num_entries; i++) {
                    loader::rom_image_header *ref_header = nullptr;
                    address entry_point = 0;

                    if (kern_->is_eka1()) {
                        // See sf.os.buildtools, release.txt of elf2e32.
                        // A mentioned by Morgan tell an dll ref entry is made up of entry point and
                        // another address to that entry point's DLL ref table. Each is 4 bytes
                        entry_point = ref_table->rom_img_headers_ref[i * 2];
                    } else {
                        ref_header = eka2l1::ptr<loader::rom_image_header>(ref_table->rom_img_headers_ref[i])
                                                               .get(mem_);

                        entry_point = ref_header->entry_point;
                    }

                    if (auto ref_seg = kern_->pull_codeseg_by_ep(entry_point)) {
                        // Add ref
                        acs->add_dependency(ref_seg);
                    } else {
                        address romimg_addr = 0;
                        address ep_org = entry_point;

                        if (kern_->is_eka1()) {
                            // Look for entry point word, from there trace back to the ROM image header
                            // Align down entry point address to word size
                            entry_point = common::align(entry_point, 4, 0);
                            address *the_word = eka2l1::ptr<address>(entry_point).get(mem_);

                            static constexpr std::uint32_t MAX_BACK_TRACE = 100;
                            std::uint32_t traced = 0;

                            while (traced < MAX_BACK_TRACE) {
                                if (*(the_word - traced) == ep_org) {
                                    break;
                                }

                                traced++;
                            }

                            if (*(the_word - traced) != ep_org) {
                                LOG_ERROR("Unable to find DLL image for entry point address: 0x{:X}", ep_org);
                            } else {
                                entry_point -= traced * sizeof(address);
                                the_word -= traced;

                                traced = 0;
                                
                                // The header has two variables: entry point and code address. They may be same, so looks on the furthest
                                while ((traced < MAX_BACK_TRACE) && (*(the_word - traced - 1) == ep_org)) {
                                    traced++;
                                }
                                
                                romimg_addr = entry_point - traced * sizeof(address) - offsetof(loader::rom_image_header, entry_point);
                            }
                        } else {
                            romimg_addr = ref_table->rom_img_headers_ref[i];
                        }

                        if (romimg_addr != 0) {
                            // TODO: Supply right size. The loader doesn't care about size right now
                            common::ro_buf_stream buf_stream(eka2l1::ptr<std::uint8_t>(romimg_addr).get(mem_), 0xFFFF);

                            // Load new romimage and add dependency
                            loader::romimg rimg = *loader::parse_romimg(reinterpret_cast<common::ro_stream *>(&buf_stream), mem_);
                            acs->add_dependency(load_as_romimg(rimg, pr));
                        }
                    }
                }
            }
        };

        dig_dependencies(&romimg.header, cs);

        return cs;
    }

    std::pair<std::optional<loader::e32img>, std::optional<loader::romimg>> lib_manager::try_search_and_parse(const std::u16string &path) {
        std::u16string lib_path = path;

        // Try opening e32img, if fail, try open as romimg
        auto open_and_get = [&](const std::u16string &path) -> std::pair<std::optional<loader::e32img>, std::optional<loader::romimg>> {
            std::pair<std::optional<loader::e32img>, std::optional<loader::romimg>>
                result{ std::nullopt, std::nullopt };

            if (io_->exist(lib_path)) {
                symfile f = io_->open_file(path, READ_MODE | BIN_MODE);
                if (!f) {
                    return result;
                }

                eka2l1::ro_file_stream image_data_stream(f.get());

                auto parse_result = loader::parse_e32img(reinterpret_cast<common::ro_stream *>(&image_data_stream));
                if (parse_result != std::nullopt) {
                    f->close();
                    result.first = std::move(parse_result);

                    return result;
                }

                image_data_stream.seek(0, common::seek_where::beg);
                auto parse_result_2 = loader::parse_romimg(reinterpret_cast<common::ro_stream *>(&image_data_stream), mem_);
                if (parse_result_2 != std::nullopt) {
                    f->close();
                    result.second = std::move(parse_result_2);

                    return result;
                }

                f->close();
            }

            return std::pair<std::optional<loader::e32img>, std::optional<loader::romimg>>{};
        };

        if (!eka2l1::has_root_dir(lib_path)) {
            // Nope ? We need to cycle through all possibilities
            for (drive_number drv = drive_z; drv >= drive_a; drv = static_cast<drive_number>(static_cast<int>(drv) - 1)) {
                lib_path = drive_to_char16(drv);
                lib_path += (kern_->is_eka1()) ? u":\\System\\Libs\\" : u":\\Sys\\Bin\\";
                lib_path += path;

                auto result = open_and_get(lib_path);
                if (result.first != std::nullopt || result.second != std::nullopt) {
                    return result;
                }
            }

            return std::pair<std::optional<loader::e32img>, std::optional<loader::romimg>>{};
        }

        return open_and_get(lib_path);
    }

    codeseg_ptr lib_manager::load(const std::u16string &name, kernel::process *pr) {
        auto load_depend_on_drive = [&](drive_number drv, const std::u16string &lib_path) -> codeseg_ptr {
            auto entry = io_->get_drive_entry(drv);

            if (entry) {
                symfile f = io_->open_file(lib_path, READ_MODE | BIN_MODE);
                if (!f) {
                    return nullptr;
                }

                eka2l1::ro_file_stream image_data_stream(f.get());

                if (entry->media_type == drive_media::rom && io_->is_entry_in_rom(lib_path)) {
                    auto romimg = loader::parse_romimg(reinterpret_cast<common::ro_stream *>(&image_data_stream), mem_);
                    if (!romimg) {
                        return nullptr;
                    }

                    return load_as_romimg(*romimg, pr, lib_path);
                } else {
                    auto e32img = loader::parse_e32img(reinterpret_cast<common::ro_stream *>(&image_data_stream));
                    if (!e32img) {
                        return nullptr;
                    }

                    return load_as_e32img(*e32img, pr, lib_path);
                }
            }

            return nullptr;
        };

        std::u16string lib_path = name;

        // Create a new codeseg, we should try search these files
        // Absolute yet ?
        if (!eka2l1::has_root_dir(lib_path)) {
            // Nope ? We need to cycle through all possibilities
            for (drive_number drv = drive_z; drv >= drive_a; drv = static_cast<drive_number>(static_cast<int>(drv) - 1)) {
                lib_path = drive_to_char16(drv);
                lib_path += (kern_->is_eka1()) ? u":\\System\\Libs\\" : u":\\Sys\\Bin\\";
                lib_path += name;

                if (io_->exist(lib_path)) {
                    auto result = load_depend_on_drive(drv, lib_path);
                    if (result != nullptr) {
                        result->set_full_path(lib_path);
                        return result;
                    }
                }
            }

            return nullptr;
        }

        drive_number drv = char16_to_drive(lib_path[0]);
        if (!io_->exist(lib_path)) {
            return nullptr;
        }

        if (auto cs = load_depend_on_drive(drv, lib_path)) {
            cs->set_full_path(lib_path);
            return cs;
        }

        return nullptr;
    }

    bool lib_manager::call_svc(sid svcnum) {
        // Lock the kernel so SVC call can operate in safety
        kern_->lock();
        auto res = svc_funcs_.find(svcnum);

        if (res == svc_funcs_.end()) {
            LOG_ERROR("Unimplement system call: 0x{:X}!", svcnum);

            kern_->unlock();
            return false;
        }

        epoc_import_func func = res->second;

        if (kern_->get_config()->log_svc) {
            LOG_TRACE("Calling SVC 0x{:x} {}", svcnum, func.name);
        }

        func.func(kern_, kern_->crr_process(), kern_->get_cpu());

        kern_->unlock();
        return true;
    }

    std::size_t lib_manager::register_codeseg_loaded_callback(codeseg_loaded_callback callback) {
        return codeseg_loaded_callback_funcs_.add(callback);
    }

    bool lib_manager::unregister_codeseg_loaded_callback(const std::size_t handle) {
        return codeseg_loaded_callback_funcs_.remove(handle);
    }

    void lib_manager::run_codeseg_loaded_callback(const std::string &lib_name, kernel::process *attacher, codeseg_ptr target) {
        for (auto &codeseg_loaded_callback_func: codeseg_loaded_callback_funcs_) {
            codeseg_loaded_callback_func(lib_name, attacher, target);
        }
    }

    bool lib_manager::build_eka1_thread_bootstrap_code() {    
        static constexpr const char *BOOTSTRAP_CHUNK_NAME = "EKA1ThreadBootstrapCodeChunk";
        bootstrap_chunk_ = kern_->create<kernel::chunk>(kern_->get_memory_system(), nullptr, BOOTSTRAP_CHUNK_NAME,
            0, 0x1000, 0x1000, prot::read_write_exec, kernel::chunk_type::normal, kernel::chunk_access::rom,
            kernel::chunk_attrib::none);

        if (!bootstrap_chunk_) {
            LOG_ERROR("Failed to create bootstrap chunk for EKA1 thread!");
            return false;
        }
        
        codeseg_ptr userlib = load(u"euser.dll", nullptr);

        if (!userlib) {
            LOG_ERROR("Unable to load euser.dll to build EKA1 bootstrap code!");
            return false;
        }

        static constexpr std::uint32_t CHUNK_HEAP_CREATE_FUNC_ORDINAL = 166;
        static constexpr std::uint32_t THREAD_EXIT_FUNC_ORDINAL = 397;
        static constexpr std::uint32_t HEAP_SWITCH_FUNC_ORDINAL = 1127;

        const address chunk_heap_create_func_addr = userlib->lookup(nullptr, CHUNK_HEAP_CREATE_FUNC_ORDINAL);
        const address heap_switch_func_addr = userlib->lookup(nullptr, HEAP_SWITCH_FUNC_ORDINAL);
        const address thread_exit_func_addr = userlib->lookup(nullptr, THREAD_EXIT_FUNC_ORDINAL);

        common::cpu_info info;
        info.bARMv7 = false;
        info.bASIMD = false;

        common::armgen::armx_emitter emitter(reinterpret_cast<std::uint8_t*>(bootstrap_chunk_->host_base()), info);

        // SP should points to the thread info struct
        entry_points_call_routine_ = emitter.get_code_pointer();
        
        const std::uint32_t STATIC_CALL_LIST_SVC_FAKE = 0xFE;
        const std::uint32_t TOTAL_EP_TO_ALLOC = 100;

        /* CODE FOR DLL's ENTRY POINTS INVOKE */
        emitter.PUSH(4, common::armgen::R3, common::armgen::R4, common::armgen::R5, common::armgen::R_LR);

        // Allocate entry point addresses on stack, plus also allocate the total entry point count
        emitter.MOVI2R(common::armgen::R3, TOTAL_EP_TO_ALLOC * sizeof(address) + sizeof(std::uint32_t));
        emitter.SUB(common::armgen::R_SP, common::armgen::R_SP, common::armgen::R3);
        emitter.MOVI2R(common::armgen::R3, TOTAL_EP_TO_ALLOC);     // Assign allocated entry point count
        emitter.STR(common::armgen::R3, common::armgen::R_SP);  // Store it to the count variable.
        emitter.MOV(common::armgen::R5, common::armgen::R0);    // Store dll entry point invoke reason in R5

        emitter.MOV(common::armgen::R0, common::armgen::R_SP);  // First arugment is pointer to total number of entry point
        emitter.ADD(common::armgen::R1, common::armgen::R_SP, sizeof(std::uint32_t));       // Second argument is pointer to EP array

        // The SVC call convention on EKA1 assigns PC after SVC with LR, which is horrible, we gotta follow it
        emitter.MOV(common::armgen::R_LR, common::armgen::R_PC);  // This PC will skip forwards to the POP, no need to add or sub more thing.
        emitter.SVC(STATIC_CALL_LIST_SVC_FAKE);                   // Call SVC StaticCallList, our own SVC :D
        
        emitter.MOV(common::armgen::R3, 0);                      // R3 is iterator
        emitter.LDR(common::armgen::R4, common::armgen::R_SP);   // R4 is total of entry point to iterate.
        emitter.SUB(common::armgen::R4, common::armgen::R4, 1);
        emitter.ADD(common::armgen::R_SP, common::armgen::R_SP, sizeof(std::uint32_t));     // Free our count variable
        
        std::uint8_t *loop_continue_ptr = emitter.get_writeable_code_ptr();
        emitter.CMP(common::armgen::R3, common::armgen::R4);
        
        common::armgen::fixup_branch entry_point_call_loop_done = emitter.B_CC(common::cc_flags::CC_GE);

        emitter.MOV(common::armgen::R0, common::armgen::R5);        // Move in the entry point invoke reason, in case other function trashed this out.
        emitter.LSL(common::armgen::R12, common::armgen::R3, 2);    // Calculate the offset of this entry point, 4 bytes
        emitter.ADD(common::armgen::R12, common::armgen::R12, common::armgen::R_SP);
        emitter.LDR(common::armgen::R12, common::armgen::R12);     // Jump
        emitter.BL(common::armgen::R12);
        emitter.ADD(common::armgen::R3, common::armgen::R3, 1);
        emitter.B(loop_continue_ptr);
        
        emitter.set_jump_target(entry_point_call_loop_done);

        emitter.MOVI2R(common::armgen::R3, TOTAL_EP_TO_ALLOC * sizeof(address));
        emitter.ADD(common::armgen::R_SP, common::armgen::R_SP, common::armgen::R3);
        emitter.POP(4, common::armgen::R3, common::armgen::R4, common::armgen::R5, common::armgen::R_PC);

        emitter.flush_lit_pool();

        /* CODE FOR THREAD INITIALIZATION */
        thread_entry_routine_ = emitter.get_code_pointer();

        emitter.MOV(common::armgen::R0, kernel::dll_reason_thread_attach);     // Set the first argument the DLL reason
        emitter.MOV(common::armgen::R4, common::armgen::R1);                   // Info struct in R1 to R4
        emitter.BL(entry_points_call_routine_);
        
        // Check the allocator in the thread create info
        emitter.LDR(common::armgen::R0, common::armgen::R4, offsetof(kernel::epoc9_std_epoc_thread_create_info, allocator));
        emitter.CMP(common::armgen::R0, 0);
        common::armgen::fixup_branch allocator_unavail_block = emitter.B_CC(common::CC_EQ);

        // Block: allocator available, switch it
        emitter.MOVI2R(common::armgen::R12, heap_switch_func_addr);
        emitter.BL(common::armgen::R12);
        common::armgen::fixup_branch allocator_setup_done = emitter.B();

        emitter.set_jump_target(allocator_unavail_block);
        emitter.MOV(common::armgen::R0, 0);
        emitter.LDR(common::armgen::R1, common::armgen::R4, offsetof(kernel::epoc9_std_epoc_thread_create_info, heap_min));
        emitter.LDR(common::armgen::R2, common::armgen::R4, offsetof(kernel::epoc9_std_epoc_thread_create_info, heap_max));
        emitter.MOVI2R(common::armgen::R3, 0x1000);         // Grow by

        emitter.MOVI2R(common::armgen::R12, chunk_heap_create_func_addr);
        emitter.BL(common::armgen::R12);

        // Switch the heap
        emitter.MOVI2R(common::armgen::R12, heap_switch_func_addr);
        emitter.BL(common::armgen::R12);

        emitter.set_jump_target(allocator_setup_done);

        // Jump to our friend
        // Load userdata to first argument.
        emitter.LDR(common::armgen::R0, common::armgen::R4, offsetof(kernel::epoc9_std_epoc_thread_create_info, ptr));
        emitter.LDR(common::armgen::R12, common::armgen::R4, offsetof(kernel::epoc9_std_epoc_thread_create_info, func_ptr));
        
        // Pop our struct friend from the stack
        emitter.ADD(common::armgen::R_SP, common::armgen::R_SP, sizeof(kernel::epoc9_std_epoc_thread_create_info));
        emitter.BL(common::armgen::R12);

        // Exit the thread, reason is already in R0 after calling our friend.
        emitter.MOVI2R(common::armgen::R12, thread_exit_func_addr);
        emitter.BL(common::armgen::R12);

        emitter.flush_lit_pool();

        return true;
    }

    lib_manager::lib_manager(kernel_system *kerns, io_system *ios, memory_system *mems)
        : kern_(kerns)
        , io_(ios)
        , mem_(mems)
        , bootstrap_chunk_(nullptr)
        , entry_points_call_routine_(nullptr)
        , thread_entry_routine_(nullptr) { 
        hle::symbols sb;
        std::string lib_name;

#define LIB(x) lib_name = #x;
#define EXPORT(x, y) \
    sb.push_back(x);
#define ENDLIB()                       \
    lib_symbols.emplace(lib_name, sb); \
    sb.clear();

        if (kern_->get_epoc_version() == epocver::epoc6) {
            //  #include <bridge/epoc6_n.def>
        } else {
            // #include <bridge/epoc9_n.def>
        }

#undef LIB
#undef EXPORT
#undef ENLIB

        switch (kern_->get_epoc_version()) {
        case epocver::epoc6:
            epoc::register_epocv6(*this);
            break;

        case epocver::epoc94:
            epoc::register_epocv94(*this);
            break;

        case epocver::epoc93:
            epoc::register_epocv93(*this);
            break;

        case epocver::epoc10:
            epoc::register_epocv10(*this);
            break;

        default:
            break;
        }
    }
    
    lib_manager::~lib_manager() {
        svc_funcs_.clear();
    }

    system *lib_manager::get_sys() {
        return kern_->get_system();
    }

    address lib_manager::get_entry_point_call_routine_address() const {
        if (!entry_points_call_routine_) {
            return 0;
        }

        return static_cast<address>(entry_points_call_routine_ - reinterpret_cast<std::uint8_t*>(bootstrap_chunk_->host_base())) +
            bootstrap_chunk_->base(nullptr).ptr_address();
    }

    address lib_manager::get_thread_entry_routine_address() const {
        if (!thread_entry_routine_) {
            return 0;
        }

        return static_cast<address>(thread_entry_routine_ - reinterpret_cast<std::uint8_t*>(bootstrap_chunk_->host_base())) +
            bootstrap_chunk_->base(nullptr).ptr_address();
    }
}

namespace eka2l1::epoc {
    bool get_image_info(hle::lib_manager *mngr, const std::u16string &name, epoc::lib_info &linfo) {
        auto imgs = mngr->try_search_and_parse(name);

        LOG_TRACE("Get Info of {}", common::ucs2_to_utf8(name));

        if (!imgs.first && !imgs.second) {
            return false;
        }

        if (!imgs.first && imgs.second) {
            auto &rimg = imgs.second;

            linfo.uid1 = rimg->header.uid1;
            linfo.uid2 = rimg->header.uid2;
            linfo.uid3 = rimg->header.uid3;
            linfo.secure_id = rimg->header.sec_info.secure_id;
            linfo.caps[0] = rimg->header.sec_info.cap1;
            linfo.caps[1] = rimg->header.sec_info.cap2;
            linfo.vendor_id = rimg->header.sec_info.vendor_id;
            linfo.major = rimg->header.major;
            linfo.minor = rimg->header.minor;

            return true;
        }

        auto &eimg = imgs.first;
        memcpy(&linfo.uid1, &eimg->header.uid1, 12);

        linfo.secure_id = eimg->header_extended.info.secure_id;
        linfo.caps[0] = eimg->header_extended.info.cap1;
        linfo.caps[1] = eimg->header_extended.info.cap2;
        linfo.vendor_id = eimg->header_extended.info.vendor_id;
        linfo.major = eimg->header.major;
        linfo.minor = eimg->header.minor;

        return true;
    }
}