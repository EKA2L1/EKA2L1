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

#include <services/accessory/accessory.h>
#include <services/alarm/alarm.h>
#include <services/applist/applist.h>
#include <services/audio/keysound/keysound.h>
#include <services/audio/mmf/audio.h>
#include <services/audio/mmf/dev.h>
#include <services/bluetooth/btman.h>
#include <services/centralrepo/centralrepo.h>
#include <services/comm/comm.h>
#include <services/drm/helper.h>
#include <services/drm/notifier/notifier.h>
#include <services/drm/rights/rights.h>
#include <services/etel/etel.h>
#include <services/fbs/fbs.h>
#include <services/featmgr/featmgr.h>
#include <services/fs/fs.h>
#include <services/goommonitor/goommonitor.h>
#include <services/hwrm/hwrm.h>
#include <services/internet/connmonitor.h>
#include <services/internet/nifman.h>
#include <services/loader/loader.h>
#include <services/msv/msv.h>
#include <services/notifier/notifier.h>
#include <services/remcon/remcon.h>
#include <services/sensor/sensor.h>
#include <services/shutdown/shutdown.h>
#include <services/sisregistry/sisregistry.h>
#include <services/sms/settings.h>
#include <services/sms/sa/sa.h>
#include <services/sms/sendas/sendas.h>
#include <services/socket/server.h>
#include <services/sysagt/sysagt.h>
#include <services/ui/cap/oom_app.h>
#include <services/ui/eikappui.h>
#include <services/ui/icon/icon.h>
#include <services/ui/skin/server.h>
#include <services/ui/view/view.h>
#include <services/uiss/uiss.h>
#include <services/unipertar/unipertar.h>
#include <services/window/window.h>

#include <services/init.h>
#include <system/epoc.h>
#include <utils/locale.h>
#include <utils/system.h>

#include <config/config.h>
#include <system/devices.h>

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

        // TODO: These are stubbed!
        // See in relation: CLocale::MonetaryLoadLocaleL in ossrv, openenvcore's libc in file localeinfo.cpp
        locale.clock_format_ = epoc::clock_digital;
        locale.start_of_week_ = epoc::monday;
        locale.date_format_ = epoc::date_format_america;
        locale.time_format_ = epoc::time_format_twenty_four_hours;
        locale.universal_time_offset_ = -14400;
        locale.device_time_state_ = epoc::device_user_time;
        locale.decimal_separator_ = '.';
        locale.negative_currency_format_ = epoc::negative_currency_leading_minus_sign;

        locale.time_separator_[0] = 0;
        locale.time_separator_[1] = ':';
        locale.time_separator_[2] = ':';
        locale.time_separator_[3] = 0;

        locale.date_separator_[0] = 0;
        locale.date_separator_[1] = '/';
        locale.date_separator_[2] = '/';
        locale.date_separator_[3] = 0;

        return locale;
    }

    static void initialize_system_properties(eka2l1::system *sys, eka2l1::config::state *cfg) {
        auto lang = epoc::locale_language{ epoc::lang_english, 0, 0, 0, 0, 0, 0, 0 };
        auto locale = epoc::get_locale_info();
        auto &dvcs = sys->get_device_manager()->get_devices();
        kernel_system *kern = sys->get_kernel_system();

        if (dvcs.size() > cfg->device) {
            auto &dvc = dvcs[cfg->device];

            if (cfg->language == -1) {
                lang.language = static_cast<epoc::language>(dvc.default_language_code);
            } else {
                lang.language = static_cast<epoc::language>(cfg->language);
            }
        }

        address am_pm_names_addr[] = {
            kern->put_global_kernel_string("am"),
            kern->put_global_kernel_string("pm"),
        };

        lang.am_pm_table = eka2l1::ptr<char>(kern->put_static_array(am_pm_names_addr, 2));

        epoc::locale_locale_settings locale_settings;
        locale_settings.locale_extra_settings_dll_ptr = 0;
        locale_settings.currency_symbols[0] = '$';
        locale_settings.currency_symbols[1] = '\0';

        // Unknown key, testing show that this prop return 65535 most of times
        // The prop belongs to HAL server, but the key usuage is unknown. (TODO)
        DEFINE_INT_PROP_D(sys, epoc::SYS_CATEGORY, epoc::UNK_KEY1, 65535);
        DEFINE_INT_PROP(sys, epoc::SYS_CATEGORY, epoc::PHONE_POWER_KEY, system_agent_state_on);
        DEFINE_INT_PROP(sys, epoc::SYS_CATEGORY, epoc::SOFTWARE_INSTALL_KEY, 0);
        DEFINE_INT_PROP(sys, epoc::SYS_CATEGORY, epoc::SOFTWARE_LASTEST_UID_INSTALLATION, 0);

        // From Domain Server request
        DEFINE_INT_PROP(sys, 0x1020e406, 0x250, 0);

        DEFINE_BIN_PROP(sys, epoc::SYS_CATEGORY, epoc::LOCALE_LANG_KEY, sizeof(epoc::locale_language), lang);
        DEFINE_BIN_PROP(sys, epoc::SYS_CATEGORY, epoc::LOCALE_DATA_KEY, sizeof(epoc::locale), locale);
        DEFINE_BIN_PROP(sys, epoc::SYS_CATEGORY, epoc::LOCALE_LOCALE_SETTINGS_KEY, sizeof(epoc::locale_locale_settings), locale_settings);
    }
}

namespace eka2l1 {
    namespace service {
        // Mostly replace startup process of a normal EPOC startup
        void init_services(system *sys) {
            CREATE_SERVER_D(sys, fs_server);
            CREATE_SERVER(sys, loader_server);
            CREATE_SERVER(sys, shutdown_server);

            config::state *cfg = sys->get_config();

            CREATE_SERVER(sys, fbs_server);
            CREATE_SERVER(sys, window_server);
            CREATE_SERVER(sys, central_repo_server);
            CREATE_SERVER(sys, featmgr_server);

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
            CREATE_SERVER(sys, nifman_server);
            CREATE_SERVER(sys, drm_notifier_server);
            CREATE_SERVER(sys, sisregistry_server);
            CREATE_SERVER(sys, alarm_server);
            CREATE_SERVER(sys, socket_server);

            CREATE_SERVER(sys, comm_server);
            CREATE_SERVER(sys, btman_server);
            CREATE_SERVER(sys, accessory_server);

            // Not really sure about this one
            CREATE_SERVER(sys, keysound_server);

            CREATE_SERVER(sys, eikappui_server);
            //CREATE_SERVER(sys, akn_icon_server);
            CREATE_SERVER(sys, akn_skin_server);

            CREATE_SERVER(sys, system_agent_server);
            CREATE_SERVER(sys, unipertar_server);
            CREATE_SERVER(sys, goom_monitor_server);

            // MMF server family
            {
                std::unique_ptr<service::server> dev_serv = std::make_unique<mmf_dev_server>(sys);
                std::unique_ptr<service::server> aud_serv = std::make_unique<mmf_audio_server>(sys,
                    reinterpret_cast<mmf_dev_server *>(dev_serv.get()));

                kernel_system *kern = sys->get_kernel_system();
                kern->add_custom_server(dev_serv);
                kern->add_custom_server(aud_serv);
            }

            epoc::initialize_system_properties(sys, cfg);
        }
        
        void init_services_post_bootup(system *sys) {
            epoc::sms::supply_sim_settings(sys);
        }
    }
}
