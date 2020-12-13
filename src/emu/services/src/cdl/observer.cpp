/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <system/epoc.h>
#include <loader/rsc.h>
#include <services/cdl/cdl.h>
#include <services/cdl/observer.h>
#include <vfs/vfs.h>

#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>

namespace eka2l1::epoc {
    bool cdl_ecom_generic_observer::read_refs_of_instance(io_system *io, const std::u16string &path, cdl_ref_collection &collection_) {
        const drive_number instance_drv = char16_to_drive(eka2l1::root_name(path)[0]);
        const std::u16string instance_fn = eka2l1::replace_extension(eka2l1::filename(path), u"");

        const std::u16string ref_rsc_path = std::u16string(1, drive_to_char16(instance_drv))
            + u":\\resource\\cdl\\" + instance_fn + u"_cdl_detail.rsc";

        symfile rsc_file = io->open_file(ref_rsc_path, READ_MODE | BIN_MODE);

        if (!rsc_file) {
            LOG_ERROR(SERVICE_CDLENG, "Can't find reference resource file for CDL instance: {}", common::ucs2_to_utf8(path));
            return false;
        }

        eka2l1::ro_file_stream ref_rsc_file_stream(rsc_file.get());
        loader::rsc_file ref_rsc_stream(reinterpret_cast<common::ro_stream *>(&ref_rsc_file_stream));

        auto ref_rsc_data = ref_rsc_stream.read(1);
        common::ro_buf_stream ref_rsc_data_stream(&ref_rsc_data[0], ref_rsc_data.size());

        // Read data
        std::uint16_t total_ref_count = 0;
        if (ref_rsc_data_stream.read(&total_ref_count, sizeof(total_ref_count)) != sizeof(total_ref_count)) {
            return false;
        }

        // Resize the refs collection
        for (std::uint16_t i = 0; i < total_ref_count; i++) {
            cdl_ref ref{};
            ref.name_ = path;

            if (ref_rsc_data_stream.read(&ref.uid_, sizeof(std::uint32_t)) != sizeof(std::uint32_t)) {
                return false;
            }

            if (ref_rsc_data_stream.read(&ref.id_, sizeof(std::uint32_t)) != sizeof(std::uint32_t)) {
                return false;
            }

            collection_.push_back(std::move(ref));
        }

        return true;
    }

    void cdl_ecom_generic_observer::entry_added(const std::u16string &plugin_path) {
        cdl_ref_collection collection_;

        if (read_refs_of_instance(serv_->get_system()->get_io_system(),
                plugin_path, collection_)) {
            serv_->add_refs(collection_);
        }
    }

    void cdl_ecom_generic_observer::entry_removed(const std::u16string &plugin_path) {
        serv_->remove_refs(plugin_path);
    }
}
