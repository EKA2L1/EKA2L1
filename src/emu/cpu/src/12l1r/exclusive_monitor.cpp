/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <cpu/12l1r/exclusive_monitor.h>

namespace eka2l1::arm::r12l1 {
    exclusive_monitor::exclusive_monitor(const std::size_t processor_count) :
        exclusive_addresses_(processor_count, INVALID_EXCLUSIVE_ADDRESS),
        exclusive_values_(processor_count) {
        unlock();
    }

    std::size_t exclusive_monitor::get_processor_count() const {
        return exclusive_addresses_.size();
    }

    void exclusive_monitor::lock() {
        while (is_locked_.test_and_set(std::memory_order_acquire)) {}
    }

    void exclusive_monitor::unlock() {
        is_locked_.clear(std::memory_order_release);
    }

    bool exclusive_monitor::check_and_clear(const std::size_t processor_id, vaddress address) {
        const vaddress masked_address = address & RESERVATION_GRANULE_MASK;

        lock();

        if (exclusive_addresses_[processor_id] != masked_address) {
            unlock();
            return false;
        }

        for (vaddress& other_address : exclusive_addresses_) {
            if (other_address == masked_address) {
                other_address = INVALID_EXCLUSIVE_ADDRESS;
            }
        }

        return true;
    }

    void exclusive_monitor::clear_exclusive() {
        lock();
        std::fill(exclusive_addresses_.begin(), exclusive_addresses_.end(), INVALID_EXCLUSIVE_ADDRESS);
        unlock();
    }

    void exclusive_monitor::clear_processor(const std::size_t processor_id) {
        lock();
        exclusive_addresses_[processor_id] = INVALID_EXCLUSIVE_ADDRESS;
        unlock();
    }

    std::uint8_t exclusive_monitor::exclusive_read8(core *cc, address vaddr) {
        return read_and_mark<std::uint8_t>(cc->core_number(), vaddr, [&]() -> std::uint8_t {
            // TODO: Access violation if there is
            std::uint8_t val = 0;
            read_8bit(cc, vaddr, &val);

            return val;
        });
    }

    std::uint16_t exclusive_monitor::exclusive_read16(core *cc, address vaddr) {
        return read_and_mark<std::uint16_t>(cc->core_number(), vaddr, [&]() -> std::uint16_t {
            // TODO: Access violation if there is
            std::uint16_t val = 0;
            read_16bit(cc, vaddr, &val);

            return val;
        });
    }

    std::uint32_t exclusive_monitor::exclusive_read32(core *cc, address vaddr) {
        return read_and_mark<std::uint32_t>(cc->core_number(), vaddr, [&]() -> std::uint32_t {
            // TODO: Access violation if there is
            std::uint32_t val = 0;
            read_32bit(cc, vaddr, &val);

            return val;
        });
    }

    std::uint64_t exclusive_monitor::exclusive_read64(core *cc, address vaddr) {
        return read_and_mark<std::uint64_t>(cc->core_number(), vaddr, [&]() -> std::uint64_t {
            // TODO: Access violation if there is
            std::uint64_t val = 0;
            read_64bit(cc, vaddr, &val);

            return val;
        });
    }

    bool exclusive_monitor::exclusive_write8(core *cc, address vaddr, std::uint8_t value) {
        return do_exclusive_operation<std::uint8_t>(cc->core_number(), vaddr, [&](std::uint8_t expected) -> bool {
            const std::int32_t res = write_8bit(cc, vaddr, value, expected);

            // TODO: Parse access violation errors
            return res > 0;
        });
    }

    bool exclusive_monitor::exclusive_write16(core *cc, address vaddr, std::uint16_t value) {
        return do_exclusive_operation<std::uint16_t>(cc->core_number(), vaddr, [&](std::uint16_t expected) -> bool {
            const std::int32_t res = write_16bit(cc, vaddr, value, expected);

            // TODO: Parse access violation errors
            return res > 0;
        });
    }

    bool exclusive_monitor::exclusive_write32(core *cc, address vaddr, std::uint32_t value) {
        return do_exclusive_operation<std::uint32_t>(cc->core_number(), vaddr, [&](std::uint32_t expected) -> bool {
            const std::int32_t res = write_32bit(cc, vaddr, value, expected);

            // TODO: Parse access violation errors
            return res > 0;
        });
    }

    bool exclusive_monitor::exclusive_write64(core *cc, address vaddr, std::uint64_t value) {
        return do_exclusive_operation<std::uint64_t>(cc->core_number(), vaddr, [&](std::uint64_t expected) -> bool {
            const std::int32_t res = write_64bit(cc, vaddr, value, expected);

            // TODO: Parse access violation errors
            return res > 0;
        });
    }
}