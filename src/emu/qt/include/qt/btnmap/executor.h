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

#pragma once

#include <common/vecx.h>
#include <drivers/input/common.h>

#include <cstdint>
#include <map>
#include <memory>
#include <vector>
#include <string>

namespace eka2l1 {
    class window_server;
    class ntimer;
}

namespace YAML {
    class Emitter;
    class Node;
}

namespace eka2l1::qt::btnmap {
    struct executor;

    static const float JOY_DEADZONE_VALUE = 0.11f;

    enum behaviour_type {
        BEHAVIOUR_TYPE_SINGLE_TOUCH,
        BEHAVIOUR_TYPE_JOYSTICK
    };

    struct behaviour {
    protected:
        executor *executor_;
        void deliver_touch(const eka2l1::vec2 &pos, const std::uint8_t touch_index, const eka2l1::drivers::mouse_action act);

    public:
        explicit behaviour(executor *exec)
            : executor_(exec) {
        }

        virtual void produce(const std::uint64_t key, const bool is_press) = 0;
        virtual void produce(const std::uint64_t key, const float axisx, const float axisy) {}
        virtual std::vector<std::uint64_t> mapped_keys() = 0;
        virtual void serialize(YAML::Emitter &emitter) = 0;
        virtual behaviour_type get_behaviour_type() const = 0;
    };

    struct single_touch_behaviour : public behaviour {
    private:
        eka2l1::vec2 position_;
        std::uint64_t keycode_;

        std::uint8_t allocated_pointer_;

    public:
        explicit single_touch_behaviour(executor *exec, const YAML::Node &info_node);
        explicit single_touch_behaviour(executor *exec, const eka2l1::vec2 &pos, const std::uint64_t keycode);

        void produce(const std::uint64_t key, const bool is_press) override;
        std::vector<std::uint64_t> mapped_keys() override;
        void serialize(YAML::Emitter &emitter) override;

        behaviour_type get_behaviour_type() const override {
            return BEHAVIOUR_TYPE_SINGLE_TOUCH;
        }

        const eka2l1::vec2 position() const {
            return position_;
        }

        const std::uint64_t keycode() const {
            return keycode_;
        }
    };

    struct joystick_behaviour : public behaviour {
    private:
        std::uint64_t keys_[5];
        std::uint8_t key_status_;

        eka2l1::vec2 current_diff_;
        eka2l1::vec2 position_;
        std::uint32_t width_;
        std::uint8_t allocated_pointer_;

    public:
        explicit joystick_behaviour(executor *exec, const eka2l1::vec2 &pos, const std::uint32_t width);
        explicit joystick_behaviour(executor *exec, const YAML::Node &joy_seri_node);

        ~joystick_behaviour();

        void produce(const std::uint64_t key, const bool is_press) override;
        void produce(const std::uint64_t key, const float axisx, const float axisy) override;
        std::vector<std::uint64_t> mapped_keys() override;
        void serialize(YAML::Emitter &emitter) override;

        // No OOB check, use carefully
        void set_key(const int index, const std::uint64_t key) {
            keys_[index] = key;
        }

        std::uint64_t key(int index) const {
            return keys_[index];
        }

        std::uint32_t width() const {
            return width_;
        }

        const eka2l1::vec2 position() const {
            return position_;
        }

        behaviour_type get_behaviour_type() const override {
            return BEHAVIOUR_TYPE_JOYSTICK;
        }
    };

    struct executor {
    public:
        static constexpr std::size_t MAX_POINTERS = 8;

    private:
        friend class editor;

        window_server *winserv_;
        ntimer *timing_;

        std::uint32_t pointer_ownership_;

        std::map<std::uint64_t, std::vector<behaviour*>> mappings_;
        std::vector<std::unique_ptr<behaviour>> behaviours_;
    
    public:
        explicit executor(window_server *server, ntimer *timing);

        bool execute(const std::uint64_t keycode, const bool is_press);
        bool execute(const std::uint64_t keycode, const float axisx, const float axisy);

        void serialize(const std::string &conf_path);
        void deserialize(const std::string &conf_path);

        void reset();
        void add_behaviour(std::unique_ptr<behaviour> &behaviour);

        std::uint8_t allocate_free_pointer();
        void free_allocated_pointer(const std::uint8_t index);

        window_server *get_window_server() {
            return winserv_;
        }

        void set_window_server(window_server *serv) {
            winserv_ = serv;
        }

        ntimer *get_timing() {
            return timing_;
        }

        bool is_active() const {
            return !behaviours_.empty();
        }
    };
}
