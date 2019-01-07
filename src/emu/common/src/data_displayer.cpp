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
#include <common/data_displayer.h>

namespace eka2l1 {
    std::shared_ptr<data_dumper_interface> data_dumper;

    void data_dump_setup(std::shared_ptr<data_dumper_interface> dd) {
        data_dumper = dd;
    }

    void dump_data(const std::string &name, std::vector<uint8_t> data, size_t start_off) {
        data_dumper->set_name(name);
        data_dumper->set_data(data);
        data_dumper->set_start_off(start_off);
    }

    void dump_data_map(const std::string &name, uint8_t *start, size_t size, size_t start_off) {
        data_dumper->set_name(name);
        data_dumper->set_data_map(start, size);
        data_dumper->set_start_off(start_off);
    }
}
