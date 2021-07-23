#pragma once

#include <common/types.h>
#include <package/registry.h>

#include <cstdint>
#include <string>

namespace eka2l1 {
    class io_system;
}

namespace eka2l1::loader {
    /**
     * \brief Parse and install old SIS format file.
     * 
     * \param path          Path to the package.
     * \param io            Pointer to IO system.
     * \param drive         Target drive to install the package.
     * \param info          The package info to fill in.
     * \param package_files A vector that will be filled with installed file paths.
     * \param progress_cb   Contains the callback that handle progress of the installation.
     * \param cancel_cb     Points to the callback that returns true if cancel was requested.
     * 
     * \returns True on success.
     */
    bool install_sis_old(const std::u16string &path, io_system *io, drive_number drive, package::object &info,
        progress_changed_callback progress_cb, cancel_requested_callback cancel_cb);
}
