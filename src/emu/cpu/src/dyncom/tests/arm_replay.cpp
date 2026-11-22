// Copyright (c) 2026 EKA2L1 Team. Licensed under GPLv2 or later.
#include <cpu/arm_factory.h>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <type_traits>

namespace {
    using namespace eka2l1::arm;
    using page = std::array<std::uint8_t, 4096>;
    struct memory_page {
        page initial, expected, working;
    };
    struct snapshot {
        std::uint32_t budget;
        core::thread_context initial, expected;
        std::map<std::uint32_t, memory_page> pages;
    };

    template <typename T> bool read_value(std::ifstream& input, T& value) {
        static_assert(std::is_trivially_copyable_v<T>);
        return bool(input.read(reinterpret_cast<char*>(&value), sizeof(T)));
    }

    bool load(std::ifstream& input, snapshot& image) {
        static_assert(std::has_unique_object_representations_v<core::thread_context>);
        if (std::endian::native != std::endian::little || !read_value(input, image.budget) || !image.budget
            || !read_value(input, image.initial) || !read_value(input, image.expected))
            return false;
        while (input.peek() != std::ifstream::traits_type::eof()) {
            std::uint32_t address;
            if (!read_value(input, address) || (address & 4095))
                return false;
            auto [entry, inserted] = image.pages.try_emplace(address);
            if (!inserted || !read_value(input, entry->second.initial) || !read_value(input, entry->second.expected))
                return false;
        }
        return !input.bad() && !image.pages.empty();
    }

    bool replay(const char* path, arm_emulator_type mode, snapshot& image) {
        auto monitor = create_exclusive_monitor(mode, 1);
        auto cpu = create_core(monitor.get(), mode);
        bool fault = false;
        auto reject = [&](auto...) {
            fault = true;
            cpu->stop();
            return false;
        };
        auto access = [&](std::uint32_t address, auto* value, bool write) {
            const auto offset = address & 4095;
            const auto base = address - offset;
            const auto it = image.pages.find(base);
            if (it == image.pages.end() || offset + sizeof(*value) > 4096)
                return reject();
            auto* data = it->second.working.data();
            cpu->set_tlb_page(base, data, prot_read_write_exec);
            if (write)
                std::memcpy(data + offset, value, sizeof(*value));
            else
                std::memcpy(value, data + offset, sizeof(*value));
            return true;
        };
        auto read = [&](auto address, auto* value) { return access(address, value, false); };
        auto write = [&](auto address, auto* value) { return access(address, value, true); };
        cpu->read_code = read;
        cpu->read_8bit = read;
        cpu->read_16bit = read;
        cpu->read_32bit = read;
        cpu->read_64bit = read;
        cpu->write_8bit = write;
        cpu->write_16bit = write;
        cpu->write_32bit = write;
        cpu->write_64bit = write;
        cpu->exclusive_write_8bit = reject;
        cpu->exclusive_write_16bit = reject;
        cpu->exclusive_write_32bit = reject;
        cpu->exclusive_write_64bit = reject;
        cpu->exception_handler = reject;
        cpu->system_call_handler = reject;
        monitor->read_8bit = reject;
        monitor->read_16bit = reject;
        monitor->read_32bit = reject;
        monitor->read_64bit = reject;
        monitor->write_8bit = reject;
        monitor->write_16bit = reject;
        monitor->write_32bit = reject;
        monitor->write_64bit = reject;
        cpu->set_asid(1);

        constexpr unsigned warmup = 4;
        std::array<double, 36> times;
        for (unsigned i = 0; i < warmup + times.size(); ++i) {
            for (auto& [address, data] : image.pages)
                data.working = data.initial;
            cpu->load_context(image.initial);
            const auto start = std::chrono::steady_clock::now();
            cpu->run(image.budget);
            const auto elapsed = std::chrono::steady_clock::now() - start;
            core::thread_context result {};
            cpu->save_context(result);
            if (fault || cpu->get_num_instruction_executed() != image.budget
                || std::memcmp(&result, &image.expected, sizeof(result))
                || std::any_of(image.pages.begin(), image.pages.end(), [](const auto& entry) {
                    return entry.second.working != entry.second.expected;
                })) {
                std::fprintf(stderr, "%s backend %d diverged on run %u at PC %08x (expected %08x)\n",
                    path, int(mode), i, result.get_pc(), image.expected.get_pc());
                return false;
            }
            if (i >= warmup)
                times[i - warmup] = std::chrono::duration<double, std::nano>(elapsed).count() / image.budget;
        }
        std::sort(times.begin(), times.end());
        std::printf("%s %d %08x %u %.3f\n", path, int(mode), image.initial.get_pc(), image.budget, times[times.size() / 2]);
        return true;
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: arm_replay snapshot.trace [...]\n");
        return 2;
    }
    for (int i = 1; i < argc; ++i) {
        snapshot image {};
        std::ifstream input(argv[i], std::ios::binary);
        if (!load(input, image)) {
            std::fprintf(stderr, "%s: invalid snapshot\n", argv[i]);
            return 2;
        }
        for (auto mode : { arm_emulator_type::dyncom, arm_emulator_type::ekalynx })
            if (!replay(argv[i], mode, image))
                return 1;
    }
}
