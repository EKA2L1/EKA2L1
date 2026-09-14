/*
 * Copyright (c) 2026 EKA2L1 Team.
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

#include <services/internet/accesspoints.h>
#include <services/centralrepo/repo.h>

#include <array>
#include <variant>

namespace eka2l1 {
    namespace {
        constexpr std::uint32_t iap_table = 0x02800000;
        constexpr std::uint32_t network_table = 0x01800000;
        constexpr std::uint32_t packet_service_table = 0x0C800000;
        constexpr std::uint32_t modem_bearer_table = 0x08000000;
        constexpr std::uint32_t lan_service_table = 0x04800000;
        constexpr std::uint32_t lan_bearer_table = 0x08800000;
        constexpr std::uint32_t wap_table = 0x03000000;
        constexpr std::uint32_t wap_bearer_table = 0x0A000000;

        std::array<bool, 255> used_records(const central_repo &repo, std::uint32_t table) {
            std::array<bool, 255> used{};
            for (const auto &entry : repo.entries) {
                const auto id = (entry.key >> 8) & 255;
                if ((entry.key & 0x7F800000) == table && id > 0 && id < 255) {
                    used[id] = true;
                }
            }
            return used;
        }

        std::uint32_t free_record(const central_repo &repo, std::uint32_t table) {
            const auto used = used_records(repo, table);
            for (std::uint32_t id = 1; id < used.size(); id++) {
                if (!used[id]) {
                    return id;
                }
            }
            return 0;
        }

        using field_value = std::variant<std::uint32_t, std::u16string>;

        void add_value(central_repo &repo, std::uint32_t key, const field_value &value) {
            central_repo_entry entry{};
            entry.key = key;
            entry.transient = true;
            if (const auto number = std::get_if<std::uint32_t>(&value)) {
                entry.data.etype = central_repo_entry_type::integer;
                entry.data.intd = *number;
            } else {
                const auto &text = std::get<std::u16string>(value);
                entry.data.etype = central_repo_entry_type::string;
                entry.data.strd.assign(reinterpret_cast<const char *>(text.data()), text.size() * sizeof(char16_t));
            }
            if (!repo.find_entry(key)) {
                repo.entries.push_back(std::move(entry));
            }
        }

        void add_record(central_repo &repo, std::uint32_t table, std::uint32_t id,
            std::initializer_list<std::pair<std::uint32_t, field_value>> fields) {
            for (const auto &[column, value] : fields) {
                add_value(repo, table | (column << 16) | (id << 8), value);
            }
            // CommsDat enumerates record nodes separately from the field values.
            for (const auto record : {id, 255U}) {
                const auto node = table | 0x007F0000 | (record << 8);
                add_value(repo, node, record == 255 ? field_value(std::u16string()) : field_value(0U));
                add_value(repo, node | 0x80000000, node | 0x80000000);
            }
        }
    }

    void provide_host_access_point(central_repo &repo) {
        if (repo.uid != 0xCCCCCC00) {
            return;
        }
        const auto iaps = used_records(repo, iap_table);
        for (const bool used : iaps) {
            if (used) {
                return;
            }
        }

        std::uint32_t packet_bearer = 0;
        const std::u16string packet_nif = u"genericnif";
        const std::string packet_nif_bytes(reinterpret_cast<const char *>(packet_nif.data()), packet_nif.size() * sizeof(char16_t));
        for (std::uint32_t id = 1; id < 255; id++) {
            const auto *entry = repo.find_entry(modem_bearer_table | 0x00030000 | (id << 8));
            if (entry && entry->data.etype == central_repo_entry_type::string && entry->data.strd == packet_nif_bytes) {
                packet_bearer = id;
                break;
            }
        }

        const auto network = free_record(repo, network_table);
        const auto service = free_record(repo, packet_bearer ? packet_service_table : lan_service_table);
        const auto bearer = packet_bearer ? packet_bearer : free_record(repo, lan_bearer_table);
        const auto wap = free_record(repo, wap_table);
        const auto wap_bearer = free_record(repo, wap_bearer_table);
        if (!network || !service || !bearer || !wap || !wap_bearer) {
            return;
        }

        const std::u16string name = u"Host network";
        add_record(repo, network_table, network, {{1, network}, {2, name}, {3, u""}});
        if (packet_bearer) {
            add_record(repo, packet_service_table, service, {
                {1, service}, {2, name}, {3, u"host"}, {4, 0U}, {6, 0U}, {7, 0U}, {8, 0U},
                {9, 0U}, {10, 0U}, {11, 0U}, {12, 0U}, {13, 0U}, {14, 0U}, {15, 0U},
                {16, 0U}, {17, 0U}, {18, 1U}, {19, 0U}, {21, u"ip"}, {22, 0U},
                {23, u""}, {24, u""}, {25, 0U}, {27, u"0.0.0.0"}, {28, 1U}, {29, u"0.0.0.0"},
                {30, 1U}, {31, u"0.0.0.0"}, {32, u"0.0.0.0"}, {33, 1U}, {34, u"::"},
                {35, u"::"}, {40, 0U}, {41, 1U}, {42, 2U}, {43, 0xFFFFFFFFU}});
        } else {
            add_record(repo, lan_service_table, service, {
                {1, service}, {2, name}, {3, u"ip,ip6"}, {4, u"0.0.0.0"}, {5, u"0.0.0.0"},
                {6, 1U}, {7, u"0.0.0.0"}, {8, 1U}, {9, u"0.0.0.0"}, {10, u"0.0.0.0"},
                {11, 1U}, {12, u"::"}, {13, u"::"}, {16, u""}, {17, u""}, {18, u""}, {19, 0U}});
            add_record(repo, lan_bearer_table, bearer, {
                {1, bearer}, {2, name}, {3, u"lan"}, {4, u""}, {5, u""}, {6, u""}, {7, u""},
                {8, u""}, {9, 0U}, {10, 0U}, {11, 0U}});
        }
        add_record(repo, iap_table, 1, {
            {1, 1U}, {2, name}, {3, packet_bearer ? u"OutgoingGPRS" : u"LANService"},
            {4, service}, {5, packet_bearer ? u"ModemBearer" : u"LANBearer"},
            {6, bearer}, {7, network}, {8, 0U}, {9, 0U}});
        add_record(repo, wap_table, wap, {{1, wap}, {2, name}, {3, u"WAPIPBearer"}, {4, u""}});
        add_record(repo, wap_bearer_table, wap_bearer, {
            {1, wap_bearer}, {2, name}, {3, wap}, {4, u"0.0.0.0"}, {5, 0U},
            {6, 1U}, {7, 0U}, {8, 0U}, {9, 0U}, {10, u""}, {11, u""}});
    }
}
