/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 * 
 * Initial contributor: pent0
 * Contributors:
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

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace eka2l1::common {
    class arg_parser;
    using arg_handler_function = std::function<bool(arg_parser *, void *, std::string *)>;

    /*! \brief A parser for command line arguments.
     *
     * This is a lightweight command line parser, works with a callback mechanic.
     * 
     * Users use this will use a callback function to handle with arguments whenether there
     * is an occurence of an option.
    */
    class arg_parser {
        int argc;
        const char **argv;

        int counter{ 0 };

        struct arg_info {
            std::vector<std::string> alternatives;
            std::string help;

            arg_handler_function handler;
        };

        std::unordered_map<std::string, arg_handler_function> handlers;
        std::vector<arg_info> infos;

    public:
        explicit arg_parser(const int argc, const char **argv);
        bool add(const std::string &option, const std::string &help,
            arg_handler_function handler);

        /*! \brief Get the next argument token.
         *
         * Intended for use in both this and the handler.
         * 
         * \returns Nullptr if there is no other token left.
        */
        const char *next_token();

        /**
         * \brief Peek the next token without increasing the read pointer.
         */
        const char *peek_token();

        /*! \brief Start parsing.
         *
         * When a handler is called, it will return a boolean, which will
         * tell parser to stop.
         *
         * \param userdata      Pointer to userdata.

         * \param err           Pointer to a string, which error code will be written to 
         *                      if the return value of this function is 0
         *
         * 
         * \returns             True on success, false on failure or force stop.
         *                      Handler should report errors on real failure. Force stop will 
         *                      leave the error string empty.
        */
        bool parse(void *userdata, std::string *err);

        // I haven't think of any case where I would like to remove an option at runtime.
        // Of course, the design already prepares for this situation
        // Let's just ... No problem here.
        // To many referencessssssss, is this emulator a movie ? Totally not
        // bool remove(const std::string &option);

        std::string get_help_string(const int tab_each_line = 1);
    };
}
