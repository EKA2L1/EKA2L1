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

#include <services/msv/cache.h>

namespace eka2l1::epoc::msv {
    entry_range_table::entry_range_table()
        : min_(0)
        , max_(0)
        , flags_(0) {
    }

    entry *entry_range_table::add(entry &ent, const bool extend_range) {
        if (get(ent.id_)) {
            return nullptr;
        }

        if (!extend_range && ((ent.id_ < min_) || (ent.id_ > max_))) {
            return nullptr;
        }

        entries_.push_back(ent);
        std::sort(entries_.begin(), entries_.end(), [](const entry &lhs, const entry &rhs) {
            return lhs.id_ < rhs.id_;
        });

        if (ent.id_ < min_) {
            min_ = ent.id_;
        }

        if (ent.id_ > max_) {
            max_ = ent.id_;
        }

        auto ite = std::lower_bound(entries_.begin(), entries_.end(), ent, [](const entry &lhs, const entry &rhs) {
            return lhs.id_ < rhs.id_;
        });

        return &(*ite);
    }

    bool entry_range_table::remove(const msv_id id) {
        auto ite = std::lower_bound(entries_.begin(), entries_.end(), id, [](const entry &lhs, const msv_id &rhs) {
            return lhs.id_ < rhs;
        });

        if ((ite == entries_.end()) || (ite->id_ != id)) {
            return false;
        }

        entries_.erase(ite);

        if (!entries_.empty()) {
            min_ = entries_.front().id_;
            max_ = entries_.back().id_;
        }

        return true;
    }

    bool entry_range_table::add_tons(std::vector<entry> &ents, const std::size_t start_index, const std::size_t end_index) {
        if (ents.empty() || (start_index > end_index) || (end_index >= ents.size())) {
            return true;
        }

        if ((ents.size() == 1) || (end_index - start_index == 0)) {
            return add(ents[0], true);
        }

        bool no_need_look_too_much = entries_.empty();

        if (!no_need_look_too_much) {
            for (std::size_t i = start_index; i <= end_index; i++) {
                if (!std::binary_search(entries_.begin(), entries_.end(), ents[i], [](const entry &lhs, const entry &rhs) {
                    return lhs.id_ < rhs.id_;
                })) {
                    // Continue with our lifes
                    entries_.push_back(ents[i]);
                }
            }
        } else {
            entries_.insert(entries_.begin(), ents.begin() + start_index, ents.begin() + end_index + 1);
        }
        
        std::sort(entries_.begin(), entries_.end(), [](const entry &lhs, const entry &rhs) {
            return lhs.id_ < rhs.id_;
        });

        min_ = entries_.front().id_;
        max_ = entries_.back().id_;

        return true;
    }

    entry *entry_range_table::get(const msv_id id) {
        if ((id < min_) || (id > max_)) {
            return nullptr;
        }

        auto ite = std::lower_bound(entries_.begin(), entries_.end(), id, [](const entry &ent, const msv_id target) {
            return ent.id_ < target;
        });

        if ((ite == entries_.end()) || (ite->id_ != id)) {
            return nullptr;
        }

        return &(*ite);
    }

    bool entry_range_table::update_child_id(const msv_id parent_id, const msv_id child_id, const bool is_add) {
        entry *parent_obj = get(parent_id);
        if (!parent_obj) {
            return false;
        }

        const bool see_me = std::binary_search(parent_obj->children_ids_.begin(), parent_obj->children_ids_.end(), child_id);
        if ((see_me && is_add) || (!see_me && !is_add)) {
            return false;
        }

        if (is_add) {
            parent_obj->children_ids_.push_back(child_id);

            std::sort(parent_obj->children_ids_.begin(), parent_obj->children_ids_.end());
        } else {
            auto ite = std::lower_bound(parent_obj->children_ids_.begin(), parent_obj->children_ids_.end(), child_id);
            if ((ite != parent_obj->children_ids_.end()) && (*ite == child_id)) {
                parent_obj->children_ids_.erase(ite);
            }
        }

        return true;
    }

    bool entry_range_table::splitable() const {
        return (entries_.size() >= CACHE_THRESHOLD);
    }

    const std::size_t entry_range_table::left_to_splittable() const {
        if (CACHE_THRESHOLD <= entries_.size()) {
            return 0;
        }

        return (CACHE_THRESHOLD - entries_.size());
    }

    bool entry_range_table::do_split(const msv_id parent_splitter) {
        // Take the middle entry 
        entry_range_table *new_table = new entry_range_table;
        const std::size_t middle_wards = entries_.size() / 2;

        grand_child_present(false);

        new_table->add_tons(entries_, middle_wards, entries_.size() - 1);
        new_table->grand_child_present(false);

        for (std::size_t i = 0; i < middle_wards; i++) {
            if (entries_[i].parent_id_ != parent_splitter) {
                grand_child_present(true);
            }
        }

        for (std::size_t i = middle_wards; i < entries_.size(); i++) {
            if (entries_[i].parent_id_ != parent_splitter) {
                new_table->grand_child_present(true);
            }
        }

        entries_.erase(entries_.begin() + middle_wards, entries_.end());
        new_table->folder_link_.insert_after(&folder_link_);

        return true;
    }

    visible_folder::visible_folder(const msv_id my_id)
        : flags_(0)
        , myid_(my_id) {
    }

    visible_folder::~visible_folder() {
        clear_tables();
    }

    void visible_folder::clear_tables() {
        while (!tables_.empty()) {
            entry_range_table *table = E_LOFF(tables_.first()->deque(), entry_range_table, folder_link_);
            delete table;
        }
    }
    
    entry *visible_folder::add(entry &ent) {
        if (tables_.empty()) {
            entry_range_table *table = new entry_range_table;
            
            table->min_range(ent.id_);
            table->max_range(ent.id_);

            entry *fin = table->add(ent);

            tables_.push(&table->folder_link_);

            if (ent.parent_id_ != myid_) {
                table->grand_child_present(true);
            }
            
            return fin;
        }

        common::double_linked_queue_element *first = tables_.first();
        common::double_linked_queue_element *end = tables_.end();

        bool is_first = true;
        bool grand_child_present = (ent.parent_id_ != myid_);

        do {
            entry_range_table *the_table = E_LOFF(first, entry_range_table, folder_link_);
            if (the_table) {
                if (entry *fin = the_table->add(ent, false)) {
                    if (grand_child_present) {
                        the_table->grand_child_present(true);
                    }

                    if (grand_child_present) {
                        update_child_id(ent.parent_id_, ent.id_, true);
                    }

                    if (the_table->splitable()) {
                        the_table->do_split(myid_);
                    }

                    return fin;
                }
            }

            // The table is sorted in number order, even when split. If an entry does not fall into the
            // first table range, we add it to the first table, same with last entry.
            if (ent.id_ < the_table->min_range()) {
                entry *fin = the_table->add(ent, true);
                if (!fin) {
                    return nullptr;
                }

                if (grand_child_present) {
                    the_table->grand_child_present(true);
                }

                if (grand_child_present) {
                    update_child_id(ent.parent_id_, ent.id_, true);
                }

                if (the_table->splitable()) {
                    the_table->do_split(myid_);
                }

                return fin;
            }

            if ((first->next == end) && (ent.id_ > the_table->max_range())) {
                entry *fin = the_table->add(ent, true);
                if (!fin) {
                    return nullptr;
                }

                if (grand_child_present) {
                    the_table->grand_child_present(true);
                }

                if (grand_child_present) {
                    update_child_id(ent.parent_id_, ent.id_, true);
                }

                if (the_table->splitable()) {
                    the_table->do_split(myid_);
                }

                return fin;
            }

            first = first->next;
        } while (first != end);

        // Unreachable
        return nullptr;
    }

    bool visible_folder::add_entry_list(std::vector<entry> &entries, const bool complete_child_of_folder) {
        if (entries.empty()) {
            if (complete_child_of_folder) {
                all_children_present(true);
            }

            return true;
        }

        auto make_tables_and_add = [&](const std::size_t starting_index, const std::size_t size) {
            for (std::size_t i = 0; i < (size + CACHE_THRESHOLD - 1) / CACHE_THRESHOLD; i++) {    
                entry_range_table *table = new entry_range_table;
                table->add_tons(entries, starting_index + i * CACHE_THRESHOLD, common::min<std::size_t>(entries.size() - 1, starting_index + (i + 1) * CACHE_THRESHOLD - 1));

                if (entries[i * CACHE_THRESHOLD].parent_id_ != myid_) {
                    table->grand_child_present(true);
                }

                tables_.push(&table->folder_link_);
            }
        };

        std::sort(entries.begin(), entries.end(), [](const entry &lhs, const entry &rhs) {
            return lhs.id_ < rhs.id_;
        });

        if (tables_.empty()) {
            // Add some new tables
            make_tables_and_add(0, entries.size());
            if (complete_child_of_folder) {
                all_children_present(true);
            }

            return true;
        }

        // Try to add to tables in range, if the table is splittable, we do splitting
        std::size_t skipped = 0;
        std::size_t last_skip = 0;

        common::double_linked_queue_element *first = tables_.first();
        common::double_linked_queue_element *end = tables_.end();

        do {
            entry_range_table *the_table = E_LOFF(first, entry_range_table, folder_link_);
            last_skip = skipped;

            bool grand_child_incoming = false;

            while (the_table && (entries[skipped].id_ < the_table->max_range())) {
                skipped++;

                if (entries[skipped].parent_id_ != myid_) {
                    grand_child_incoming = true;
                }
            }

            if (grand_child_incoming) {
                the_table->grand_child_present(true);
            }

            if (skipped)
                the_table->add_tons(entries, last_skip, skipped);

            if (the_table->splitable()) {
                the_table->do_split(myid_);
            }

            if (first->next == end) {
                if (skipped < entries.size()) {
                    // We spends some last effort trying to add it to the last table till it reachs the threshold
                    // Then we can create new table for it.
                    const std::size_t more_to = the_table->left_to_splittable();
                    const std::size_t to_take = common::min<std::size_t>(more_to, entries.size() - skipped);

                    if (to_take > 0) {
                        the_table->add_tons(entries, skipped, to_take);
                        skipped += to_take;
                    }

                    // Make new tables and add
                    if (skipped < entries.size()) {
                        make_tables_and_add(skipped, entries.size() - skipped);
                        break;
                    }
                }
            }

            first = first->next;
        } while (first != end);

        if (complete_child_of_folder) {
            all_children_present(true);
        }

        return true;
    }

    entry *visible_folder::get_entry(const msv_id id) {
        common::double_linked_queue_element *first = tables_.first();
        common::double_linked_queue_element *end = tables_.end();

        do {
            if (!first) {
                break;
            }

            entry_range_table *the_table = E_LOFF(first, entry_range_table, folder_link_);
            entry *result = the_table->get(id);

            if (result) {
                return result;
            }

            first = first->next;
        } while (first != end);

        return nullptr;
    }

    bool visible_folder::remove_entry(const msv_id id) {
        common::double_linked_queue_element *first = tables_.first();
        common::double_linked_queue_element *end = tables_.end();

        do {
            if (!first) {
                break;
            }

            entry_range_table *the_table = E_LOFF(first, entry_range_table, folder_link_);
            if (the_table->in_range(id) && the_table->remove(id)) {
                if (the_table->empty()) {
                    the_table->folder_link_.deque();
                    delete the_table;
                }

                return true;
            }

            first = first->next;
        } while (first != end);

        return false;
    }

    bool visible_folder::update_child_id(const msv_id parent_id, const msv_id child_id, const bool is_add) {
        common::double_linked_queue_element *first = tables_.first();
        common::double_linked_queue_element *end = tables_.end();

        do {
            entry_range_table *the_table = E_LOFF(first, entry_range_table, folder_link_);
            if (the_table->in_range(parent_id)) {
                return the_table->update_child_id(parent_id, child_id, is_add);
            }

            first = first->next;
        } while (first != end);

        return false;
    }

    std::vector<entry *> visible_folder::get_children_by_parent(const msv_id parent_id, visible_folder_children_query_error *error) {
        std::vector<entry*> ents;

        if (error) {
            *error = visible_folder_children_query_ok;
        }
        
        if (parent_id == myid_) {
            // With the creation of visible folder, there may be only some entries which got added to the cache.
            // That does not gurantee the visible folder is complete.
            if (!all_children_present()) {
                if (error) {
                    *error = visible_folder_children_incomplete;
                }

                return ents;
            }

            // Searching in the same folder, iterate all tables
            common::double_linked_queue_element *first = tables_.first();
            common::double_linked_queue_element *end = tables_.end();

            do {
                if (!first) {
                    break;
                }

                entry_range_table *the_table = E_LOFF(first, entry_range_table, folder_link_);
                if (the_table->grand_child_present()) {
                    // The table contains children that is not directly belongs to this folder
                    for (std::size_t i = 0; i < the_table->entries_.size(); i++) {
                        if (the_table->entries_[i].parent_id_ == myid_) {
                            ents.push_back(&the_table->entries_[i]);
                        }
                    }
                } else {
                    for (std::size_t i = 0; i < the_table->entries_.size(); i++) {
                        ents.push_back(&the_table->entries_[i]);
                    }
                }

                first = first->next;
            } while (first != end);

            return ents;
        }

        // In case not directly belongs
        common::double_linked_queue_element *first = tables_.first();
        common::double_linked_queue_element *end = tables_.end();

        do {
            entry_range_table *the_table = E_LOFF(first, entry_range_table, folder_link_);
            if (the_table->in_range(parent_id)) {
                entry *ent = the_table->get(parent_id);
                if (!ent || !ent->children_looked_up()) {
                    if (error) {
                        *error = visible_folder_children_query_no_child_id_array;
                    }

                    return ents;
                }

                for (std::size_t i = 0; i < ent->children_ids_.size(); i++) {
                    ents.push_back(get_entry(ent->children_ids_[i]));
                }
            }

            first = first->next;
        } while (first != end);

        return ents;
    }
}