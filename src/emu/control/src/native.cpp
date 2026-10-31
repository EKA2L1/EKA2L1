// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later
#include <algorithm>
#include <atomic>
#include <cmath>
#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/language.h>
#include <common/log.h>
#include <common/path.h>
#include <config/config.h>
#include <control/native.h>
#include <drivers/audio/audio.h>
#include <drivers/graphics/graphics.h>
#include <drivers/input/common.h>
#include <filesystem>
#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <mutex>
#include <package/manager.h>
#include <services/applist/applist.h>
#include <services/window/screen.h>
#include <services/window/window.h>
#include <set>
#include <system/devices.h>
#include <system/epoc.h>
#include <system/installation/archive.h>
#include <system/installation/rpkg.h>
#include <utils/apacmd.h>
#include <vfs/vfs.h>
#include <zlib.h>

namespace eka2l1::control {
    namespace fs = std::filesystem;
    static void require(bool condition, const std::string &code, const std::string &message, int status = 422) {
        if (!condition)
            throw api_error(status, code, message);
    }
    static response png(const std::vector<std::uint8_t> &pixels, int width, int height, double scale = 1) {
        require(width > 0 && height > 0 && std::isfinite(scale) && scale > 0 && scale <= 8,
            "bad_request", "invalid screenshot size or scale", 400);
        const int w = std::max(1, int(std::round(width * scale))), h = std::max(1, int(std::round(height * scale)));
        require(std::uint64_t(w) * h <= 64 * 1024 * 1024, "too_large", "image exceeds 64 megapixels", 413);
        std::string raw((std::size_t(w) * 4 + 1) * h, 0);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const auto src = (std::size_t(std::min(height - 1, int(y / scale))) * width + std::min(width - 1, int(x / scale))) * 4;
                std::copy_n(pixels.data() + src, 4, raw.data() + std::size_t(y) * (w * 4 + 1) + 1 + x * 4);
            }
        auto put32 = [](std::string &out, std::uint32_t n) { for (int shift : {24,16,8,0}) out += char(n >> shift); };
        std::string out("\x89PNG\r\n\x1a\n", 8);
        auto chunk = [&](const char *type, const std::string &data) {
            put32(out, static_cast<std::uint32_t>(data.size()));
            auto start = out.size();
            out.append(type, 4);
            out += data;
            put32(out, crc32(0, reinterpret_cast<const Bytef *>(out.data() + start), static_cast<uInt>(data.size() + 4)));
        };
        std::string header;
        put32(header, w);
        put32(header, h);
        header.append("\x08\x06\0\0\0", 5);
        chunk("IHDR", header);
        uLongf size = compressBound(raw.size());
        std::string compressed(size, 0);
        require(compress2(reinterpret_cast<Bytef *>(compressed.data()), &size,
                    reinterpret_cast<const Bytef *>(raw.data()), raw.size(), Z_BEST_SPEED)
                == Z_OK,
            "encode_failed", "PNG compression failed");
        compressed.resize(size);
        chunk("IDAT", compressed);
        chunk("IEND", {});
        return { 200, "image/png", std::move(out), {} };
    }
    struct native_backend::impl : std::enable_shared_from_this<impl> {
        native_frontend frontend;
        std::atomic_flag handling = ATOMIC_FLAG_INIT;
        std::mutex session_mutex;
        json session{ { "state", "none" }, { "screenVisible", false } };
        std::uint64_t generation = 0;
        kernel::uid thread_id = 0;
        std::set<std::uint32_t> held_keys;
        std::set<int> held_pointers;
        json acceleration;
        std::string card_path;

        explicit impl(native_frontend frontend)
            : frontend(std::move(frontend)) {}
        system &sys() {
            auto *s = frontend.get_system();
            require(s != nullptr, "not_ready", "emulator has not started", 503);
            return *s;
        }
        window_server *windows() {
            auto *kern = sys().get_kernel_system();
            return kern ? reinterpret_cast<window_server *>(kern->get_by_name<service::server>(get_winserv_name_by_epocver(kern->get_epoc_version()))) : nullptr;
        }
        applist_server *apps() {
            auto *kern = sys().get_kernel_system();
            auto *server = kern ? reinterpret_cast<applist_server *>(kern->get_by_name<service::server>(get_app_list_server_name_by_epocver(kern->get_epoc_version()))) : nullptr;
            require(server != nullptr, "no_device", "no device is booted", 409);
            return server;
        }
        json devices() {
            json::array list;
            auto *manager = sys().get_device_manager();
            std::lock_guard<std::mutex> guard(manager->lock);
            int index = 0;
            for (const auto &d : manager->get_devices()) {
                list.push_back({ { "index", index }, { "code", d.firmware_code }, { "manufacturer", d.manufacturer },
                    { "model", d.model }, { "name", d.model }, { "current", index == manager->get_current_index() } });
                ++index;
            }
            return list;
        }
        json current_device() {
            auto list = devices();
            for (const auto &d : list.items())
                if (d["current"].boolean())
                    return d;
            return nullptr;
        }
        int device_index(const std::string &text) {
            auto list = devices();
            for (const auto &d : list.items())
                if (lower(d["code"].string()) == lower(text) || std::to_string(int(d["index"].number())) == text)
                    return int(d["index"].number());
            throw api_error(404, "not_found", "no device " + text);
        }
        json app_list(bool include_system) {
            json::array list;
            auto *server = apps();
            kernel_lock guard(sys().get_kernel_system());
            for (auto &reg : server->get_registerations()) {
                bool system_app = reg.land_drive == drive_z && reg.mandatory_info.uid < 0x10300000;
                if (reg.caps.is_hidden || (!include_system && system_app))
                    continue;
                list.push_back({ { "uid", hex(reg.mandatory_info.uid) }, { "name", common::ucs2_to_utf8(reg.mandatory_info.long_caption.to_std_string(nullptr)) }, { "system", system_app } });
            }
            return list;
        }
        json session_snapshot() {
            std::lock_guard<std::mutex> guard(session_mutex);
            return session;
        }
        json card() { return { { "mounted", !card_path.empty() }, { "name", card_path.empty() ? json(nullptr) : json(fs::path(card_path).filename().string()) } }; }
        json screen() {
            auto *server = windows();
            kernel_lock guard(sys().get_kernel_system());
            auto *scr = server ? server->get_current_focus_screen() : nullptr;
            if (!scr)
                return nullptr;
            std::lock_guard<std::mutex> screen_guard(scr->screen_mutex);
            json::array modes;
            for (int i = 0; i < scr->total_screen_mode(); ++i)
                modes.emplace_back(i);
            return { { "width", scr->current_mode().size.x }, { "height", scr->current_mode().size.y },
                { "mode", int(scr->crr_mode) }, { "current", int(scr->crr_mode) }, { "modes", modes } };
        }
        json status() {
            return { { "api", 1 }, { "started", true }, { "busy", false }, { "device", current_device() },
                { "card", card() }, { "session", session_snapshot() }, { "frames", frontend.frames() },
                { "documents", frontend.documents }, { "log", frontend.log }, { "screen", screen() } };
        }
        fs::path host_path(std::string raw) {
            std::replace(raw.begin(), raw.end(), '\\', '/');
            if (lower(raw).substr(0, 5) == "host:") {
                fs::path host = fs::u8path(raw.substr(5));
                require(host.is_absolute(), "bad_request", "host: requires an absolute path", 400);
                return host;
            }
            fs::path path = fs::u8path(raw);
#if defined(_WIN32)
            if (path.is_absolute() && (raw.size() < 2 || std::string("cCdDeE").find(raw[0]) == std::string::npos))
                return path;
#else
            if (path.is_absolute())
                return path;
#endif
            std::string root, rest;
            if (lower(raw).substr(0, 5) == "docs:") {
                root = frontend.documents;
                rest = raw.substr(5);
            } else if (raw.size() >= 2 && raw[1] == ':' && std::string("cCdDeE").find(raw[0]) != std::string::npos) {
                root = sys().get_device_drive_path(static_cast<drive_number>(std::tolower(raw[0]) - 'a'));
                rest = raw.substr(2);
            } else
                throw api_error(400, "bad_request", "path must be docs:/..., C:/..., D:/..., E:/... or an absolute host path");
            require(!root.empty(), "no_device", "device drive unavailable", 409);
            fs::path resolved(root);
            for (const auto &part : fs::path(rest).relative_path()) {
                require(part != "..", "bad_request", ".. is not allowed", 400);
                if (part == ".")
                    continue;
                auto next = resolved / part;
                if (!fs::exists(next) && fs::is_directory(resolved)) {
                    for (const auto &entry : fs::directory_iterator(resolved))
                        if (lower(entry.path().filename().string()) == lower(part.string())) {
                            next = entry.path();
                            break;
                        }
                }
                resolved = next;
            }
            return resolved;
        }
        json input_state() {
            json::array keys, pointers;
            for (auto key : held_keys)
                keys.emplace_back(hex(key));
            for (auto id : held_pointers)
                pointers.emplace_back(id);
            return { { "heldKeys", keys }, { "heldPointers", pointers }, { "acceleration", acceleration } };
        }
        void key(std::uint32_t scan, bool down) {
            auto *server = windows();
            require(server, "no_device", "no device is booted", 409);
            drivers::input_event event{};
            event.type_ = drivers::input_event_type::key_raw;
            event.key_.code_ = scan;
            event.key_.state_ = down ? drivers::key_state::pressed : drivers::key_state::released;
            server->queue_input_from_driver(event);
            if (down)
                held_keys.insert(scan);
            else
                held_keys.erase(scan);
        }
        void touch(const json &body) {
            auto *server = windows();
            require(server, "no_device", "no device is booted", 409);
            double x = required_number(body, "x"), y = required_number(body, "y");
            auto space = body["space"].string("guest");
            if (space == "normalized") {
                auto s = screen();
                x *= s["width"].number();
                y *= s["height"].number();
            }
            require(space == "guest" || space == "normalized", "unsupported", "native touch supports guest and normalized coordinates", 400);
            require(std::isfinite(x) && std::isfinite(y) && x >= INT32_MIN && x <= INT32_MAX && y >= INT32_MIN && y <= INT32_MAX,
                "bad_request", "touch coordinates out of range", 400);
            auto id = body["id"].number();
            require(id >= 0 && id <= 255 && id == std::floor(id), "bad_request", "pointer id must be 0..255", 400);
            auto action = body["action"].string();
            drivers::input_event event{};
            event.type_ = drivers::input_event_type::touch;
            event.mouse_.pos_x_ = int(x);
            event.mouse_.pos_y_ = int(y);
            event.mouse_.pos_z_ = 1;
            event.mouse_.mouse_id = 0xEC00000 + unsigned(id);
            event.mouse_.button_ = drivers::mouse_button_left;
            event.mouse_.action_ = action == "down" ? drivers::mouse_action_press : action == "up" ? drivers::mouse_action_release
                                                                                                   : drivers::mouse_action_repeat;
            event.mouse_.raw_screen_pos_ = true;
            server->queue_input_from_driver(event);
            if (action == "up")
                held_pointers.erase(int(id));
            else
                held_pointers.insert(int(id));
        }
        void release() {
            auto keys = held_keys;
            for (auto scan : keys)
                key(scan, false);
            auto pointers = held_pointers;
            for (auto id : pointers)
                touch({ { "x", 0 }, { "y", 0 }, { "id", id }, { "action", "up" } });
            if (frontend.acceleration)
                frontend.acceleration(nullptr);
            acceleration = nullptr;
        }
        json language_snapshot() {
            json::array languages;
            auto *d = sys().get_device_manager()->get_current();
            if (d)
                for (auto code : d->languages)
                    languages.push_back({ { "code", code }, { "name", common::get_language_name_by_code(code) } });
            return { { "current", sys().get_config()->language }, { "available", languages } };
        }
        json config_snapshot() {
            auto &c = *sys().get_config();
            return { { "audioMasterVolume", c.audio_master_volume }, { "cpuBackend", c.cpu_backend }, { "language", c.language },
                { "cpuLoadSave", c.cpu_load_save }, { "enableHwGles1", c.enable_hw_gles1 }, { "nearestNeighborFiltering", c.nearest_neighbor_filtering },
                { "integerScaling", c.integer_scaling }, { "rtosLevel", c.rtos_level }, { "imei", c.imei }, { "mmcId", c.mmc_id },
                { "extensiveLogging", c.extensive_logging }, { "deviceDisplayName", c.device_display_name }, { "backgroundImage", c.background_image },
                { "btnetDiscoveryMode", c.btnet_discovery_mode }, { "btnetListenPort", c.internet_bluetooth_port }, { "btnetPassword", c.btnet_password }, { "btCentralServerUrl", c.bt_central_server_url },
                { "hideSystemApps", c.hide_system_apps }, { "logFilter", c.log_filter }, { "screenBufferSync", c.screen_buffer_sync_string } };
        }
        response route(const request &req) {
            auto p = req.path;
            require(!p.empty() && p[0] == "v1", "not_found", "unknown API version", 404);
            if (p == std::vector<std::string>{ "v1", "capabilities" } && req.method == "GET")
                return response::from_json({ { "platform", frontend.platform }, { "hostScreenshot", false }, { "accelerometer", bool(frontend.acceleration) }, { "touchSpaces", json::array{ "guest", "normalized" } } });
            if (req.method == "RESOLVE" && p == std::vector<std::string>{ "v1", "files" }) {
                const auto raw = req.param("path");
                require(!raw.empty(), "bad_request", "path query parameter required", 400);
                return response::from_json({ { "hostPath", host_path(raw).string() } });
            }
            if (req.method == "RESOLVE" && p == std::vector<std::string>{ "v1", "log" })
                return response::from_json({ { "hostPath", frontend.log } });
            auto body = req.data();
            if (p == std::vector<std::string>{ "v1", "status" } && req.method == "GET")
                return response::from_json(status());
            if (p == std::vector<std::string>{ "v1", "devices" } && req.method == "GET")
                return response::from_json({ { "devices", devices() }, { "current", current_device() } });
            if (p.size() >= 2 && p[1] == "devices") {
                auto *manager = sys().get_device_manager();
                if (p.size() == 2 && req.method == "POST") {
                    const auto storage = sys().get_config()->storage;
                    const auto roms = add_path(storage, "roms/"), drives = add_path(storage, "drives/z/");
                    const bool isolate = body["isolateDrives"].boolean();
                    auto result = body.contains("archive") ? loader::install_archive(manager, required_string(body, "archive"), roms, drives, isolate, [&req](std::size_t done, std::size_t total) { if (req.progress && total) req.progress(double(done) / total); }, nullptr) : loader::install_rom_with_optional_rpkg(manager, required_string(body, "rom"), body["rpkg"].string(), roms, drives, isolate, [&req](std::size_t done, std::size_t total) { if (req.progress && total) req.progress(double(done) / total); }, nullptr);
                    require(result == device_installation_none, "install_failed", "device installation failed: " + std::to_string(int(result)));
                    if (body["boot"].boolean(true))
                        require(frontend.boot(int(manager->total()) - 1), "boot_failed", "boot failed");
                    frontend.changed(native_change::devices);
                    return response::from_json({ { "devices", devices() }, { "current", current_device() } });
                }
                if (p.size() == 3 && p[2] == "rescan" && req.method == "POST") {
                    sys().rescan_devices(drive_z);
                    frontend.changed(native_change::devices);
                    return response::from_json({ { "devices", devices() }, { "current", current_device() } });
                }
                if (p.size() >= 3) {
                    int index = device_index(p[2]);
                    if (p.size() == 4 && p[3] == "boot" && req.method == "POST") {
                        release();
                        require(frontend.boot(index), "boot_failed", "boot failed");
                        frontend.changed(native_change::devices);
                        return response::from_json(current_device());
                    }
                    if (p.size() == 3 && req.method == "PATCH") {
                        manager->get_devices()[index].model = required_string(body, "name");
                        manager->save_devices();
                        frontend.changed(native_change::devices);
                        return response::from_json(devices().items()[index]);
                    }
                    if (p.size() == 3 && req.method == "DELETE") {
                        require(index != manager->get_current_index(), "busy", "boot another device before deleting the active device", 409);
                        auto code = manager->get_devices()[index].firmware_code;
                        require(manager->delete_device(code), "delete_failed", "device deletion failed");
                        delete_device_storage(sys().get_config()->storage, code);
                        frontend.changed(native_change::devices);
                        return response::from_json({ { "devices", devices() }, { "current", current_device() } });
                    }
                }
            }
            if (p == std::vector<std::string>{ "v1", "apps" } && req.method == "GET")
                return response::from_json({ { "apps", app_list(req.param("system") == "1") } });
            if (p == std::vector<std::string>{ "v1", "apps" } && req.method == "POST") {
                auto before = app_list(true);
                json::array results;
                auto paths = body.contains("paths") ? body["paths"] : json(json::array{ required_string(body, "path") });
                require(paths.is_array(), "bad_request", "paths must be an array", 400);
                for (auto &value : paths.items()) {
                    auto path = value.string();
                    require(fs::exists(fs::u8path(path)), "not_found", "install path not found", 404);
                    auto ext = lower(fs::u8path(path).extension().string());
                    auto type = body["type"].string(ext == ".sis" || ext == ".sisx" ? "sis" : ext == ".n-gage" ? "ngage2"
                                                                                                               : "ngage");
                    bool ok = false;
                    if (type == "sis")
                        ok = sys().install_package(common::utf8_to_ucs2(path), sys().is_s80_device_active() ? drive_d : drive_e) == package::installation_result_success;
                    else if (type == "ngage")
                        ok = int(sys().install_ngage_game_card(path, nullptr)) == 0;
                    else if (type == "ngage2") {
                        auto dir = fs::u8path(sys().get_device_drive_path(drive_e)) / "n-gage";
                        fs::create_directories(dir);
                        fs::copy_file(fs::u8path(path), dir / fs::u8path(path).filename(), fs::copy_options::overwrite_existing);
                        ok = true;
                    } else
                        throw api_error(400, "bad_request", "unknown install type");
                    require(ok, "install_failed", "app installation failed");
                    results.push_back({ { "path", path }, { "type", type }, { "ok", ok } });
                }
                apps()->rescan_registries(sys().get_io_system());
                frontend.changed(native_change::applications);
                json::array added;
                auto after = app_list(true);
                for (const auto &app : after.items()) {
                    bool exists = false;
                    for (const auto &old : before.items())
                        if (old["uid"].string() == app["uid"].string())
                            exists = true;
                    if (!exists)
                        added.push_back(app);
                }
                return response::from_json({ { "results", results }, { "added", added } });
            }
            if (p.size() >= 3 && p[1] == "apps") {
                const auto uid = parse_uid(p[2]);
                if (p.size() == 4 && p[3] == "launch" && req.method == "POST") {
                    auto *server = apps();
                    require(server->get_registration(uid), "not_found", "app is not installed", 404);
                    release();
                    auto previous = session_snapshot()["state"].string();
                    if (previous != "none" && previous != "closed")
                        frontend.close();
                    {
                        std::lock_guard<std::mutex> guard(session_mutex);
                        session = { { "state", "launching" }, { "uid", hex(uid) }, { "screenVisible", false } };
                    }
                    if (!frontend.launch(uid)) {
                        std::lock_guard<std::mutex> guard(session_mutex);
                        session["state"] = "launch_failed";
                        throw api_error(422, "launch_failed", "app launch failed");
                    }
                    return response::from_json(session_snapshot());
                }
                if (p.size() == 4 && p[3] == "icon" && req.method == "GET") {
                    auto *reg = apps()->get_registration(uid);
                    require(reg, "not_found", "app is not installed", 404);
                    auto icon = apps()->get_host_icon(*reg, add_path(sys().get_config()->storage, "cache/icons"));
                    require(bool(icon), "not_found", "no app icon", 404);
                    return png(icon->pixels, icon->size.x, icon->size.y);
                }
                if (p.size() == 3 && req.method == "DELETE") {
                    auto *pkg = sys().get_packages()->package_owning_executable(uid);
                    require(pkg, "not_found", "no installed package for app", 404);
                    require(sys().get_packages()->uninstall_package(*pkg), "uninstall_failed", "uninstall failed");
                    apps()->rescan_registries(sys().get_io_system());
                    frontend.changed(native_change::applications);
                    return response::from_json({ { "uninstalled", hex(uid) } });
                }
            }
            if (p == std::vector<std::string>{ "v1", "session" }) {
                if (req.method == "GET")
                    return response::from_json(session_snapshot());
                if (req.method == "DELETE") {
                    release();
                    frontend.close();
                    return response::from_json(session_snapshot());
                }
            }
            if (p.size() == 3 && p[1] == "session" && (p[2] == "screen-mode" || p[2] == "frame-limit")) {
                auto *server = windows();
                auto *scr = server ? server->get_current_focus_screen() : nullptr;
                require(scr, "no_screen", "no emulator screen", 409);
                if (p[2] == "screen-mode") {
                    if (req.method == "PUT") {
                        const int mode = int(required_number(body, "mode"));
                        require(mode >= 0 && mode < scr->total_screen_mode(), "bad_request", "invalid screen mode", 400);
                        kernel_lock guard(sys().get_kernel_system());
                        scr->set_screen_mode(server, sys().get_graphics_driver(), mode);
                    } else
                        require(req.method == "GET", "not_found", "unknown operation", 404);
                    return response::from_json(screen());
                }
                if (req.method == "PUT") {
                    auto limit = required_number(body, "limit");
                    require(limit >= 0 && limit <= 240 && limit == std::floor(limit), "bad_request", "limit must be 0..240", 400);
                    kernel_lock guard(sys().get_kernel_system());
                    scr->refresh_rate = limit == 0 ? 240 : int(limit);
                } else
                    require(req.method == "GET", "not_found", "unknown operation", 404);
                return response::from_json({ { "limit", scr->refresh_rate > 60 ? 0 : int(scr->refresh_rate) } });
            }
            if (p == std::vector<std::string>{ "v1", "card" }) {
                if (req.method == "PUT") {
                    auto path = required_string(body, "path");
                    if (fs::is_directory(fs::u8path(path)))
                        sys().mount(drive_e, drive_media::physical, path, io_attrib_removeable | io_attrib_write_protected);
                    else
                        require(sys().mount_game_zip(drive_e, drive_media::physical, path, io_attrib_removeable | io_attrib_write_protected) == zip_mount_error_none, "mount_failed", "card mount failed");
                    card_path = path;
                } else if (req.method == "DELETE") {
                    sys().mount_device_drive(drive_e);
                    card_path.clear();
                } else
                    require(req.method == "GET", "not_found", "unknown operation", 404);
                if (req.method != "GET") {
                    apps()->rescan_registries(sys().get_io_system());
                    frontend.changed(native_change::applications);
                }
                return response::from_json(card());
            }
            if (p.size() == 3 && p[1] == "input") {
                if (p[2] == "key" && req.method == "POST") {
                    auto keys = body.contains("keys") ? body["keys"] : json(json::array{ body["key"] });
                    require(keys.is_array(), "bad_request", "keys must be an array", 400);
                    for (const auto &scan : keys.items())
                        key(std::uint32_t(scan.number()), body["action"].string() == "down");
                } else if (p[2] == "touch" && req.method == "POST")
                    touch(body);
                else if (p[2] == "release" && req.method == "POST")
                    release();
                else if (p[2] == "accelerometer") {
                    if (req.method == "PUT") {
                        require(bool(frontend.acceleration), "unsupported", "accelerometer override unavailable", 501);
                        auto unit = body["unit"].string("ms2");
                        require(unit == "g" || unit == "ms2", "bad_request", "unit must be g or ms2", 400);
                        double values[3];
                        int i = 0;
                        for (const char *axis : { "x", "y", "z" })
                            values[i++] = required_number(body, axis) * (unit == "g" ? 9.80665 : 1);
                        frontend.acceleration(values);
                        acceleration = { { "x", values[0] }, { "y", values[1] }, { "z", values[2] } };
                    } else if (req.method == "DELETE") {
                        if (frontend.acceleration)
                            frontend.acceleration(nullptr);
                        acceleration = nullptr;
                    } else
                        require(req.method == "GET", "not_found", "unknown operation", 404);
                } else
                    throw api_error(404, "not_found", "unknown input operation");
                return response::from_json(input_state());
            }
            if (p == std::vector<std::string>{ "v1", "screenshot" } && req.method == "GET") {
                require(req.param("region", "guest") == "guest", "unsupported", "host window screenshot unavailable", 501);
                require(frontend.can_capture() && session_snapshot()["screenVisible"].boolean(),
                    "no_screen", "no active emulator surface", 409);
                auto *server = windows();
                auto *scr = server ? server->get_current_focus_screen() : nullptr;
                require(scr && sys().get_graphics_driver(), "no_screen", "no screen", 409);
                auto size = scr->current_mode().size * scr->display_scale_factor;
                std::vector<std::uint8_t> pixels(std::size_t(size.x) * size.y * 4);
                require(drivers::read_bitmap(sys().get_graphics_driver(), scr->screen_texture, point(0, 0), size, 32, pixels.data()), "capture_failed", "screen readback failed");
                double scale = 1;
                try {
                    scale = std::stod(req.param("scale", "1"));
                } catch (...) {
                    throw api_error(400, "bad_request", "invalid scale");
                }
                auto res = png(pixels, size.x, size.y, scale / scr->display_scale_factor);
                res.headers["X-Frame-Count"] = std::to_string(frontend.frames());
                return res;
            }
            if (p == std::vector<std::string>{ "v1", "language" }) {
                if (req.method == "PUT") {
                    auto code = int(required_number(body, "code"));
                    auto *d = sys().get_device_manager()->get_current();
                    require(d && std::find(d->languages.begin(), d->languages.end(), code) != d->languages.end(), "bad_request", "language not shipped by device", 400);
                    sys().get_config()->language = code;
                    sys().set_system_language(static_cast<::language>(code));
                    sys().get_config()->serialize();
                    apps()->rescan_registries(sys().get_io_system());
                    frontend.changed(native_change::applications);
                } else
                    require(req.method == "GET", "not_found", "unknown operation", 404);
                return response::from_json(language_snapshot());
            }
            if (p == std::vector<std::string>{ "v1", "config" }) {
                if (req.method == "PATCH") {
                    auto &c = *sys().get_config();
                    std::vector<std::function<void()>> apply;
                    std::map<std::string, bool *> booleans{ { "cpuLoadSave", &c.cpu_load_save }, { "enableHwGles1", &c.enable_hw_gles1 },
                        { "nearestNeighborFiltering", &c.nearest_neighbor_filtering }, { "integerScaling", &c.integer_scaling },
                        { "hideSystemApps", &c.hide_system_apps }, { "extensiveLogging", &c.extensive_logging } };
                    std::map<std::string, std::string *> strings{ { "imei", &c.imei }, { "mmcId", &c.mmc_id }, { "rtosLevel", &c.rtos_level },
                        { "cpuBackend", &c.cpu_backend }, { "deviceDisplayName", &c.device_display_name }, { "logFilter", &c.log_filter },
                        { "backgroundImage", &c.background_image }, { "btnetPassword", &c.btnet_password }, { "btCentralServerUrl", &c.bt_central_server_url } };
                    for (const auto &[key, value] : body.members()) {
                        if (booleans.count(key)) {
                            require(value.is_boolean(), "bad_request", key + " must be boolean", 400);
                            auto *target = booleans.at(key);
                            const bool v = value.boolean();
                            apply.emplace_back([target, v] { *target = v; });
                        } else if (strings.count(key)) {
                            require(value.is_string(), "bad_request", key + " must be a string", 400);
                            auto *target = strings.at(key);
                            auto v = value.string();
                            if (key == "cpuBackend")
                                require(v == "dynarmic" || v == "dyncom" || v == "r12l1", "bad_request", "invalid CPU backend", 400);
                            apply.emplace_back([target, v] { *target = v; });
                        } else if (key == "audioMasterVolume" || key == "btnetListenPort" || key == "btnetDiscoveryMode") {
                            double v = required_number(body, key);
                            require(v == std::floor(v) && v >= 0 && v <= 65535, "bad_request", "invalid numeric config value", 400);
                            if (key == "audioMasterVolume")
                                apply.emplace_back([&c, v] { c.audio_master_volume = std::clamp(int(v), 0, 100); });
                            else if (key == "btnetListenPort")
                                apply.emplace_back([&c, v] { c.internet_bluetooth_port = std::max(1, int(v)); });
                            else
                                apply.emplace_back([&c, v] { c.btnet_discovery_mode = std::uint32_t(v); });
                        } else
                            throw api_error(400, "bad_request", "unsupported config key: " + key);
                    }
                    for (const auto &change : apply)
                        change();
                    if (auto *audio = sys().get_audio_driver())
                        audio->master_volume(c.audio_master_volume);
                    if (log::filterings)
                        log::filterings->parse_filter_string(c.log_filter);
                    c.serialize();
                    if (body.contains("hideSystemApps") || body.contains("deviceDisplayName") || body.contains("backgroundImage"))
                        frontend.changed(native_change::configuration);
                } else
                    require(req.method == "GET", "not_found", "unknown operation", 404);
                return response::from_json(config_snapshot());
            }
            if (p.size() == 3 && p[1] == "emulator" && (p[2] == "pause" || p[2] == "resume") && req.method == "POST") {
                const bool paused = p[2] == "pause";
                frontend.pause(paused);
                return response::from_json({ { "paused", paused } });
            }
            throw api_error(404, "not_found", "unknown endpoint; see GET /v1");
        }
    };
    native_backend::native_backend(native_frontend frontend)
        : impl_(std::make_shared<impl>(std::move(frontend))) {}
    native_backend::~native_backend() = default;
    response native_backend::handle(const request &req) {
        if (impl_->handling.test_and_set())
            return response::error(409, "busy", "frontend operation is in progress");
        struct finish {
            std::atomic_flag &flag;
            ~finish() { flag.clear(); }
        } guard{ impl_->handling };
        try {
            return impl_->route(req);
        } catch (const api_error &e) {
            return response::error(e.status, e.code, e.what());
        } catch (const std::exception &e) {
            return response::error(500, "internal", e.what());
        }
    }
    bool native_backend::launch_app(std::uint32_t uid, std::function<void(kernel::process *)> exited) {
        auto self = impl_;
        auto *server = self->apps();
        auto *reg = server->get_registration(uid);
        if (!reg)
            return false;
        std::uint64_t generation;
        {
            std::lock_guard<std::mutex> guard(self->session_mutex);
            generation = ++self->generation;
            self->session = { { "state", "launching" }, { "uid", hex(uid) }, { "name", common::ucs2_to_utf8(reg->mandatory_info.long_caption.to_std_string(nullptr)) }, { "screenVisible", true } };
        }
        kernel_lock guard(self->sys().get_kernel_system());
        epoc::apa::command_line command;
        std::weak_ptr<impl> weak = self;
        bool ok = server->launch_app(*reg, command, &self->thread_id, [weak, generation, exited](kernel::process *process) {
            if (auto self = weak.lock()) {
                std::lock_guard<std::mutex> lock(self->session_mutex);
                if (generation != self->generation)
                    return;
                const bool panic = process->get_exit_type() == kernel::entity_exit_type::panic;
                self->session["state"] = panic ? "crashed" : "exited";
                self->session["screenVisible"] = false;
                self->session["lastExit"] = { { "uid", hex(process->get_uid()) }, { "fatal", panic ? json(common::ucs2_to_utf8(process->get_exit_category()) + "/" + std::to_string(process->get_exit_reason())) : json(nullptr) } };
            }
            if (exited)
                exited(process);
        });
        {
            std::lock_guard<std::mutex> lock(self->session_mutex);
            self->session["state"] = ok ? "running" : "launch_failed";
        }
        return ok;
    }
    void native_backend::reset_session() {
        impl_->release();
        std::lock_guard<std::mutex> guard(impl_->session_mutex);
        ++impl_->generation;
        impl_->thread_id = 0;
        impl_->session["state"] = "closed";
        impl_->session["screenVisible"] = false;
    }
}
