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

#include <scripting/common/loader.h>
#include <epoc/vfs.h>

#include <pybind11/pybind11.h>

namespace py = pybind11;
namespace scripting = eka2l1::scripting;

namespace eka2l1::scripting {
    mbm_reader::mbm_reader(const std::string &path) {
        eka2l1::symfile f = eka2l1::physical_file_proxy(path, READ_MODE | BIN_MODE);

        if (!f) {
            throw std::runtime_error("File doesn't exists!");
        }

        stream_ = std::reinterpret_pointer_cast<common::ro_stream>(std::make_shared<eka2l1::ro_file_stream>(f.get()));
        mbm_ = std::make_unique<loader::mbm_file>(reinterpret_cast<common::ro_stream*>(&(*stream_)));

        if (!mbm_->do_read_headers()) {
            throw std::runtime_error("Reading MBM headers failed!");
        }
    }

    std::uint32_t mbm_reader::bitmap_count() {
        return static_cast<std::uint32_t>(mbm_->sbm_headers.size());
    }

    bool mbm_reader::save_bitmap(const std::uint32_t idx, const std::string &dest_path) {
        return mbm_->save_bitmap_to_file(idx, dest_path.c_str());
    }

    std::uint32_t mbm_reader::bits_per_pixel(const std::uint32_t idx) {
        if (mbm_->sbm_headers.size() <= idx) {
            throw pybind11::index_error("Index of bitmap is out of range. Use bitmapCount to get maximum number of bitmap!");
            return 0;
        }

        return mbm_->sbm_headers[idx].bit_per_pixels;
    }

    std::pair<int, int> mbm_reader::bitmap_size(const std::uint32_t idx) {
        if (mbm_->sbm_headers.size() <= idx) {
            throw pybind11::index_error("Index of bitmap is out of range. Use bitmapCount to get maximum number of bitmap!");
            return {};
        }

        return { mbm_->sbm_headers[idx].size_pixels.width(), mbm_->sbm_headers[idx].size_pixels.height() };
    }

    mif_reader::mif_reader(const std::string &path) {
        eka2l1::symfile f = eka2l1::physical_file_proxy(path, READ_MODE | BIN_MODE);

        if (!f) {
            throw std::runtime_error("File doesn't exists!");
        }

        stream_ = std::reinterpret_pointer_cast<common::ro_stream>(std::make_shared<eka2l1::ro_file_stream>(f.get()));
        mif_ = std::make_unique<loader::mif_file>(reinterpret_cast<common::ro_stream*>(&(*stream_)));

        if (!mif_->do_parse()) {
            throw std::runtime_error("Reading MIF headers failed!");
        }
    }

    std::uint32_t mif_reader::entry_count() {
        return mif_->header_.array_len;
    }

    int mif_reader::entry_data_size(const std::size_t idx) {
        int size = 0;

        if (!mif_->read_mif_entry(idx, nullptr, size)) {
            throw pybind11::index_error("Index of MIF entry out of range!");
        }

        return size;
    }

    py::bytes mif_reader::read_entry(const std::size_t idx) {
        std::string dat;
        int size = 0;
       
        if (!mif_->read_mif_entry(idx, nullptr, size)) {
            throw pybind11::index_error("Index of MIF entry out of range!");
        }

        dat.resize(size);
        mif_->read_mif_entry(idx, reinterpret_cast<std::uint8_t*>(&dat[0]), size);

        return dat;
    }
}
