/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
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

#include <scripting/common/compression.h>
#include <scripting/common/loader.h>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
namespace scripting = eka2l1::scripting;

PYBIND11_MODULE(common, m) {
    py::class_<scripting::ibytepair_stream_wrapper>
        bytepair_stream(m, "BytePairReaderStream");

    bytepair_stream.def(py::init([](const std::string &path) {
        return std::make_unique<scripting::ibytepair_stream_wrapper>(path);
    }));

    bytepair_stream.def("getAllPageOffsets", &scripting::ibytepair_stream_wrapper::get_all_page_offsets, R"pbdoc(
        Get the start offset of all pages available in the bytepair stream.
    )pbdoc");

    bytepair_stream.def("readPageTable", &scripting::ibytepair_stream_wrapper::get_all_page_offsets, R"pbdoc(
        Read the page table. Page table in the bytepair stream is where all pages information are stored.
    )pbdoc");

    bytepair_stream.def("readPage", &scripting::ibytepair_stream_wrapper::read_page, R"pbdoc(
        Read a page at specified index. Throw IndexError if index is out of available pages range.
    )pbdoc");

    bytepair_stream.def("readPages", &scripting::ibytepair_stream_wrapper::read_page, R"pbdoc(
        Read all pages available.
    )pbdoc");

    py::class_<scripting::mbm_reader>(m, "MbmReader")
        .def(py::init([](const std::string &path) { return std::make_unique<scripting::mbm_reader>(path); }))
        .def("bitmapCount", &scripting::mbm_reader::bitmap_count, R"pbdoc(
            Get total of bitmap this MBM file consists of.
        )pbdoc")
        .def("saveBitmap", &scripting::mbm_reader::save_bitmap, R"pbdoc(
            Save bitmap as BMP file.

            Parameters
            ------------------------
            idx: int
                 Index of the bitmap. The index is zero-based.

            destPath: str
                 The path to the file to store

            Returns
            ------------------------
            bool
                 Returns true if success.
        )pbdoc")
        .def("bitsPerPixel", &scripting::mbm_reader::bits_per_pixel, R"pbdoc(
            Get total of bits a pixel consists of in a bitmap.

            Parameters
            ------------------------
            idx: int
                 Index of the bitmap. The index is zero-based.

            Returns
            ------------------------
            int
                 The number of bits a pixel consists of.
        )pbdoc")
        .def("dimension", &scripting::mbm_reader::bitmap_size, R"pbdoc(
            Get dimension of a bitmap.

            Parameters
            ------------------------
            idx: int
                 Index of the bitmap. The index is zero-based.

            Returns
            ------------------------
            tuple (int, int)
                 Dimension of the target bitmap.

        )pbdoc");

    py::class_<scripting::mif_reader>(m, "MifReader")
        .def(py::init([](const std::string &path) { return std::make_unique<scripting::mif_reader>(path); }))
        .def("entryCount", &scripting::mif_reader::entry_count, R"pbdoc(
            Get total of entry this MIF file consists of.
        )pbdoc")
        .def("entryDataSize", &scripting::mif_reader::entry_data_size, R"pbdoc(
            Get the binary data size of a MIF entry.
            
            Parameters
            ------------------------
            idx: int
                 Index of the entry. The index is zero-based.

            Returns
            ------------------------
            int
                 The size needed.
        )pbdoc")
        .def("readEntry", &scripting::mif_reader::read_entry, R"pbdoc(
            Read data of a MIF entry.

            Parameters
            ------------------------
            idx: int
                 Index of the entry. The index is zero-based.

            Returns
            ------------------------
            str
                 String contains binary data of the entry.
        )pbdoc");
}
