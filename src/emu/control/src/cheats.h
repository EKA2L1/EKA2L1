// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <control/api.h>
#include <kernel/common.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace eka2l1 {
    class system;
    namespace kernel {
        class process;
    }
}

namespace eka2l1::control {
    // Memory search and freeze in the foreground app, for the /v1/cheats endpoints. Everything runs
    // on the emulator thread, between two CPU slices (system::run_between_slices).
    class cheat_engine {
    public:
        enum class compare {
            equal,
            different,
            changed,
            unchanged,
            increased,
            decreased,
            not_increased,
            not_decreased
        };

        struct code {
            std::uint32_t address;
            int width;
            std::uint32_t value;
            bool frozen;
            // frozen when the game last ran, armed again on request
            bool was_frozen;
            std::string name;
        };

    private:
        struct page {
            std::uint32_t address;
            std::vector<std::uint8_t> bytes;
        };

        std::filesystem::path folder_;

        std::uint32_t app_uid_ = 0;
        std::string app_name_;
        kernel::uid process_id_ = 0;

        int width_ = 0;
        bool searching_ = false;
        // a search from an unknown value starts from a copy of the memory
        std::vector<page> snapshot_;
        std::vector<std::uint32_t> addresses_;
        std::vector<std::uint32_t> values_;

        std::vector<code> codes_;

        kernel::process *process(system &sys);
        void attach(system &sys);
        std::filesystem::path file() const;
        void load();
        void save() const;
        void reset_search();
        std::uint64_t count() const;

        std::vector<std::pair<std::uint32_t, std::uint32_t>> ranges(system &sys, kernel::process *pr);
        std::optional<std::uint32_t> read(system &sys, kernel::process *pr, std::uint32_t address, int width);
        bool write(system &sys, kernel::process *pr, std::uint32_t address, int width, std::uint32_t value);

        std::uint64_t start(system &sys, int width, std::optional<std::uint32_t> value);
        std::uint64_t next(system &sys, compare how, std::uint32_t value);

        json state() const;
        json code_list() const;

    public:
        explicit cheat_engine(std::filesystem::path folder);

        response handle(system &sys, const request &req);

        // Writes the frozen values again, after every CPU slice
        void freeze(system &sys);
    };
}
