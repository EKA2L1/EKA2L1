/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <common/log.h>
#include <services/ui/skin/skn.h>

namespace eka2l1::epoc {
    skn_file::skn_file(common::ro_stream *stream, plat_ver platform_version, language lang)
        : master_chunk_size_(0)
        , master_chunk_count_(0)
        , crr_filename_id_(0)
        , stream_(stream)
        , ver_(std::move(platform_version))
        , importer_lang_(lang) {
        if (!read_master_chunk()) {
            LOG_ERROR(SERVICE_UI, "Reading master chunk failed!");
        }
    }

    bool skn_file::read_master_chunk() {
        if (stream_->read(skn_desc_dfo_common_len, &master_chunk_size_, 4) != 4) {
            return false;
        }

        std::uint16_t master_chunk_type = 0;

        if (stream_->read(skn_desc_dfo_common_type, &master_chunk_type, 2) != 2) {
            return false;
        }

        if (master_chunk_type != as_desc_skin_desc) {
            LOG_ERROR(SERVICE_UI, "Master chunk of file corrupted!");
            return false;
        }

        if (stream_->read(skn_desc_dfo_skin_chunks_count, &master_chunk_count_, 4) != 4) {
            return false;
        }

        process_chunks(skn_desc_dfo_skin_content, master_chunk_count_);

        return true;
    }

    bool skn_file::process_chunks(std::uint32_t base_offset, const std::int32_t count) {
        for (std::int32_t i = 0; i < count; i++) {
            std::uint32_t chunk_size = 0;
            std::uint16_t chunk_type = 0;

            if (stream_->read(base_offset + skn_desc_dfo_common_len, &chunk_size, 4) != 4) {
                return false;
            }

            if (stream_->read(base_offset + skn_desc_dfo_common_type, &chunk_type, 2) != 2) {
                return false;
            }

            switch (chunk_type) {
            case as_desc_info: {
                base_offset += handle_info_chunk(base_offset, info_);
                break;
            }

            case as_desc_name: {
                base_offset += handle_name_chunk(base_offset, skin_name_);
                break;
            }

            case as_desc_filename: {
                base_offset += handle_filename_chunk(base_offset);
                break;
            }

            case as_desc_skin_desc_class: {
                base_offset += handle_class_chunk(base_offset);
                break;
            }

            case as_desc_release26: {
                base_offset += handle_release_26_restriction_chunk(base_offset);
                break;
            }

            case as_desc_release_generic: {
                base_offset += handle_release_generic_restriction_chunk(base_offset);
                break;
            }

            case as_desc_lang: {
                base_offset += handle_lang_restriction_chunk(base_offset);
                break;
            }

            default: {
                LOG_ERROR(SERVICE_UI, "Unhandled chunk type: {}", chunk_type);
                base_offset += chunk_size;

                break;
            }
            }
        }

        return true;
    }

    std::uint32_t skn_file::handle_info_chunk(std::uint32_t base_offset, skn_file_info &info) {
        std::uint32_t chunk_size = 0;

        stream_->read(base_offset + skn_desc_dfo_common_len, &chunk_size, 4);
        stream_->read(base_offset + skn_desc_dfo_info_compiler_ver, &info.version, 4);

        std::uint16_t author_str_len = 0;
        stream_->read(base_offset + skn_desc_dfo_info_author_len, &author_str_len, 2);

        info.author.resize(author_str_len);

        author_str_len *= 2;
        stream_->read(base_offset + skn_desc_dfo_info_author_str, &info.author[0], author_str_len);

        std::uint16_t copyright_str_len = 0;
        stream_->read(base_offset + skn_desc_dfo_info_author_str + author_str_len, &copyright_str_len, 2);

        info.copyright.resize(copyright_str_len);

        copyright_str_len *= 2;
        stream_->read(base_offset + skn_desc_dfo_info_author_str + author_str_len + 2, &info.copyright[0], copyright_str_len);
        stream_->read(base_offset + skn_desc_dfo_info_author_str + author_str_len + 2 + copyright_str_len, &info.plat, 2);

        return chunk_size;
    }

    std::uint32_t skn_file::handle_name_chunk(std::uint32_t base_offset, skn_name &name) {
        std::uint32_t chunk_size = 0;
        stream_->read(base_offset + skn_desc_dfo_common_len, &chunk_size, 4);
        stream_->read(base_offset + skn_desc_dfo_name_lang, &name.lang, 2);

        std::uint16_t name_len = 0;
        stream_->read(base_offset + skn_desc_dfo_name_name_Len, &name_len, 2);

        name.name.resize(name_len);
        stream_->read(base_offset + skn_desc_dfo_name_name, &name.name[0], name_len * 2);

        return chunk_size;
    }

    std::uint32_t skn_file::handle_filename_chunk(std::uint32_t base_offset) {
        std::uint32_t chunk_size = 0;
        stream_->read(base_offset + skn_desc_dfo_common_len, &chunk_size, 4);

        std::int32_t id = 0;
        stream_->read(base_offset + skn_desc_dfo_filename_filename_id, &id, 4);

        id += crr_filename_id_;

        std::uint16_t strlen = 0;
        stream_->read(base_offset + skn_desc_dfo_filename_len, &strlen, 2);

        std::u16string name;
        name.resize(strlen);

        stream_->read(base_offset + skn_desc_dfo_filename_filename, &name[0], strlen * 2);

        filenames_.emplace(id, std::move(name));

        return chunk_size;
    }

    std::uint32_t skn_file::handle_class_chunk(std::uint32_t base_offset) {
        std::uint32_t chunk_size = 0;
        stream_->read(base_offset + skn_desc_dfo_common_len, &chunk_size, 4);

        std::uint32_t count = 0;
        stream_->read(base_offset + skn_desc_dfo_class_chunk_n, &count, 4);

        process_class_def_chunks(base_offset + skn_desc_dfo_class_content, count);

        return chunk_size;
    }

    void skn_file::process_class_def_chunks(std::uint32_t base_offset, std::int32_t chunk_count) {
        for (std::int32_t i = 0; i < chunk_count; i++) {
            std::uint32_t chunk_size = 0;
            stream_->read(base_offset + skn_desc_dfo_common_len, &chunk_size, 4);

            std::uint16_t chunk_type = 0;
            stream_->read(base_offset + skn_desc_dfo_common_type, &chunk_type, 2);

            switch (chunk_type) {
            case as_desc_skin_desc_bmp_item_def: {
                process_bitmap_def_chunk(base_offset);
                break;
            }

            case as_desc_skin_desc_img_tbl_item_def: {
                process_image_table_def_chunk(base_offset);
                break;
            }

            case as_desc_skin_desc_color_tbl_item_def: {
                process_color_table_def_chunk(base_offset);
                break;
            }

            case as_desc_skin_desc_img_bmp_anim: {
                process_bitmap_anim_def_chunk(base_offset);
                break;
            }

            case as_desc_effect_queue: {
                process_effect_queue_chunk(base_offset);
                break;
            }

            default: {
                LOG_ERROR(SERVICE_UI, "Unknown class def chunk type: {}", chunk_type);
                break;
            }
            }

            base_offset += chunk_size;
        }
    }

    void skn_file::process_bitmap_def_chunk(std::uint32_t base_offset) {
        skn_bitmap_info bmp_info_{};
        bmp_info_.type = skn_def_type::bitmap;

        stream_->read(base_offset + skn_desc_dfo_bitmap_hash_id, &bmp_info_.id_hash, 8);
        stream_->read(base_offset + skn_desc_dfo_bitmap_filename_id, &bmp_info_.filename_id, 4);
        stream_->read(base_offset + skn_desc_dfo_bitmap_bitmap_idx, &bmp_info_.bmp_idx, 4);
        stream_->read(base_offset + skn_desc_dfo_bitmap_mask_idx, &bmp_info_.mask_bitmap_idx, 4);

        process_attrib(base_offset + skn_desc_dfo_bitmap_attribs, bmp_info_.attrib);

        bitmaps_.emplace(bmp_info_.id_hash, std::move(bmp_info_));
    }

    void skn_file::process_image_table_def_chunk(std::uint32_t base_offset) {
        skn_image_table tab_{};
        tab_.type = skn_def_type::img_tbl;

        stream_->read(base_offset + skn_desc_dfo_bitmap_hash_id, &tab_.id_hash, 8);

        // Read count
        std::uint16_t count = 0;
        stream_->read(base_offset + skn_desc_dfo_img_table_images_count, &count, 2);

        count &= 0xFF;

        std::uint32_t offset_of_attrib = base_offset + skn_desc_dfo_img_table_image_major0
            + count * skn_desc_dfo_img_table_image_size;

        process_attrib(offset_of_attrib, tab_.attrib);

        // Move first
        skn_image_table &tab_ref_ = img_tabs_.emplace(tab_.id_hash, std::move(tab_)).first->second;

        // Read entry
        for (std::uint16_t i = 0; i < count; i++) {
            std::uint64_t img_hash = 0;
            stream_->read(base_offset + skn_desc_dfo_img_table_image_major0 + i * skn_desc_dfo_img_table_image_size,
                &img_hash, 8);

            tab_ref_.images.push_back(img_hash);
        }
    }

    void skn_file::process_color_table_def_chunk(std::uint32_t base_offset) {
        skn_color_table tab_{};
        tab_.type = skn_def_type::color_tbl;

        stream_->read(base_offset + skn_desc_dfo_bitmap_hash_id, &tab_.id_hash, 8);

        // Read count
        std::uint16_t count = 0;
        stream_->read(base_offset + skn_desc_dfo_color_tab_colors_count, &count, 2);

        count &= 0xFF;

        std::uint32_t offset_of_attrib = base_offset + skn_desc_dfo_color_tab_color_idx0
            + count * skn_desc_dfo_color_tab_color_size;

        process_attrib(offset_of_attrib, tab_.attrib);

        // Move first
        skn_color_table &tab_ref_ = color_tabs_.emplace(tab_.id_hash, std::move(tab_)).first->second;

        // Read entry
        for (std::uint16_t i = 0; i < count; i++) {
            // Read index
            std::int16_t idx = 0;
            stream_->read(base_offset + skn_desc_dfo_color_tab_color_idx0 + i * skn_desc_dfo_color_tab_color_size,
                &idx, 2);

            // Read RGB value
            common::rgba color = 0;
            stream_->read(base_offset + skn_desc_dfo_color_tab_color_rgb0 + i * skn_desc_dfo_color_tab_color_size,
                &color, 4);

            tab_ref_.colors.push_back({ idx, color });
        }
    }

    void skn_file::process_bitmap_anim_def_chunk(std::uint32_t base_offset) {
        skn_bitmap_animation anim_{};
        anim_.type = skn_def_type::color_tbl;

        stream_->read(base_offset + skn_desc_dfo_bitmap_hash_id, &anim_.id_hash, 8);

        // Read count
        std::int16_t count = 0;
        stream_->read(base_offset + skn_desc_dfo_bmp_anim_frames_count, &count, 2);

        count &= 0xFF;

        std::uint32_t offset_of_attrib = base_offset + skn_desc_dfo_bmp_anim_frame_major0
            + count * skn_desc_dfo_bmp_anim_frame_size;

        process_attrib(offset_of_attrib, anim_.attrib);

        // Read interval and play mode
        stream_->read(base_offset + skn_desc_dfo_bmp_anim_playmode, &anim_.play_mode, 2);
        stream_->read(base_offset + skn_desc_dfo_bmp_anim_interval, &anim_.interval, 2);

        for (std::int16_t i = 0; i < count; i++) {
            skn_anim_frame frame{};

            stream_->read(base_offset + skn_desc_dfo_bmp_anim_frame_major0
                    + i * skn_desc_dfo_bmp_anim_frame_size,
                &frame.frame_bmp_hash, 8);

            stream_->read(base_offset + skn_desc_dfo_bmp_anim_frame_time0
                    + i * skn_desc_dfo_bmp_anim_frame_size,
                &frame.time, 2);

            stream_->read(base_offset + skn_desc_dfo_bmp_anim_frame_posx0
                    + i * skn_desc_dfo_bmp_anim_frame_size,
                &frame.posx, 2);

            stream_->read(base_offset + skn_desc_dfo_bmp_anim_frame_posy0
                    + i * skn_desc_dfo_bmp_anim_frame_size,
                &frame.posy, 2);

            anim_.frames.push_back(frame);
        }

        bitmap_anims_.emplace(anim_.id_hash, std::move(anim_));
    }

    std::string skn_file::process_string(std::uint32_t base_offset, const std::uint16_t size) {
        std::string buf;
        buf.resize(size);

        stream_->read(base_offset, &buf[0], size);

        return buf;
    }

    void skn_file::process_effect_parameters(std::uint32_t &base_offset, std::vector<skn_effect_parameter> &parameters) {
        for (std::uint16_t i = 0; i < static_cast<std::uint16_t>(parameters.size()); i++) {
            std::uint16_t parameter_size = 0;
            stream_->read(base_offset + skn_desc_dfo_param_len, &parameter_size, 2);
            stream_->read(base_offset + skn_desc_dfo_param_type, &parameters[i].type, 1);

            // Data layout:
            // offset 0: is the parameter size (2 bytes)
            // offset 2: reserved (1 bytes)
            // offset 3: parameter type (1 bytes)
            // The rest is parameter data
            std::uint16_t real_parameter_size = parameter_size - 4;
            parameters[i].data = process_string(base_offset + 4, real_parameter_size);

            base_offset += parameter_size;
        }
    }

    void skn_file::process_effects(std::uint32_t &base_offset, std::vector<skn_effect> &effects) {
        for (std::uint16_t i = 0; i < static_cast<std::uint16_t>(effects.size()); i++) {
            stream_->read(base_offset + skn_desc_dfo_effect_uid, &effects[i].uid, 4);
            stream_->read(base_offset + skn_desc_dfo_effect_input_layerA_idx, &effects[i].input_layer_a_index, 1);
            stream_->read(base_offset + skn_desc_dfo_effect_input_layerA_mode, &effects[i].input_layer_a_mode, 1);
            stream_->read(base_offset + skn_desc_dfo_effect_input_layerB_idx, &effects[i].input_layer_b_index, 1);
            stream_->read(base_offset + skn_desc_dfo_effect_input_layerB_mode, &effects[i].input_layer_b_mode, 1);
            stream_->read(base_offset + skn_desc_dfo_effect_output_layer_idx, &effects[i].output_layer_index, 1);
            stream_->read(base_offset + skn_desc_dfo_effect_output_layer_mode, &effects[i].output_layer_mode, 1);

            if (effects[i].input_layer_a_mode > 8 || effects[i].input_layer_b_index > 8 || effects[i].output_layer_index > 8) {
                LOG_ERROR(SERVICE_UI, "Invalid effect input/output mode!");
                return;
            }

            std::uint16_t param_count = 0;
            stream_->read(base_offset + skn_desc_dfo_effect_param_count, &param_count, 2);

            if (param_count > 0x40) {
                LOG_ERROR(SERVICE_UI, "Parameters count exceed maximum: {} (vs 64), skip this effect queue", param_count);
                return;
            }

            base_offset += skn_desc_dfo_effect_param_count + 2;

            effects[i].parameters.resize(param_count);

            // Let's read the parameters!
            process_effect_parameters(base_offset, effects[i].parameters);
        }
    }

    void skn_file::process_effect_queue_chunk(std::uint32_t base_offset) {
        skn_effect_queue effect_queue;

        stream_->read(base_offset + skn_desc_dfo_effect_queue_major, &effect_queue.id_hash, 8);
        stream_->read(base_offset + skn_desc_dfo_effect_queue_ref_major, &effect_queue.ref_major, 4);
        stream_->read(base_offset + skn_desc_dfo_effect_queue_ref_minor, &effect_queue.ref_minor, 4);
        stream_->read(base_offset + skn_desc_dfo_effect_queue_input_layer_index, &effect_queue.input_layer_index, 1);
        stream_->read(base_offset + skn_desc_dfo_anim_input_layer_mode, &effect_queue.input_layer_mode, 1);
        stream_->read(base_offset + skn_desc_dfo_effect_queue_output_layer_index, &effect_queue.output_layer_index, 1);
        stream_->read(base_offset + skn_desc_dfo_effect_queue_output_layer_mode, &effect_queue.output_layer_mode, 1);

        std::uint16_t effect_count = 0;
        stream_->read(base_offset + skn_desc_dfo_effect_queue_effect_count, &effect_count, 2);

        if (effect_queue.input_layer_mode > 8 || effect_queue.output_layer_mode > 8 || effect_count > 64) {
            LOG_ERROR(SERVICE_UI, "Corrupted effect queue chunk!");
            return;
        }

        if (effect_count > 0x20) {
            LOG_ERROR(SERVICE_UI, "Effect count exceed maximum {} (vs 32), skipping this effect.", effect_count);
            return;
        }

        if (effect_queue.ref_major && effect_queue.ref_minor) {
            // TODO: ????
            return;
        }

        // Read each effect description
        effect_queue.effects.resize(effect_count);
        base_offset += skn_desc_dfo_effect_queue_effects;

        process_effects(base_offset, effect_queue.effects);

        effect_queues_.push_back(std::move(effect_queue));
    }

    void skn_file::process_attrib(std::uint32_t base_offset, skn_attrib_info &attrib) {
        std::uint16_t ver = 0;
        stream_->read(base_offset + skn_desc_dfo_common_ver, &ver, 2);

        std::uint16_t attrib_and_align = 0;
        stream_->read(base_offset + skn_desc_dfo_attribs_attrib_flags, &attrib_and_align, 2);

        attrib.attrib = attrib_and_align & 0xFF;
        attrib.align = (attrib_and_align & 0xFF00) >> 8;

        stream_->read(base_offset + skn_desc_dfo_attribs_coordx, &attrib.image_coord_x, 2);
        stream_->read(base_offset + skn_desc_dfo_attribs_coordy, &attrib.image_coord_y, 2);
        stream_->read(base_offset + skn_desc_dfo_attribs_sizew, &attrib.image_size_x, 2);
        stream_->read(base_offset + skn_desc_dfo_attribs_sizeh, &attrib.image_size_y, 2);

        if (ver >= 2) {
            // 16-bit flag
            stream_->read(base_offset + skn_desc_dfo_attribs_ext_attrib_flags, &attrib_and_align, 2);
            attrib.attrib |= attrib_and_align << 8;
        }
    }

    std::uint32_t skn_file::handle_release_26_restriction_chunk(std::uint32_t base_offset) {
        std::uint32_t chunk_size = 0;
        stream_->read(base_offset + skn_desc_dfo_common_len, &chunk_size, 4);

        std::uint32_t count = 0;
        stream_->read(base_offset + skn_desc_dfo_release26_chunks_count, &count, 4);

        [[maybe_unused]] std::int8_t plat_major = 0;
        [[maybe_unused]] std::int8_t plat_minor = 0;

        stream_->read(base_offset + skn_desc_dfo_release26_plat_major, &plat_major, 1);
        stream_->read(base_offset + skn_desc_dfo_release26_plat_minor, &plat_minor, 1);

        if (ver_ >= plat_ver{ 2, 6 }) {
            process_chunks(base_offset + skn_desc_dfo_release26_content, count);
        }

        return chunk_size;
    }

    std::uint32_t skn_file::handle_release_generic_restriction_chunk(std::uint32_t base_offset) {
        std::uint32_t chunk_size = 0;
        stream_->read(base_offset + skn_desc_dfo_common_len, &chunk_size, 4);

        std::uint32_t count = 0;
        stream_->read(base_offset + skn_desc_dfo_release_generic_chunks_count, &count, 4);

        [[maybe_unused]] std::int8_t plat_major = 0;
        [[maybe_unused]] std::int8_t plat_minor = 0;

        stream_->read(base_offset + skn_desc_dfo_release_generic_plat_major, &plat_major, 1);
        stream_->read(base_offset + skn_desc_dfo_release_generic_plat_minor, &plat_minor, 1);

        if (ver_ >= plat_ver{ plat_major, plat_minor }) {
            process_chunks(base_offset + skn_desc_dfo_release_generic_content, count);
        }

        return chunk_size;
    }

    std::uint32_t skn_file::handle_lang_restriction_chunk(std::uint32_t base_offset) {
        std::uint32_t chunk_size = 0;
        stream_->read(base_offset + skn_desc_dfo_common_len, &chunk_size, 4);

        std::uint32_t count = 0;
        stream_->read(base_offset + skn_desc_dfo_lang_lang_count, &count, 4);

        std::uint16_t lang_restr = 0;
        stream_->read(base_offset + skn_desc_dfo_lang_lang_restr, &lang_restr, 2);

        [[maybe_unused]] std::uint16_t general_restr = 0;
        stream_->read(base_offset + skn_desc_dfo_lang_gen_restr, &general_restr, 2);

        if (general_restr == 0 || lang_restr == 0 || importer_lang_ == language::any || (static_cast<language>(lang_restr) == importer_lang_)) {
            process_chunks(base_offset + skn_desc_dfo_lang_content, count);
        }

        return chunk_size;
    }
}
