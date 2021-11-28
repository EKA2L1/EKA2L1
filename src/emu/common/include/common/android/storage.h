// Copyright (c) 2012- PPSSPP Project.
// Copyright (c) 2021- EKA2L1 Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#pragma once

#include <common/android/jniutils.h>

#include <optional>
#include <string>
#include <vector>

namespace eka2l1::common::android {
    // To emphasize that Android storage mode strings are different, let's just use
    // an enum.
    enum class open_content_uri_mode {
        READ = 0, // "r"
        READ_WRITE = 1, // "rw"
        READ_WRITE_TRUNCATE = 2 // "rwt"
    };

    // Matches the constants in PpssppActivity.java.
    enum class storage_error {
        SUCCESS = 0,
        UNKNOWN = -1,
        NOT_FOUND = -2,
        DISK_FULL = -3,
        ALREADY_EXISTS = -4
    };

    inline storage_error storage_error_from_int(int ival) {
        if (ival >= 0) {
            return storage_error::SUCCESS;
        } else {
            return (storage_error)ival;
        }
    }

    int open_content_uri_fd(const std::string &uri, const open_content_uri_mode mode);
    storage_error create_directory(const std::string &parentTreeUri, const std::string &dirName);
    storage_error create_file(const std::string &parentTreeUri, const std::string &fileName);
    storage_error move_file(const std::string &fileUri, const std::string &srcParentUri, const std::string &destParentUri);
    storage_error copy_file(const std::string &fileUri, const std::string &destParentUri);
    storage_error remove_file(const std::string &fileUri);
    storage_error rename_file_to(const std::string &fileUri, const std::string &newName);
    std::optional<std::string> get_file_info_as_string(const std::string &fileUri);
    bool file_exists(const std::string &fileUri);
    bool is_external_storage_preserved_legacy();
    const char *error_to_string(storage_error error);

    std::vector<std::string> list_content_uri(const std::string &uri);
    void register_storage_callbacks(JNIEnv *env);

    void set_external_files_dir(const std::string &dir);
    void set_external_dir(const std::string &dir);

    std::string get_external_files_dir();
    std::string get_external_dir();
}
