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

#include <common/platform.h>

#include <services/akn/icon/icon.h>
#include <services/akn/skin/server.h>
#include <services/applist/applist.h>
#include <services/audio/keysound/keysound.h>
#include <services/audio/mmf/audio.h>
#include <services/audio/mmf/dev.h>
#include <services/backup/backup.h>
#include <services/cdl/cdl.h>
#include <services/centralrepo/centralrepo.h>
#include <services/connmonitor/connmonitor.h>
#include <services/domain/domain.h>
#include <services/drm/notifier/notifier.h>
#include <services/drm/helper.h>
#include <services/drm/rights.h>
#include <services/ecom/ecom.h>
#include <services/etel/etel.h>
#include <services/fbs/fbs.h>
#include <services/featmgr/featmgr.h>
#include <services/fs/fs.h>
#include <services/hwrm/hwrm.h>
#include <services/install/install.h>
#include <services/loader/loader.h>
#include <services/msv/msv.h>
#include <services/notifier/notifier.h>
#include <services/remcon/remcon.h>
#include <services/sensor/sensor.h>
#include <services/sms/sa/sa.h>
#include <services/socket/socket.h>
#include <services/ui/cap/oom_app.h>
#include <services/ui/eikappui.h>
#include <services/ui/view/view.h>
#include <services/window/window.h>

#include <epoc/epoc.h>
#include <services/init.h>
#include <utils/locale.h>
#include <utils/system.h>

#include <config/config.h>
#include <manager/device_manager.h>
#include <manager/manager.h>

#if EKA2L1_PLATFORM(WIN32)
#include <Windows.h>
#endif

#define CREATE_SERVER_D(sys, svr, ...)                                                 \
    std::unique_ptr<service::server> temp = std::make_unique<svr>(sys, ##__VA_ARGS__); \
    sys->get_kernel_system()->add_custom_server(temp)

#define CREATE_SERVER(sys, svr, ...)                  \
    temp = std::make_unique<svr>(sys, ##__VA_ARGS__); \
    sys->get_kernel_system()->add_custom_server(temp)

#define DEFINE_INT_PROP_D(sys, category, key, data)                            \
    property_ptr prop = sys->get_kernel_system()->create<service::property>(); \
    prop->first = category;                                                    \
    prop->second = key;                                                        \
    prop->define(service::property_type::int_data, 0);                         \
    prop->set_int(data);

#define DEFINE_INT_PROP(sys, category, key, data)                 \
    prop = sys->get_kernel_system()->create<service::property>(); \
    prop->first = category;                                       \
    prop->second = key;                                           \
    prop->define(service::property_type::int_data, 0);            \
    prop->set_int(data);

#define DEFINE_BIN_PROP_D(sys, category, key, size, data)                      \
    property_ptr prop = sys->get_kernel_system()->create<service::property>(); \
    prop->first = category;                                                    \
    prop->second = key;                                                        \
    prop->define(service::property_type::bin_data, size);                      \
    prop->set(data);

#define DEFINE_BIN_PROP(sys, category, key, size, data)           \
    prop = sys->get_kernel_system()->create<service::property>(); \
    prop->first = category;                                       \
    prop->second = key;                                           \
    prop->define(service::property_type::bin_data, size);         \
    prop->set(data);

namespace eka2l1::epoc {
    epoc::locale get_locale_info() {
        epoc::locale locale;

        // TODO: Move to common
#if EKA2L1_PLATFORM(WIN32)
        locale.country_code_ = static_cast<int>(GetProfileInt("intl", "iCountry", 0));
#endif

        locale.clock_format_ = epoc::clock_digital;
        locale.start_of_week_ = epoc::monday;
        locale.date_format_ = epoc::date_format_america;
        locale.time_format_ = epoc::time_format_twenty_four_hours;
        locale.universal_time_offset_ = -14400;
        locale.device_time_state_ = epoc::device_user_time;

        return locale;
    }

    static void initialize_system_properties(eka2l1::system *sys, eka2l1::config::state *cfg) {
        auto lang = epoc::locale_language{ epoc::lang_english, 0, 0, 0, 0, 0, 0, 0 };
        auto locale = epoc::get_locale_info();
        auto &dvcs = sys->get_manager_system()->get_device_manager()->get_devices();

        if (dvcs.size() > cfg->device) {
            auto &dvc = dvcs[cfg->device];

            if (cfg->language == -1) {
                lang.language = static_cast<epoc::language>(dvc.default_language_code);
            } else {
                lang.language = static_cast<epoc::language>(cfg->language);
            }
        }

        // Unknown key, testing show that this prop return 65535 most of times
        // The prop belongs to HAL server, but the key usuage is unknown. (TODO)
        DEFINE_INT_PROP_D(sys, epoc::SYS_CATEGORY, epoc::UNK_KEY1, 65535);
        DEFINE_INT_PROP(sys, epoc::SYS_CATEGORY, epoc::PHONE_POWER_KEY, system_agent_state_on);

        // From Domain Server request
        DEFINE_INT_PROP(sys, 0x1020e406, 0x250, 0);

        DEFINE_BIN_PROP(sys, epoc::SYS_CATEGORY, epoc::LOCALE_LANG_KEY, sizeof(epoc::locale_language), lang);
        DEFINE_BIN_PROP(sys, epoc::SYS_CATEGORY, epoc::LOCALE_DATA_KEY, sizeof(epoc::locale), locale);
    }
}

namespace eka2l1 {
    namespace service {
        // Mostly replace startup process of a normal EPOC startup
        void init_services(system *sys) {
            CREATE_SERVER_D(sys, fs_server);
            CREATE_SERVER(sys, loader_server);

            config::state *cfg = sys->get_config();

            if (cfg->enable_srv_ecom)
                CREATE_SERVER(sys, ecom_server);

            CREATE_SERVER(sys, fbs_server);
            CREATE_SERVER(sys, window_server);

            if (cfg->enable_srv_cenrep)
                CREATE_SERVER(sys, central_repo_server);

            CREATE_SERVER(sys, featmgr_server);

            if (cfg->enable_srv_backup)
                CREATE_SERVER(sys, backup_server);

            if (cfg->enable_srv_install)
                CREATE_SERVER(sys, install_server);

            if (cfg->enable_srv_rights)
                CREATE_SERVER(sys, rights_server);

            if (cfg->enable_srv_sa)
                CREATE_SERVER(sys, sa_server);

            if (cfg->enable_srv_drm)
                CREATE_SERVER(sys, drm_helper_server);

            // These needed to be HLEd
            CREATE_SERVER(sys, applist_server);
            CREATE_SERVER(sys, oom_ui_app_server);
            CREATE_SERVER(sys, hwrm_server);
            CREATE_SERVER(sys, view_server);
            CREATE_SERVER(sys, remcon_server);
            CREATE_SERVER(sys, etel_server);
            CREATE_SERVER(sys, notifier_server);
            CREATE_SERVER(sys, msv_server);
            CREATE_SERVER(sys, sensor_server);
            CREATE_SERVER(sys, connmonitor_server);
            CREATE_SERVER(sys, drm_notifier_server);
            CREATE_SERVER(sys, socket_server);

            // Not really sure about this one
            CREATE_SERVER(sys, keysound_server);

            if (cfg->enable_srv_eikapp_ui)
                CREATE_SERVER(sys, eikappui_server);

            if (cfg->enable_srv_akn_icon)
                CREATE_SERVER(sys, akn_icon_server);

            if (cfg->enable_srv_cdl)
                CREATE_SERVER(sys, cdl_server);

            if (cfg->enable_srv_akn_skin)
                CREATE_SERVER(sys, akn_skin_server);

            // Don't change order
            temp = std::make_unique<domainmngr_server>(sys);
            kernel_system *kern = sys->get_kernel_system();

            auto &dmmngr = reinterpret_cast<domainmngr_server *>(temp.get())->get_domain_manager();
            dmmngr->add_hierarchy_from_database(service::database::hierarchy_power_id);
            dmmngr->add_hierarchy_from_database(service::database::hierarchy_startup_id);

            kern->add_custom_server(temp);

            // Create the domain server
            temp = std::make_unique<domain_server>(sys, dmmngr);
            kern->add_custom_server(temp);

            std::unique_ptr<service::server> dev_serv = std::make_unique<mmf_dev_server>(sys);
            std::unique_ptr<service::server> audio_serv = std::make_unique<mmf_audio_server>(sys,
                reinterpret_cast<mmf_dev_server *>(dev_serv.get()));

            kern->add_custom_server(dev_serv);
            kern->add_custom_server(audio_serv);

            epoc::initialize_system_properties(sys, cfg);
        }
    }
}
