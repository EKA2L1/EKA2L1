/*
 * Copyright (c) 2026 EKA2L1 Team
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

#include <common/types.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace eka2l1::common {
    struct archive_entry_info {
        // Path as stored in the archive, normalised to '/' separators.
        std::string path;
        std::uint64_t size;
        bool is_directory;
    };

    /**
     * @brief Read the table of contents of an archive.
     *
     * Which container it is comes from the file's contents, not its name, so this reads .7z today and
     * whatever else libarchive is built with tomorrow.
     *
     * @returns False if the file could not be opened or is not an archive we can read.
     */
    bool list_archive(const std::string &path, std::vector<archive_entry_info> &entries_out);

    /**
     * @brief Unpack an archive, one entry at a time, letting the caller pick what lands where.
     *
     * The archive is walked front to back exactly once - which is the only efficient way through a solid
     * .7z, where a member can only be reached by decoding everything ahead of it. Entries arrive in the
     * same order list_archive() reported them, so a caller that has already decided what it wants can
     * simply index into its own plan.
     *
     * @param choose_destination Called for every entry with its info; return the host path to write it
     *                           to, or an empty string to skip it. Directories are created as needed.
     * @param progress_cb        Called after every entry, skipped ones included, with how much of the
     *                           archive has been walked past and how big it is uncompressed - streaming
     *                           past an unwanted entry costs time too. Optional.
     * @param cancel_cb          Polled between entries; returning true aborts. Files already written are
     *                           left behind for the caller to clean up. Optional.
     *
     * @returns False on a read/decode error, a failed write, or a cancel.
     */
    bool extract_archive(const std::string &path,
        const std::function<std::string(const archive_entry_info &)> &choose_destination,
        progress_changed_callback progress_cb, cancel_requested_callback cancel_cb);
}
