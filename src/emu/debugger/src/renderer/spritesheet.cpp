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

#include <pugixml.hpp>

#include <debugger/renderer/common.h>
#include <debugger/renderer/spritesheet.h>
#include <drivers/graphics/graphics.h>
#include <drivers/itc.h>

namespace eka2l1::renderer {
    spritesheet::spritesheet() {
    }

    bool spritesheet::parse_metadata(const std::string &path) {
        pugi::xml_document doc;
        const auto result = doc.load_file(path.c_str());

        if (!result) {
            // Game over... No sheet for you (good pun, isnt it... ha ha ha...)
            return false;
        }

        auto sprite_metadatas = doc.child("TextureAtlas");

        if (!sprite_metadatas) {
            return false;
        }

        for (auto &sprite_meta: sprite_metadatas.children("sprite")) {
            // New meta
            frame_metadata meta;
            meta.frame_name_ = sprite_meta.attribute("n").as_string();
            meta.position_.x = sprite_meta.attribute("x").as_int();
            meta.position_.y = sprite_meta.attribute("y").as_int();
            meta.size_.x = sprite_meta.attribute("w").as_int();
            meta.size_.y = sprite_meta.attribute("h").as_int();

            metas_.push_back(meta);
        }

        return true;
    }

    bool spritesheet::load(drivers::graphics_driver *driver, drivers::graphics_command_list_builder *builder,
        const std::string &texture_filename, const std::string &meta_filename, const int fps) {
        sheet_ = renderer::load_texture_from_file(driver, builder, texture_filename.c_str(), false, &width_, &height_);

        if (sheet_ == 0) {
            return false;
        }

        // We want to load the metadata now. Format is from TexturePacker xml.
        // Yep... I love the tool. God, feel like developing a game, except in unity it parses for me... (pent0)
        if (!parse_metadata(meta_filename)) {
            return false;
        }

        // Divide the frame time to each frame
        const float milli_per_frame = 1000.0f / fps;

        for (auto &meta: metas_) {
            meta.frame_time_ = milli_per_frame;
        }

        return true;
    }

    void spritesheet::play(const float elapsed) {
        remaining_ -= elapsed;

        if (remaining_ <= 0) {
            // Switch frame
            current_  = (current_ + 1) % static_cast<int>(metas_.size());
            remaining_ = metas_[current_].frame_time_;
        }
    }

    void spritesheet::get_current_frame_uv_coords(float &uv_x_min, float &uv_x_max, float &uv_y_min, float &uv_y_max) {
        const float texel_width = 1.0f / width_;
        const float texel_height = 1.0f/ height_;

        uv_x_min = metas_[current_].position_.x * texel_width;
        uv_x_max = (metas_[current_].position_.x + metas_[current_].size_.x) * texel_width;
        
        uv_y_min = metas_[current_].position_.y * texel_height;
        uv_y_max = (metas_[current_].position_.y + metas_[current_].size_.y) * texel_height;
    }
}