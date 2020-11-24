/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <common/log.h>
#include <cpu/arm_interface.h>
#include <config/config.h>
#include <mem/mmu.h>
#include <mem/control.h>

#include <mem/model/flexible/mmu.h>
#include <mem/model/multiple/mmu.h>

namespace eka2l1::mem {
    mmu_base::mmu_base(control_base *manager, arm::core *cpu, config::state *conf)
        : manager_(manager)
        , cpu_(cpu)
        , conf_(conf) {
        // Set CPU read/write functions
        cpu->read_8bit = [this](const vm_address addr, std::uint8_t* data) { return read_8bit_data(addr, data); };
        cpu->read_16bit = [this](const vm_address addr, std::uint16_t* data) { return read_16bit_data(addr, data); };
        cpu->read_32bit = [this](const vm_address addr, std::uint32_t* data) { return read_32bit_data(addr, data); };
        cpu->read_64bit = [this](const vm_address addr, std::uint64_t* data) { return read_64bit_data(addr, data); };

        cpu->write_8bit = [this](const vm_address addr, std::uint8_t* data) { return write_8bit_data(addr, data); };
        cpu->write_16bit = [this](const vm_address addr, std::uint16_t* data) { return write_16bit_data(addr, data); };
        cpu->write_32bit = [this](const vm_address addr, std::uint32_t* data) { return write_32bit_data(addr, data); };
        cpu->write_64bit = [this](const vm_address addr, std::uint64_t* data) { return write_64bit_data(addr, data); };
    }

    void mmu_base::map_to_cpu(const vm_address addr, const std::size_t size, void *ptr, const prot perm) {
        cpu_->map_backing_mem(addr, size, reinterpret_cast<std::uint8_t *>(ptr), perm);
    }

    void mmu_base::unmap_from_cpu(const vm_address addr, const std::size_t size) {
        cpu_->unmap_memory(addr, size);
    }

    /// ================== MISCS ====================

    bool mmu_base::read_8bit_data(const vm_address addr, std::uint8_t *data) {
        std::uint8_t *ptr = reinterpret_cast<std::uint8_t*>(manager_->
            get_host_pointer(current_addr_space(), addr));

        if (!ptr) {
            return false;
        }

        *data = *ptr;

        if (conf_->log_read) {
            LOG_TRACE("Read 1 byte from address 0x{:X}", addr);
        }

        return true;
    }

    bool mmu_base::read_16bit_data(const vm_address addr, std::uint16_t *data) {
        std::uint16_t *ptr = reinterpret_cast<std::uint16_t*>(manager_->
            get_host_pointer(current_addr_space(), addr));

        if (!ptr) {
            return false;
        }

        *data = *ptr;

        if (conf_->log_read) {
            LOG_TRACE("Read 2 bytes from address 0x{:X}", addr);
        }

        return true;
    }

    bool mmu_base::read_32bit_data(const vm_address addr, std::uint32_t *data) {
        std::uint32_t *ptr = reinterpret_cast<std::uint32_t*>(manager_->
            get_host_pointer(current_addr_space(), addr));

        if (!ptr) {
            return false;
        }

        *data = *ptr;

        if (conf_->log_read) {
            LOG_TRACE("Read 4 bytes from address 0x{:X}", addr);
        }

        return true;
    }

    bool mmu_base::read_64bit_data(const vm_address addr, std::uint64_t *data) {
        std::uint64_t *ptr = reinterpret_cast<std::uint64_t*>(manager_->
            get_host_pointer(current_addr_space(), addr));

        if (!ptr) {
            return false;
        }

        *data = *ptr;

        if (conf_->log_read) {
            LOG_TRACE("Read 8 bytes from address 0x{:X}", addr);
        }

        return true;
    }

    bool mmu_base::write_8bit_data(const vm_address addr, std::uint8_t *data) {
        std::uint8_t *ptr = reinterpret_cast<std::uint8_t*>(manager_->
            get_host_pointer(current_addr_space(), addr));

        if (!ptr) {
            return false;
        }

        *ptr = *data;

        if (conf_->log_write) {
            LOG_TRACE("Write 1 byte to address 0x{:X}", addr);
        }

        return true;
    }

    bool mmu_base::write_16bit_data(const vm_address addr, std::uint16_t *data) {
        std::uint16_t *ptr = reinterpret_cast<std::uint16_t*>(manager_->
            get_host_pointer(current_addr_space(), addr));

        if (!ptr) {
            return false;
        }

        *ptr = *data;

        if (conf_->log_write) {
            LOG_TRACE("Write 2 bytes to address 0x{:X}", addr);
        }

        return true;
    }

    bool mmu_base::write_32bit_data(const vm_address addr, std::uint32_t *data) {
        std::uint32_t *ptr = reinterpret_cast<std::uint32_t*>(manager_->get_host_pointer(
            current_addr_space(), addr));

        if (!ptr) {
            return false;
        }

        *ptr = *data;

        if (conf_->log_write) {
            LOG_TRACE("Write 4 bytes to address 0x{:X}", addr);
        }

        return true;
    }

    bool mmu_base::write_64bit_data(const vm_address addr, std::uint64_t *data) {
        std::uint64_t *ptr = reinterpret_cast<std::uint64_t*>(manager_->
            get_host_pointer(current_addr_space(), addr));

        if (!ptr) {
            return false;
        }

        *ptr = *data;

        if (conf_->log_write) {
            LOG_TRACE("Write 8 bytes to address 0x{:X}", addr);
        }

        return true;
    }
}
