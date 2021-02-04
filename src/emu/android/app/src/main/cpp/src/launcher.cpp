/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <android/launcher.h>
#include <android/state.h>

#include <system/devices.h>
#include <package/manager.h>

#include <system/installation/raw_dump.h>
#include <system/installation/rpkg.h>
#include <common/path.h>
#include <common/fileutils.h>
#include <utils/locale.h>
#include <utils/system.h>

namespace eka2l1::android {

    launcher::launcher(eka2l1::system *sys)
        :sys(sys)
        , conf(sys->get_config())
        , kern(sys->get_kernel_system())
        , alserv(nullptr)
    {
        if (kern) {
            alserv = reinterpret_cast<eka2l1::applist_server *>(kern->get_by_name<service::server>(get_app_list_server_name_by_epocver(
                kern->get_epoc_version())));
            winserv = reinterpret_cast<eka2l1::window_server *>(kern->get_by_name<service::server>(get_winserv_name_by_epocver(
                kern->get_epoc_version())));
        }
    }

    std::vector<std::string> launcher::get_apps() {
        std::vector<apa_app_registry> &registerations = alserv->get_registerations();
        std::vector<std::string> info;
        for (auto &reg : registerations) {
            std::string name = common::ucs2_to_utf8(reg.mandatory_info.long_caption.to_std_string(nullptr));
            std::string uid = std::to_string(reg.mandatory_info.uid);
            info.push_back(uid);
            info.push_back(name);
        }
        return info;
    }

    void launcher::launch_app(std::uint32_t uid) {
        apa_app_registry *reg = alserv->get_registration(uid);

        epoc::apa::command_line cmdline;
        cmdline.launch_cmd_ = epoc::apa::command_create;
        kern->lock();
        alserv->launch_app(*reg, cmdline, nullptr);
        kern->unlock();
    }

    bool launcher::install_app(std::string &path) {
        std::u16string upath = common::utf8_to_ucs2(path);

        return sys->install_package(upath,drive_number::drive_e);
    }

    std::vector<std::string> launcher::get_devices() {
        device_manager *dvc_mngr = sys->get_device_manager();
        auto &dvcs = dvc_mngr->get_devices();
        std::vector<std::string> info;
        for (auto &device : dvcs) {
            std::string name = device.model;
            info.push_back(name);
        }
        return info;
    }

    void launcher::set_language_to_property(const language new_one) {
        property_ptr lang_prop = kern->get_prop(epoc::SYS_CATEGORY, epoc::LOCALE_LANG_KEY);
        auto current_lang = lang_prop->get_pkg<epoc::locale_language>();

        if (!current_lang) {
            return;
        }

        current_lang->language = static_cast<epoc::language>(new_one);
        lang_prop->set<epoc::locale_language>(current_lang.value());
    }

    void launcher::set_language_current(const language lang) {
        conf->language = static_cast<int>(lang);
        sys->set_system_language(lang);
        set_language_to_property(lang);
    }

    void launcher::set_current_device(std::uint32_t id) {
        device_manager *dvc_mngr = sys->get_device_manager();
        auto &dvcs = dvc_mngr->get_devices();

        if (conf->device != id) {
            // Check if the language currently in config exists in the new device
            if (std::find(dvcs[id].languages.begin(), dvcs[id].languages.end(), conf->language) == dvcs[id].languages.end()) {
                set_language_current(static_cast<language>(dvcs[id].default_language_code));
            }
        }

        conf->device = id;
        conf->serialize();
        dvc_mngr->set_current(id);
    }

    std::uint32_t launcher::get_current_device() {
        return conf->device;
    }

    bool launcher::install_device(std::string &rpkg_path, std::string &rom_path, bool install_rpkg) {
        std::string firmware_code;
        std::atomic<int> progress_tracker;
        device_manager *dvc_mngr = sys->get_device_manager();
        bool result;

        std::string root_z_path = add_path(conf->storage, "drives/z/");
        if (install_rpkg) {
            result = eka2l1::loader::install_rpkg(dvc_mngr, rpkg_path, root_z_path, firmware_code, progress_tracker);
        } else {
            result = eka2l1::loader::install_raw_dump(dvc_mngr, rpkg_path + eka2l1::get_separator(), root_z_path, firmware_code, progress_tracker);
        }

        if (!result) {
            return false;
        }

        dvc_mngr->save_devices();
        const std::string rom_directory = add_path(conf->storage, add_path("roms", firmware_code + "\\"));

        eka2l1::create_directories(rom_directory);
        common::copy_file(rom_path, add_path(rom_directory, "SYM.ROM"), true);
        return true;
    }

    std::vector<std::string> launcher::get_packages() {
        manager::packages *manager = sys->get_packages();
        std::vector<std::string> info;
        for (int i = 0; i < manager->package_count(); i++) {
            const manager::package_info *pkg = manager->package(i);
            std::string name = common::ucs2_to_utf8(pkg->name);
            std::string uid = std::to_string(pkg->id);
            info.push_back(uid);
            info.push_back(name);
        }
        return info;
    }

    void launcher::uninstall_package(std::uint32_t uid) {
        manager::packages *manager = sys->get_packages();
        manager->uninstall_package(uid);
    }

    void launcher::mount_sd_card(std::string &path) {
        std::u16string upath = common::utf8_to_ucs2(path);

        io_system *io = sys->get_io_system();
        io->unmount(drive_e);
        io->mount_physical_path(drive_e, drive_media::physical,
                io_attrib_removeable, upath);
    }

    void launcher::load_config() {
        conf->deserialize();
    }

    void launcher::set_language(std::uint32_t language_id) {
        set_language_current(static_cast<language>(language_id));
    }

    void launcher::set_rtos_level(std::uint32_t level) {
        kern->get_ntimer()->set_realtime_level(static_cast<realtime_level>(level));
    }

    void launcher::update_app_setting(std::uint32_t uid) {
        kern->get_app_settings()->update_setting(uid);
    }

    static void advance_dsa_pos_around_origin(eka2l1::rect &origin_normal_rect, const int rotation) {
        switch (rotation) {
        case 90:
            origin_normal_rect.top.x += origin_normal_rect.size.x;
            break;

        case 180:
            origin_normal_rect.top.x += origin_normal_rect.size.x;
            break;

        case 270:
            origin_normal_rect.top.y += origin_normal_rect.size.y;
            break;

        default:
            break;
        }
    }

    void launcher::draw(drivers::graphics_command_list_builder *builder, std::uint32_t window_width,
            std::uint32_t window_height) {
        epoc::screen *scr = winserv->get_screens();
        if (scr) {
            eka2l1::rect viewport;
            eka2l1::rect src;
            eka2l1::rect dest;

            drivers::filter_option filter = conf->nearest_neighbor_filtering ? drivers::filter_option::nearest :
                drivers::filter_option::linear;

            eka2l1::vec2 swapchain_size(window_width, window_height);
            viewport.size = swapchain_size;
            builder->set_swapchain_size(swapchain_size);

            builder->backup_state();

            builder->clear({ 0xFF, 0xD0, 0xD0, 0xD0 }, drivers::draw_buffer_bit_color_buffer);
            builder->set_cull_mode(false);
            builder->set_depth(false);
            //builder->set_clipping(true);
            builder->set_viewport(viewport);

            for (std::uint32_t i = 0; scr && scr->screen_texture; i++, scr = scr->next) {
                scr->screen_mutex.lock();
                auto &crr_mode = scr->current_mode();

                eka2l1::vec2 size = crr_mode.size;
                src.size = size;

                float mult = (float)(window_width) / size.x;
                float width = size.x * mult;
                float height = size.y * mult;
                std::uint32_t x = 0;
                std::uint32_t y = 0;
                if (height > swapchain_size.y) {
                    height = swapchain_size.y;
                    mult = height / size.y;
                    width = size.x * mult;
                    x = (swapchain_size.x - width) / 2;
                }
                scr->scale_x = mult;
                scr->scale_y = mult;
                scr->absolute_pos.x = static_cast<int>(x);
                scr->absolute_pos.y = static_cast<int>(y);

                dest.top = eka2l1::vec2(x, y);
                dest.size = eka2l1::vec2(width, height);

                builder->set_texture_filter(scr->screen_texture, filter, filter);
                builder->draw_bitmap(scr->screen_texture, 0, dest, src, eka2l1::vec2(0, 0), 0.0f,
                    drivers::bitmap_draw_flag_no_flip);
                if (scr->dsa_texture) {
                    builder->set_texture_filter(scr->dsa_texture, filter, filter);
                    advance_dsa_pos_around_origin(dest, crr_mode.rotation);

                    // Rotate back to original size
                    if (crr_mode.rotation % 180 != 0) {
                        std::swap(dest.size.x, dest.size.y);
                        std::swap(src.size.x, src.size.y);
                    }

                    builder->draw_bitmap(scr->dsa_texture, 0, dest, src, eka2l1::vec2(0, 0),
                        static_cast<float>(crr_mode.rotation), drivers::bitmap_draw_flag_no_flip);
                }

                scr->screen_mutex.unlock();
            }
            builder->load_backup_state();
        }
    }
}
