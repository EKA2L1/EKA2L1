/*
 * Copyright (c) 2018 EKA2L1 Team
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

#pragma once

#include <services/applist/common.h>
#include <services/framework.h>

#include <utils/des.h>
#include <vfs/vfs.h>

#include <mutex>
#include <vector>

namespace eka2l1 {
    class io_system;
    class fbs_server;

    struct fbsbitmap;

    namespace common {
        class ro_stream;
    }

    namespace kernel {
        class process;
    }

    namespace epoc {
        struct bitwise_bitmap;
    }

    namespace epoc::apa {
        struct command_line;
    }

    struct apa_app_info {
        std::uint32_t uid; ///< The UID of the application.
        epoc::filename app_path; ///< The path to the application DLL (EKA1) / EXE (EKA2)
        epoc::apa_app_caption short_caption; ///< Short version of the caption
        epoc::apa_app_caption long_caption; ///< Long caption of the app

        explicit apa_app_info() {}
    };

    struct apa_app_icon {
        std::uint16_t number_;
        fbsbitmap *bmp_;
        address bmp_rom_addr_;
    };

    enum apa_legacy_level {
        APA_LEGACY_LEVEL_OLD = -2,
        APA_LEGACY_LEVEL_TRANSITION = -1,
        APA_LEGACY_LEVEL_MORDEN = 0
    };

    using apa_app_masked_icon_bitmap = std::pair<epoc::bitwise_bitmap*, epoc::bitwise_bitmap*>;

    struct apa_app_registry {
        apa_app_info mandatory_info;
        apa_capability caps;

        std::u16string rsc_path;
        std::u16string localised_info_rsc_path;
        std::uint32_t localised_info_rsc_id{ 1 };

        std::uint8_t default_screen_number{ 0 };

        std::int16_t icon_count;
        std::u16string icon_file_path;

        std::vector<apa_app_icon> app_icons;
        std::vector<data_type> data_types;
        std::vector<view_data> view_datas;
        file_ownership_list ownership_list;

        drive_number land_drive;

        /**
         * @brief Get parameters to launch this app registry.
         * 
         * @param native_executable_path        Path to the host executable that runs this app.
         * @param args                          Command line arguments. You can prefill variables that you want,
         *                                      this function will fill parameters that you don't yet know.
         */
        void get_launch_parameter(std::u16string &native_executable_path, epoc::apa::command_line &args);
    };

    /**
     * \brief Read registeration info from a stream.
     * 
     * The function does not know the app UID. To know the UID yourself, check out UID3 field of
     * the app binary. App binary path is guranteed to be filled in the struct on success.
     * 
     * \param stream      A read-only stream contains registeration info.
     * \param reg         APA registry struct. This will be filled with info on success.
     * \param land_drive  The drive contains this registeration.
     * 
     * \returns True on success.
     */
    bool read_registeration_info(common::ro_stream *stream, apa_app_registry &reg, const drive_number land_drive);

    /**
     * \brief Read registeration info from AIF file.
     * 
     * \param stream      A read-only stream contains registeration info.
     * \param reg         APA registry struct. This will be filled with info on success.
     * \param land_drive  The drive contains this registeration.
     * \param lang        The language of the system.
     * 
     * \returns True on success.
     */
    bool read_registeration_info_aif(common::ro_stream *stream, apa_app_registry &reg, const drive_number land_drive,
        const language lang);

    /**
     * \brief   Read icon data from AIF file.
     * 
     * \param   stream      A read-only stream contains registeration info.
     * \param   serv        Pointer to font bitmap server, used to create bitmap.
     * \param   icon_list   List of icon to be filled.
     * \param   rom_addr    The address of the registeration file in ROM. 0 for unavailable.
     * 
     * \returns True on success.
     */
    bool read_icon_data_aif(common::ro_stream *stream, fbs_server *serv, std::vector<apa_app_icon> &icon_list, const address rom_addr = 0);

    /**
     * \brief   Read caption data from localised RSC file
     * 
     * \param   stream      A read-only stream contains caption data.
     * \param   reg         Registeration info to fill captions in.
     * 
     * \returns True on success.
     */
    bool read_caption_data_oldarch(common::ro_stream *stream, apa_app_registry &reg);

    /**
     * \brief Read localised registeration info from a stream.
     * 
     * \param stream    Read-only stream contains localised registeration info.
     * \param reg         APA registry struct. This will be filled with info on success.
     * \param land_drive  The drive contains this registeration.
     * 
     * \returns True on success.
     */
    bool read_localised_registration_info(common::ro_stream *stream, apa_app_registry &reg, const drive_number land_drive);

    const std::string get_app_list_server_name_by_epocver(const epocver ver);

    class applist_session : public service::typical_session {
    public:
        explicit applist_session(service::typical_server *svr, kernel::uid client_ss_uid, epoc::version client_ver);
        void fetch(service::ipc_context *ctx);
    };

    /*! \brief Applist services
     *
     * Provide external information about application management,
     * and HAL information regards to each app.
	 *
	 * Disable for LLE testings.
     */
    class applist_server : public service::typical_server {
        friend class applist_session;

        std::vector<apa_app_registry> regs;

        std::uint32_t flags{ 0 };

        std::vector<std::int64_t> watchs_;
        fbs_server *fbsserv;

        enum {
            AL_INITED = 0x1
        };

        void sort_registry_list();
        void init();

        bool delete_registry(const std::u16string &rsc_path);

        bool load_registry(eka2l1::io_system *io, const std::u16string &path, drive_number land_drive,
            const language ideal_lang = language::en);

        bool load_registry_oldarch(eka2l1::io_system *io, const std::u16string &path, drive_number land_drive,
            const language ideal_lang = language::en);

        void on_register_directory_changes(eka2l1::io_system *io, const std::u16string &base, drive_number land_drive,
            common::directory_changes &changes);

        void on_drive_change(void *userdata, drive_number drv, drive_action act);

        void remove_registries_on_drive(const drive_number drv);

        void rescan_registries_on_drive_oldarch(eka2l1::io_system *io, const drive_number num);
        void rescan_registries_on_drive_newarch(eka2l1::io_system *io, const drive_number num);

        void rescan_registries(eka2l1::io_system *io);

        /*! \brief Get the number of screen shared for an app. 
         * 
         * \param arg0: application UID.
         * \param request_sts: KErrNotFound if app doesn't exist.
        */
        void default_screen_number(service::ipc_context &ctx);

        /*! \brief Get the application language.
         *
         * \param arg0: App's uid3.
         * \param arg1: App language.
         *
         * Expected request status: KErrNone. 
        */
        void app_language(service::ipc_context &ctx);

        /*! \brief Request the server to run app.
         *
         * Iter through every AppList plugins, set status to true
         * if application is allowed to run
         * 
         * \param arg0: App's uid3
        */
        void is_accepted_to_run(service::ipc_context &ctx);

        /*! \brief Get the info of the specified application
         *
         * \param arg0: App's uid3
         * \param arg1: Descriptor contains apa_app_info
        */
        void get_app_info(service::ipc_context &ctx);

        /*! \brief Get the capability of an app.
         *
         * \param arg0: Descriptor contains the capability
         * \param arg1: The application's UID
        */
        void get_capability(service::ipc_context &ctx);

        /**
         * \brief Get the path to an app's icon.
         * 
         * The first argument contains the app UID. The second argument contains the
         * filename package.
         */
        void get_app_icon_file_name(service::ipc_context &ctx);

        /**
         * \brief Get app icon bitmap handles.
         */
        void get_app_icon(service::ipc_context &ctx);

        void get_native_executable_name_if_non_native(service::ipc_context &ctx);
                
        void launch_app(service::ipc_context &ctx);
        void is_program(service::ipc_context &ctx);

        void connect(service::ipc_context &ctx) override;

    protected:
        bool launch_app(const std::u16string &exe_path, const std::u16string &cmd, kernel::uid *thread_id,
            kernel::process *requester = nullptr);

    public:
        explicit applist_server(system *sys);
        ~applist_server() override;

        /**
         * @brief       Get the legacy level of the server.
         */
        int legacy_level();

        bool launch_app(apa_app_registry &registry, epoc::apa::command_line &parameter, kernel::uid *thread_id);
        std::optional<apa_app_masked_icon_bitmap> get_icon(apa_app_registry &registry, const std::int8_t index);

        std::mutex list_access_mut_;

        /**
         * \brief Get an app registeration
         * 
         * \param uid The UID of the app.
         * \returns Nullptr if the registeration does not exist. Else the pointer to it.
         */
        apa_app_registry *get_registration(const std::uint32_t uid);

        /**
         * \brief Get all app registerations.
         */
        std::vector<apa_app_registry> &get_registerations();
    };
}
