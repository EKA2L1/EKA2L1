/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <epoc/kernel.h>
#include <epoc/services/property.h>
#include <epoc/services/ui/cap/sgc.h>

namespace eka2l1::epoc::cap {
    void sgc_server::wg_state::set_fullscreen(bool set) {
        if (set)
            flags_.set(FLAG_FULLSCREEN);
        else
            flags_.unset(FLAG_FULLSCREEN);
    }

    bool sgc_server::wg_state::is_fullscreen() const {
        return flags_.get(FLAG_FULLSCREEN);
    }

    void sgc_server::wg_state::set_legacy_layout(const bool set) {
        if (set)
            flags_.set(FLAG_LEGACY_LAYOUT);
        else
            flags_.unset(FLAG_LEGACY_LAYOUT);
    }

    bool sgc_server::wg_state::is_legacy_layout() const {
        return flags_.get(FLAG_LEGACY_LAYOUT);
    }
    
    void sgc_server::wg_state::set_understand_partial_foreground(const bool set) {
        if (set)
            flags_.set(FLAG_UNDERSTAND_PARTIAL_FOREGROUND);
        else
            flags_.unset(FLAG_UNDERSTAND_PARTIAL_FOREGROUND);
    }
    
    bool sgc_server::wg_state::understands_partial_foreground() const {
        return flags_.get(FLAG_UNDERSTAND_PARTIAL_FOREGROUND);
    }
    
    void sgc_server::wg_state::set_orientation_specified(const bool set) {
        if (set)
            flags_.set(FLAG_ORIENTATION_SPECIFIED);
        else
            flags_.unset(FLAG_ORIENTATION_SPECIFIED);
    }

    bool sgc_server::wg_state::orientation_specified() const {
        return flags_.get(FLAG_ORIENTATION_SPECIFIED);
    }

    void sgc_server::wg_state::set_orientation_landspace(const bool set) {
        if (set)
            flags_.set(FLAG_ORIENTATION_LANDSCAPE);
        else
            flags_.unset(FLAG_ORIENTATION_LANDSCAPE);
    }

    bool sgc_server::wg_state::orientation_landscape() const {
        return flags_.get(FLAG_ORIENTATION_LANDSCAPE);
    }
    
    enum sgc_app_flags {
        SGC_APP_FLAG_FULLSCREEN = 0,
        SGC_APP_FLAG_LEGACY_LAYOUT = 1,
        SGC_APP_FLAG_ORIENTATION_SPECIFIED = 2,
        SGC_APP_FLAG_ORIENTATION_LANDSCAPE = 3
    };

    static constexpr int UIK_ORIENTATION_NORMAL = 0;
    static constexpr int UIK_ORIENTATION_ALTERNATE = 1;

    sgc_server::sgc_server()
        : orientation_prop_(nullptr) {
    }

    bool sgc_server::init(kernel_system *kern) {
        orientation_prop_ = kern->create<service::property>();

        if (!orientation_prop_) {
            return false;
        }
        
        orientation_prop_->define(service::property_type::int_data, 0);
        orientation_prop_->first = UIKON_UID;
        orientation_prop_->second = UIK_PREFERRED_ORIENTATION_KEY;
        orientation_prop_->set_int(UIK_ORIENTATION_NORMAL);

        return true;
    }

    sgc_server::wg_state &sgc_server::get_wg_state(const std::uint32_t wg_id) {
        auto result = std::find_if(states_.begin(), states_.end(), [=](const wg_state &state) {
            return state.id_ == wg_id;
        });

        if (result != states_.end()) {
            // Found it
            return states_[std::distance(states_.begin(), result)];
        }

        // Create new one
        states_.push_back(wg_state());
        states_.back().id_ = wg_id;

        return states_.back();
    }

    void sgc_server::change_wg_param(const std::uint32_t id, wg_state::wg_state_flags &flags, const std::int32_t sp_layout,
        const std::int32_t sp_flags, const std::int32_t app_screen_mode) {
        wg_state &state = get_wg_state(id);

        const bool was_fullscreen = state.is_fullscreen();
        state.set_fullscreen(flags.get(SGC_APP_FLAG_FULLSCREEN));

        state.set_legacy_layout(flags.get(SGC_APP_FLAG_LEGACY_LAYOUT));
        state.sp_layout_ = sp_layout;
        state.sp_flags_ = sp_flags;

        state.set_understand_partial_foreground(true);
        state.set_orientation_specified(flags.get(SGC_APP_FLAG_ORIENTATION_SPECIFIED));
        state.set_orientation_landspace(flags.get(SGC_APP_FLAG_ORIENTATION_LANDSCAPE));

        state.app_screen_mode_ = app_screen_mode;
    }
}