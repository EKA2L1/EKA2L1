/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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

#include <atomic>
#include <functional>
#include <string>
#include <stack>
#include <vector>

#include <manager/sis_fields.h>

namespace eka2l1 {
    class system;
    class io_system;
    class window_server;

    namespace config {
        struct state;
    }

    namespace manager {
        class package_manager;
        struct device;
    }

    namespace common {
        class ro_stream;
    }

    namespace loader {
        // An interpreter that runs SIS install script
        class ss_interpreter {
            sis_controller *main_controller;
            sis_data *install_data;
            config::state *conf;

            std::stack<sis_controller*> current_controllers;

            drive_number install_drive;

            common::ro_stream *data_stream;

            io_system *io;
            window_server *winserv;

            manager::package_manager *mngr;
            manager::device *current_dvc;

            bool skip_next_file{ false };

            bool appprop(const sis_uid uid, sis_property prop);
            bool package(const sis_uid uid);

            /**
             * \brief   Check if the given expression's condition can be passed.
             * \return  1 if it can be passed, -1 for error, 0 if not passed.
             */
            int condition_passed(sis_expression *expr);

            /**
             * \brief Get the true integral value from an expression.
             */
            int gasp_true_form_of_integral_expression(const sis_expression &expr);

        public:
            show_text_func show_text; ///< Hook function to display texts.
            choose_lang_func choose_lang; ///< Hook function to choose controller's language.

            /**
             * \brief Get the data in the index of a buffer block in the SIS.
             * 
             * The function assumes that data has small size, and will load it into a buffer.
             * Of course, if the buffer has size of something like 200 MB, it will crash.
             * 
             * This function is usually used for extracting text file.
             * 
             * \param data_idx      The index of the source buffer in block buffer.
             * \param crr_block_idx The block index.
             * 
             * \returns A vector contains binary data, uncompressed if neccessary.
             */
            std::vector<uint8_t> get_small_file_buf(uint32_t data_idx, uint16_t crr_blck_idx);

            /**
             * \brief Get the data in the index of a buffer block in the SIS, write it to a physical file.
             * 
             * Usually uses for extracting large app data.
             * 
             * \param path          UTF-8 path to the physical file.
             * \param data_idx      The index of the source buffer in block buffer.
             * \param crr_block_idx The block index..
             */
            void extract_file(const std::string &path, const uint32_t idx, uint16_t crr_blck_idx);

            explicit ss_interpreter();
            explicit ss_interpreter(common::ro_stream *stream,
                system *sys,
                manager::package_manager *pkgmngr,
                manager::device *crr_dvc,
                config::state *conf,
                sis_controller *main_controller,
                sis_data *inst_data,
                drive_number install_drv);

            bool interpret(sis_install_block &install_block, std::atomic<int> &progress,
                uint16_t crr_blck_idx = 0);

            bool interpret(sis_controller *controller, const std::uint16_t base_data_idx, std::atomic<int> &progress);

            bool interpret(std::atomic<int> &progress) {
                return interpret(main_controller, 0, progress);
            }
        };
    }
}
