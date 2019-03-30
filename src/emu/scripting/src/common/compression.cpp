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

#include <common/buffer.h>
#include <common/bytepair.h>
#include <scripting/common/compression.h>

#include <epoc/vfs.h>

#include <pybind11/pybind11.h>

namespace py = pybind11;
namespace scripting = eka2l1::scripting;

namespace eka2l1::scripting {
    ibytepair_stream_wrapper::ibytepair_stream_wrapper(const std::string &path) {
        eka2l1::symfile f = eka2l1::physical_file_proxy(path, READ_MODE | BIN_MODE);

        if (!f) {
            throw pybind11::value_error("File doesn't exists!");
        }

        raw_fstream = std::reinterpret_pointer_cast<common::ro_stream>(std::make_shared<eka2l1::ro_file_stream>(f));
        bytepair_stream.set_stream(reinterpret_cast<common::ro_stream*>(&(*raw_fstream)));
    }

    std::vector<uint32_t> ibytepair_stream_wrapper::get_all_page_offsets() {
        return bytepair_stream.page_offsets(0);
    }

    void ibytepair_stream_wrapper::read_page_table() {
        bytepair_stream.read_table();
    }

    std::vector<char> ibytepair_stream_wrapper::read_page(uint16_t index) {
        const auto tab = bytepair_stream.table();

        if (index >= tab.header.number_of_pages) {
            throw pybind11::index_error("The index is out of the range of available pages");
        }

        std::vector<char> result;
        result.resize(tab.page_size[index]);

        bytepair_stream.read_page(&result[0], index, result.size());

        return result;
    }

    std::vector<char> ibytepair_stream_wrapper::read_pages() {
        const auto tab = bytepair_stream.table();

        std::vector<char> result;
        result.resize(tab.header.size_of_data);

        bytepair_stream.read_pages(&result[0], result.size());

        return result;
    }
}

PYBIND11_MODULE(common, m) {
    py::class_<scripting::ibytepair_stream_wrapper>
        bytepair_stream(m, "BytePairReaderStream");

    bytepair_stream.def(py::init([](const std::string &path) {
        return std::make_unique<scripting::ibytepair_stream_wrapper>(path);
    }));

    bytepair_stream.def("getAllPageOffsets", &scripting::ibytepair_stream_wrapper::get_all_page_offsets,
        R"pbdoc(
        Get the start offset of all pages available in the bytepair stream.
        )pbdoc");

    bytepair_stream.def("readPageTable", &scripting::ibytepair_stream_wrapper::get_all_page_offsets,
        R"pbdoc(
        Read the page table. Page table in the bytepair stream is where all pages information are stored.
        )pbdoc");

    bytepair_stream.def("readPage", &scripting::ibytepair_stream_wrapper::read_page,
        R"pbdoc(
        Read a page at specified index. Throw IndexError if index is out of available pages range.
        )pbdoc");

    bytepair_stream.def("readPages", &scripting::ibytepair_stream_wrapper::read_page,
        R"pbdoc(
        Read all pages available.
        )pbdoc");
}
