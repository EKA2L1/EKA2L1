/*
 * Copyright (c) 2022 EKA2L1 Team.
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

#include <j2me/applist.h>
#include <j2me/common.h>

#include <sqlite3.h>

#include <common/fileutils.h>
#include <common/log.h>
#include <common/path.h>
#include <config/config.h>

namespace eka2l1::j2me {
    app_list::app_list(const config::state &conf)
        : database_(nullptr)
        , find_existing_stmt_(nullptr)
        , insert_entry_stmt_(nullptr)
        , update_name_stmt_(nullptr)
        , update_entry_stmt_(nullptr)
        , delete_entry_stmt_(nullptr)
        , version_query_stmt_(nullptr)
        , individual_select_stmt_(nullptr)
        , conf_(conf) {
        create_and_initialize_database(conf);
    }

    void app_list::reset() {
        if (find_existing_stmt_) {
            sqlite3_finalize(find_existing_stmt_);
            find_existing_stmt_ = nullptr;
        }
        if (insert_entry_stmt_) {
            sqlite3_finalize(insert_entry_stmt_);
            insert_entry_stmt_ = nullptr;
        }
        if (update_name_stmt_) {
            sqlite3_finalize(update_name_stmt_);
            update_name_stmt_ = nullptr;
        }
        if (update_entry_stmt_) {
            sqlite3_finalize(update_entry_stmt_);
            update_entry_stmt_ = nullptr;
        }
        if (delete_entry_stmt_) {
            sqlite3_finalize(delete_entry_stmt_);
            delete_entry_stmt_ = nullptr;
        }
        if (version_query_stmt_) {
            sqlite3_finalize(version_query_stmt_);
            version_query_stmt_ = nullptr;
        }
        if (individual_select_stmt_) {
            sqlite3_finalize(individual_select_stmt_);
            individual_select_stmt_ = nullptr;
        }
        sqlite3_close(database_);
    }

    app_list::~app_list() {
        reset();
    }

    void app_list::create_and_initialize_database(const config::state &conf) {
        common::create_directories(eka2l1::add_path(conf.storage, APPLIST_DB_DIR));
        const std::string path_to_db = eka2l1::add_path(conf.storage, APPLIST_DB_NAME);
        const int result = sqlite3_open(path_to_db.c_str(), &database_);
        if (result != SQLITE_OK) {
            LOG_ERROR(J2ME, "Failed to open new J2ME app database connection!");
            return;
        }

        static const char *CREATE_APP_LIST_TABLE = "CREATE TABLE IF NOT EXISTS apps("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "name TEXT NOT NULL,"
            "author TEXT NOT NULL,"
            "iconPath TEXT,"
            "title TEXT,"
            "originalTitle TEXT,"
            "version TEXT NOT NULL)";

        if (sqlite3_exec(database_, CREATE_APP_LIST_TABLE, nullptr, nullptr, nullptr) != SQLITE_OK) {
            LOG_ERROR(J2ME, "Failed to create app list table in database!");
            return;
        }

        static const char *CREATE_VERSION_TABLE = "CREATE TABLE IF NOT EXISTS version("
            "version INTEGER PRIMARY_KEY)";

        if (sqlite3_exec(database_, CREATE_VERSION_TABLE, nullptr, nullptr, nullptr) != SQLITE_OK) {
            LOG_WARN(J2ME, "Failed to create version table in the database");
        }
    }

    bool app_list::execute_insert(const app_entry &entry) {
        if (!insert_entry_stmt_) {
            static const char *INSERT_APP_ENTRY_STMT = "INSERT INTO apps (name, author, iconPath, title, originalTitle, version)"
                "VALUES (:name, :author, :iconPath, :title, :originalTitle, :version)";

            if (sqlite3_prepare(database_, INSERT_APP_ENTRY_STMT, -1, &insert_entry_stmt_, nullptr) != SQLITE_OK) {
                LOG_ERROR(J2ME, "Failed to prepare insert app statement");
                return false;
            }
        }

        sqlite3_reset(insert_entry_stmt_);
        sqlite3_bind_text(insert_entry_stmt_, 1, entry.name_.c_str(), static_cast<int>(entry.name_.length()), nullptr);
        sqlite3_bind_text(insert_entry_stmt_, 2, entry.author_.c_str(), static_cast<int>(entry.author_.length()), nullptr);
        sqlite3_bind_text(insert_entry_stmt_, 3, entry.icon_path_.c_str(), static_cast<int>(entry.icon_path_.length()), nullptr);
        sqlite3_bind_text(insert_entry_stmt_, 4, entry.title_.c_str(), static_cast<int>(entry.title_.length()), nullptr);
        sqlite3_bind_text(insert_entry_stmt_, 5, entry.original_title_.c_str(), static_cast<int>(entry.original_title_.length()), nullptr);
        sqlite3_bind_text(insert_entry_stmt_, 6, entry.version_.c_str(), static_cast<int>(entry.version_.length()), nullptr);

        if (sqlite3_step(insert_entry_stmt_) != SQLITE_DONE) {
            LOG_ERROR(J2ME, "Failed to insert new app entry to database");
            return false;
        }

        return true;
    }

    bool app_list::update_name(const std::uint32_t id, const std::string &new_name) {
        if (!update_name_stmt_) {
            static const char *UPDATE_NAME_STMT = "UPDATE apps SET title=:title WHERE id=:id";

            if (sqlite3_prepare(database_, UPDATE_NAME_STMT, -1, &update_name_stmt_, nullptr) != SQLITE_OK) {
                LOG_ERROR(J2ME, "Failed to prepare update name statement! Error: {}", sqlite3_errmsg(database_));
                return false;
            }
        }

        sqlite3_reset(update_name_stmt_);
        sqlite3_bind_text(update_name_stmt_, 1, new_name.c_str(), static_cast<int>(new_name.length()), nullptr);
        sqlite3_bind_int(update_name_stmt_, 2, static_cast<int>(id));

        if (sqlite3_step(update_name_stmt_) != SQLITE_DONE) {
            LOG_ERROR(J2ME, "Failed to update existing app entry!");
            return false;
        }

        return true;
    }

    std::uint32_t app_list::update_entry(const app_entry &entry) {
        if (!update_entry_stmt_) {
            static const char *UPDATE_ENTRY_STMT = "UPDATE apps SET "
                "name=:name,"
                "author=:author,"
                "iconPath=iconPath,"
                "title=:title,"
                "originalTitle=:originalTitle,"
                "version=:version WHERE id=:id";

            if (sqlite3_prepare(database_, UPDATE_ENTRY_STMT, -1, &update_entry_stmt_, nullptr) != SQLITE_OK) {
                LOG_ERROR(J2ME, "Failed to prepare update entry statement! Error: {}", sqlite3_errmsg(database_));
                return false;
            }
        }

        sqlite3_reset(update_entry_stmt_);
        sqlite3_bind_text(update_entry_stmt_, 1, entry.name_.c_str(), static_cast<int>(entry.name_.length()), nullptr);
        sqlite3_bind_text(update_entry_stmt_, 2, entry.author_.c_str(), static_cast<int>(entry.author_.length()), nullptr);
        sqlite3_bind_text(update_entry_stmt_, 3, entry.icon_path_.c_str(), static_cast<int>(entry.icon_path_.length()), nullptr);
        sqlite3_bind_text(update_entry_stmt_, 4, entry.title_.c_str(), static_cast<int>(entry.title_.length()), nullptr);
        sqlite3_bind_text(update_entry_stmt_, 5, entry.original_title_.c_str(), static_cast<int>(entry.original_title_.length()), nullptr);
        sqlite3_bind_text(update_entry_stmt_, 6, entry.version_.c_str(), static_cast<int>(entry.version_.length()), nullptr);
        sqlite3_bind_int(update_entry_stmt_, 7, static_cast<int>(entry.id_));

        if (sqlite3_step(update_entry_stmt_) != SQLITE_DONE) {
            LOG_ERROR(J2ME, "Failed to update new app entry to database");
            return static_cast<std::uint32_t>(-1);
        }

        return entry.id_;
    }

    std::uint32_t app_list::add_entry(const app_entry &entry, const bool replace_existing) {
        static const char *FIND_EXISTING_STMT = "SELECT id FROM apps WHERE (name=:name) AND (author=:author) AND (version=:version) COLLATE NOCASE";
        if (!find_existing_stmt_) {
            if (sqlite3_prepare(database_, FIND_EXISTING_STMT, -1, &find_existing_stmt_, nullptr) != SQLITE_OK) {
                LOG_ERROR(J2ME, "Failed to prepare find existing app entry statement!");
                return static_cast<std::uint32_t>(-1);
            }
        }

        sqlite3_reset(find_existing_stmt_);
        sqlite3_bind_text(find_existing_stmt_, 1, entry.name_.c_str(), static_cast<int>(entry.name_.length()), nullptr);
        sqlite3_bind_text(find_existing_stmt_, 2, entry.author_.c_str(), static_cast<int>(entry.author_.length()), nullptr);
        sqlite3_bind_text(find_existing_stmt_, 3, entry.version_.c_str(), static_cast<int>(entry.version_.length()), nullptr);

        const int res = sqlite3_step(find_existing_stmt_);
        if (res == SQLITE_DONE) {
            // No entry, just add it
            if (!execute_insert(entry)) {
                return false;
            }

            return true;
        }

        if (!replace_existing) {
            return false;
        }

        int id = sqlite3_column_int(find_existing_stmt_, 0);
        app_entry entry_copy = entry;
        entry_copy.id_ = static_cast<std::uint32_t>(id);

        return update_entry(entry_copy);
    }

    void app_list::add_entries(const std::vector<app_entry> &entries) {
        for (std::size_t i = 0; i < entries.size(); i++) {
            add_entry(entries[i], true);
        }
    }

    bool app_list::remove_entry(const std::uint32_t id) {
        if (!delete_entry_stmt_) {
            static const char *DELETE_ENTRY_STMT = "DELETE FROM apps WHERE id=:id";
            if (sqlite3_prepare(database_, DELETE_ENTRY_STMT, -1, &delete_entry_stmt_, nullptr) != SQLITE_OK) {
                LOG_ERROR(J2ME, "Failed to prepare delete entry statement!");
                return false;
            }
        }

        sqlite3_reset(delete_entry_stmt_);
        sqlite3_bind_int(delete_entry_stmt_, 1, static_cast<int>(id));

        if (sqlite3_step(delete_entry_stmt_) != SQLITE_DONE) {
            return false;
        }

        return true;
    }

    std::uint32_t app_list::get_launching_version() {
        if (!version_query_stmt_) {
            static const char *SELECT_VERSION_STMT = "SELECT version FROM version";
            if (sqlite3_prepare(database_, SELECT_VERSION_STMT, -1, &version_query_stmt_, nullptr) != SQLITE_OK) {
                LOG_ERROR(J2ME, "Failed to prepare get launching version statement");
                return eka2l1::j2me::CURRENT_VERSION;
            }
        }

        sqlite3_reset(version_query_stmt_);
        if (sqlite3_step(version_query_stmt_) == SQLITE_DONE) {
            const std::string version_add = fmt::format("INSERT INTO version (version) VALUES ({})", eka2l1::j2me::CURRENT_VERSION);
            if (sqlite3_exec(database_, version_add.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
                LOG_ERROR(J2ME, "Failed to add current version into database");
                return eka2l1::j2me::CURRENT_VERSION;
            }
        }
        return static_cast<std::uint32_t>(sqlite3_column_int(version_query_stmt_, 1));
    }

    void app_list::update_launching_version() {
        const std::string update_str = fmt::format("UPDATE version SET version={}", eka2l1::j2me::CURRENT_VERSION);
        sqlite3_exec(database_, update_str.c_str(), nullptr, nullptr, nullptr);
    }

    static void parse_app_entry_from_stmt_res(sqlite3_stmt *stmt, app_entry &entry) {
        entry.id_ = sqlite3_column_int(stmt, 0);
        entry.name_ = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        entry.author_ = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        entry.icon_path_ =  reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        entry.title_ = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        entry.original_title_ = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        entry.version_ =  reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
    }

    std::vector<app_entry> app_list::get_entries() {
        sqlite3_stmt *all_statement = nullptr;
        if (sqlite3_prepare(database_, "SELECT * from apps", -1, &all_statement, nullptr) != SQLITE_OK) {
            LOG_ERROR(J2ME, "Fail to prepare get entries statement!");
            return {};
        }

        std::vector<app_entry> entries;
        app_entry entry;

        sqlite3_reset(all_statement);
        do {
            int result = sqlite3_step(all_statement);
            if (result == SQLITE_DONE) {
                break;
            } else if (result != SQLITE_ROW) {
                LOG_ERROR(J2ME, "Error while iterating app entry {}", result);
                sqlite3_finalize(all_statement);

                return entries;
            }

            parse_app_entry_from_stmt_res(all_statement, entry);
            entries.push_back(std::move(entry));
        } while (true);

        sqlite3_finalize(all_statement);
        return entries;
    }

    std::optional<app_entry> app_list::get_entry(const std::uint32_t id) {
        if (!individual_select_stmt_) {
            static const char *SELECT_SINGLE_STMT = "SELECT * from apps WHERE id=:id";
            if (sqlite3_prepare(database_, SELECT_SINGLE_STMT, -1, &individual_select_stmt_, nullptr) != SQLITE_OK) {
                LOG_ERROR(J2ME, "Error preparing get entry statement for J2ME apps database!");
                return std::nullopt;
            }
        }

        sqlite3_reset(individual_select_stmt_);
        sqlite3_bind_int(individual_select_stmt_, 1, static_cast<int>(id));

        if (sqlite3_step(individual_select_stmt_) != SQLITE_ROW) {
            return std::nullopt;
        }

        app_entry result;
        parse_app_entry_from_stmt_res(individual_select_stmt_, result);

        return result;
    }

    void app_list::flush() {
        reset();
        
        common::create_directories(eka2l1::add_path(conf_.storage, APPLIST_DB_DIR));
        const std::string path_to_db = eka2l1::add_path(conf_.storage, APPLIST_DB_NAME);
        const int result = sqlite3_open(path_to_db.c_str(), &database_);

        if (result != SQLITE_OK) {
            LOG_ERROR(J2ME, "Failed to open new J2ME app database connection after flushing!");
        }
    }
}