/*
 * Copyright (c) 2019 EKA2L1 Team
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

#include <cstdint>

#include <common/types.h>
#include <utils/des.h>
#include <utils/locale.h>

namespace eka2l1 {
    struct entry_info;
    class io_system;
}

namespace eka2l1::epoc::fs {
    enum file_mode {
        file_share_exclusive,
        file_share_readers_only,
        file_share_any,
        file_share_readers_or_writers,
        file_stream = 0,
        file_stream_text = 0x100,
        file_read = 0,
        file_write = 0x200,
        file_read_async_all = 0x400,
        file_write_buffered = 0x00000800,
        file_write_direct_io = 0x00001000,
        file_read_buffered = 0x00002000,
        file_read_direct_io = 0x00004000,
        file_read_ahead_on = 0x00008000,
        file_read_ahead_off = 0x00010000,
        file_delete_on_close = 0x00020000,
        file_big_size = 0x00040000,
        file_sequential = 0x00080000
    };

    constexpr std::uint32_t entry_att_normal = 0x0000;
    constexpr std::uint32_t entry_att_read_only = 0x0001;
    constexpr std::uint32_t entry_att_hidden = 0x0002;
    constexpr std::uint32_t entry_att_system = 0x0004;
    constexpr std::uint32_t entry_att_volume = 0x0008;
    constexpr std::uint32_t entry_att_dir = 0x0010;
    constexpr std::uint32_t entry_att_archive = 0x0020;
    constexpr std::uint32_t entry_att_xip = 0x0080;
    constexpr std::uint32_t entry_att_remove = 0x0100;
    constexpr std::uint32_t entry_att_mask_filesystem_specific = 0x00FF0000;
    constexpr std::uint32_t entry_att_match_exclusive = 0x40000000;
    constexpr std::uint32_t entry_att_match_exclude = 0x8000000;
    constexpr std::uint32_t entry_att_match_mask = (entry_att_hidden | entry_att_system | entry_att_dir);

    constexpr std::uint32_t drive_att_transaction = 0x80;
    constexpr std::uint32_t drive_att_pageable = 0x100;
    constexpr std::uint32_t drive_att_logically_removable = 0x200;
    constexpr std::uint32_t drive_att_hidden = 0x400;
    constexpr std::uint32_t drive_att_external = 0x800;
    constexpr std::uint32_t drive_att_all = 0x100000;
    constexpr std::uint32_t drive_att_exclude = 0x40000;
    constexpr std::uint32_t drive_att_exclusive = 0x80000;

    constexpr std::uint32_t drive_att_local = 0x01;
    constexpr std::uint32_t drive_att_rom = 0x02;
    constexpr std::uint32_t drive_att_redirected = 0x04;
    constexpr std::uint32_t drive_att_substed = 0x08;
    constexpr std::uint32_t drive_att_internal = 0x10;
    constexpr std::uint32_t drive_att_removable = 0x20;

    constexpr std::uint32_t media_att_write_protected = 0x08;

    enum media_type {
        media_not_present,
        media_unknown,
        media_floppy,
        media_hard_disk,
        media_cd_rom,
        media_ram,
        media_flash,
        media_rom,
        media_remote,
        media_nand_flash,
        media_rotating
    };

    enum battery_state {
        battery_state_not_supported,
        battery_state_good,
        battery_state_low
    };

    enum connection_bus_type {
        connection_bus_internal,
        connection_bus_usb
    };

    enum class extended_fs_query_command {
        file_system_sub_type,
        io_param_info,
        is_drive_sync,
        is_drive_finalised,
        extensions_supported
    };

    enum file_cache_flags {
        file_cache_read_enabled = 0x01,
        file_cache_read_on = 0x02,
        file_cache_read_ahead_enabled = 0x04,
        file_cache_read_ahead_on = 0x08,
        file_cache_write_enabled = 0x10,
        file_cache_write_on = 0x20,
    };

    struct drive_info_v1 {
        epoc::fs::media_type type;
        epoc::fs::battery_state battery;
        std::uint32_t drive_att;
        std::uint32_t media_att;
    };
    
    static_assert(sizeof(drive_info_v1) == 16, "Size of drive info v1 is incorrect");

    struct drive_info_v2 : public drive_info_v1 {
        epoc::fs::connection_bus_type connection_bus_type;
    };

    #pragma pack(push, 1)
    struct volume_info_v1 {
        drive_info_v1 drv_info;
        std::uint32_t uid;
        std::int64_t size;
        std::int64_t free;
        epoc::bufc_static<char16_t, 0x100> name;
    };
    #pragma pack(pop)

    static_assert(sizeof(volume_info_v1) == 552, "Size of volume info v1 is incorrect");

    #pragma pack(push, 1)
    struct volume_info_v2 {
        drive_info_v2 drv_info;
        std::uint32_t uid;
        std::int64_t size;
        std::int64_t free;
        epoc::bufc_static<char16_t, 0x100> name;
        file_cache_flags cache_flags;
        std::uint8_t vol_size_async;

        std::uint8_t reserved1;
        std::uint16_t reserved2;
        std::uint32_t reserved3;
        std::uint32_t reserved4;
    };
    #pragma pack(pop)

    struct io_drive_param_info {
        int block_size;
        int cluster_size;
        int rec_read_buf_size;
        int rec_write_buf_size;
        std::uint64_t max_supported_file_size;
    };

    static constexpr std::size_t entry_standard_size = 28;

    struct entry {
        std::uint32_t attrib;
        std::uint32_t size;

        epoc::time modified;
        std::uint32_t uid1;
        std::uint32_t uid2;
        std::uint32_t uid3;

        epoc::bufc_static<char16_t, 0x100> name;

        // For 64-bit file support.
        std::uint32_t size_high;
        std::uint32_t reserved;
    };

    std::string get_server_name_through_epocver(const epocver ver);
    std::uint32_t build_attribute_from_entry_info(entry_info &info);
    void build_symbian_entry_from_emulator_entry(io_system *io, entry_info &info, epoc::fs::entry &sym_entry);
}
