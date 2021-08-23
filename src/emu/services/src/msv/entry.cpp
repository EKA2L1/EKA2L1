/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <common/chunkyseri.h>
#include <common/log.h>
#include <common/path.h>
#include <common/time.h>
#include <common/cvt.h>

#include <loader/rsc.h>
#include <services/msv/common.h>
#include <services/msv/entry.h>
#include <vfs/vfs.h>

#include <utils/bafl.h>
#include <utils/err.h>

#include <string>

namespace eka2l1::epoc::msv {
    entry_indexer::entry_indexer(io_system *io, const std::u16string &msg_folder, const language preferred_lang)
        : io_(io)
        , rom_drv_(drive_z)
        , preferred_lang_(preferred_lang)
        , msg_dir_(msg_folder) {
        drive_number drv_target = drive_z;

        for (drive_number drv = drive_z; drv >= drive_a; drv--) {
            auto drv_entry = io->get_drive_entry(drv);

            if (drv_entry && (drv_entry->media_type == drive_media::rom)) {
                rom_drv_ = drv;
                break;
            }
        }
        
        root_entry_.id_ = MSV_ROOT_ID_VALUE;
        root_entry_.mtm_uid_ = MTM_SERVICE_UID_ROOT;
        root_entry_.type_uid_ = MTM_SERVICE_UID_ROOT;
        root_entry_.parent_id_ = epoc::error_not_found;
        root_entry_.data_ = 0;
        root_entry_.time_ = common::get_current_utc_time_in_microseconds_since_0ad();
        root_entry_.visible_id_ = 0;

        root_entry_.visible_folder(true);
    }

    entry_indexer::~entry_indexer() {
        while (!folders_.empty()) {
            visible_folder *ff = E_LOFF(folders_.first()->deque(), visible_folder, indexer_link_);
            delete ff;
        }
    }

    static bool msv_entry_comparator(const entry &lhs, const entry &rhs) {
        return (lhs.id_ & 0x0FFFFFFF) < (rhs.id_ & 0x0FFFFFFF);
    }

    struct entry_index_info {
        std::uint32_t id_;
        std::uint32_t service_id_;
        std::uint32_t parent_id_;
        std::uint32_t flags_;
    };

    enum msv_folder_type {
        MSV_FOLDER_TYPE_NORMAL = 0,
        MSV_FOLDER_TYPE_PATH = 1 << 0,
        MSV_FOLDER_TYPE_SERVICE = 1 << 1
    };

    static std::u16string get_folder_name(const std::uint32_t id, const msv_folder_type type) {
        if ((id == 0) && (type == MSV_FOLDER_TYPE_SERVICE)) {
            return u"";
        }

        std::u16string in_hex = fmt::format(u"{:08X}", id);

        switch (type) {
        case MSV_FOLDER_TYPE_PATH:
            in_hex += u"_F";
            break;

        case MSV_FOLDER_TYPE_SERVICE:
            in_hex += u"_S";
            break;

        default:
            // Intentional
            break;
        }

        return in_hex;
    }

    std::optional<std::u16string> entry_indexer::get_entry_data_file(entry &ent) {
        std::u16string file_path = eka2l1::add_path(msg_dir_, get_folder_name(ent.service_id_, MSV_FOLDER_TYPE_SERVICE));
        if (!io_->exist(file_path)) {
            io_->create_directories(file_path);
        }

        return eka2l1::add_path(file_path, eka2l1::add_path(fmt::format(u"\\{:X}\\", ent.id_ & 0xF), get_folder_name(ent.id_, MSV_FOLDER_TYPE_NORMAL)));
    }

    bool entry_indexer::create_standard_entries(drive_number crr_drive) {
        std::u16string DEFAULT_STANDARD_ENTRIES_FILE = u"!:\\resource\\messaging\\msgs.rsc";
        DEFAULT_STANDARD_ENTRIES_FILE[0] = drive_to_char16(rom_drv_);

        std::u16string nearest_default_entries_file = utils::get_nearest_lang_file(io_,
            DEFAULT_STANDARD_ENTRIES_FILE, preferred_lang_, rom_drv_);

        if (nearest_default_entries_file.empty()) {
            DEFAULT_STANDARD_ENTRIES_FILE = u"!:\\system\\data\\msgs.rsc";
            DEFAULT_STANDARD_ENTRIES_FILE[0] = drive_to_char16(rom_drv_);

            nearest_default_entries_file = utils::get_nearest_lang_file(io_, DEFAULT_STANDARD_ENTRIES_FILE, preferred_lang_, rom_drv_);
        }

        if (nearest_default_entries_file.empty()) {
            LOG_ERROR(SERVICE_MSV, "Failed to get the path to standard entries info!");
            return false;
        }

        // Open resource loader and read this file
        symfile nearest_default_entries_file_io = io_->open_file(nearest_default_entries_file, READ_MODE | BIN_MODE);

        if (!nearest_default_entries_file_io) {
            LOG_ERROR(SERVICE_MSV, "Unable to create standard entries (default msgs file not found!)");
            return false;
        }

        ro_file_stream nearest_default_entries_file_stream(nearest_default_entries_file_io.get());
        loader::rsc_file nearest_default_entries_loader(reinterpret_cast<common::ro_stream *>(&nearest_default_entries_file_stream));

        auto entries_info_buf = nearest_default_entries_loader.read(1);

        if (entries_info_buf.size() < 2) {
            LOG_ERROR(SERVICE_MSV, "Default messages file is corrupted, unable to create standard msg entries!");
            return false;
        }

        common::chunkyseri seri(entries_info_buf.data(), entries_info_buf.size(), common::SERI_MODE_READ);
        std::uint16_t entry_count = 0;

        seri.absorb(entry_count);

        for (std::uint16_t i = 0; i < entry_count; i++) {
            epoc::msv::entry ent;

            seri.absorb(ent.id_);

            // Reserve 4 bits for drive ID
            ent.id_ &= 0x0FFFFFFF;

            seri.absorb(ent.parent_id_);
            seri.absorb(ent.service_id_);
            seri.absorb(ent.type_uid_);
            seri.absorb(ent.mtm_uid_);
            seri.absorb(ent.data_);

            loader::absorb_resource_string(seri, ent.description_);
            loader::absorb_resource_string(seri, ent.details_);

            ent.time_ = common::get_current_utc_time_in_microseconds_since_0ad();

            if (!add_entry(ent)) {
                return false;
            }
        }

        // Add standard SMS entry :)
        epoc::msv::entry sms_root_entry;
        sms_root_entry.mtm_uid_ = MSV_MSG_TYPE_UID;
        sms_root_entry.type_uid_ = MSV_SERVICE_UID;
        sms_root_entry.service_id_ = 0;
        sms_root_entry.id_ = 0;
        sms_root_entry.parent_id_ = MSV_ROOT_ID_VALUE;
        sms_root_entry.visible(false);
        sms_root_entry.time_ = common::get_current_utc_time_in_microseconds_since_0ad();

        if (!add_entry(sms_root_entry)) {
            return false;
        }

        common::double_linked_queue_element *first = folders_.first();
        common::double_linked_queue_element *end = folders_.end();

        // This is default occurence, set to all present
        do {
            if (!first) {
                break;
            }

            visible_folder *folder = E_LOFF(first, visible_folder, indexer_link_);
            folder->all_children_present(true);

            first = first->next;
        } while (first != end);

        return true;
    }
    
    entry *entry_indexer::add_entry(entry &ent) {
        // Provide that the proper visible folder has been found
        common::double_linked_queue_element *first = folders_.first();
        common::double_linked_queue_element *end = folders_.end();

        do {
            if (!first) {
                break;
            }

            visible_folder *folder = E_LOFF(first, visible_folder, indexer_link_);
            if (folder->id() == ent.visible_id_) {
                return folder->add(ent);
            }

            first = first->next;
        } while (first != end);

        if (ent.visible_id_ == static_cast<std::uint32_t>(epoc::error_not_found)) {
            return nullptr;
        }

        // It's a shame really. No folder for this? We shall creates one
        visible_folder *new_folder = new visible_folder(ent.visible_id_);
        entry *fin = new_folder->add(ent);

        folders_.push(&new_folder->indexer_link_);

        return fin;
    }
    
    entry *entry_indexer::get_entry(const std::uint32_t id) {
        if (id == MSV_ROOT_ID_VALUE) {
            return &root_entry_;
        }

        // WHERE IS IT!!!!! Get out aaaaaaaa
        common::double_linked_queue_element *first = folders_.first();
        common::double_linked_queue_element *end = folders_.end();

        do {
            if (!first) {
                break;
            }

            visible_folder *folder = E_LOFF(first, visible_folder, indexer_link_);
            if (auto result = folder->get_entry(id)) {
                return result;
            }

            first = first->next;
        } while (first != end);

        return nullptr;
    }
 
    bool entry_indexer::change_entry(entry &ent) {
        // WHERE IS IT!!!!! Get out aaaaaaaa
        common::double_linked_queue_element *first = folders_.first();
        common::double_linked_queue_element *end = folders_.end();

        entry decent_copy;
        bool need_copy = false;

        do {
            if (!first) {
                break;
            }

            visible_folder *folder = E_LOFF(first, visible_folder, indexer_link_);
            if (auto result = folder->get_entry(ent.id_)) {
                entry *to_cop = nullptr;

                if (result->visible_id_ == folder->id()) {
                    to_cop = result;
                } else {
                    // Delete this, make a copy and call add later
                    need_copy = true;
                    decent_copy = *result;
                    to_cop = &decent_copy;
                }

                if (to_cop) {
                    to_cop->parent_id_ = ent.visible_id_;
                    to_cop->service_id_ = ent.service_id_;
                    to_cop->related_id_ = ent.related_id_;
                    to_cop->pc_sync_count_ = ent.pc_sync_count_;
                    to_cop->type_uid_ = ent.type_uid_;
                    to_cop->visible_id_ = ent.visible_id_;
                    to_cop->mtm_uid_ = ent.mtm_uid_;
                    to_cop->data_ = ent.data_;
                    to_cop->description_ = ent.description_;
                    to_cop->details_ = ent.details_;
                    to_cop->time_ = ent.time_;
                    to_cop->size_ = ent.size_;
                    to_cop->error_ = ent.error_;
                    to_cop->bio_type_ = ent.bio_type_;
                    to_cop->reserved_ = ent.reserved_;
                    to_cop->mtm_datas_[0] = ent.mtm_datas_[0];
                    to_cop->mtm_datas_[1] = ent.mtm_datas_[1];
                    to_cop->mtm_datas_[2] = ent.mtm_datas_[2];

                    break;
                }
            }

            first = first->next;
        } while (first != end);

        if (need_copy) {
            return entry_indexer::add_entry(decent_copy);
        }

        return true;
    }

    sql_entry_indexer::sql_entry_indexer(io_system *io, const std::u16string &msg_folder, const language preferred_lang)
        : entry_indexer(io, msg_folder, preferred_lang)
        , database_(nullptr)
        , create_entry_stmt_(nullptr)
        , change_entry_stmt_(nullptr)
        , visible_folder_find_stmt_(nullptr)
        , find_entry_stmt_(nullptr)
        , query_child_entries_stmt_(nullptr)
        , id_counter_(MSV_FIRST_FREE_ENTRY_ID) {
        bool newly_created = false;

        if (load_or_create_databases(newly_created)) {
            if (newly_created) {
                add_entry(root_entry_);
                create_standard_entries(drive_c);
            }
        }
    }

    sql_entry_indexer::~sql_entry_indexer() {
        if (create_entry_stmt_) {
            sqlite3_finalize(create_entry_stmt_);
        }

        if (change_entry_stmt_) {
            sqlite3_finalize(change_entry_stmt_);
        }

        if (visible_folder_find_stmt_) {
            sqlite3_finalize(visible_folder_find_stmt_);
        }

        if (find_entry_stmt_) {
            sqlite3_finalize(find_entry_stmt_);
        }

        if (query_child_entries_stmt_) {
            sqlite3_finalize(query_child_entries_stmt_);
        }

        if (database_) {
            sqlite3_close(database_);
        }
    }

    static constexpr std::uint32_t MSV_SQL_DATABASE_VERSION = 2;
    
    bool sql_entry_indexer::load_or_create_databases(bool &newly_created) {
        const std::u16string root = eka2l1::root_name(msg_dir_, true);
        drive_number database_reside = drive_c;

        newly_created = false;

        if (root.length() > 1) {
            database_reside = char16_to_drive(root[0]);
        }

        std::u16string vert_path = fmt::format(u"{}:\\messaging.db", drive_to_char16(database_reside));
        std::optional<std::u16string> database_real_path = io_->get_raw_path(vert_path);

        if (!database_real_path) {
            LOG_ERROR(SERVICE_MSV, "Can't retrieve messaging database path!");
            return false;
        }

        if (!io_->exist(vert_path)) {
            newly_created = true;
        }

        std::string database_path_u8 = common::ucs2_to_utf8(database_real_path.value());
        if (sqlite3_open(database_path_u8.c_str(), &database_) != SQLITE_OK) {
            LOG_ERROR(SERVICE_MSV, "Fail to establish messaging database!");
            return false;
        }

        // Create some fundamental tables to store our msg entries
        const char *INDEX_ENTRY_TABLE_CREATE_STM = "CREATE TABLE IF NOT EXISTS IndexEntry("
            "id INTEGER,"
            "parentId INTEGER,"
            "serviceId INTEGER,"
            "mtmId INTEGER,"
            "type INTEGER,"
            "date INTEGER,"
            "data INTEGER,"
            "size INTEGER,"
            "error INTEGER,"
            "mtmData1 INTEGER,"
            "mtmData2 INTEGER,"
            "mtmData3 INTEGER,"
            "relatedId INTEGER,"
            "bioType INTEGER,"
            "pcSyncCount INTEGER,"
            "reserved INTEGER,"
            "visibleParent INTEGER,"
            "description TEXT,"
            "details TEXT,"
            "PRIMARY KEY (id)"
            ");";

        if (sqlite3_exec(database_, INDEX_ENTRY_TABLE_CREATE_STM, nullptr, nullptr, nullptr) != SQLITE_OK) {
            LOG_ERROR(SERVICE_MSV, "Fail to create index entry table!");
            return false;
        }

        // Follow Symbian footsteps, since there are opcodes which lurk up entries with specific parent id,
        // create an index for it
        const char *INDEX_ENTRY_CREATE_PARENT_ID_INDEX_STM = "CREATE INDEX IF NOT EXISTS IndexEntry_ParentIndex ON IndexEntry(parentId);";
        
        if (sqlite3_exec(database_, INDEX_ENTRY_CREATE_PARENT_ID_INDEX_STM, nullptr, nullptr, nullptr) != SQLITE_OK) {
            LOG_WARN(SERVICE_MSV, "Fail to create index entry's parent indexing!");
        }

        // Create version table
        const char *VERSION_TABLE_CREATE_STM = "CREATE TABLE IF NOT EXISTS VersionTable(version INTEGER PRIMARY KEY);";

        if (sqlite3_exec(database_, VERSION_TABLE_CREATE_STM, nullptr, nullptr, nullptr) != SQLITE_OK) {
            LOG_WARN(SERVICE_MSV, "Fail to create version table!");
        } else {
            // Add the versioning
            std::string VERSION_TABLE_ADD_VERSION_STM = fmt::format("INSERT INTO VersionTable Values ({});", MSV_SQL_DATABASE_VERSION);
            if (sqlite3_exec(database_, VERSION_TABLE_ADD_VERSION_STM.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
                LOG_WARN(SERVICE_MSV, "Can't add version number to version table!");
            }
        }

        // Get the ID counter starting value. This is ID of last entry added
        const char *MAX_ID_GET_STM = "SELECT MAX(id) FROM IndexEntry;";
        sqlite3_stmt *max_id_get_stmt_obj = nullptr;
        
        if (sqlite3_prepare(database_, MAX_ID_GET_STM, -1, &max_id_get_stmt_obj, nullptr) == SQLITE_OK) {
            const int res = sqlite3_step(max_id_get_stmt_obj);
            if ((res == SQLITE_ROW) || (res == SQLITE_DONE)) {
                // Well, we got one result, let's see it
                id_counter_ = common::max<std::uint32_t>(id_counter_, static_cast<std::uint32_t>(sqlite3_column_int(max_id_get_stmt_obj, 0)));
            }

            sqlite3_finalize(max_id_get_stmt_obj);
        } else {
            LOG_WARN(SERVICE_MSV, "Can't get max ID for ID counter!");
        }

        return true;
    }
    
    msv_id sql_entry_indexer::get_suitable_visible_parent_id(const msv_id parent_id) {
        // Try to find the parent entry in cache first
        entry *parent_ent = entry_indexer::get_entry(parent_id);
        if (parent_ent) {
            if (!parent_ent->visible_folder()) {
                return parent_ent->visible_id_;
            } else {
                return parent_id;
            }
        }

        // Conduct a search in the database
        if (!visible_folder_find_stmt_) {
            const char *FIND_SUITABLE_VISIBLE_PARENT_STMT = "SELECT data, visibleParent FROM IndexEntry WHERE id=:parentId";
            if (sqlite3_prepare(database_, FIND_SUITABLE_VISIBLE_PARENT_STMT, -1, &visible_folder_find_stmt_, nullptr) != SQLITE_OK) {
                LOG_ERROR(SERVICE_MSV, "Unable to prepare find visible folder statement!");
                return 0;
            }
        }

        sqlite3_reset(visible_folder_find_stmt_);

        if (sqlite3_bind_int(visible_folder_find_stmt_, 1, parent_id) != SQLITE_OK) {
            LOG_ERROR(SERVICE_MSV, "Fail to bind parent id to visible folder find statement");
            return 0;
        }

        // Only step one time, as only one entry should exists
        const int res = sqlite3_step(visible_folder_find_stmt_);
        msv_id result_id = 0;

        if ((res == SQLITE_ROW) || (res == SQLITE_DONE)) {
            const int data = sqlite3_column_int(visible_folder_find_stmt_, 0);
            const std::uint32_t visible_folder_id = static_cast<std::uint32_t>(sqlite3_column_int(visible_folder_find_stmt_, 1));

            if (data & entry::DATA_FLAG_INVISIBLE) {
                return visible_folder_id;
            }

            return parent_id;
        }

        return result_id;
    }

    bool sql_entry_indexer::add_or_change_entry(entry &ent, entry *&result, const bool is_add) {
        sqlite3_stmt *target_stmt = nullptr;

        if (is_add) {
            if (!create_entry_stmt_) {
                const char *CREATE_ENTRY_STMT_STRING = "INSERT INTO IndexEntry VALUES("
                    ":id, :parentId, :serviceId, :mtmId, :type, :date, :data, :size, :error, :mtmData1,"
                    ":mtmData2, :mtmData3, :relatedId, :bioType, :pcSyncCount, :reserved, :visibleParent,"
                    ":description, :details)";

                if (sqlite3_prepare(database_, CREATE_ENTRY_STMT_STRING, -1, &create_entry_stmt_, nullptr) != SQLITE_OK) {
                    LOG_ERROR(SERVICE_MSV, "Unable to prepare index entry insert statement!");
                    return false;
                }
            }
            
            target_stmt = create_entry_stmt_;
        }

        if (!is_add) {
            if (!change_entry_stmt_) {
                const char *MODIFY_ENTRY_STMT_STRING = "UPDATE IndexEntry SET "
                    "parentId=:parentId, serviceId=:serviceId, mtmId=:mtmId, type=:type, date=:date, data=:data,"
                    "size=:size, error=:error, mtmData1=:mtmData1, mtmData2=:mtmData2, mtmData3=:mtmData3, relatedId=:relatedId,"
                    "bioType=:bioType, pcSyncCount=:pcSyncCount, reserved=:reserved, visibleParent=:visibleParent, description=:description, details=:details "
                    "WHERE id=:id";

                if (sqlite3_prepare(database_, MODIFY_ENTRY_STMT_STRING, -1, &change_entry_stmt_, nullptr) != SQLITE_OK) {
                    LOG_ERROR(SERVICE_MSV, "Unable to prepare index entry modify statement!");
                    return false;
                }
            }

            target_stmt = change_entry_stmt_;
        }

        sqlite3_reset(target_stmt);

        // We want to first find a good visible folder for this entry
        const msv_id visible_folder_id = get_suitable_visible_parent_id(ent.parent_id_);
        if ((ent.type_uid_ != MTM_SERVICE_UID_ROOT) && (visible_folder_id == 0)) {
            return false;
        }

        // Get a new entry slot for us all
        if (is_add) {
            if (ent.id_ == 0) {
                ent.id_ = id_counter_ + 1;
            }
        }

        if (ent.service_id_ == 0) {
            if (ent.type_uid_ == MSV_SERVICE_UID) {
                ent.service_id_ = ent.id_;
            } else {
                ent.service_id_ = MSV_LOCAL_SERVICE_ID_VALUE;
            }
        }

        ent.visible_id_ = visible_folder_id;

        // Parent's visbility affects child visiblity
        // The fact that the visible folder ID retrieved not equals to parent ID, means that
        // the parent entry is not a visible folder. So the child can not be too
        if (ent.type_uid_ == MTM_SERVICE_UID_ROOT) {
            ent.visible_folder(true);
        } else {
            if (ent.parent_id_ == visible_folder_id) {
                if (ent.visible() && ((ent.type_uid_ == MSV_SERVICE_UID) || (ent.type_uid_ == MSV_FOLDER_UID))) {
                    ent.visible_folder(true);
                } else {
                    ent.visible_folder(false);
                }
            } else {
                ent.visible_folder(false);
            }
        }

        // Let's bind value to add in the database
        int index = 1;

        if (is_add) {
            sqlite3_bind_int(target_stmt, index++, static_cast<int>(ent.id_));
        }

        sqlite3_bind_int(target_stmt, index++, static_cast<int>(ent.parent_id_));
        sqlite3_bind_int(target_stmt, index++, static_cast<int>(ent.service_id_));
        sqlite3_bind_int(target_stmt, index++, static_cast<int>(ent.mtm_uid_));
        sqlite3_bind_int(target_stmt, index++, static_cast<int>(ent.type_uid_));
        sqlite3_bind_int64(target_stmt, index++, static_cast<sqlite_int64>(ent.time_));
        sqlite3_bind_int(target_stmt, index++, static_cast<int>(ent.data_));
        sqlite3_bind_int(target_stmt, index++, static_cast<int>(ent.size_));
        sqlite3_bind_int(target_stmt, index++, static_cast<int>(ent.error_));
        sqlite3_bind_int(target_stmt, index++, static_cast<int>(ent.mtm_datas_[0]));
        sqlite3_bind_int(target_stmt, index++, static_cast<int>(ent.mtm_datas_[1]));
        sqlite3_bind_int(target_stmt, index++, static_cast<int>(ent.mtm_datas_[2]));
        sqlite3_bind_int(target_stmt, index++, static_cast<int>(ent.related_id_));
        sqlite3_bind_int(target_stmt, index++, static_cast<int>(ent.bio_type_));
        sqlite3_bind_int(target_stmt, index++, static_cast<int>(ent.pc_sync_count_));
        sqlite3_bind_int(target_stmt, index++, static_cast<int>(ent.reserved_));
        sqlite3_bind_int(target_stmt, index++, static_cast<int>(ent.visible_id_));
        sqlite3_bind_text16(target_stmt, index++, ent.description_.c_str(), static_cast<int>(ent.description_.size() * sizeof(char16_t)), nullptr);
        sqlite3_bind_text16(target_stmt, index++, ent.details_.c_str(), static_cast<int>(ent.details_.size() * sizeof(char16_t)), nullptr);

        if (!is_add) {
            sqlite3_bind_int(target_stmt, index++, static_cast<int>(ent.id_));
        }

        const int err = sqlite3_step(target_stmt);

        if (err != SQLITE_DONE) {
            if (is_add) {
                LOG_ERROR(SERVICE_MSV, "Failed to add entry to database!");
            } else {
                LOG_ERROR(SERVICE_MSV, "Failed to apply entry change to database");
            }

            return false;
        }

        id_counter_++;

        if (is_add) {
            result = entry_indexer::add_entry(ent);
            return true;
        }

        return entry_indexer::change_entry(ent);
    }

    entry *sql_entry_indexer::add_entry(entry &ent) {
        entry *result = nullptr;

        if (!add_or_change_entry(ent, result, true)) {
            return nullptr;
        }

        return result;
    }

    bool sql_entry_indexer::change_entry(entry &ent) {
        entry *result = nullptr;
        return add_or_change_entry(ent, result, false);
    }

    void sql_entry_indexer::fill_entry_information(entry &the_entry, sqlite3_stmt *stmt, const bool have_extra_id) {
        the_entry.parent_id_ = static_cast<std::int32_t>(sqlite3_column_int(stmt, 0));
        the_entry.service_id_ = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 1));
        the_entry.mtm_uid_ = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 2));
        the_entry.type_uid_ = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 3));
        the_entry.time_ = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 4));
        the_entry.data_ = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 5));
        the_entry.size_ = static_cast<std::int32_t>(sqlite3_column_int(stmt, 6));
        the_entry.error_ = static_cast<std::int32_t>(sqlite3_column_int(stmt, 7));
        the_entry.mtm_datas_[0] = static_cast<std::int32_t>(sqlite3_column_int(stmt, 8));
        the_entry.mtm_datas_[1] = static_cast<std::int32_t>(sqlite3_column_int(stmt, 9));
        the_entry.mtm_datas_[2] = static_cast<std::int32_t>(sqlite3_column_int(stmt, 10));
        the_entry.related_id_ = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 11));
        the_entry.bio_type_ = static_cast<std::int32_t>(sqlite3_column_int(stmt, 12));
        the_entry.pc_sync_count_ = static_cast<std::int32_t>(sqlite3_column_int(stmt, 13));
        the_entry.reserved_ = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 14));
        the_entry.visible_id_ = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 15));
        the_entry.description_ = std::u16string(reinterpret_cast<const char16_t*>(sqlite3_column_text16(stmt, 16)));
        the_entry.details_ = std::u16string(reinterpret_cast<const char16_t*>(sqlite3_column_text16(stmt, 17)));

        if (have_extra_id) {
            the_entry.id_ = static_cast<std::uint32_t>(sqlite3_column_int(stmt, 18));
        }
    }

    entry *sql_entry_indexer::get_entry(const std::uint32_t id) {
        // Lookup in the cache first
        entry *result = entry_indexer::get_entry(id);
        if (!result) {
            // This time try to look in the database, see what can we gather
            if (!find_entry_stmt_) {
                static const char *FIND_ENTRY_STR_STM = "SELECT parentId, serviceId, mtmId, type, date, data, size, error, mtmData1,"
                    "mtmData2, mtmData3, relatedId, bioType, pcSyncCount, reserved, visibleParent,"
                    "description, details from IndexEntry WHERE id=:id";

                if (sqlite3_prepare(database_, FIND_ENTRY_STR_STM, -1, &find_entry_stmt_, nullptr) != SQLITE_OK) {
                    LOG_ERROR(SERVICE_MSV, "Unable to prepare find entry statement!");
                    return false;
                }
            }

            sqlite3_reset(find_entry_stmt_);
            if (sqlite3_bind_int(find_entry_stmt_, 1, id) != SQLITE_OK) {
                LOG_ERROR(SERVICE_MSV, "Unable to bind id to find entry in database!");
                return nullptr;
            }

            const int result = sqlite3_step(find_entry_stmt_);
            if (result == SQLITE_DONE) {
                // Nothing found
                return nullptr;
            }

            // There should be no possbility of duplicated entries. But in case, only take the first one
            entry the_entry;
            the_entry.id_ = id;
            
            fill_entry_information(the_entry, find_entry_stmt_);

            // Add to cache and receive the temporary pointer to it.
            return entry_indexer::add_entry(the_entry);
        }

        return result;
    }

    bool sql_entry_indexer::collect_children_entries(const msv_id parent_id, std::vector<entry> &entries) {
        if (!query_child_entries_stmt_) {
            static const char *QUERY_CHILD_ENTRIES_STM_STR = "SELECT parentId, serviceId, mtmId, type, date, data, size, error, mtmData1,"
                    "mtmData2, mtmData3, relatedId, bioType, pcSyncCount, reserved, visibleParent,"
                    "description, details, id from IndexEntry WHERE parentId=:parent_id";

            if (sqlite3_prepare(database_, QUERY_CHILD_ENTRIES_STM_STR, -1, &query_child_entries_stmt_, nullptr) != SQLITE_OK) {
                LOG_ERROR(SERVICE_MSV, "Can't prepare collect children IDs statement!");
                return false;
            }
        }

        sqlite3_reset(query_child_entries_stmt_);
        if (sqlite3_bind_int(query_child_entries_stmt_, 1, parent_id) != SQLITE_OK) {
            LOG_ERROR(SERVICE_MSV, "Can't bind parent id to query children ids statement!");
            return false;
        }

        do {
            int result = sqlite3_step(query_child_entries_stmt_);
            if (result == SQLITE_DONE) {
                return true;
            }

            if (result != SQLITE_ROW) {
                break;
            }

            if (sqlite3_column_count(query_child_entries_stmt_) != 19) {
                LOG_ERROR(SERVICE_MSV, "Query children entries statement is corrupted!");
                break;
            }

            entry an_entry;
            fill_entry_information(an_entry, query_child_entries_stmt_, true);

            entries.push_back(std::move(an_entry));
        } while (true);

        return false;
    }

    std::vector<entry *> sql_entry_indexer::get_entries_by_parent(const std::uint32_t parent_id) {
        visible_folder_children_query_error error = visible_folder_children_query_ok;
        std::vector<entry*> entries;

        // Find the visible folder that parent stays in, if not complete, do db queries...
        msv_id visible_folder_id = get_suitable_visible_parent_id(parent_id);
        
        common::double_linked_queue_element *first = folders_.first();
        common::double_linked_queue_element *end = folders_.end();

        auto gather_visible_folder_entries = [&](visible_folder *ff) -> bool {
            std::vector<entry> queries;
            if (!collect_children_entries(visible_folder_id, queries)) {
                LOG_ERROR(SERVICE_MSV, "Unable to query visible folder children entries from database!");
                return false;
            }

            // Add them to the folder
            ff->add_entry_list(queries, true);
            return true;
        };

        auto gather_target_parent_entries = [&](visible_folder *ff) -> bool {
            // Need to do another query
            std::vector<entry> queries;
            if (!collect_children_entries(parent_id, queries)) {
                LOG_ERROR(SERVICE_MSV, "Unable to query visible folder children entries from database!");
                return false;
            }

            entry *parent_entry = ff->get_entry(parent_id);
            if (!parent_entry) {
                LOG_ERROR(SERVICE_MSV, "Parent entry still doesn't exist!");
                return false;
            }

            for (std::size_t i = 0; i < queries.size(); i++) {
                parent_entry->children_ids_.push_back(queries[i].id_);
            }

            parent_entry->children_looked_up(true);
            ff->add_entry_list(queries);

            return true;
        };

        do {
            if (!first) {
                break;
            }

            visible_folder *ff = E_LOFF(first, visible_folder, indexer_link_);
            if (ff->id() == visible_folder_id) {
                entries = ff->get_children_by_parent(parent_id, &error);
                if (error == visible_folder_children_incomplete) {
                    // Query all entries and then do transformation
                    gather_visible_folder_entries(ff);
                }

                if ((parent_id != visible_folder_id) && (error != visible_folder_children_query_ok)) {
                    gather_target_parent_entries(ff);
                }

                // Reattempt this time again
                if (error != visible_folder_children_query_ok) {
                    entries = ff->get_children_by_parent(parent_id, &error);

                    if (error != visible_folder_children_query_ok) {
                        LOG_ERROR(SERVICE_MSV, "An error occured that made it unable to retrieve children entries");
                    }
                }

                return entries;
            }

            first = first->next;
        } while (first != end);

        // Create a new folder
        visible_folder *new_folder = new visible_folder(visible_folder_id);
        gather_visible_folder_entries(new_folder);

        if (parent_id != visible_folder_id) {
            gather_target_parent_entries(new_folder);
        }

        folders_.push(&new_folder->indexer_link_);
        entries = new_folder->get_children_by_parent(parent_id, &error);

        if (error != visible_folder_children_query_ok) {
            LOG_ERROR(SERVICE_MSV, "An error occured that made it unable to retrieve children entries");
        }

        return entries;
    }
}