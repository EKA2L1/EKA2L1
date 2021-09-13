/*
 * Copyright (c) 2021 EKA2L1 Team
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

#include <services/msv/common.h>
#include <vector>

#include <common/linked.h>

namespace eka2l1::epoc::msv {
    static constexpr std::uint32_t CACHE_THRESHOLD = 120;

    struct entry_range_table {
    private:
        msv_id min_;
        msv_id max_;

        std::uint32_t flags_;

        std::vector<entry> entries_;
        common::double_linked_queue_element folder_link_;

        enum {
            FLAG_GRAND_CHILD_PRESENT = 1 << 0,          // Some entry that parent is not directly this entry table visible folder exists
        };

        friend struct visible_folder;

    public:
        explicit entry_range_table();

        entry *add(entry &ent, const bool extend_range = false);

        /**
         * @brief Add list of entries to the table, with no overlap checking.
         * 
         * This method automatically expands the table range.
         * 
         * If there's only one entry in this list, add is called, with overlap checking.
         * 
         * @param ents Entry list sorted by ID.
         */
        bool add_tons(std::vector<entry> &ents, const std::size_t start_index, const std::size_t end_index);

        entry *get(const msv_id id);
        bool remove(const msv_id id);

        void min_range(msv_id min) {
            min_ = min;
        }

        void max_range(msv_id max) {
            max_ = max;
        }

        const msv_id min_range() const {
            return min_;
        }

        const msv_id max_range() const {
            return max_;
        }

        const bool in_range(const msv_id id) const {
            return (id >= min_) && (id <= max_);
        }

        bool grand_child_present() const {
            return flags_ & FLAG_GRAND_CHILD_PRESENT;
        }

        void grand_child_present(const bool value) {
            flags_ &= ~FLAG_GRAND_CHILD_PRESENT;
            if (value) {
                flags_ |= FLAG_GRAND_CHILD_PRESENT;
            }
        }

        bool splitable() const;
        const std::size_t left_to_splittable() const;
        bool do_split(const msv_id parent_splitter);

        bool update_child_id(const msv_id parent_id, const msv_id child_id, const bool is_add);

        const bool empty() const {
            return entries_.empty();
        }
    };

    enum visible_folder_children_query_error {
        visible_folder_children_query_ok = 0,
        visible_folder_children_query_no_child_id_array = 1,
        visible_folder_children_incomplete = 2
    };

    struct visible_folder {
    private:
        common::roundabout tables_;
        std::uint32_t flags_;
        msv_id myid_;

        enum {
            FLAG_ALL_CHILDREN_PRESENT = 1 << 0
        };

        void clear_tables();
        bool update_child_id(const msv_id parent_id, const msv_id child_id, const bool is_add);

    public:
        common::double_linked_queue_element indexer_link_;

        explicit visible_folder(const msv_id my_id);
        ~visible_folder();

        entry *add(entry &ent);
        bool add_entry_list(std::vector<entry> &ent, const bool complete_child_of_folder = false);

        entry *get_entry(const msv_id id);
        bool remove_entry(const msv_id id);

        std::vector<entry *> get_children_by_parent(const msv_id parent_id, visible_folder_children_query_error *error = nullptr);

        const msv_id id() const {
            return myid_;
        }

        bool all_children_present() const {
            return flags_ & FLAG_ALL_CHILDREN_PRESENT;
        }

        void all_children_present(const bool value) {
            flags_ &= ~FLAG_ALL_CHILDREN_PRESENT;
            if (value) {
                flags_ |= FLAG_ALL_CHILDREN_PRESENT;
            }
        }
    };
}