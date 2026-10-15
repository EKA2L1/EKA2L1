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

#include <control/dispatcher.h>
#include <control/new_file.h>

#include <common/platform.h>

#include <string>

#if EKA2L1_PLATFORM(WIN32)
#include <common/cvt.h>

#include <Windows.h>
#else
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace eka2l1::control {
    static rpc_error already_there(const std::string &path) {
        return rpc_error(error_failed, path + " already exists; pick a path where nothing is");
    }

#if EKA2L1_PLATFORM(WIN32)
    void write_new_file(const std::string &path, const std::string_view data) {
        // CREATE_NEW fails on anything already there, and opens no reparse point it would follow.
        HANDLE file = CreateFileW(common::utf8_to_wstr(path).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL, nullptr);

        if (file == INVALID_HANDLE_VALUE) {
            const DWORD reason = GetLastError();

            if ((reason == ERROR_FILE_EXISTS) || (reason == ERROR_ALREADY_EXISTS)) {
                throw already_there(path);
            }

            throw rpc_error(error_failed, "Cannot create " + path + " (Windows error " + std::to_string(reason) + ")");
        }

        std::size_t done = 0;
        bool failed = false;

        while ((done < data.size()) && !failed) {
            const std::size_t left = data.size() - done;
            const DWORD chunk = static_cast<DWORD>((left > (1u << 30)) ? (1u << 30) : left);
            DWORD written = 0;

            failed = !WriteFile(file, data.data() + done, chunk, &written, nullptr) || (written == 0);
            done += written;
        }

        failed = !CloseHandle(file) || failed;

        if (failed) {
            throw rpc_error(error_failed, "Writing " + path + " failed; the file is left as far as it got");
        }
    }
#else
    void write_new_file(const std::string &path, const std::string_view data) {
        // O_EXCL: fail on anything already there, a symbolic link included (O_NOFOLLOW says so twice).
        const int file = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0644);

        if (file < 0) {
            const int reason = errno;

            if (reason == EEXIST) {
                throw already_there(path);
            }

            throw rpc_error(error_failed, "Cannot create " + path + ": " + std::strerror(reason));
        }

        std::size_t done = 0;
        int reason = 0;

        while ((done < data.size()) && (reason == 0)) {
            const ssize_t written = write(file, data.data() + done, data.size() - done);

            if (written > 0) {
                done += static_cast<std::size_t>(written);
            } else if ((written < 0) && (errno != EINTR)) {
                reason = errno;
            }
        }

        if ((close(file) != 0) && (reason == 0)) {
            reason = errno;
        }

        if (reason != 0) {
            throw rpc_error(error_failed, "Writing " + path + " failed (" + std::strerror(reason) + "); the file is left as far as it got");
        }
    }
#endif
}
