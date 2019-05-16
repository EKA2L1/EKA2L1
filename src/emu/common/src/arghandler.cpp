/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
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
#include <common/arghandler.h>

#include <sstream>

namespace eka2l1::common {
    arg_parser::arg_parser(const int argc, char **argv)
        : argc(argc)
        , argv(argv)
        , counter(1) {
    }

    std::string arg_parser::get_help_string(const int tab_each_line) {
        std::ostringstream ss;

        for (auto &info : infos) {
            for (int i = 0; i < tab_each_line; i++) {
                ss << "\t";
            }

            for (std::size_t i = 0; i < info.alternatives.size(); i++) {
                ss << info.alternatives[i];

                if (info.alternatives.size() - 1 != i) {
                    ss << ",";
                }
            }

            ss << " : " << info.help << '\n';
        }

        return ss.str();
    }

    const char *arg_parser::next_token() {
        if (counter == argc) {
            return nullptr;
        }

        return argv[counter++];
    }

    const char *arg_parser::peek_token() {
        if (counter == argc) {
            return nullptr;
        }

        return argv[counter];
    }

    bool arg_parser::add(const std::string &option, const std::string &help,
        arg_handler_function handler) {
        // SLAYY THEM OFF
        // Calm down
        // TIL that getline can slay them off for me, thank you Kanye, very cool
        std::istringstream input(option);
        arg_info info;

        while (input.good()) {
            std::string op;
            std::getline(input, op, ',');

            op = common::trim_spaces(op);

            info.alternatives.push_back(op);

            // Emplace the handler in
            handlers.emplace(op, handler);
        }

        info.help = help;
        info.handler = handler;

        infos.push_back(std::move(info));
        return true;
    }

    bool arg_parser::parse(std::string *err) {
        while (const char *tok = next_token()) {
            auto ite = handlers.find(tok);
            if (ite == handlers.end()) {
                *err = "Unknown argument: ";
                *err += tok;

                return false;
            }

            if (!ite->second(this, err)) {
                return false;
            }
        }

        return true;
    }
}
