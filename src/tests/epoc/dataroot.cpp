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

#include <catch2/catch.hpp>

#include <common/buffer.h>
#include <common/fileutils.h>
#include <common/path.h>
#include <config/app_settings.h>
#include <config/config.h>
#include <config/panic_blacklist.h>
#include <drivers/audio/audio.h>
#include <system/devices.h>
#include <utils/panic.h>

#include <string>

using namespace eka2l1;

namespace {
    // A throwaway data folder, made the process-wide data root for the test's
    // lifetime. Everything the code under test writes must land inside it.
    //
    // Its name is not ASCII, as a user's folder often is: on Windows a path like
    // this only opens when it reaches the host as a wide string.
    struct data_root_env {
        std::string parent;
        std::string root;

        explicit data_root_env(const std::string &name) {
            std::string current_dir;
            common::get_current_directory(current_dir);

            // "dani" ("data") in Cyrillic, written as its UTF-8 bytes so the source
            // encoding does not matter.
            parent = add_path(current_dir, std::string("datarootenv_\xD0\xB4\xD0\xB0\xD0\xBD\xD1\x96") + eka2l1::get_separator());
            root = add_path(parent, name + eka2l1::get_separator());
            common::delete_folder(root);
            common::create_directories(root);

            set_data_root(root);
        }

        ~data_root_env() {
            set_data_root("");
            common::delete_folder(root);

            // Empty again now, so it goes too and nothing is left behind.
            common::remove(parent);
        }

        bool has(const std::string &relative) const {
            return common::exists(add_path(root, relative));
        }

        std::string read(const std::string &relative) const {
            common::ro_std_file_stream stream(add_path(root, relative), true);
            std::string content(stream.size(), ' ');
            stream.read(content.data(), content.size());
            return content;
        }

        void write(const std::string &relative, const std::string &content) const {
            const std::string full = add_path(root, relative);
            common::create_directories(eka2l1::file_directory(full));

            common::wo_std_file_stream stream(full, true);
            stream.write(content.data(), content.size());
        }
    };
}

TEST_CASE("config_and_bindings_are_saved_under_the_data_root", "data_root") {
    data_root_env env("config_saved");

    config::state conf;
    conf.gdb_port = 24690;
    conf.serialize();

    REQUIRE(env.has("config.yml"));
    REQUIRE(env.has("bindings/default.yml"));

    config::state reloaded;
    reloaded.deserialize();

    REQUIRE(reloaded.gdb_port == 24690);
}

TEST_CASE("relative_storage_resolves_against_the_data_root", "data_root") {
    data_root_env env("storage_resolved");

    config::state conf;
    conf.storage = "data";
    REQUIRE(conf.storage_path() == env.root + "data");

    conf.storage = "/mnt/symbian/storage";
    REQUIRE(conf.storage_path() == "/mnt/symbian/storage");
}

TEST_CASE("relative_storage_is_saved_as_written", "data_root") {
    data_root_env env("storage_saved");

    // A copied data folder must keep pointing at its own drives, so the root
    // never leaks into config.yml.
    config::state conf;
    conf.storage = "data";
    conf.serialize(false);

    REQUIRE(env.has("config.yml"));
    REQUIRE(env.read("config.yml").find(env.root) == std::string::npos);

    config::state reloaded;
    reloaded.deserialize(false);

    REQUIRE(reloaded.storage == "data");
}

TEST_CASE("configured_midi_banks_are_looked_up_under_the_data_root", "data_root") {
    data_root_env env("configured_banks");

    env.write("resources/custom.hsb", "bank");
    env.write("resources/custom.sf2", "bank");

    config::state conf;
    conf.hsb_bank_path = "resources/custom.hsb";
    conf.sf2_bank_path = "resources/custom.sf2";
    conf.serialize(false);

    // Both banks are in the data folder, so both are kept.
    config::state reloaded;
    reloaded.deserialize(false);

    REQUIRE(reloaded.hsb_bank_path == "resources/custom.hsb");
    REQUIRE(reloaded.sf2_bank_path == "resources/custom.sf2");
}

TEST_CASE("missing_midi_banks_fall_back_to_the_shipped_ones", "data_root") {
    data_root_env env("missing_banks");

    env.write("resources/custom.hsb", "bank");

    config::state conf;
    conf.hsb_bank_path = "resources/custom.hsb";
    conf.sf2_bank_path = "resources/missing.sf2";
    conf.serialize(false);

    config::state reloaded;
    reloaded.deserialize(false);

    REQUIRE(reloaded.hsb_bank_path == "resources/custom.hsb");
    REQUIRE(reloaded.sf2_bank_path == "resources/defaultbank.sf2");

    // An empty path names no bank, even though it names the data folder itself.
    conf.hsb_bank_path = "";
    conf.sf2_bank_path = "";
    conf.serialize(false);

    reloaded.deserialize(false);

    REQUIRE(reloaded.hsb_bank_path == "resources/defaultbank.hsb");
    REQUIRE(reloaded.sf2_bank_path == "resources/defaultbank.sf2");
}

TEST_CASE("storage_path_is_unchanged_without_a_data_root", "data_root") {
    config::state conf;
    conf.storage = "data";

    REQUIRE(conf.storage_path() == "data");
}

TEST_CASE("devices_yml_is_saved_in_the_storage_under_the_data_root", "data_root") {
    data_root_env env("devices_saved");

    // Installing a device is what creates the storage folder.
    common::create_directories(add_path(env.root, "data"));

    config::state conf;
    conf.storage = "data";

    {
        device_manager manager(&conf);
        REQUIRE(manager.add_new_device("RM-409", "5320", "Nokia", epocver::epoc94, 0, false) == add_device_none);
        manager.save_devices();
    }

    REQUIRE(env.has("data/devices.yml"));

    device_manager reloaded(&conf);
    REQUIRE(reloaded.total() == 1);
}

TEST_CASE("app_compat_settings_live_under_the_data_root", "data_root") {
    data_root_env env("compat_saved");

    config::state conf;
    config::app_settings settings(&conf);

    REQUIRE(env.has("compat/"));

    config::app_setting setting;
    setting.fps = 30;
    REQUIRE(settings.add_or_replace_setting(0xE1234567, setting));

    REQUIRE(env.has("compat/E1234567.yml"));
}

TEST_CASE("panic_blacklist_is_read_from_the_data_root", "data_root") {
    data_root_env env("panic_blacklist");

    env.write("compat/panicBlackList.json",
        "{ \"game.exe\": { \"Main\": { \"category\": \"KERN-EXEC\", \"code\": 3 } } }");

    config::panic_blacklist blacklist;
    REQUIRE(blacklist.should_be_blocked("game.exe", "Main", "KERN-EXEC", 3));
}

TEST_CASE("panic_descriptions_are_read_from_the_data_root", "data_root") {
    {
        data_root_env env("panic_descriptions");

        env.write("panic.json", "{ \"Panic\": { \"KERN-EXEC\": { \"action\": \"script\" } } }");

        REQUIRE(epoc::init_panic_descriptions());
        REQUIRE_FALSE(epoc::is_panic_category_action_default("KERN-EXEC"));
    }

    // The descriptions are process-wide: load them again without the root, so later
    // tests do not see the ones from the deleted folder.
    epoc::init_panic_descriptions();
    REQUIRE(epoc::is_panic_category_action_default("KERN-EXEC"));
}

namespace {
    // Only the bank bookkeeping matters here; nothing is ever played.
    struct silent_audio_driver : public drivers::audio_driver {
        std::unique_ptr<drivers::audio_output_stream> new_output_stream(const std::uint32_t, const std::uint8_t,
            drivers::data_callback) override {
            return nullptr;
        }

        std::unique_ptr<drivers::audio_input_stream> new_input_stream(const std::uint32_t, const std::uint8_t,
            drivers::data_callback) override {
            return nullptr;
        }

        std::uint32_t native_sample_rate() override {
            return 44100;
        }
    };
}

TEST_CASE("shipped_midi_banks_resolve_against_the_data_root", "data_root") {
    data_root_env env("midi_banks");

    silent_audio_driver driver;
    std::string changed_to;
    driver.add_bank_change_callback([&](const drivers::midi_bank_type, const std::string &path) {
        changed_to = path;
    });

    driver.set_bank_path(drivers::MIDI_BANK_TYPE_SF2, "resources/defaultbank.sf2");

    REQUIRE(driver.get_bank_path(drivers::MIDI_BANK_TYPE_SF2) == env.root + "resources/defaultbank.sf2");
    REQUIRE(changed_to == env.root + "resources/defaultbank.sf2");

    // A bank the user picked elsewhere stays where it is.
    driver.set_bank_path(drivers::MIDI_BANK_TYPE_HSB, "/home/user/banks/my.hsb");
    REQUIRE(driver.get_bank_path(drivers::MIDI_BANK_TYPE_HSB) == "/home/user/banks/my.hsb");
}
