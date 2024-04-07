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

#include <qt/btnmap/executor.h>
#include <common/log.h>

#include <yaml-cpp/yaml.h>

#include <common/algorithm.h>
#include <common/fileutils.h>

#include "qt/btnmap/base.h"
#include <kernel/timing.h>
#include <services/window/window.h>

namespace eka2l1::qt::btnmap {
    static const char *SINGLE_TOUCH_YAML_TYPE = "singletouch";
    static const char *JOYSTICK_YAML_TYPE = "joystick";
    static const char *CAMERA_PAM_YAML_TYPE = "camerapan";

    executor::executor(window_server *server, ntimer *timing)
        : winserv_(server)
        , timing_(timing)
        , pointer_ownership_(0) {

    }

    bool executor::execute(const std::uint64_t keycode, const bool is_press) {
        auto ite = mappings_.find(keycode);
        if (ite != mappings_.end()) {
            for (behaviour *beha: ite->second) {
                beha->produce(keycode, is_press);
            }
            return true;
        }
        return false;
    }

    bool executor::execute(const std::uint64_t keycode, const float axisx, const float axisy) {
        auto ite = mappings_.find(keycode);
        if (ite != mappings_.end()) {
            for (behaviour *beha: ite->second) {
                beha->produce(keycode, axisx, axisy);
            }
            return true;
        }
        return false;
    }

    void executor::serialize(const std::string &conf_path) {
        YAML::Emitter emitter;

        emitter << YAML::BeginSeq;
        for (std::size_t i = 0; i < behaviours_.size(); i++) {
            behaviours_[i]->serialize(emitter);
        }
        emitter << YAML::EndSeq;

        FILE *f = common::open_c_file(conf_path, "wb");
        fwrite(emitter.c_str(), 1, emitter.size(), f);
        fclose(f);
    }

    void executor::deserialize(const std::string &conf_path) {
        FILE *f = common::open_c_file(conf_path, "rb");
        if (!f) {
            return;
        }

        fseek(f, 0, SEEK_END);
        auto stream_size = ftell(f);
        fseek(f, 0, SEEK_SET);

        std::string stream_string;
        stream_string.resize(stream_size);

        fread(&stream_string[0], 1, stream_size, f);
        reset();

        YAML::Node whole_file = YAML::Load(stream_string.c_str());
        for (const YAML::Node &child: whole_file) {
            std::string type_of_comp = child["type"].as<std::string>("");
            if (type_of_comp.empty()) {
                continue;
            }
            std::unique_ptr<behaviour> beha = nullptr;
            if (eka2l1::common::compare_ignore_case(type_of_comp.c_str(), SINGLE_TOUCH_YAML_TYPE) == 0) {
                beha = std::make_unique<single_touch_behaviour>(this, child);
            } else if (eka2l1::common::compare_ignore_case(type_of_comp.c_str(), JOYSTICK_YAML_TYPE) == 0) {
                beha = std::make_unique<joystick_behaviour>(this, child);
            } else if (eka2l1::common::compare_ignore_case(type_of_comp.c_str(), CAMERA_PAM_YAML_TYPE) == 0) {
                beha = std::make_unique<camera_pan_behavior>(this, child);
            }
            if (beha) {
                add_behaviour(beha);
            }
        }
    }

    void executor::reset() {
        pointer_ownership_ = 0;
        mappings_.clear();
        behaviours_.clear();
    }

    void executor::add_behaviour(std::unique_ptr<behaviour> &beha) {
        std::vector<std::uint64_t> keys = beha->mapped_keys();
        for (std::uint64_t key : keys) {
            mappings_[key].push_back(beha.get());
        }
        behaviours_.push_back(std::move(beha));
    }

    std::uint8_t executor::allocate_free_pointer() {
        for (std::uint8_t i = 0; i < static_cast<std::uint8_t>(MAX_POINTERS); i++) {
            if ((pointer_ownership_ & (1 << i)) == 0) {
                pointer_ownership_ |= (1 << i);
                return i;
            }
        }

        return 0xFF;
    }

    void executor::free_allocated_pointer(const std::uint8_t index) {
        pointer_ownership_ &= ~(1 << index);
    }

    void behaviour::deliver_touch(const eka2l1::vec2 &pos, const std::uint8_t touch_index, const eka2l1::drivers::mouse_action act) {
        drivers::input_event evt;
        evt.type_ = eka2l1::drivers::input_event_type::touch;
        evt.mouse_.pos_x_ = static_cast<int>(pos.x);
        evt.mouse_.pos_y_ = static_cast<int>(pos.y);
        evt.mouse_.pos_z_ = static_cast<int>(0);    // TODO: Support pressure
        evt.mouse_.mouse_id = static_cast<std::uint32_t>(touch_index);
        evt.mouse_.button_ = eka2l1::drivers::mouse_button::mouse_button_left;
        evt.mouse_.action_ = act;
        evt.mouse_.raw_screen_pos_ = true;

        if (executor_->get_window_server()) {
            executor_->get_window_server()->queue_input_from_driver(evt);
        }
    }

    single_touch_behaviour::single_touch_behaviour(executor *exec, const YAML::Node &info_node)
        : behaviour(exec)
        , allocated_pointer_(0xFF) {
        position_ = eka2l1::vec2(info_node["x"].as<int>(-1), info_node["y"].as<int>(-1));
        keycode_ = info_node["keycode"].as<std::uint64_t>(0);
    }

    single_touch_behaviour::single_touch_behaviour(executor *exec, const eka2l1::vec2 &pos, const std::uint64_t keycode)
        : behaviour(exec)
        , position_(pos)
        , keycode_(keycode)
        , allocated_pointer_(0xFF){
    }

    void single_touch_behaviour::produce(const std::uint64_t key, const bool is_press) {
        if (is_press) {
            if (allocated_pointer_ == 0xFF) {
                allocated_pointer_ = executor_->allocate_free_pointer();

                if (allocated_pointer_ == 0xFF) {
                    LOG_ERROR(FRONTEND_UI, "Out of virtual pointer for key to touch mapping!");
                    return;
                }
            }

            deliver_touch(position_, allocated_pointer_, drivers::mouse_action_press);
        } else {
            executor_->free_allocated_pointer(allocated_pointer_);
            deliver_touch(position_, allocated_pointer_, drivers::mouse_action_release);

            allocated_pointer_ = 0xFF;
        }
    }

    std::vector<std::uint64_t> single_touch_behaviour::mapped_keys() {
        return std::vector<std::uint64_t>{ keycode_ };
    }

    void single_touch_behaviour::serialize(YAML::Emitter &emitter) {
        emitter << YAML::BeginMap;
        emitter << YAML::Key << "type" << YAML::Value << SINGLE_TOUCH_YAML_TYPE;
        emitter << YAML::Key << "x" << YAML::Value << position_.x;
        emitter << YAML::Key << "y" << YAML::Value << position_.y;
        emitter << YAML::Key << "keycode" << YAML::Value << keycode_;
        emitter << YAML::EndMap;
    }

    joystick_behaviour::joystick_behaviour(executor *exec, const eka2l1::vec2 &pos, const std::uint32_t width)
        : behaviour(exec)
        , key_status_(0)
        , position_(pos)
        , width_(width)
        , allocated_pointer_(0xFF) {
        current_diff_.x = current_diff_.y = 0;
    }

    joystick_behaviour::joystick_behaviour(executor *exec, const YAML::Node &info_node)
        : behaviour(exec)
        , key_status_(0)
        , width_(0)
        , allocated_pointer_(0xFF)  {
        position_ = eka2l1::vec2(info_node["x"].as<int>(-1), info_node["y"].as<int>(-1));
        keys_[0] = info_node["keycode"][0].as<std::uint64_t>('W');
        keys_[1] = info_node["keycode"][1].as<std::uint64_t>('S');
        keys_[2] = info_node["keycode"][2].as<std::uint64_t>('A');
        keys_[3] = info_node["keycode"][3].as<std::uint64_t>('D');
        keys_[4] = info_node["keycode"][4].as<std::uint64_t>(eka2l1::drivers::CONTROLLER_BUTTON_CODE_LEFT_STICK);
        width_ = info_node["width"].as<std::uint32_t>(0);

        current_diff_.x = current_diff_.y = 0;
    }

    joystick_behaviour::~joystick_behaviour() {

    }

    void joystick_behaviour::produce(const std::uint64_t key, const float axisx, const float axisy) {
        if ((key_status_ & (1 << 4)) == 0) {
            if ((std::abs(axisx) < JOY_DEADZONE_VALUE) && (std::abs(axisy) < JOY_DEADZONE_VALUE)) {
                return;
            }
            if (allocated_pointer_ == 0xFF) {
                allocated_pointer_ = executor_->allocate_free_pointer();

                if (allocated_pointer_ == 0xFF) {
                    LOG_ERROR(FRONTEND_UI, "Out of virtual pointer for key to joystick mapping!");
                    return;
                }
            }

            key_status_ |= (1 << 4);
            deliver_touch(position_ + eka2l1::vec2(width_ / 2), allocated_pointer_, drivers::mouse_action_press);
        } else {
            if ((std::abs(axisx) < JOY_DEADZONE_VALUE) && (std::abs(axisy) < JOY_DEADZONE_VALUE)) {
                deliver_touch(position_, allocated_pointer_, drivers::mouse_action_release);
                executor_->free_allocated_pointer(allocated_pointer_);

                allocated_pointer_ = 0xFF;
                key_status_ &= ~(1 << 4);

                return;
            }
        }

        deliver_touch(position_ + eka2l1::vec2(width_ / 2) + eka2l1::vec2(static_cast<int>(width_ * axisx / 2), static_cast<int>(width_ * axisy / 2)),
                      allocated_pointer_, drivers::mouse_action_repeat);
    }

    void joystick_behaviour::produce(const std::uint64_t key, const bool is_press) {
        if ((key_status_ == 0) && is_press) {
            if (allocated_pointer_ == 0xFF) {
                allocated_pointer_ = executor_->allocate_free_pointer();

                if (allocated_pointer_ == 0xFF) {
                    LOG_ERROR(FRONTEND_UI, "Out of virtual pointer for key to joystick mapping!");
                    return;
                }
            }
            deliver_touch(position_ + eka2l1::vec2(width_ / 2), allocated_pointer_, drivers::mouse_action_press);
        }
        for (int i = 0; i < 4; i++) {
            if (keys_[i] == key) {
                static const eka2l1::vec2 INC_OFF[4] = {
                    eka2l1::vec2(0, -1),
                    eka2l1::vec2(0, 1),
                    eka2l1::vec2(-1, 0),
                    eka2l1::vec2(1, 0)
                };

                current_diff_ += (INC_OFF[i] * (is_press ? static_cast<int>(width_) : -static_cast<int>(width_))) / 2;

                if (is_press) {
                    deliver_touch(position_ + eka2l1::vec2(width_ / 2) + current_diff_, allocated_pointer_, drivers::mouse_action_repeat);
                    key_status_ |= (1 << i);
                } else {
                    key_status_ &= ~(1 << i);
                    if (key_status_ == 0) {
                        deliver_touch(position_, allocated_pointer_, drivers::mouse_action_release);
                        executor_->free_allocated_pointer(allocated_pointer_);

                        allocated_pointer_ = 0xFF;
                    } else {
                        deliver_touch(position_ + eka2l1::vec2(width_ / 2) + current_diff_, allocated_pointer_, drivers::mouse_action_repeat);
                    }
                }

                return;
            }
        }
    }

    std::vector<std::uint64_t> joystick_behaviour::mapped_keys() {
        return { keys_[0], keys_[1], keys_[2], keys_[3], keys_[4] };
    }

    void joystick_behaviour::serialize(YAML::Emitter &emitter) {
        emitter << YAML::BeginMap;
        emitter << YAML::Key << "type" << YAML::Value << JOYSTICK_YAML_TYPE;
        emitter << YAML::Key << "x" << YAML::Value << position_.x;
        emitter << YAML::Key << "y" << YAML::Value << position_.y;
        emitter << YAML::Key << "keycode" << YAML::Value;
        {
            emitter << YAML::BeginSeq << keys_[0] << keys_[1] << keys_[2] << keys_[3] << keys_[4] << YAML::EndSeq;
        }
        emitter << YAML::Key << "width" << YAML::Value << width_;
        emitter << YAML::EndMap;
    }

    camera_pan_behavior::camera_pan_behavior(executor *exec, const eka2l1::vec2 &pos, const float pan_speed)
        : behaviour(exec)
        , accumulated_axis_x_(0)
        , accumulated_axis_y_(0)
        , is_active_(false)
        , pan_speed_(pan_speed)
        , pos_(pos)  {

    }

    camera_pan_behavior::camera_pan_behavior(executor *exec, const YAML::Node &info_node)
        : behaviour(exec)
        , accumulated_axis_x_(0)
        , accumulated_axis_y_(0)
        , is_active_(false)
        , pan_speed_(0.005f) {
        pan_speed_ = info_node["joystick-pan-speed"].as<float>(0.005f);
        pos_ = eka2l1::vec2(info_node["x"].as<int>(-1), info_node["y"].as<int>(-1));
    }

    camera_pan_behavior::~camera_pan_behavior() {
        if (is_active_) {
            executor_->free_allocated_pointer(pointer_);
        }
    }

    void camera_pan_behavior::produce(const std::uint64_t key, const bool is_press) {

    }

    void camera_pan_behavior::produce(const std::uint64_t key, const float axisx, const float axisy) {
        auto winserv = executor_->get_window_server();
        auto screen_size = winserv->get_current_focus_screen()->size();
        auto center_screen = screen_size / 2;

        if (!is_active_) {
            if ((std::abs(axisx) < JOY_DEADZONE_VALUE) && (std::abs(axisy) < JOY_DEADZONE_VALUE)) {
                return;
            }

            is_active_ = true;

            accumulated_axis_x_ = 0;
            accumulated_axis_y_ = 0;

            pointer_ = executor_->allocate_free_pointer();
            previous_time_point_ = std::chrono::system_clock::now();

            deliver_touch(center_screen, pointer_, drivers::mouse_action_press);
        } else {
            if ((std::abs(axisx) < JOY_DEADZONE_VALUE) && (std::abs(axisy) < JOY_DEADZONE_VALUE)) {
                is_active_ = false;
                executor_->free_allocated_pointer(pointer_);

                float final_x_in_range = accumulated_axis_x_ * pan_speed_;
                float final_y_in_range = accumulated_axis_y_ * pan_speed_;

                auto final_pos = center_screen + eka2l1::vec2(static_cast<int>(static_cast<float>(screen_size.x) * final_x_in_range),
                     static_cast<int>(static_cast<float>(screen_size.y) * final_y_in_range));

                deliver_touch(final_pos, pointer_, drivers::mouse_action_release);
                return;
            }
        }

        if (is_mouse_controlling_) {
            is_mouse_controlling_ = false;

            accumulated_axis_x_ = 0;
            accumulated_axis_y_ = 0;
        }

        auto delta_time = std::chrono::system_clock::now() - previous_time_point_;
        auto delta_time_in_secs = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(delta_time).count()) / 1000.0f;

        previous_time_point_ = std::chrono::system_clock::now();

        accumulated_axis_x_ += axisx * delta_time_in_secs;
        accumulated_axis_y_ += axisy * delta_time_in_secs;

        float final_x_in_range = accumulated_axis_x_ * pan_speed_;
        float final_y_in_range = accumulated_axis_y_ * pan_speed_;


        auto final_pos = center_screen + eka2l1::vec2(static_cast<int>(static_cast<float>(screen_size.x) * final_x_in_range),
                                                      static_cast<int>(static_cast<float>(screen_size.y) * final_y_in_range));

        bool should_discharge = false;

        if (std::abs(final_x_in_range) >= 0.5f || std::abs(final_y_in_range) >= 0.5f) {
            accumulated_axis_x_ = 0.0f;
            accumulated_axis_y_ = 0.0f;

            should_discharge = true;
        }

        deliver_touch(final_pos, pointer_, drivers::mouse_action::mouse_action_repeat);

        if (should_discharge) {
            deliver_touch(screen_size, pointer_, drivers::mouse_action::mouse_action_release);

            deliver_touch(center_screen, pointer_, drivers::mouse_action::mouse_action_press);
            deliver_touch(center_screen, pointer_, drivers::mouse_action::mouse_action_repeat);
        }
    }

    std::vector<std::uint64_t> camera_pan_behavior::mapped_keys() {
        return { (static_cast<std::uint64_t>(qt::btnmap::map_type::MAP_TYPE_GAMEPAD) << 32) | drivers::controller_button_code::CONTROLLER_BUTTON_CODE_RIGHT_STICK };
    }

    void camera_pan_behavior::serialize(YAML::Emitter &emitter) {
        emitter << YAML::BeginMap;
        emitter << YAML::Key << "type" << YAML::Value << CAMERA_PAM_YAML_TYPE;
        emitter << YAML::Key << "joystick-pan-speed" << YAML::Value << pan_speed_;
        emitter << YAML::Key << "x" << YAML::Value << pos_.x;
        emitter << YAML::Key << "y" << YAML::Value << pos_.y;
        emitter << YAML::EndMap;
    }
}
