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

#include <common/url.h>
#include <common/pystr.h>
#include <common/path.h>
#include <fmt/format.h>

#include <cctype>

namespace eka2l1::common::android {
    class content_uri {
    private:
        std::string provider;
        std::string root;
        std::string file;
    public:
        content_uri() {}
        explicit content_uri(const std::string &path) {
            parse(path);
        }

        bool parse(const std::string &path) {
            const char *prefix = "content://";
            if (path.rfind(prefix) != 0) {
                return false;
            }

            common::pystr components(path.substr(strlen(prefix)));
            std::vector<common::pystr> parts = components.split('/');

            if (parts.size() == 3) {
                // Single file URI.
                provider = parts[0].std_str();
                if (parts[1] == "tree") {
                    // Single directory URI.
                    // Not sure when we encounter these?
                    // file empty signals this type.
                    root = uri_decode(parts[2].std_str());
                    return true;
                } else if (parts[1] == "document") {
                    // root empty signals this type.
                    file = uri_decode(parts[2].std_str());
                    return true;
                } else {
                    // What's this?
                    return false;
                }
            } else if (parts.size() == 5) {
                // Tree URI
                provider = parts[0].std_str();
                if (parts[1] != "tree") {
                    return false;
                }
                root = uri_decode(parts[2].std_str());
                if (parts[3] != "document") {
                    return false;
                }
                file = uri_decode(parts[4].std_str());

                // Sanity check file path.
                return file.substr(root.length()) == root;
            } else {
                // Invalid Content URI
                return false;
            }
        }

        content_uri with_root_file_path(const std::string &file_path) {
            if (root.empty()) {
                LOG_ERROR(COMMON, "WithRootFilePath cannot be used with single file URIs.");
                return *this;
            }

            content_uri uri = *this;
            uri.file = uri.root;

            if (!file_path.empty()) {
                uri.file += "/" + file_path;
            }

            return uri;
        }

        content_uri navigate_forward(const std::string &path) {
            if (root.empty()) {
                LOG_ERROR(COMMON, "NavigateForward cannot be used with single file URIs.");
                return *this;
            }

            content_uri uri = *this;
            if (uri.file.empty()) {
                uri.file = uri.root;
            }

            if (!path.empty()) {
                uri.file += "/" + path;
            }

            return uri;
        }

        content_uri with_component(const std::string &file_path) {
            content_uri uri = *this;
            uri.file = uri.file + "/" + file_path;

            return uri;
        }

        content_uri with_extra_extension(const std::string &extension) {
            content_uri uri = *this;
            uri.file = uri.file + extension;
            return uri;
        }

        content_uri with_replaced_extension(const std::string &new_extension) const {
            content_uri uri = *this;
            if (file.empty()) {
                return uri;
            }
            std::string extension = get_file_extension();
            uri.file = file.substr(0, file.size() - extension.size()) + new_extension;

            return uri;
        }

        bool can_navigate_up() const {
            if (root.empty()) {
                return false;
            }

            return file.size() > root.size();
        }

        // Only goes downwards in hierarchies. No ".." will ever be generated.
        bool compute_path_to(const content_uri &other, std::string &path) const {
            size_t offset = file.size() + 1;
            std::string other_file_path = other.file;

            if (offset >= other_file_path.size()) {
                LOG_ERROR(COMMON, "Bad call to PathTo. '{}' -> '{}'", file, other_file_path);
                return false;
            }

            path = other_file_path.substr(file.size() + 1);
            return true;
        }

        std::string get_file_extension() const {
            size_t pos = file.rfind(".");
            if (pos == std::string::npos) {
                return "";
            }
            size_t slash_pos = file.rfind("/");
            if (slash_pos != std::string::npos && slash_pos > pos) {
                // Don't want to detect "df/file" from "/as.df/file"
                return "";
            }
            std::string ext = file.substr(pos);
            for (size_t i = 0; i < ext.size(); i++) {
                ext[i] = std::tolower(ext[i]);
            }
            return ext;
        }

        std::string get_last_part() const {
            if (file.empty()) {
                // Can't do anything anyway.
                return std::string();
            }

            if (!can_navigate_up()) {
                size_t colon = file.rfind(':');
                if (colon == std::string::npos) {
                    return std::string();
                }
                return file.substr(colon + 1);
            }

            size_t slash = file.rfind('/');
            if (slash == std::string::npos) {
                return std::string();
            }

            std::string part = file.substr(slash + 1);
            return part;
        }

        bool navigate_up() {
            if (!can_navigate_up()) {
                return false;
            }

            size_t slash = file.rfind('/');
            if (slash == std::string::npos) {
                return false;
            }

            file = file.substr(0, slash);
            return true;
        }

        bool tree_contains(const content_uri &uri) {
            if (root.empty()) {
                return false;
            }
            return uri.file.substr(root.length()) == root;
        }

        std::string to_string() const {
            if (file.empty()) {
                // Tree URI
                return fmt::format("content://{}/tree/{}", provider, uri_encode(root));
            } else if (root.empty()) {
                // Single file URI
                return fmt::format("content://{}/document/{}", provider, uri_encode(file));
            } else {
                // File URI from Tree
                return fmt::format("content://{}/tree/{}/document/{}", provider,
                                   uri_encode(root), uri_encode(file));
            }
        }

        // Never store the output of this, only show it to the user.
        std::string to_visual_string() const {
            return file;
        }

        const std::string &file_path() const {
            return file;
        }

        const std::string &root_path() const {
            return root.empty() ? file : root;
        }
    };
}