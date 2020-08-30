/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#pragma once

#include <common/vecx.h>
#include <cstdint>

namespace eka2l1::epoc {
    enum raw_event_type {
        raw_event_type_redraw = 5,
        raw_event_enable_key_block = 50,
        raw_event_disable_key_block = 51
    };

    struct raw_event_eka1 {
        std::uint32_t type_;
        std::uint32_t time_in_ticks_;

        union {
            struct {
                std::uint32_t x_;
                std::uint32_t y_;
            } pos_;

            std::int32_t scancode_;
            std::uint32_t mods_;
        } data_;
    };

    struct raw_event {
        std::uint8_t type_;
        std::uint8_t tip_;
        std::uint8_t pointer_num_;
        std::uint8_t screen_num_;
        std::uint32_t time_in_ticks_;

        union {
            struct {
                std::uint32_t x_;
                std::uint32_t y_;
            } pos_;

            struct {
                std::uint32_t x_;
                std::uint32_t y_;
                std::uint32_t z_;
                std::int32_t phi_;
                std::int32_t pheta_;
                std::int32_t alpha_;
            } pos_3d_;

            struct {
                std::int32_t scancode_;
                std::uint32_t repeats_;
            } key_data_;
            std::uint32_t mods_;
        } data_;

        void from_eka1_event(const raw_event_eka1 &old_evt) {
            type_ = static_cast<std::uint8_t>(old_evt.type_);
            time_in_ticks_ = old_evt.time_in_ticks_;

            tip_ = 0;
            pointer_num_ = 0;
            screen_num_ = 0;

            data_.pos_.x_ = old_evt.data_.pos_.x_;
            data_.pos_.y_ = old_evt.data_.pos_.y_;
        }
    };
}