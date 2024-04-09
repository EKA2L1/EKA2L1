/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <mem/ptr.h>
#include <services/framework.h>
#include <services/ui/skin/chunk_maintainer.h>
#include <services/ui/skin/common.h>
#include <services/ui/skin/icon_cfg.h>
#include <services/ui/skin/settings.h>
#include <services/ui/skin/skn.h>

#include <queue>
#include <map>
#include <memory>

namespace eka2l1 {
    static constexpr const char *AKN_SKIN_SERVER_NAME = "!AknSkinServer";

    class akn_skin_server_session : public service::typical_session {
        eka2l1::ptr<void> client_handler_;
        epoc::notify_info nof_info_; ///< Notify info
        std::queue<epoc::akn_skin_server_change_handler_notification> nof_list_;
        std::vector<epoc::akn_skin_info_package> package_list_;

        std::uint32_t flags{ 0 };

        enum {
            ASS_FLAG_CANCELED = 0x1
        };

        void check_icon_config(service::ipc_context *ctx);

        void do_set_notify_handler(service::ipc_context *ctx);

        /**
         * \brief Get the next event in the notify list and notify the waiter.
         * 
         * - If cancel flag is set, the client will be notify with code KErrCancel.
         * - If the notify list is just empty, the requester of this next event will be
         *   the next one waiting.
         * - Else, the first element in the queue is popped and the client will be notify
         *   will that code.
         */
        void do_next_event(service::ipc_context *ctx);

        /**
         * \brief Cancel a pending signal request, if there is one available.
         * 
         * It will be cancel only if 2 conditions are satisfied:
         * - No pending notifications.
         * - A notify handler must be set.
         */
        void do_cancel(service::ipc_context *ctx);

        void store_scalable_gfx(service::ipc_context *ctx);

        void enumerate_packages(service::ipc_context *ctx);
        void retrieve_packages(service::ipc_context *ctx);

    public:
        explicit akn_skin_server_session(service::typical_server *svr, kernel::uid client_ss_uid, epoc::version client_version);
        ~akn_skin_server_session() override {}

        void fetch(service::ipc_context *ctx) override;
    };

    class akn_skin_server : public service::typical_server {
        std::unique_ptr<epoc::akn_ss_settings> settings_;
        std::unique_ptr<epoc::akn_skin_icon_config_map> icon_config_map_;
        std::unique_ptr<epoc::akn_skin_chunk_maintainer> chunk_maintainer_;

        chunk_ptr skin_chunk_;
        sema_ptr skin_chunk_sema_;
        mutex_ptr skin_chunk_render_mut_;

        fbs_server *fbss;

        std::map<epoc::pid, epoc::skn_file> skin_file_cache_;
        std::vector<epoc::akn_skin_info_package> skin_info_cache_list_;

        bool skin_info_list_cached_;

        void do_initialisation_pre();
        void do_initialisation();

        /**
         * \brief Merge active skin ID to the skin chunk.
         * \param io Pointer to the IO system.
         */
        void merge_active_skin(eka2l1::io_system *io);

    public:
        explicit akn_skin_server(eka2l1::system *sys);

        int is_icon_configured(const epoc::uid app_uid);

        void store_scalable_gfx(
            const epoc::pid item_id,
            const epoc::skn_layout_info layout_info,
            const std::uint32_t bmp_handle,
            const std::uint32_t msk_handle);

        void connect(service::ipc_context &ctx) override;

        /**
         * \brief Get the skin file with the given PID.
         *
         * The skin will be cached for future load.
         *
         * \param io Pointer to the IO system.
         * \param skin_pid The PID of the skin file.
         *
         * \return The pointer to the skin file. Nullptr if the skin is not found.
         */
        const epoc::skn_file *get_skin(io_system *io, const epoc::pid skin_pid);

        /**
         * \brief Get the active skin file.
         *
         * The skin will be cached for future load.
         *
         * \param io Pointer to the IO system.
         *
         * \return The pointer to the active skin file. Nullptr if the skin is not found.
         */
        const epoc::skn_file *get_active_skin(io_system *io);

        epoc::pid get_active_skin_pid(io_system *io);

        void set_active_skin_pid(const epoc::pid skin_pid);

        /**
         * \brief Get list of all skin information on the device.
         *
         * The function scans all drives, looking for files with the extension .skn. The file is not loaded in its whole,
         * but only the header is read to get the information about the skin.<br><br>
         *
         * The function does not modify the skin file cache.<br><br>
         *
         * \return Array of skin information packages.
         */
        std::vector<epoc::akn_skin_info_package> get_all_skin_infos();

        /**
         * \brief Get list of all skin information on specified device's drives.
         *
         * The function scans drives specified in the drive bitmask, looking for files with the extension .skn.
         * The file is not loaded in its whole, but only the header is read to get the information about the skin.<br><br>
         *
         * To get this function to scan a specific drive, you must turn the <i>n</i>th bit on the drive mask, where <i>n</i>
         * is the drive number. For example, to scan drive C, you must set the 2nd bit of the mask.<br><br>
         *
         * \code{.cpp}
         *     drive_mask |= (1 &lt;&lt; drive_c);
         * \endcode
         *
         * The function does not modify the skin file cache.<br><br>
         *
         * \param io Pointer to the IO system.
         * \param drive_mask The mask of drives to scan.
         *
         * \return Array of skin information packages.
         */
        std::vector<epoc::akn_skin_info_package> fresh_get_all_skin_infos(io_system *io, const std::uint32_t drive_mask);
    };

    /**
     * \brief Retrieve information about an application's current skin icon.
     *
     * Some skins will have custom icons for applications. This function will try to get information from skin file
     * about the path of the MIF containing the icon and the index of the icon in the MIF.
     *
     * @param skin_server       The pointer to the skin server instance.
     * @param app_uid           The UID of the application.
     * @param out_mif_path      On success, the path of the MIF containing the icon in the Symbian filesystem.
     * @param out_index         On success, the index of the icon in the MIF.
     * @param out_mask_index    On success, the index of the mask of the icon in the MIF.
     *
     * @return True if the skin icon is found, false otherwise.
     */
    bool get_skin_icon(akn_skin_server *skin_server, io_system *io, const std::uint32_t app_uid, std::u16string &out_mif_path,
        std::uint32_t &out_index, std::uint32_t &out_mask_index);
}
