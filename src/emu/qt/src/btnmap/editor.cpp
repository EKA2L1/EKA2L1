/*
 * Copyright (c) 2022 EKA2L1 Team.
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

#include <qt/btnmap/editor.h>
#include <qt/btnmap/singletouch.h>
#include <qt/btnmap/joystick.h>
#include <qt/btnmap/executor.h>
#include <common/fileutils.h>
#include <common/cvt.h>

#include <vector>

namespace eka2l1::qt::btnmap {
    editor::editor()
        : adapter_(nullptr)
        , atlas_(nullptr)
        , selected_entity_(nullptr) {
    }

    void editor::clean(drivers::graphics_driver *driver) {
        // TODO: Really clean it out! But the driver should clear it out at the end anyway ;_;
        drivers::graphics_command_builder builder;

        for (auto &resource: resources_) {
            builder.destroy(resource.second);
        }

        resources_.clear();
    }

    void editor::add_managed_resource(const std::string &name, drivers::handle h) {
        resources_[name] = h;
    }

    drivers::handle editor::get_managed_resource(const std::string &name) {
        if (resources_.find(name) == resources_.end()) {
            return 0;
        }

        return resources_[name];
    }

    void editor::draw_text(drivers::graphics_driver *driver, drivers::graphics_command_builder &builder, const eka2l1::rect &draw_rect, epoc::text_alignment alignment, const std::string &str, const float scale_factor) {
        static const char *FONT_OVERLAY_PATH = "resources\\overlay_font.ttf";
        if (!adapter_) {
            FILE *f = common::open_c_file(FONT_OVERLAY_PATH, "rb");
            if (!f) {
                return;
            }

            fseek(f, 0, SEEK_END);
            long size = ftell(f);

            std::vector<std::uint8_t> data;
            data.resize(size);

            fseek(f, 0, SEEK_SET);
            fread(data.data(), 1, size, f);

            fclose(f);

            adapter_ = epoc::adapter::make_font_file_adapter(epoc::adapter::font_file_adapter_kind::stb, data);
        }

        if (!atlas_) {
            atlas_ = std::make_unique<epoc::font_atlas>(adapter_.get(), 0, 0x20, 0xFF - 0x20, DEFAULT_OVERLAY_FONT_SIZE);
        }

        atlas_->draw_text(common::utf8_to_ucs2(str), draw_rect, alignment, driver, builder, scale_factor);
    }

    void editor::draw(drivers::graphics_driver *driver, drivers::graphics_command_builder &builder, const float scale_factor) {
        // Draw a gray overlay
        builder.set_feature(eka2l1::drivers::graphics_feature::blend, true);
        builder.blend_formula(drivers::blend_equation::add, drivers::blend_equation::add,
            drivers::blend_factor::frag_out_alpha, drivers::blend_factor::one_minus_frag_out_alpha,
            drivers::blend_factor::one, drivers::blend_factor::one);

        // 0x808080, gray near bit to the black
        builder.set_brush_color_detail(eka2l1::vec4(0x80, 0x80, 0x80, 215));
        builder.draw_rectangle(eka2l1::rect(eka2l1::vec2(0, 0), eka2l1::vec2(0, 0)));

        {
            const std::lock_guard<std::mutex> guard(lock_);

            for (std::size_t i = 0; i < entities_.size(); i++) {
                entities_[i]->draw(driver, builder, scale_factor);
            }
        }
        
        builder.set_feature(eka2l1::drivers::graphics_feature::blend, false);
    }

    void editor::on_mouse_event(const eka2l1::vec3 &pos, const int button_id, const int action_id,
        const int mouse_id) {
        // Left click
        if (button_id == 0) {
            for (std::size_t i = 0; i < entities_.size(); i++) {
                const action_result res = entities_[i]->on_mouse_action(eka2l1::vec2(pos.x, pos.y), action_id,
                    (selected_entity_ == entities_[i].get()));
                if (res == ACTION_SELECTED) {
                    selected_entity_ = entities_[i].get();
                } else if (res == ACTION_DESELECTED) {
                    if (selected_entity_ == entities_[i].get()) {
                        selected_entity_ = nullptr;
                    }
                }
            }
        }
    }
    
    void editor::on_key_press(const std::uint32_t key) {
        if (selected_entity_) {
            selected_entity_->on_key_press(key);
        }
    }

    void editor::on_controller_button_press(const std::uint32_t code) {
        if (selected_entity_) {
            selected_entity_->on_controller_button_press(code);
        }
    }

    void editor::on_joy_move(const std::uint32_t code, const float axisx, const float axisy) {
        if (selected_entity_) {
            selected_entity_->on_joy_move(code, axisx, axisy);
        }
    }

    void editor::add_and_select_entity(base_instance &instance) {
        if (!instance) {
            return;
        }

        entities_.push_back(std::move(instance));
        selected_entity_ = entities_.back().get();
    }

    void editor::delete_entity(base *b) {
        for (std::size_t i = 0; i < entities_.size(); i++) {
            if (entities_[i].get() == b) {
                if (selected_entity_ == b) {
                    selected_entity_ = nullptr;
                }

                entities_.erase(entities_.begin() + i);
                return;
            }
        }
    }

    void editor::import_from(executor *exec) {
        const std::lock_guard<std::mutex> guard(lock_);
        entities_.clear();

        for (std::size_t i = 0; i < exec->behaviours_.size(); i++) {
            behaviour *beha = exec->behaviours_[i].get();
            switch (beha->get_behaviour_type()) {
            case BEHAVIOUR_TYPE_SINGLE_TOUCH: {
                single_touch_behaviour *beha_touch = reinterpret_cast<single_touch_behaviour*>(beha);
                base_instance inst = std::make_unique<single_touch>(this, beha_touch->position());

                reinterpret_cast<single_touch*>(inst.get())->keycode(beha_touch->keycode() & 0xFFFFFFFF);
                inst->type(static_cast<map_type>((beha_touch->keycode() >> 32) & 0xFFFFFFFF));

                entities_.push_back(std::move(inst));
                break;
            }

            case BEHAVIOUR_TYPE_JOYSTICK: {
                joystick_behaviour *beha_joy = reinterpret_cast<joystick_behaviour*>(beha);
                base_instance inst = std::make_unique<joystick>(this, beha_joy->position());

                joystick *joy_ent = reinterpret_cast<joystick*>(inst.get());

                for (int i = 0; i < 5; i++) {
                    if (beha_joy->key(i) != 0) {
                        joy_ent->keys_[i] = std::make_unique<single_touch>(this, eka2l1::vec2());
                        joy_ent->keys_[i]->keycode(beha_joy->key(i) & 0xFFFFFFFF);
                        joy_ent->keys_[i]->type(static_cast<map_type>((beha_joy->key(i) >> 32) & 0xFFFFFFFF));
                    }
                }

                joy_ent->width_ = beha_joy->width();
                joy_ent->positioning_bind_buttons();

                entities_.push_back(std::move(inst));
                break;
            }

            default:
                break;
            }

        }
    }

    void editor::export_to(executor *exec) {
        exec->reset();

        const std::lock_guard<std::mutex> guard(lock_);

        for (std::size_t i = 0; i < entities_.size(); i++) {
            switch (entities_[i]->entity_type()) {
            case EDITOR_ENTITY_TYPE_SINGLE_TOUCH: {
                single_touch *touch_ent = reinterpret_cast<single_touch*>(entities_[i].get());
                std::unique_ptr<behaviour> beha = std::make_unique<single_touch_behaviour>(exec, touch_ent->position(),
                    (static_cast<std::uint64_t>(touch_ent->type()) << 32) | touch_ent->keycode_);

                exec->add_behaviour(beha);
                break;
            }
            case EDITOR_ENTITY_TYPE_JOYSTICK: {
                joystick *joy_ent = reinterpret_cast<joystick*>(entities_[i].get());
                std::unique_ptr<behaviour> beha = std::make_unique<joystick_behaviour>(exec, joy_ent->position(),
                    joy_ent->width_);

                joystick_behaviour *joy_beha = reinterpret_cast<joystick_behaviour*>(beha.get());
                for (int i = 0; i < 5; i++) {
                    if (joy_ent->keys_[i]) {
                        joy_beha->set_key(i, (static_cast<std::uint64_t>(joy_ent->keys_[i]->type()) << 32) | joy_ent->keys_[i]->keycode());
                    }
                }
                exec->add_behaviour(beha);
                break;
            }
            default:
                break;
            }
        }
    }
}
