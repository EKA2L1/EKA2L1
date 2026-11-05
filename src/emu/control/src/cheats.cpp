// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "cheats.h"

#include <common/cvt.h>
#include <common/log.h>
#include <kernel/chunk.h>
#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <mem/mem.h>
#include <mem/process.h>
#include <services/ui/cap/oom_app.h>
#include <services/window/classes/wingroup.h>
#include <services/window/screen.h>
#include <services/window/window.h>
#include <system/epoc.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>

namespace eka2l1::control {
    namespace fs = std::filesystem;

    static void require(bool condition, const std::string &code, const std::string &message, int status = 422) {
        if (!condition)
            throw api_error(status, code, message);
    }

    static std::uint32_t mask(int width) {
        return width == 4 ? 0xFFFFFFFFu : ((1u << (width * 8)) - 1);
    }

    // Negative values are entered as they are stored, in two's complement
    static std::uint32_t truncate(double value, int width) {
        return static_cast<std::uint32_t>(static_cast<std::int64_t>(std::llround(value))) & mask(width);
    }

    static std::uint32_t peek(const std::uint8_t *data, int width) {
        switch (width) {
        case 1:
            return *data;
        case 2: {
            std::uint16_t value;
            std::memcpy(&value, data, 2);
            return value;
        }
        default: {
            std::uint32_t value;
            std::memcpy(&value, data, 4);
            return value;
        }
        }
    }

    static bool matches(cheat_engine::compare how, std::uint32_t before, std::uint32_t now, std::uint32_t value) {
        switch (how) {
        case cheat_engine::compare::equal:
            return now == value;
        case cheat_engine::compare::different:
            return now != value;
        case cheat_engine::compare::changed:
            return now != before;
        case cheat_engine::compare::unchanged:
            return now == before;
        case cheat_engine::compare::increased:
            return now > before;
        case cheat_engine::compare::decreased:
            return now < before;
        case cheat_engine::compare::not_increased:
            return now <= before;
        case cheat_engine::compare::not_decreased:
            return now >= before;
        }
        return false;
    }

    static int parse_width(const json &value) {
        const int width = static_cast<int>(value.number(4));
        require(width == 1 || width == 2 || width == 4, "bad_request", "width must be 1, 2 or 4 bytes", 400);
        return width;
    }

    static cheat_engine::compare parse_compare(const std::string &text) {
        static const std::pair<const char *, cheat_engine::compare> names[] = {
            { "equal", cheat_engine::compare::equal }, { "different", cheat_engine::compare::different },
            { "changed", cheat_engine::compare::changed }, { "unchanged", cheat_engine::compare::unchanged },
            { "increased", cheat_engine::compare::increased }, { "decreased", cheat_engine::compare::decreased },
            { "not-increased", cheat_engine::compare::not_increased }, { "not-decreased", cheat_engine::compare::not_decreased }
        };
        for (const auto &[name, how] : names)
            if (text == name)
                return how;
        throw api_error(400, "bad_request", "compare must be equal, different, changed, unchanged, increased, decreased, not-increased or not-decreased");
    }

    cheat_engine::cheat_engine(fs::path folder)
        : folder_(std::move(folder)) {
    }

    // next to compat/<UID>.yml, the per-app settings
    fs::path cheat_engine::file() const {
        return folder_ / (hex(app_uid_).substr(2) + ".json");
    }

    kernel::process *cheat_engine::process(system &sys) {
        return process_id_ ? sys.get_kernel_system()->get_by_id<kernel::process>(process_id_) : nullptr;
    }

    // The app whose window has the focus, found as the Qt frontend finds the app its settings apply to
    void cheat_engine::attach(system &sys) {
        kernel_system *kern = sys.get_kernel_system();
        require(kern != nullptr, "no_device", "no device is booted", 409);
        auto *winserv = reinterpret_cast<window_server *>(kern->get_by_name<service::server>(get_winserv_name_by_epocver(kern->get_epoc_version())));
        epoc::screen *scr = winserv ? winserv->get_current_focus_screen() : nullptr;
        std::optional<akn_running_app_info> found;
        if (scr && scr->focus) {
            for (auto *group = scr->get_group_chain(); group; group = reinterpret_cast<epoc::window_group *>(group->sibling)) {
                auto info = get_akn_app_info_from_window_group(group);
                if (!info.has_value())
                    continue;
                if (group == scr->focus) {
                    found = info;
                    break;
                }
                kernel::thread *owner = scr->focus->client->get_client();
                if (owner && info->associated_->has_child_process(owner->owning_process()))
                    found = info;
            }
        }
        require(found.has_value() && found->associated_, "no_app", "no app has the focus", 409);
        if (found->app_uid_ != app_uid_) {
            reset_search();
            app_uid_ = found->app_uid_;
            load();
        }
        app_name_ = common::ucs2_to_utf8(found->app_name_);
        if (found->associated_->unique_id() != process_id_) {
            // the game started again: its old results point at memory it no longer has
            reset_search();
            process_id_ = found->associated_->unique_id();
        }
    }

    void cheat_engine::reset_search() {
        width_ = 0;
        searching_ = false;
        snapshot_.clear();
        snapshot_.shrink_to_fit();
        addresses_.clear();
        addresses_.shrink_to_fit();
        values_.clear();
        values_.shrink_to_fit();
    }

    std::uint64_t cheat_engine::count() const {
        if (!snapshot_.empty()) {
            std::uint64_t bytes = 0;
            for (const auto &p : snapshot_)
                bytes += p.bytes.size();
            return bytes / width_;
        }
        return addresses_.size();
    }

    // The pages of the app's own writable memory (heaps, stacks, static data): no code, no ROM
    std::vector<std::pair<std::uint32_t, std::uint32_t>> cheat_engine::ranges(system &sys, kernel::process *pr) {
        kernel_system *kern = sys.get_kernel_system();
        memory_system *mem = sys.get_memory_system();
        const std::uint32_t page_size = static_cast<std::uint32_t>(mem->get_page_size());
        const mem::asid id = pr->get_mem_model()->address_space_id();
        std::vector<std::pair<std::uint32_t, std::uint32_t>> pages;
        for (auto &obj : kern->get_chunk_list()) {
            auto *chunk = reinterpret_cast<kernel::chunk *>(obj.get());
            if (!chunk || !chunk->valid() || chunk->get_own_process() != pr)
                continue;
            const auto access = chunk->position_access();
            if (access == kernel::chunk_access::code || access == kernel::chunk_access::rom || access == kernel::chunk_access::kernel_mapping)
                continue;
            const std::uint32_t base = chunk->base(pr).ptr_address();
            const std::uint64_t end = std::uint64_t(base) + chunk->max_size();
            for (std::uint64_t address = base; address < end; address += page_size)
                if (mem->get_real_pointer(static_cast<std::uint32_t>(address), id))
                    pages.emplace_back(static_cast<std::uint32_t>(address), page_size);
        }
        std::sort(pages.begin(), pages.end());
        pages.erase(std::unique(pages.begin(), pages.end()), pages.end());
        return pages;
    }

    std::optional<std::uint32_t> cheat_engine::read(system &sys, kernel::process *pr, std::uint32_t address, int width) {
        if (!pr || (address % width))
            return std::nullopt;
        auto *data = reinterpret_cast<std::uint8_t *>(sys.get_memory_system()->get_real_pointer(address, pr->get_mem_model()->address_space_id()));
        if (!data)
            return std::nullopt;
        return peek(data, width);
    }

    bool cheat_engine::write(system &sys, kernel::process *pr, std::uint32_t address, int width, std::uint32_t value) {
        if (!pr || (address % width))
            return false;
        auto *data = reinterpret_cast<std::uint8_t *>(sys.get_memory_system()->get_real_pointer(address, pr->get_mem_model()->address_space_id()));
        if (!data)
            return false;
        // Data pages only: the JIT keeps no copy of them, nothing to invalidate
        std::memcpy(data, &value, width);
        return true;
    }

    std::uint64_t cheat_engine::start(system &sys, int width, std::optional<std::uint32_t> value) {
        kernel::process *pr = process(sys);
        require(pr != nullptr, "no_app", "the app is gone", 409);
        reset_search();
        width_ = width;
        searching_ = true;
        memory_system *mem = sys.get_memory_system();
        const mem::asid id = pr->get_mem_model()->address_space_id();
        for (const auto &[address, size] : ranges(sys, pr)) {
            auto *data = reinterpret_cast<const std::uint8_t *>(mem->get_real_pointer(address, id));
            if (!data)
                continue;
            if (!value.has_value()) {
                snapshot_.push_back({ address, std::vector<std::uint8_t>(data, data + size) });
                continue;
            }
            for (std::uint32_t offset = 0; offset + width <= size; offset += width)
                if (peek(data + offset, width) == *value) {
                    addresses_.push_back(address + offset);
                    values_.push_back(*value);
                }
        }
        return count();
    }

    std::uint64_t cheat_engine::next(system &sys, compare how, std::uint32_t value) {
        require(searching_, "no_search", "start a new search first", 409);
        kernel::process *pr = process(sys);
        require(pr != nullptr, "no_app", "the app is gone", 409);
        memory_system *mem = sys.get_memory_system();
        const mem::asid id = pr->get_mem_model()->address_space_id();
        const std::uint32_t page_mask = ~(static_cast<std::uint32_t>(mem->get_page_size()) - 1);
        if (!snapshot_.empty()) {
            for (const auto &p : snapshot_) {
                auto *data = reinterpret_cast<const std::uint8_t *>(mem->get_real_pointer(p.address, id));
                if (!data)
                    continue;
                for (std::uint32_t offset = 0; offset + width_ <= p.bytes.size(); offset += width_) {
                    const std::uint32_t now = peek(data + offset, width_);
                    if (matches(how, peek(p.bytes.data() + offset, width_), now, value)) {
                        addresses_.push_back(p.address + offset);
                        values_.push_back(now);
                    }
                }
            }
            snapshot_.clear();
            snapshot_.shrink_to_fit();
            return count();
        }
        std::size_t kept = 0;
        std::uint32_t cached_page = 1;
        const std::uint8_t *cached = nullptr;
        for (std::size_t i = 0; i < addresses_.size(); ++i) {
            const std::uint32_t address = addresses_[i];
            if ((address & page_mask) != cached_page) {
                cached_page = address & page_mask;
                cached = reinterpret_cast<const std::uint8_t *>(mem->get_real_pointer(cached_page, id));
            }
            if (!cached)
                continue;
            const std::uint32_t now = peek(cached + (address - cached_page), width_);
            if (matches(how, values_[i], now, value)) {
                addresses_[kept] = address;
                values_[kept++] = now;
            }
        }
        addresses_.resize(kept);
        values_.resize(kept);
        return count();
    }

    void cheat_engine::load() {
        codes_.clear();
        std::ifstream in(file());
        if (!in)
            return;
        std::stringstream text;
        text << in.rdbuf();
        try {
            const json saved = json::parse(text.str());
            for (const auto &item : saved["codes"].items()) {
                const int width = static_cast<int>(item["width"].number(4));
                if (width != 1 && width != 2 && width != 4)
                    continue;
                // armed again from the menu: the game may not have that memory in place yet
                codes_.push_back({ static_cast<std::uint32_t>(item["address"].number()), width,
                    truncate(item["value"].number(), width), false, item["frozen"].boolean(), item["name"].string() });
            }
        } catch (const std::exception &e) {
            LOG_WARN(FRONTEND_CMDLINE, "Ignoring the unreadable cheat file of {}: {}", hex(app_uid_), e.what());
        }
    }

    void cheat_engine::save() const {
        if (!app_uid_)
            return;
        json::array list;
        for (const auto &c : codes_)
            list.push_back({ { "address", c.address }, { "width", c.width }, { "value", c.value },
                { "frozen", c.frozen || c.was_frozen }, { "name", c.name } });
        std::error_code error;
        fs::create_directories(folder_, error);
        std::ofstream out(file(), std::ios::trunc);
        out << json(json::object{ { "uid", hex(app_uid_) }, { "codes", list } }).dump();
        if (!out)
            LOG_WARN(FRONTEND_CMDLINE, "Unable to save the cheats of {} in {}", hex(app_uid_), folder_.string());
    }

    json cheat_engine::code_list() const {
        json::array list;
        for (const auto &c : codes_)
            list.push_back({ { "address", c.address }, { "addressHex", hex(c.address) }, { "width", c.width },
                { "value", c.value }, { "frozen", c.frozen }, { "wasFrozen", c.was_frozen }, { "name", c.name } });
        return list;
    }

    json cheat_engine::state() const {
        return { { "app", app_uid_ ? json({ { "uid", hex(app_uid_) }, { "name", app_name_ } }) : json(nullptr) },
            { "search", { { "active", searching_ }, { "width", width_ }, { "count", count() }, { "unknown", !snapshot_.empty() } } },
            { "codes", code_list() } };
    }

    response cheat_engine::handle(system &sys, const request &req) {
        attach(sys);
        const auto &p = req.path;
        const auto body = req.data();
        auto refresh_values = [&] {
            kernel::process *pr = process(sys);
            for (auto &c : codes_)
                if (!c.frozen && !c.was_frozen)
                    if (auto now = read(sys, pr, c.address, c.width))
                        c.value = *now;
        };
        if (p.size() == 2 && req.method == "GET") {
            refresh_values();
            return response::from_json(state());
        }
        if (p.size() >= 3 && p[2] == "search") {
            if (p.size() == 3 && req.method == "POST") {
                const int width = parse_width(body["width"]);
                std::optional<std::uint32_t> value;
                if (body.contains("value") && !body["value"].is_null())
                    value = truncate(body["value"].number(), width);
                start(sys, width, value);
                return response::from_json(state());
            }
            if (p.size() == 4 && p[3] == "next" && req.method == "POST") {
                const auto how = parse_compare(body["compare"].string("equal"));
                const bool needs_value = how == compare::equal || how == compare::different;
                require(!needs_value || body.contains("value"), "bad_request", "value required", 400);
                next(sys, how, needs_value ? truncate(body["value"].number(), width_) : 0);
                return response::from_json(state());
            }
            if (p.size() == 3 && req.method == "DELETE") {
                reset_search();
                return response::from_json(state());
            }
            if (p.size() == 4 && p[3] == "results" && req.method == "GET") {
                require(searching_ && snapshot_.empty(), "no_results", "narrow the search down first", 409);
                const std::size_t offset = std::min<std::size_t>(std::stoul(req.param("offset", "0")), addresses_.size());
                const std::size_t limit = std::min<std::size_t>(std::stoul(req.param("limit", "100")), 1000);
                kernel::process *pr = process(sys);
                json::array list;
                for (std::size_t i = offset; i < std::min(addresses_.size(), offset + limit); ++i) {
                    auto now = read(sys, pr, addresses_[i], width_);
                    list.push_back({ { "address", addresses_[i] }, { "addressHex", hex(addresses_[i]) },
                        { "previous", values_[i] }, { "value", now ? json(*now) : json(nullptr) } });
                }
                return response::from_json({ { "count", count() }, { "width", width_ }, { "offset", offset }, { "results", list } });
            }
        }
        if (p.size() >= 3 && p[2] == "codes") {
            kernel::process *pr = process(sys);
            if (p.size() == 3 && req.method == "POST") {
                const std::uint32_t address = static_cast<std::uint32_t>(required_number(body, "address"));
                const int width = body.contains("width") ? parse_width(body["width"]) : (width_ ? width_ : 4);
                auto now = read(sys, pr, address, width);
                require(now.has_value(), "bad_address", "the app has no memory at " + hex(address));
                auto same = std::find_if(codes_.begin(), codes_.end(), [&](const code &c) { return c.address == address && c.width == width; });
                if (same == codes_.end())
                    codes_.push_back({ address, width, *now, false, false, body["name"].string() });
                save();
                return response::from_json(state());
            }
            if (p.size() == 4 && p[3] == "restore" && req.method == "POST") {
                for (auto &c : codes_) {
                    c.frozen = c.frozen || c.was_frozen;
                    c.was_frozen = false;
                }
                save();
                return response::from_json(state());
            }
            if (p.size() == 4) {
                std::size_t index = 0;
                try {
                    index = std::stoul(p[3]);
                } catch (const std::exception &) {
                    throw api_error(404, "not_found", "no code " + p[3]);
                }
                require(index < codes_.size(), "not_found", "no code " + p[3], 404);
                code &c = codes_[index];
                if (req.method == "PATCH") {
                    if (body.contains("name"))
                        c.name = body["name"].string();
                    if (body.contains("value")) {
                        c.value = truncate(body["value"].number(), c.width);
                        write(sys, pr, c.address, c.width, c.value);
                    }
                    if (body.contains("frozen")) {
                        c.frozen = body["frozen"].boolean();
                        c.was_frozen = false;
                        if (!c.frozen)
                            if (auto now = read(sys, pr, c.address, c.width))
                                c.value = *now;
                    }
                    save();
                    return response::from_json(state());
                }
                if (req.method == "DELETE") {
                    codes_.erase(codes_.begin() + index);
                    save();
                    return response::from_json(state());
                }
            }
        }
        throw api_error(404, "not_found", "unknown cheats endpoint");
    }

    void cheat_engine::freeze(system &sys) {
        if (codes_.empty())
            return;
        kernel::process *pr = nullptr;
        for (const auto &c : codes_) {
            if (!c.frozen)
                continue;
            if (!pr && !(pr = process(sys)))
                return;
            write(sys, pr, c.address, c.width, c.value);
        }
    }
}
