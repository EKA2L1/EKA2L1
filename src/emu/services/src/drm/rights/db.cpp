/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#include <services/drm/rights/db.h>

#include <common/crypt.h>
#include <common/log.h>
#include <common/algorithm.h>

namespace eka2l1::epoc::drm {
    rights_database::rights_database(const std::u16string &database_path)
        : database_(nullptr)
        , get_cid_exist_stmt_(nullptr)
        , delete_record_stmt_(nullptr)
        , add_right_info_stmt_(nullptr)
        , add_perm_info_stmt_(nullptr)
        , add_constraint_info_stmt_(nullptr)
        , set_perm_constraints_stmt_(nullptr)
        , query_perms_stmt_(nullptr)
        , query_constraint_stmt_(nullptr)
        , get_encrypt_key_stmt_(nullptr)
        , get_cid_exist_full_stmt_(nullptr)
        , db_path_(database_path) {
        int result = sqlite3_open16(database_path.data(), &database_);
        if (result != SQLITE_OK) {
            LOG_ERROR(SERVICE_DRMSYS, "Fail to open DRM database!");
            return;
        }

        result = sqlite3_exec(database_, "PRAGMA foreign_keys = ON", nullptr, nullptr, nullptr);
        if (result != SQLITE_OK) {
            LOG_WARN(SERVICE_DRMSYS, "Foreign keys can not be enabled on DRM database!");
        }

        static const char *CREATE_RIGHT_INFO_TABLE = "CREATE TABLE IF NOT EXISTS RightInfo("
            "id INTEGER PRIMARY KEY,"
            "contentId STRING,"
            "contentHash STRING,"
            "issuer STRING,"
            "contentName STRING,"
            "authSeed STRING,"
            "encryptKey STRING"
            ")";

        result = sqlite3_exec(database_, CREATE_RIGHT_INFO_TABLE, nullptr, nullptr, nullptr);

        if (result != SQLITE_OK) {
            LOG_ERROR(SERVICE_DRMSYS, "Fail to create right info table. Last error: {}!", sqlite3_errmsg(database_));
            return;
        }

        static const char *CREATE_PERMISSION_INFO_TABLE = "CREATE TABLE IF NOT EXISTS PermissionInfo("
            "id INTEGER PRIMARY KEY,"
            "rightsId INTEGER,"
            "insertTime INTEGER,"
            "version INTEGER,"
            "topLevelConstraintId INTEGER,"
            "playConstraintId INTEGER,"
            "displayConstraintId INTEGER,"
            "executeConstraintId INTEGER,"
            "printConstraintId INTEGER,"
            "exportConstraintId INTEGER,"
            "exportMode INTEGER,"
            "infoBits INTEGER,"
            "parentRightsCid STRING,"
            "myRightsCid STRING,"
            "domainCid STRING,"
            "rightIssuerIdentifier STRING,"
            "onExpiredUrl STRING,"
            "FOREIGN KEY (topLevelConstraintId) REFERENCES ConstraintInfo(id),"
            "FOREIGN KEY (playConstraintId) REFERENCES ConstraintInfo(id),"
            "FOREIGN KEY (displayConstraintId) REFERENCES ConstraintInfo(id),"
            "FOREIGN KEY (executeConstraintId) REFERENCES ConstraintInfo(id),"
            "FOREIGN KEY (printConstraintId) REFERENCES ConstraintInfo(id),"
            "FOREIGN KEY (exportConstraintId) REFERENCES ConstraintInfo(id),"
            "CONSTRAINT fk_rightsId FOREIGN KEY (rightsId) REFERENCES RightInfo(id) ON DELETE CASCADE"
            ")";

        result = sqlite3_exec(database_, CREATE_PERMISSION_INFO_TABLE, nullptr, nullptr, nullptr);

        if (result != SQLITE_OK) {
            LOG_ERROR(SERVICE_DRMSYS, "Fail to create permission info table!");
            return;
        }

        static const char *CREATE_INDEX_PERM_TABLE_STMT_STRING = "CREATE INDEX IF NOT EXISTS PermissionInfo_RightsId ON PermissionInfo(rightsId)";
        result = sqlite3_exec(database_, CREATE_INDEX_PERM_TABLE_STMT_STRING, nullptr, nullptr, nullptr);

        if (result != SQLITE_OK) {
            LOG_WARN(SERVICE_DRMSYS, "Fail to create indexing table for permission info!");
        }

        static const char *CREATE_CONSTRAINT_INFO_TABLE = "CREATE TABLE IF NOT EXISTS ConstraintInfo("
            "id INTEGER PRIMARY KEY,"
            "permissionId INTEGER,"
            "startTime INTEGER,"
            "endTime INTEGER,"
            "intervalStart INTEGER,"
            "interval INTEGER,"
            "counter INTEGER,"
            "originalCounter INTEGER,"
            "timedCounter INTEGER,"
            "timedInterval INTEGER,"
            "accumulatedTime INTEGER,"
            "individual STRING,"
            "system STRING,"
            "vendorId INTEGER,"
            "secureId INTEGER,"
            "constraintFlags INTEGER,"
            "meteringGraceTime INTEGER,"
            "originalTimedCounter INTEGER,"
            "CONSTRAINT fk_permId FOREIGN KEY (permissionId) REFERENCES PermissionInfo(id) ON DELETE CASCADE"
            ")";

        result = sqlite3_exec(database_, CREATE_CONSTRAINT_INFO_TABLE, nullptr, nullptr, nullptr);

        if (result != SQLITE_OK) {
            LOG_ERROR(SERVICE_DRMSYS, "Fail to create constraint info table!");
            return;
        }

        static const char *CREATE_INDEX_CONSTRAINT_TABLE_STMT_STRING = "CREATE INDEX IF NOT EXISTS ConstraintInfo_PermId ON ConstraintInfo(permissionId)";
        result = sqlite3_exec(database_, CREATE_INDEX_CONSTRAINT_TABLE_STMT_STRING, nullptr, nullptr, nullptr);

        if (result != SQLITE_OK) {
            LOG_WARN(SERVICE_DRMSYS, "Fail to create indexing table for constraint info!");
        }
    }

    rights_database::~rights_database() {
        reset();
    }

    void rights_database::reset() {
        if (get_cid_exist_stmt_) {
            sqlite3_finalize(get_cid_exist_stmt_);
            get_cid_exist_stmt_ = nullptr;
        }

        if (delete_record_stmt_) {
            sqlite3_finalize(delete_record_stmt_);
            delete_record_stmt_ = nullptr;
        }

        if (add_right_info_stmt_) {
            sqlite3_finalize(add_right_info_stmt_);
            add_right_info_stmt_ = nullptr;
        }

        if (add_perm_info_stmt_) {
            sqlite3_finalize(add_perm_info_stmt_);
            add_perm_info_stmt_ = nullptr;
        }

        if (add_constraint_info_stmt_) {
            sqlite3_finalize(add_constraint_info_stmt_);
            add_constraint_info_stmt_ = nullptr;
        }

        if (set_perm_constraints_stmt_) {
            sqlite3_finalize(set_perm_constraints_stmt_);
            set_perm_constraints_stmt_ = nullptr;
        }

        if (query_perms_stmt_) {
            sqlite3_finalize(query_perms_stmt_);
            query_perms_stmt_ = nullptr;
        }

        if (query_constraint_stmt_) {
            sqlite3_finalize(query_constraint_stmt_);
            query_constraint_stmt_ = nullptr;
        }

        if (get_encrypt_key_stmt_) {
            sqlite3_finalize(get_encrypt_key_stmt_);
            get_encrypt_key_stmt_ = nullptr;
        }

        if (get_cid_exist_full_stmt_) {
            sqlite3_finalize(get_cid_exist_full_stmt_);
            get_cid_exist_full_stmt_ = nullptr;
        }

        if (database_) {
            sqlite3_close(database_);
            database_ = nullptr;
        }
    }

    void rights_database::flush() {
        reset();

        // Reopen the database. Previous reset will save the database to disk
        int result = sqlite3_open16(db_path_.c_str(), &database_);
        if (result != SQLITE_OK) {
            LOG_ERROR(SERVICE_DRMSYS, "Fail to reopen DRM database!");
            return;
        }
    }

    int rights_database::add_constraint(const rights_constraint &constraint, const int perm_id) {
        // Try add constraint now
        if (!add_constraint_info_stmt_) {
            static const char *ADD_CONSTRAINT_STMT_STRING = "INSERT INTO ConstraintInfo (permissionId, startTime, endTime, intervalStart, interval, counter,"
                "originalCounter, timedCounter, timedInterval, accumulatedTime, individual, system, vendorId, secureId, constraintFlags, meteringGraceTime, originalTimedCounter)"
                "VALUES (:targetPermissionId, :targetStartTime, :targetEndTime, :targetIntervalStart, :targetInterval, :targetCounter, :targetOriginalCounter, :targetTimedCounter,"
                ":targetTimedInterval, :targetAccumulatedTime, :targetIndividual, :targetSystem, :targetVendorId, :targetSecureId, :targetConstraintFlags, :targetMeteringGraceTime,"
                ":targetOriginalTimedCounter) RETURNING id";

            if (sqlite3_prepare(database_, ADD_CONSTRAINT_STMT_STRING, -1, &add_constraint_info_stmt_, nullptr) != 0) {
                LOG_WARN(SERVICE_DRMSYS, "Fail to prepare add constraint statement! Error message: {}", sqlite3_errmsg(database_));
                return -1;
            }
        }

        sqlite3_reset(add_constraint_info_stmt_);
        sqlite3_bind_int(add_constraint_info_stmt_, 1, perm_id);
        sqlite3_bind_int64(add_constraint_info_stmt_, 2, static_cast<std::int64_t>(constraint.start_time_));
        sqlite3_bind_int64(add_constraint_info_stmt_, 3, static_cast<std::int64_t>(constraint.end_time_));
        sqlite3_bind_int64(add_constraint_info_stmt_, 4, static_cast<std::int64_t>(constraint.interval_start_));
        sqlite3_bind_int(add_constraint_info_stmt_, 5, static_cast<int>(constraint.interval_));
        sqlite3_bind_int(add_constraint_info_stmt_, 6, static_cast<int>(constraint.counter_));
        sqlite3_bind_int(add_constraint_info_stmt_, 7, static_cast<int>(constraint.original_counter_));
        sqlite3_bind_int(add_constraint_info_stmt_, 8, static_cast<int>(constraint.timed_counter_));
        sqlite3_bind_int(add_constraint_info_stmt_, 9, static_cast<int>(constraint.timed_interval_));
        sqlite3_bind_int(add_constraint_info_stmt_, 10, static_cast<int>(constraint.accumulated_time_));
        
        // Again these buffer should be small enough!
        std::string encoded_individual = crypt::base64_encode(reinterpret_cast<const std::uint8_t*>(constraint.individual_.data()), constraint.individual_.size());
        std::string encoded_system = crypt::base64_encode(reinterpret_cast<const std::uint8_t*>(constraint.system_.data()), constraint.system_.size());

        sqlite3_bind_text(add_constraint_info_stmt_, 11, encoded_individual.data(), static_cast<int>(encoded_individual.size()), 0);
        sqlite3_bind_text(add_constraint_info_stmt_, 12, encoded_system.data(), static_cast<int>(encoded_system.size()), 0);

        sqlite3_bind_int(add_constraint_info_stmt_, 13, static_cast<int>(constraint.vendor_id_));
        sqlite3_bind_int(add_constraint_info_stmt_, 14, static_cast<int>(constraint.secure_id_));
        sqlite3_bind_int(add_constraint_info_stmt_, 15, static_cast<int>(constraint.active_constraints_));
        sqlite3_bind_int64(add_constraint_info_stmt_, 16, (static_cast<std::int64_t>(constraint.metering_grace_time_) << 1) | constraint.allow_use_without_metering_ & 0b1);
        sqlite3_bind_int(add_constraint_info_stmt_, 17, static_cast<int>(constraint.original_timed_counter_));

        if (sqlite3_step(add_constraint_info_stmt_) != SQLITE_ROW) {
            LOG_ERROR(SERVICE_DRMSYS, "Fail to add constraint to database!");
            return -1;
        }

        return sqlite3_column_int(add_constraint_info_stmt_, 0);
    }

    int rights_database::get_entry_id(const std::string &content_id) {
        // If the CID already exists, just forget it
        if (!get_cid_exist_stmt_) {
            static const char *get_cid_EXIST_STMT_STRING = "SELECT id FROM RightInfo WHERE contentId=:targetContentId COLLATE NOCASE";
            if (sqlite3_prepare(database_, get_cid_EXIST_STMT_STRING, -1, &get_cid_exist_stmt_, nullptr) != SQLITE_OK) {
                LOG_ERROR(SERVICE_DRMSYS, "Fail to prepare check CID exist SQL statement! Error: {}", sqlite3_errmsg(database_));
                return -1;
            }
        }

        sqlite3_reset(get_cid_exist_stmt_);
        if (sqlite3_bind_text(get_cid_exist_stmt_, 1, content_id.data(), static_cast<int>(content_id.length()), nullptr) != SQLITE_OK) {
            LOG_ERROR(SERVICE_DRMSYS, "Fail to bind content ID string to SQL statement!");
            return -1;
        }

        if (sqlite3_step(get_cid_exist_stmt_) == SQLITE_ROW) {
            return sqlite3_column_int(get_cid_exist_stmt_, 0);
        }

        return -1;
    }

    bool rights_database::add_or_update_record(rights_object &obj) {
        if (obj.common_data_.content_id_.empty()) {
            LOG_ERROR(SERVICE_DRMSYS, "Try to add a right object that has no content ID!");
            return false;
        }

        // If the CID already exists, delete
        int existing_cid = get_entry_id(obj.common_data_.content_id_);

        if (existing_cid >= 0) {
            if (!delete_record_stmt_) {
                static const char *DELETE_RECORD_STMT_STRING = "DELETE FROM RightInfo WHERE id=:targetId";
                if (sqlite3_prepare(database_, DELETE_RECORD_STMT_STRING, -1, &delete_record_stmt_, nullptr) != SQLITE_OK) {
                    LOG_ERROR(SERVICE_DRMSYS, "Fail to prepare delete record statement!");
                    return false;
                }
            }

            sqlite3_reset(delete_record_stmt_);
            if (sqlite3_bind_int(delete_record_stmt_, 1, existing_cid) != SQLITE_OK) {
                LOG_ERROR(SERVICE_DRMSYS, "Fail to bind ID to SQL delete statement!");
                return false;
            }

            // Have cascade so other thing will delete too
            if (sqlite3_step(delete_record_stmt_) != SQLITE_DONE) {
                LOG_ERROR(SERVICE_DRMSYS, "Fail to delete rights object record from database!");
                return false;
            }
        }

        // Start adding things
        if (!add_right_info_stmt_) {
            static const char *ADD_RIGHT_INFO_STMT_STRING = "INSERT INTO RightInfo (contentId, contentHash, issuer, contentName, authSeed, encryptKey) VALUES(:contentId, :contentHash,"
                ":issuer, :contentName, :authSeed, :encryptKey) RETURNING id";

            if (sqlite3_prepare(database_, ADD_RIGHT_INFO_STMT_STRING, -1, &add_right_info_stmt_, nullptr) != SQLITE_OK) {
                LOG_ERROR(SERVICE_DRMSYS, "Fail to prepare add right entry to database!");
                return false;
            }
        }

        sqlite3_reset(add_right_info_stmt_);
        sqlite3_bind_text(add_right_info_stmt_, 1, obj.common_data_.content_id_.data(), static_cast<int>(obj.common_data_.content_id_.length()), nullptr);

        std::string encoded_content_hash = crypt::base64_encode(reinterpret_cast<std::uint8_t*>(obj.common_data_.content_hash_.data()), obj.common_data_.content_hash_.size());
        std::string encoded_auth_seed = crypt::base64_encode(reinterpret_cast<std::uint8_t*>(obj.common_data_.auth_seed_.data()), obj.common_data_.auth_seed_.size());
        std::string encoded_key = crypt::base64_encode(reinterpret_cast<std::uint8_t*>(obj.encrypt_key_.data()), obj.encrypt_key_.size());

        sqlite3_bind_text(add_right_info_stmt_, 2, encoded_content_hash.data(), static_cast<int>(encoded_content_hash.size()), nullptr);
        sqlite3_bind_text(add_right_info_stmt_, 3, obj.common_data_.issuer_.data(), static_cast<int>(obj.common_data_.issuer_.size()), nullptr);
        sqlite3_bind_text(add_right_info_stmt_, 4, obj.common_data_.content_name_.data(), static_cast<int>(obj.common_data_.content_name_.size()), nullptr);
        sqlite3_bind_text(add_right_info_stmt_, 5, encoded_auth_seed.data(), static_cast<int>(encoded_auth_seed.size()), nullptr);
        sqlite3_bind_text(add_right_info_stmt_, 6, encoded_key.data(), static_cast<int>(encoded_key.size()), nullptr);

        int right_obj_id = 0;

        if (sqlite3_step(add_right_info_stmt_) == SQLITE_ROW) {
            right_obj_id = sqlite3_column_int(add_right_info_stmt_, 0);
        } else {
            LOG_ERROR(SERVICE_DRMSYS, "Fail to add new rights object into database! Last error: {}", sqlite3_errmsg(database_));
            return false;
        }

        if (!add_perm_info_stmt_) {
            static const char *ADD_PERM_INFO_STMT_STRING = "INSERT INTO PermissionInfo (rightsId, insertTime, version, exportMode, infoBits, parentRightsCid, myRightsCid, domainCid, rightIssuerIdentifier, onExpiredUrl) "
                "VALUES (:targetRightsId, :targetInsertTime, :targetVersion, :targetExportMode, :targetInfoBits, :targetParentRightsCid, :targetMyRightsCid, :targetDomainCid, :targetRightIssuerIdentifier, :targetOnExpiredUrl) "
                "RETURNING id";

            if (sqlite3_prepare(database_, ADD_PERM_INFO_STMT_STRING, -1, &add_perm_info_stmt_, nullptr) != SQLITE_OK) {
                LOG_ERROR(SERVICE_DRMSYS, "Fail to prepare add permission info statement! Error: {}", sqlite3_errmsg(database_));
                return false;
            }
        }

        for (std::size_t i = 0; i < obj.permissions_.size(); i++) {
            sqlite3_reset(add_perm_info_stmt_);
            sqlite3_bind_int(add_perm_info_stmt_, 1, right_obj_id);
            sqlite3_bind_int64(add_perm_info_stmt_, 2, obj.permissions_[i].insert_time_);
            sqlite3_bind_int(add_perm_info_stmt_, 3, static_cast<int>((static_cast<std::uint32_t>(obj.permissions_[i].version_rights_main_) << 16) |
                obj.permissions_[i].version_rights_sub_));
            sqlite3_bind_int(add_perm_info_stmt_, 4, obj.permissions_[i].export_mode_);
            sqlite3_bind_int(add_perm_info_stmt_, 5, obj.permissions_[i].info_bits_);
            
            std::string encoded_parent_right_cid = crypt::base64_encode(reinterpret_cast<std::uint8_t*>(obj.permissions_[i].parent_rights_id_.data()), obj.permissions_[i].parent_rights_id_.length());
            std::string encoded_my_right_cid = crypt::base64_encode(reinterpret_cast<std::uint8_t*>(obj.permissions_[i].rights_obj_id.data()), obj.permissions_[i].rights_obj_id.length());
            std::string encoded_domain_cid = crypt::base64_encode(reinterpret_cast<std::uint8_t*>(obj.permissions_[i].domain_id_.data()), obj.permissions_[i].domain_id_.length());
            std::string encoded_issuer_id = crypt::base64_encode(reinterpret_cast<std::uint8_t*>(obj.permissions_[i].right_issuer_identifier_.data()), obj.permissions_[i].right_issuer_identifier_.size());
            std::string encoded_on_expired_url = crypt::base64_encode(reinterpret_cast<std::uint8_t*>(obj.permissions_[i].on_expired_url_.data()), obj.permissions_[i].on_expired_url_.length());

            sqlite3_bind_text(add_perm_info_stmt_, 6, encoded_parent_right_cid.data(), static_cast<int>(encoded_parent_right_cid.size()), nullptr);
            sqlite3_bind_text(add_perm_info_stmt_, 7, encoded_my_right_cid.data(), static_cast<int>(encoded_my_right_cid.size()), nullptr);
            sqlite3_bind_text(add_perm_info_stmt_, 8, encoded_domain_cid.data(), static_cast<int>(encoded_domain_cid.size()), nullptr);
            sqlite3_bind_text(add_perm_info_stmt_, 9, encoded_issuer_id.data(), static_cast<int>(encoded_issuer_id.size()), nullptr);
            sqlite3_bind_text(add_perm_info_stmt_, 10, encoded_on_expired_url.data(), static_cast<int>(encoded_on_expired_url.size()), nullptr);

            int perm_obj_id = 0;

            if (sqlite3_step(add_perm_info_stmt_) == SQLITE_ROW) {
                perm_obj_id = sqlite3_column_int(add_perm_info_stmt_, 0);
            } else {
                LOG_WARN(SERVICE_DRMSYS, "Unable to add permission object to database");
            }

            int top_constraint_id = -1;
            int play_constraint_id = -1;
            int display_constraint_id = -1;
            int execute_constraint_id = -1;
            int print_constraint_id = -1;
            int export_constraint_id = -1;

            top_constraint_id = add_constraint(obj.permissions_[i].top_level_constraint_, perm_obj_id);

            if (obj.permissions_[i].available_rights_ & rights_type_play) {
                play_constraint_id = add_constraint(obj.permissions_[i].play_constraint_, perm_obj_id);
            }
            
            if (obj.permissions_[i].available_rights_ & rights_type_display) {
                display_constraint_id = add_constraint(obj.permissions_[i].display_constraint_, perm_obj_id);
            }
            
            if (obj.permissions_[i].available_rights_ & rights_type_execute) {
                execute_constraint_id = add_constraint(obj.permissions_[i].execute_constraint_, perm_obj_id);
            }
            
            if (obj.permissions_[i].available_rights_ & rights_type_print) {
                print_constraint_id = add_constraint(obj.permissions_[i].print_constraint_, perm_obj_id);
            }

            if (obj.permissions_[i].available_rights_ & rights_type_export) {
                export_constraint_id = add_constraint(obj.permissions_[i].export_contraint_, perm_obj_id);
            }

            if (!set_perm_constraints_stmt_) {
                static const char *SET_PERM_CONSTRAINT_STMT_STRING = "UPDATE PermissionInfo SET "
                    "topLevelConstraintId=:id1,"
                    "playConstraintId=:id2,"
                    "displayConstraintId=:id3,"
                    "executeConstraintId=:id4,"
                    "printConstraintId=:id5,"
                    "exportConstraintId=:id6 "
                    "WHERE id=:permFinId";

                if (sqlite3_prepare(database_, SET_PERM_CONSTRAINT_STMT_STRING, -1, &set_perm_constraints_stmt_, nullptr) != SQLITE_OK) {
                    LOG_ERROR(SERVICE_DRMSYS, "Fail to prepare update permission contraints info statement");
                    continue;
                }
            }

            sqlite3_reset(set_perm_constraints_stmt_);
            if (top_constraint_id < 0) {
                sqlite3_bind_null(set_perm_constraints_stmt_, 1);
            } else {
                sqlite3_bind_int(set_perm_constraints_stmt_, 1, top_constraint_id);
            }

            if (play_constraint_id < 0) {
                sqlite3_bind_null(set_perm_constraints_stmt_, 2);
            } else {
                sqlite3_bind_int(set_perm_constraints_stmt_, 2, play_constraint_id);
            }
            
            if (display_constraint_id < 0) {
                sqlite3_bind_null(set_perm_constraints_stmt_, 3);
            } else {
                sqlite3_bind_int(set_perm_constraints_stmt_, 3, display_constraint_id);
            }

            if (execute_constraint_id < 0) {
                sqlite3_bind_null(set_perm_constraints_stmt_, 4);
            } else {
                sqlite3_bind_int(set_perm_constraints_stmt_, 4, execute_constraint_id);
            }

            if (print_constraint_id < 0) {
                sqlite3_bind_null(set_perm_constraints_stmt_, 5);
            } else {
                sqlite3_bind_int(set_perm_constraints_stmt_, 5, print_constraint_id);
            }

            if (export_constraint_id < 0) {
                sqlite3_bind_null(set_perm_constraints_stmt_, 6);
            } else {
                sqlite3_bind_int(set_perm_constraints_stmt_, 6, export_constraint_id);
            }

            sqlite3_bind_int(set_perm_constraints_stmt_, 7, perm_obj_id);

            int result = sqlite3_step(set_perm_constraints_stmt_);

            if ((result != SQLITE_DONE) && (result != SQLITE_OK)) {
                LOG_ERROR(SERVICE_DRMSYS, "Unable to update permission's constraint info!");
            }
        }

        return true;
    }

    bool rights_database::get_record(const std::string &content_id, rights_object &result) {
        if (!get_cid_exist_full_stmt_) {
            static const char *GET_CID_EXIST_FULL_STMT_STRING = "SELECT * FROM RightInfo WHERE contentId=:targetContentId COLLATE NOCASE";
            if (sqlite3_prepare(database_, GET_CID_EXIST_FULL_STMT_STRING, -1, &get_cid_exist_full_stmt_, nullptr) != SQLITE_OK) {
                LOG_ERROR(SERVICE_DRMSYS, "Fail to prepare get record entry full statement!");
                return false;
            }
        }

        sqlite3_reset(get_cid_exist_full_stmt_);
        sqlite3_bind_text(get_cid_exist_full_stmt_, 1, content_id.data(), static_cast<int>(content_id.length()), nullptr);

        if (sqlite3_step(get_cid_exist_full_stmt_) != SQLITE_ROW) {
            return false;
        }

        int result_id = sqlite3_column_int(get_cid_exist_full_stmt_, 0);
        result.common_data_.content_id_ = reinterpret_cast<const char*>(sqlite3_column_text(get_cid_exist_full_stmt_, 1));
        result.common_data_.issuer_ = reinterpret_cast<const char*>(sqlite3_column_text(get_cid_exist_full_stmt_, 3));
        result.common_data_.content_name_ = reinterpret_cast<const char*>(sqlite3_column_text(get_cid_exist_full_stmt_, 4));

        const unsigned char *content_hash_data_string = sqlite3_column_text(get_cid_exist_full_stmt_, 2);
        const unsigned char *auth_seed_data_string = sqlite3_column_text(get_cid_exist_full_stmt_, 5);
        const unsigned char *encrypt_key_data_string = sqlite3_column_text(get_cid_exist_full_stmt_, 6);

        result.common_data_.content_hash_.resize(crypt::base64_decode(content_hash_data_string, strlen(reinterpret_cast<const char*>(content_hash_data_string)), nullptr, 0));
        result.common_data_.auth_seed_.resize(crypt::base64_decode(auth_seed_data_string, strlen(reinterpret_cast<const char*>(auth_seed_data_string)), nullptr, 0));
        result.encrypt_key_.resize(crypt::base64_decode(encrypt_key_data_string, strlen(reinterpret_cast<const char*>(encrypt_key_data_string)), nullptr, 0));

        crypt::base64_decode(content_hash_data_string, strlen(reinterpret_cast<const char*>(content_hash_data_string)), result.common_data_.content_hash_.data(), result.common_data_.content_hash_.size());
        crypt::base64_decode(auth_seed_data_string, strlen(reinterpret_cast<const char*>(auth_seed_data_string)), result.common_data_.auth_seed_.data(), result.common_data_.auth_seed_.size());
        crypt::base64_decode(encrypt_key_data_string, strlen(reinterpret_cast<const char*>(encrypt_key_data_string)), result.encrypt_key_.data(), result.encrypt_key_.size());

        return get_permission_list(result_id, result.permissions_);
    }

    bool rights_database::get_encryption_key(const std::string &content_id, std::string &output_key) {
        if (!get_encrypt_key_stmt_) {
            static const char *GET_ENCRYPT_KEY_STMT_STRING = "SELECT encryptKey FROM RightInfo WHERE contentId=:targetContentId COLLATE NOCASE";
            if (sqlite3_prepare(database_, GET_ENCRYPT_KEY_STMT_STRING, -1, &get_encrypt_key_stmt_, nullptr) != SQLITE_OK) {
                LOG_ERROR(SERVICE_DRMSYS, "Fail to prepare get encryption key statement!");
                return false;
            }
        }

        sqlite3_reset(get_encrypt_key_stmt_);
        sqlite3_bind_text(get_encrypt_key_stmt_, 1, content_id.data(), static_cast<int>(content_id.length()), nullptr);

        if (sqlite3_step(get_encrypt_key_stmt_) != SQLITE_ROW) {
            return false;
        }

        const std::uint8_t *encrypt_key_data = sqlite3_column_text(get_encrypt_key_stmt_, 0);
        output_key.resize(crypt::base64_decode(encrypt_key_data, strlen(reinterpret_cast<const char*>(encrypt_key_data)), nullptr, 0));
        crypt::base64_decode(encrypt_key_data,  strlen(reinterpret_cast<const char*>(encrypt_key_data)), output_key.data(), output_key.size());

        return true;
    }

    bool rights_database::get_constraint(const int constraint_id, rights_constraint &result) {
        if (!query_constraint_stmt_) {
            static const char *QUERY_CONSTRAINT_STMT_STRING = "SELECT * FROM ConstraintInfo WHERE id=:targetId";
            if (sqlite3_prepare(database_, QUERY_CONSTRAINT_STMT_STRING, -1, &query_constraint_stmt_, nullptr) != SQLITE_OK) {
                LOG_ERROR(SERVICE_DRMSYS, "Fail to prepare get constraint statement!");
                return false;
            }
        }

        sqlite3_reset(query_constraint_stmt_);
        sqlite3_bind_int(query_constraint_stmt_, 1, constraint_id);

        if (sqlite3_step(query_constraint_stmt_) != SQLITE_ROW) {
            return false;
        }

        result.start_time_ = static_cast<std::uint64_t>(sqlite3_column_int64(query_constraint_stmt_, 2));
        result.end_time_ = static_cast<std::uint64_t>(sqlite3_column_int64(query_constraint_stmt_, 3));
        result.interval_start_ = static_cast<std::uint64_t>(sqlite3_column_int64(query_constraint_stmt_, 4));
        result.interval_ = static_cast<std::uint32_t>(sqlite3_column_int(query_constraint_stmt_, 5));
        result.counter_ = static_cast<std::uint32_t>(sqlite3_column_int(query_constraint_stmt_, 6));
        result.original_counter_ = static_cast<std::uint32_t>(sqlite3_column_int(query_constraint_stmt_, 7));
        result.timed_counter_ = static_cast<std::uint32_t>(sqlite3_column_int(query_constraint_stmt_, 8));
        result.timed_interval_ = static_cast<std::uint32_t>(sqlite3_column_int(query_constraint_stmt_, 9));
        result.accumulated_time_ = static_cast<std::uint32_t>(sqlite3_column_int(query_constraint_stmt_, 10));
        result.vendor_id_ = static_cast<std::uint32_t>(sqlite3_column_int(query_constraint_stmt_, 13));
        result.secure_id_ = static_cast<std::uint32_t>(sqlite3_column_int(query_constraint_stmt_, 14));
        result.active_constraints_ = static_cast<std::uint32_t>(sqlite3_column_int(query_constraint_stmt_, 15));    
        std::uint64_t metering_grace = static_cast<std::uint64_t>(sqlite3_column_int64(query_constraint_stmt_, 16));
        result.metering_grace_time_ = static_cast<std::uint32_t>(metering_grace >> 1);
        result.allow_use_without_metering_ = static_cast<std::uint8_t>(metering_grace & 1);
        result.original_timed_counter_ = static_cast<std::uint32_t>(sqlite3_column_int(query_constraint_stmt_, 17));

        std::size_t individual_decoded_buffer_size = 0;
        std::size_t system_decoded_buffer_size = 0;
        
        const unsigned char *individual_data = sqlite3_column_text(query_constraint_stmt_, 11);
        const unsigned char *system_data = sqlite3_column_text(query_constraint_stmt_, 12);

        individual_decoded_buffer_size = crypt::base64_decode(individual_data, strlen(reinterpret_cast<const char*>(individual_data)), nullptr, 0);
        system_decoded_buffer_size = crypt::base64_decode(system_data, strlen(reinterpret_cast<const char*>(system_data)), nullptr, 0);

        result.individual_.resize(individual_decoded_buffer_size);
        result.system_.resize(system_decoded_buffer_size);

        crypt::base64_decode(individual_data, strlen(reinterpret_cast<const char*>(individual_data)), result.individual_.data(), result.individual_.size());
        crypt::base64_decode(system_data, strlen(reinterpret_cast<const char*>(system_data)), result.system_.data(), result.system_.size());

        return true;
    }

    bool rights_database::get_permission_list(const int rights_id, std::vector<rights_permission> &permissions) {
        if (!query_perms_stmt_) {
            static const char *QUERY_PERMISSION_ENTRIES_STMT_STRING = "SELECT * FROM PermissionInfo WHERE rightsId=:targetRightsId";
            if (sqlite3_prepare(database_, QUERY_PERMISSION_ENTRIES_STMT_STRING, -1, &query_perms_stmt_, nullptr) != SQLITE_OK) {
                LOG_ERROR(SERVICE_DRMSYS, "Fail to prepare permission query statement!");
                return false;
            }
        }

        sqlite3_reset(query_perms_stmt_);
        sqlite3_bind_int(query_perms_stmt_, 1, rights_id);

        do {
            int result = sqlite3_step(query_perms_stmt_);
            if (result != SQLITE_ROW) {
                break;
            }

            rights_permission perm;

            perm.unique_id_ = static_cast<std::uint32_t>(sqlite3_column_int(query_perms_stmt_, 0));
            perm.insert_time_ = static_cast<std::uint64_t>(sqlite3_column_int64(query_perms_stmt_, 2));
            std::uint32_t version = static_cast<std::uint32_t>(sqlite3_column_int(query_perms_stmt_, 3));
            perm.version_rights_main_ = static_cast<std::uint16_t>(version >> 16);
            perm.version_rights_sub_ = static_cast<std::uint16_t>(version);

            if (sqlite3_column_type(query_perms_stmt_, 4) != SQLITE_NULL) {
                get_constraint(sqlite3_column_int(query_perms_stmt_, 4), perm.top_level_constraint_);
            }

            if (sqlite3_column_type(query_perms_stmt_, 5) != SQLITE_NULL) {
                if (get_constraint(sqlite3_column_int(query_perms_stmt_, 5), perm.play_constraint_)) {
                    perm.available_rights_ |= rights_type_play;
                }
            }

            if (sqlite3_column_type(query_perms_stmt_, 6) != SQLITE_NULL) {
                if (get_constraint(sqlite3_column_int(query_perms_stmt_, 6), perm.display_constraint_)) {
                    perm.available_rights_ |= rights_type_display;
                }
            }

            if (sqlite3_column_type(query_perms_stmt_, 7) != SQLITE_NULL) {
                if (get_constraint(sqlite3_column_int(query_perms_stmt_, 7), perm.execute_constraint_)) {
                    perm.available_rights_ |= rights_type_execute;
                }
            }

            if (sqlite3_column_type(query_perms_stmt_, 8) != SQLITE_NULL) {
                if (get_constraint(sqlite3_column_int(query_perms_stmt_, 8), perm.print_constraint_)) {
                    perm.available_rights_ |= rights_type_print;
                }
            }
            
            if (sqlite3_column_type(query_perms_stmt_, 9) != SQLITE_NULL) {
                if (get_constraint(sqlite3_column_int(query_perms_stmt_, 9), perm.export_contraint_)) {
                    perm.available_rights_ |= rights_type_export;
                }
            }

            perm.export_mode_ = static_cast<rights_export_mode>(sqlite3_column_int(query_perms_stmt_, 10));
            perm.info_bits_ = static_cast<std::uint32_t>(sqlite3_column_int(query_perms_stmt_, 11));

            const std::uint8_t *parent_rights_cid_data = sqlite3_column_text(query_perms_stmt_, 12);
            const std::uint8_t *my_rights_cid_data = sqlite3_column_text(query_perms_stmt_, 13);
            const std::uint8_t *domain_cid_data = sqlite3_column_text(query_perms_stmt_, 14);
            const std::uint8_t *rights_issuer_id_data = sqlite3_column_text(query_perms_stmt_, 15);
            const std::uint8_t *on_expired_url_data = sqlite3_column_text(query_perms_stmt_, 16);

            perm.parent_rights_id_.resize(crypt::base64_decode(parent_rights_cid_data, strlen(reinterpret_cast<const char*>(parent_rights_cid_data)), nullptr, 0));
            perm.rights_obj_id.resize(crypt::base64_decode(my_rights_cid_data, strlen(reinterpret_cast<const char*>(my_rights_cid_data)), nullptr, 0));
            perm.domain_id_.resize(crypt::base64_decode(domain_cid_data, strlen(reinterpret_cast<const char*>(domain_cid_data)), nullptr, 0));
            perm.on_expired_url_.resize(crypt::base64_decode(on_expired_url_data, strlen(reinterpret_cast<const char*>(on_expired_url_data)), nullptr, 0));

            crypt::base64_decode(parent_rights_cid_data, strlen(reinterpret_cast<const char*>(parent_rights_cid_data)), perm.parent_rights_id_.data(), perm.parent_rights_id_.size());
            crypt::base64_decode(my_rights_cid_data, strlen(reinterpret_cast<const char*>(my_rights_cid_data)), perm.rights_obj_id.data(), perm.rights_obj_id.size());
            crypt::base64_decode(domain_cid_data, strlen(reinterpret_cast<const char*>(domain_cid_data)), perm.domain_id_.data(), perm.domain_id_.size());
            crypt::base64_decode(rights_issuer_id_data, strlen(reinterpret_cast<const char*>(rights_issuer_id_data)), reinterpret_cast<char*>(perm.right_issuer_identifier_.data()), perm.right_issuer_identifier_.size());
            crypt::base64_decode(on_expired_url_data, strlen(reinterpret_cast<const char*>(on_expired_url_data)), perm.on_expired_url_.data(), perm.on_expired_url_.size());
        
            permissions.push_back(perm);
        } while (true);

        return true;
    }

    bool rights_database::get_permission_list(const std::string &content_id, std::vector<rights_permission> &permissions) {
        int rights_id = get_entry_id(content_id);
        if (rights_id < 0) {
            return false;
        }

        return get_permission_list(rights_id, permissions);
    }
}