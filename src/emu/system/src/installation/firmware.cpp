/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#include <system/devices.h>
#include <system/installation/firmware.h>
#include <system/software.h>

#include <common/algorithm.h>
#include <common/buffer.h>
#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/log.h>
#include <common/path.h>

#include <loader/fpsx.h>
#include <loader/rofs.h>
#include <loader/rom.h>

#include <fstream>
#include <pugixml.hpp>

#include <fat/fat.h>

namespace eka2l1 {
    static void extract_file(Fat::Image &img, Fat::Entry &entry, const std::string &path) {
        const std::u16string filename_16 = entry.get_filename();
        std::string filename = eka2l1::add_path(path, common::ucs2_to_utf8(filename_16));

        if (common::is_platform_case_sensitive()) {
            filename = common::lowercase_string(filename);
        }

        common::wo_std_file_stream f(filename, true);

        static constexpr std::uint32_t CHUNK_SIZE = 0x10000;

        std::vector<std::uint8_t> temp_buf;
        temp_buf.resize(CHUNK_SIZE);

        std::uint32_t size_left = entry.entry.file_size;
        std::uint32_t offset = 0;

        while (size_left != 0) {
            std::uint32_t size_to_take = std::min<std::uint32_t>(CHUNK_SIZE, size_left);
            if (img.read_from_cluster(&temp_buf[0], offset, entry.entry.starting_cluster, size_to_take) != size_to_take) {
                break;
            }

            size_left -= size_to_take;
            offset += size_to_take;

            f.write(reinterpret_cast<const char *>(&temp_buf[0]), size_to_take);
        }
    }

    static void extract_directory(Fat::Image &img, Fat::Entry mee, std::string dir_path) {
        common::create_directories(dir_path);

        while (img.get_next_entry(mee)) {
            if (mee.entry.file_attributes & (int)Fat::EntryAttribute::DIRECTORY) {
                // Also check if it's not the back folder (. and ..)
                // This can be done by gathering the name
                if (mee.entry.get_entry_type_from_filename() != Fat::EntryType::DIRECTORY) {
                    Fat::Entry baby;
                    if (!img.get_first_entry_dir(mee, baby))
                        break;

                    auto dir_name = mee.get_filename();

                    if (common::is_platform_case_sensitive()) {
                        dir_name = common::lowercase_ucs2_string(dir_name);
                    }

                    extract_directory(img, baby, dir_path + common::ucs2_to_utf8(dir_name) + "\\");
                }
            }

            if ((mee.entry.file_attributes & (int)Fat::EntryAttribute::ARCHIVE) || (!(mee.entry.file_attributes & (int)Fat::EntryAttribute::DIRECTORY) && (mee.entry.file_size != 0))) {
                extract_file(img, mee, dir_path);
            }
        }
    }

    static device_installation_error dump_data_from_fpsx(loader::firmware::fpsx_header &header, common::ro_stream &stream, const std::string &drives_c_path,
        const std::string &drives_e_path, const std::string &drives_z_path, const std::string &rom_resident_path,
        progress_changed_callback progress_cb, cancel_requested_callback cancel_cb) {
        if (header.type_ == loader::firmware::FPSX_TYPE_INVALID) {
            if (progress_cb)
                progress_cb(1, 1);
            return device_installation_none;
        }

        if (cancel_cb && cancel_cb()) {
            return device_installation_general_failure;
        }

        std::vector<char> buf;

        if (header.type_ == loader::firmware::FPSX_TYPE_CORE) {
            const std::string rom_path = eka2l1::add_path(rom_resident_path, "SYM_TEMP.ROM");
            const std::string rom_final_path = eka2l1::add_path(rom_resident_path, "SYM.ROM");

            {
                common::wo_std_file_stream rom_stream(rom_path, true);
                std::uint32_t ignore_bootstrap_left = 0xC00;

                for (auto &root : header.btree_.roots_) {
                    if (root.cert_blocks_.size() == 0) {
                        continue;
                    }

                    if (root.cert_blocks_[0].btype_ == loader::firmware::BLOCK_TYPE_ROFS_HASH) {
                        loader::firmware::block_header_rofs_hash &header_rofs = static_cast<decltype(header_rofs)>(*root.cert_blocks_[0].header_);

                        const std::string stt = header_rofs.description_;
                        bool canceled = false;

                        if (stt.find("CORE") != std::string::npos) {
                            for (std::size_t i = 0; i < root.code_blocks_.size(); i++) {
                                if (cancel_cb && cancel_cb()) {
                                    canceled = true;
                                    break;
                                }
                                std::uint32_t skip_taken = 0;

                                if (ignore_bootstrap_left != 0) {
                                    if (root.code_blocks_[i].header_->data_size_ <= ignore_bootstrap_left) {
                                        skip_taken = std::min<std::uint32_t>(ignore_bootstrap_left, root.code_blocks_[i].header_->data_size_);

                                        if (skip_taken == root.code_blocks_[i].header_->data_size_)
                                            continue;
                                    } else {
                                        skip_taken = ignore_bootstrap_left;
                                    }
                                }

                                stream.seek(root.code_blocks_[i].data_offset_in_stream_ + skip_taken, eka2l1::common::seek_where::beg);
                                buf.resize(root.code_blocks_[i].header_->data_size_ - skip_taken);

                                stream.read(buf.data(), buf.size());
                                rom_stream.write(buf.data(), buf.size());

                                ignore_bootstrap_left -= skip_taken;
                            }
                        }

                        if (cancel_cb && cancel_cb()) {
                            return device_installation_general_failure;
                        }
                    }
                }
            }

            int defrag_result = 0;

            {
                common::ro_std_file_stream rom_read_stream(rom_path, true);
                common::wo_std_file_stream rom_defraged_stream(rom_final_path, true);

                defrag_result = loader::defrag_rom(reinterpret_cast<common::ro_stream *>(&rom_read_stream),
                    reinterpret_cast<common::wo_stream *>(&rom_defraged_stream));
            }

            if (defrag_result < 0) {
                LOG_ERROR(SYSTEM, "Rom defrag failed with error: {}", defrag_result);
                return device_installation_rom_file_corrupt;
            } else {
                if (defrag_result == 0) {
                    common::remove(rom_final_path);
                    common::move_file(rom_path, rom_final_path);
                } else {
                    common::remove(rom_path);
                }
            }

            common::ro_std_file_stream rom_read_stream(rom_final_path, true);

            if (!loader::dump_rom_files(reinterpret_cast<common::ro_stream *>(&rom_read_stream),
                    drives_z_path, progress_cb, cancel_cb)) {
                common::remove(rom_final_path);
                return device_installation_rom_file_corrupt;
            }
        }

        std::string image_path = eka2l1::add_path(rom_resident_path, "TEMP.IMG");

        {
            common::wo_std_file_stream rom_stream(image_path, true);

            loader::firmware::block_tree_entry *appropiate_block = nullptr;
            if (header.type_ == loader::firmware::FPSX_TYPE_UDA) {
                appropiate_block = &header.btree_.roots_[0];
            } else {
                appropiate_block = header.find_block_with_description("ROFS");
                if (!appropiate_block) {
                    appropiate_block = header.find_block_with_description("ROFX");
                }
                if (!appropiate_block) {
                    appropiate_block = header.find_block_with_description("MCUSW");
                }
            }

            auto &blocks = appropiate_block->code_blocks_;
            bool flag = true;

            for (auto &uda_bin_block : blocks) {
                if (uda_bin_block.ctype_ == loader::firmware::CONTENT_TYPE_CODE) {
                    stream.seek(uda_bin_block.data_offset_in_stream_, eka2l1::common::seek_where::beg);
                    buf.resize(uda_bin_block.header_->data_size_);

                    stream.read(buf.data(), buf.size());

                    if (flag) {
                        std::uint32_t size_start_write = 0;
                        while ((size_start_write < buf.size()) && (buf[size_start_write] == 0)) {
                            size_start_write++;
                        }

                        if (buf.size() != size_start_write) {
                            rom_stream.write(buf.data() + size_start_write, buf.size() - size_start_write);
                            flag = false;
                        }
                    } else {
                        rom_stream.write(buf.data(), buf.size());
                    }
                }
            }
        }

        // What to do with it now?
        if (header.type_ == loader::firmware::FPSX_TYPE_UDA) {
            // Extract the FAT image, with some twists
            // I was using ifstream, but not sure why it fucked up
            common::ro_std_file_stream fat_image_file(image_path, true);
            Fat::Image fat_img(
                &fat_image_file,
                // Read hook
                [](void *userdata, void *buffer, std::uint32_t size) -> std::uint32_t {
                    common::ro_std_file_stream *stream = reinterpret_cast<common::ro_std_file_stream *>(userdata);
                    return static_cast<std::uint32_t>(stream->read(buffer, size));
                },
                // Seek hook
                [](void *userdata, std::uint32_t offset, int mode) -> std::uint32_t {
                    common::ro_std_file_stream *stream = reinterpret_cast<common::ro_std_file_stream *>(userdata);
                    stream->seek(offset, (mode == Fat::IMAGE_SEEK_MODE_BEG ? common::seek_where::beg : (mode == Fat::IMAGE_SEEK_MODE_CUR ? common::seek_where::cur : common::seek_where::end)));

                    return static_cast<std::uint32_t>(stream->tell());
                });

            std::string fat_dump_base;
            fat_dump_base = drives_c_path;

            Fat::Entry bootstrap_entry;
            extract_directory(fat_img, bootstrap_entry, fat_dump_base);

            if (progress_cb)
                progress_cb(1, 1);
        } else {
            // Extract the ROFS image
            common::ro_std_file_stream rofs_img_stream(image_path, true);

            // Dump quality ROFS content!!!!!
            if (!loader::dump_rofs_system(rofs_img_stream, drives_z_path, progress_cb, cancel_cb)) {
                LOG_ERROR(SYSTEM, "Error while dumping ROFS!");
                return device_installation_rofs_corrupt;
            }
        }

        // Remove the image, no need it no more :((
        eka2l1::common::remove(image_path);
        return device_installation_none;
    }

    device_installation_error install_firmware(device_manager *dvcmngr, const std::string &vpl_path,
        const std::string &drives_c_path, const std::string &drives_e_path, const std::string &drives_z_path,
        const std::string &rom_resident_path, device_firmware_choose_variant_callback choose_callback,
        progress_changed_callback progress_callback, cancel_requested_callback cancel_callback) {
        std::string cur_dir;
        if (!common::get_current_directory(cur_dir)) {
            LOG_ERROR(SYSTEM, "Can't get current directory!");
            return device_installation_general_failure;
        }

        const std::string full_vpl = eka2l1::absolute_path(vpl_path, cur_dir);
        const std::string full_folder = eka2l1::file_directory(full_vpl);

        common::ro_std_file_stream doc_stream(full_vpl, true);

        std::vector<std::uint8_t> temp_buf;
        temp_buf.resize(doc_stream.size());
        doc_stream.read(temp_buf.data(), temp_buf.size());

        pugi::xml_document document;
        const pugi::xml_parse_result parse_result = document.load_buffer(temp_buf.data(), temp_buf.size());

        if (!parse_result) {
            LOG_ERROR(SYSTEM, "VPL file parse failed, description: {}", parse_result.description());
            return device_installation_vpl_file_invalid;
        }

        pugi::xml_node variant_list = document.child("VariantPackingList");
        if (!variant_list) {
            LOG_ERROR(SYSTEM, "VPL file does not have variant tracking list");
            return device_installation_vpl_file_invalid;
        }

        auto all_variants = variant_list.children("Variant");
        std::vector<std::string> all_variant_strings;

        for (auto &variant : all_variants) {
            pugi::xml_node variant_identification = variant.child("VariantIdentification");
            if (!variant_identification) {
                LOG_ERROR(SYSTEM, "VPL variant does not contain identification!");
                return device_installation_vpl_file_invalid;
            }

            // Get description string
            pugi::xml_node variant_id_des = variant_identification.child("Description");
            if (!variant_id_des) {
                LOG_ERROR(SYSTEM, "VPL variant does not contain product description");
                return device_installation_vpl_file_invalid;
            }

            std::string des_string = variant_id_des.first_child().value();
            if (auto firm_code = variant_identification.child("ProductCode")) {
                des_string += " (" + std::string(firm_code.first_child().value()) + ")";
            }

            all_variant_strings.push_back(des_string);
        }

        const int variant_index = choose_callback(all_variant_strings);
        if ((variant_index >= all_variant_strings.size()) || (variant_index < 0)) {
            LOG_ERROR(SYSTEM, "Invalid variant index!");
            return device_installation_general_failure;
        }

        pugi::xml_node final_variant;
        int i = 0;

        for (auto &variant : all_variants) {
            if (i == variant_index) {
                final_variant = variant;
            } else {
                i++;
            }
        }

        if (!final_variant) {
            LOG_ERROR(SYSTEM, "Can't find variant for some reason!");
            return device_installation_general_failure;
        }

        // Get file list. And we only want to grab file with the following subtype:
        // MCU, PPM, Content (Internal drive), MemoryCardContent
        pugi::xml_node file_list = final_variant.child("FileList");
        if (!file_list) {
            LOG_ERROR(SYSTEM, "VPL variant unable to load FileList node!");
            return device_installation_vpl_file_invalid;
        }

        std::vector<std::string> filenames;
        int average_progress_max = 0;

        auto file_nodes = file_list.children("File");
        for (auto &file_node : file_nodes) {
            pugi::xml_node type_of_file = file_node.child("FileType");
            if (!type_of_file || (common::compare_ignore_case(type_of_file.first_child().value(), "Binary") != 0)) {
                continue;
            }

            std::string file_sub_type;
            pugi::xml_node file_subtype_node = file_node.child("FileSubType");
            if (!file_subtype_node) {
                continue;
            }

            file_sub_type = file_subtype_node.first_child().value();

            if ((common::compare_ignore_case(file_sub_type.c_str(), "Mcu") == 0) || (common::compare_ignore_case(file_sub_type.c_str(), "Ppm") == 0) || (common::compare_ignore_case(file_sub_type.c_str(), "Content") == 0) || (common::compare_ignore_case(file_sub_type.c_str(), "MemoryCardContent") == 0)) {
                // Get the filename
                pugi::xml_node fname = file_node.child("Name");
                if (!fname) {
                    continue;
                }

                const std::string full_fpsx_path = eka2l1::add_path(full_folder, fname.first_child().value());
                if (!common::exists(full_fpsx_path)) {
                    continue;
                }

                filenames.push_back(full_fpsx_path);
                if (common::compare_ignore_case(file_sub_type.c_str(), "Mcu") == 0) {
                    average_progress_max += 2;
                } else {
                    average_progress_max += 1;
                }
            }
        }

        if (average_progress_max == 0) {
            LOG_ERROR(SYSTEM, "Why is there nothing to install!");
            return device_installation_vpl_file_invalid;
        }

        std::string drives_z_temp_path = eka2l1::add_path(drives_z_path, "temp\\");
        std::size_t so_far = 0;

        for (auto &fpsx_filename : filenames) {
            common::ro_std_file_stream fpsx_file_stream(fpsx_filename, true);
            std::optional<loader::firmware::fpsx_header> fpsx_head = loader::firmware::read_fpsx_header(
                fpsx_file_stream);

            if (!fpsx_head) {
                LOG_ERROR(SYSTEM, "FPSX file is corrupted!");
                return device_installation_fpsx_corrupt;
            }

            progress_changed_callback wrapped_progress = nullptr;
            if (progress_callback) {
                wrapped_progress = [progress_callback, average_progress_max, so_far](const std::size_t taken, const std::size_t max) {
                    return progress_callback(so_far * (100 / average_progress_max) + taken * (100 / average_progress_max) / max, 100);
                };
            }

            const auto result = dump_data_from_fpsx(fpsx_head.value(), fpsx_file_stream, drives_c_path, drives_e_path, drives_z_temp_path,
                rom_resident_path, wrapped_progress, cancel_callback);

            if (result != device_installation_none) {
                common::delete_folder(drives_z_temp_path);
                return result;
            }

            so_far++;
        }

        // Start analyze and put it into device list
        const epocver ver = loader::determine_rpkg_symbian_version(drives_z_temp_path);

        std::string manufacturer;
        std::string firmcode;
        std::string model;

        if (!loader::determine_rpkg_product_info(drives_z_temp_path, manufacturer, firmcode, model)) {
            LOG_ERROR(SYSTEM, "Revert all changes");
            eka2l1::common::delete_folder(drives_z_temp_path);

            return device_installation_determine_product_failure;
        }

        const std::string current_temp_rom = eka2l1::add_path(rom_resident_path, "SYM.ROM");

        if (dvcmngr->get(firmcode)) {
            LOG_ERROR(SYSTEM, "The device already exists, revert all changes");
            eka2l1::common::delete_folder(drives_z_temp_path);
            eka2l1::common::remove(current_temp_rom);

            return device_installation_already_exist;
        }

        auto firmcode_low = common::lowercase_string(firmcode);

        // Rename temp folder to its product code
        common::move_file(drives_z_temp_path, add_path(drives_z_path, firmcode_low + "\\"));
        const add_device_error err_adddvc = dvcmngr->add_new_device(firmcode, model, manufacturer, ver, 0);

        if (err_adddvc != add_device_none) {
            LOG_ERROR(SYSTEM, "This device ({}) failed to be install, revert all changes", firmcode);
            eka2l1::common::delete_folder(add_path(drives_z_path, firmcode_low + "\\"));
            eka2l1::common::remove(current_temp_rom);

            return device_installation_general_failure;
        }

        const std::string target_rom_path = eka2l1::add_path(rom_resident_path, firmcode_low + "\\SYM.ROM");
        common::create_directories(eka2l1::file_directory(target_rom_path));
        common::move_file(current_temp_rom, target_rom_path);

        if (progress_callback) {
            progress_callback(1, 1);
        }

        return device_installation_none;
    }
}
