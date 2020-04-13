/*
 * Copyright (c) 2018 EKA2L1 Team.
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

#include <stdexcept>
#include <string>

namespace eka2l1 {
    /*! \brief Absolute a path.
	    \param current_dir The current directory.
		\param str The path
	*/
    std::string absolute_path(std::string str, std::string current_dir, bool symbian_use = false);

    /*! \brief Absolute a path.
	    \param current_dir The current directory.
		\param str The path
	*/
    std::u16string absolute_path(std::u16string str, std::u16string current_dir, bool symbian_use = false);

    /*! \brief Get the relative path.
		\param str The path
	*/
    std::string relative_path(const std::string &str, bool symbian_use = false);

    /*! \brief Get the relative path.
		\param str The path
	*/
    std::u16string relative_path(const std::u16string &str, bool symbian_use = false);

    /*! \brief Merge two paths together.
	 * \returns The new path.
	*/
    std::string add_path(const std::string &path1, const std::string &path2, bool symbian_use = false);

    /*! \brief Merge two paths together.
	 * \returns The new path.
	*/
    std::u16string add_path(const std::u16string &path1, const std::u16string &path2, bool symbian_use = false);

    /*! \brief Get the root name.
		\param str The path
	*/
    std::string root_name(std::string path, bool symbian_use = false);

    /*! \brief Get the root name.
		\param str The path
	*/
    std::u16string root_name(std::u16string path, bool symbian_use = false);

    /*! \brief Get the root directory.
		\param str The path
	*/
    std::string root_dir(std::string path, bool symbian_use = false);

    /*! \brief Get the root directory.
		\param str The path
	*/
    std::u16string root_dir(std::u16string path, bool symbian_use = false);

    /*! \brief Get the root path.
		\param str The path
	*/
    std::string root_path(std::string path, bool symbian_use = false);

    /*! \brief Get the root path.
		\param str The path
	*/
    std::u16string root_path(std::u16string path, bool symbian_use = false);

    /*! \brief Get the file name.
		\param str The path
	*/
    std::string filename(std::string path, bool symbian_use = false);

    /*! \brief Get the file name.
		\param str The path
	*/
    std::u16string filename(std::u16string path, bool symbian_use = false);

    /*! \brief Get the file directory.
	*/
    std::string file_directory(std::string path, bool symbian_use = false);

    /*! \brief Get the file directory.
	*/
    std::u16string file_directory(std::u16string path, bool symbian_use = false);

    std::string path_extension(const std::string &path);

    std::u16string path_extension(const std::u16string &path);

    std::string replace_extension(const std::string &path, const std::string &new_ext);

    std::u16string replace_extension(const std::u16string &path, const std::u16string &new_ext);

    /*! \brief Create a directory. */
    void create_directory(std::string path);

    /*! \brief Check if a file or directory exists. */
    bool exists(std::string path);

    /*! \brief Check if the path points to a directory. */
    bool is_dir(std::string path);

    /*! \brief Create directories. */
    void create_directories(std::string path);

    bool set_current_directory(const std::string &path);
    bool get_current_directory(std::string &path);

    bool is_separator(const char sep);
    bool is_separator(const char16_t sep);

    char get_separator(bool symbian_use = false);
    char16_t get_separator_16(bool symbian_use = false);

    /*! \brief Iterate through components of a path */
    struct path_iterator {
        std::string path;
        std::string comp;
        uint32_t crr_pos;

    public:
        path_iterator()
            : crr_pos(0) {
            (*this)++;
        }

        path_iterator(std::string p)
            : path(p)
            , crr_pos(0) {
            (*this)++;
        }

        path_iterator operator++(int dummy) {
            if (crr_pos > path.length()) {
                throw std::runtime_error("Iterator is invalid");
                return *this;
            }

            if (crr_pos == 0 && (is_separator(path[crr_pos]))) {
                crr_pos++;
                comp = "";
                return *this;
            }

            if (crr_pos < path.length()) {
                comp = "";
            } else {
                crr_pos++;
                return *this;
            }

            while (crr_pos < path.length() && !is_separator(path[crr_pos])) {
                comp += path[crr_pos];
                crr_pos += 1;
            }

            while (crr_pos < path.length() && is_separator(path[crr_pos])) {
                crr_pos += 1;
            }

            return *this;
        }

        std::string operator*() const {
            return comp;
        }

        operator bool() const {
            return crr_pos <= path.length();
        }
    };

    /*! \brief Check if the path is absoluted or not 
	    \param current_dir The current directory.
		\param str The path.
	*/
    template <typename T>
    bool is_absolute(std::basic_string<T> str, std::basic_string<T> current_dir, bool symbian_use = false) {
        return absolute_path(str, current_dir, symbian_use) == str;
    }
    /*! \brief Check if the path is relative or not
		\param str The path.
	*/
    template <typename T>
    bool is_relative(std::basic_string<T> str, bool symbian_use = false) {
        return !relative_path(str, symbian_use).empty();
    }

    /*! \brief Check if the path has root name or not.
		\param str The path.
	*/
    template <typename T>
    bool has_root_name(std::basic_string<T> path, bool symbian_use = false) {
        return !root_name(path, symbian_use).empty();
    }

    /*! \brief Check if the path has root directory or not.
		\param str The path.
	*/
    template <typename T>
    bool has_root_dir(std::basic_string<T> path, bool symbian_use = false) {
        return !root_dir(path, symbian_use).empty();
    }

    template <typename T>
    bool has_root_path(std::basic_string<T> path, bool symbian_use) {
        return !root_path(path, symbian_use).empty();
    }

    template <typename T>
    bool has_filename(std::basic_string<T> path, bool symbian_use) {
        return filename(path, symbian_use) != "";
    }
}
