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
        cpu->read_code = [this](const vm_address addr, std::uint32_t *data) { return read_code(addr, data); };

        cpu->write_8bit = [this](const vm_address addr, std::uint8_t* data) { return write_8bit_data(addr, data); };
        cpu->write_16bit = [this](const vm_address addr, std::uint16_t* data) { return write_16bit_data(addr, data); };
        cpu->write_32bit = [this](const vm_address addr, std::uint32_t* data) { return write_32bit_data(addr, data); };
        cpu->write_64bit = [this](const vm_address addr, std::uint64_t* data) { return write_64bit_data(addr, data); };

        cpu->exclusive_write_8bit = [this](const vm_address addr, std::uint8_t value, std::uint8_t expected) {
            return write_exclusive<std::uint8_t>(addr, value, expected);
        };

        cpu->exclusive_write_16bit = [this](const vm_address addr, std::uint16_t value, std::uint16_t expected) {
            return write_exclusive<std::uint16_t>(addr, value, expected);
        };

        cpu->exclusive_write_32bit = [this](const vm_address addr, std::uint32_t value, std::uint32_t expected) {
            return write_exclusive<std::uint32_t>(addr, value, expected);
        };

        cpu->exclusive_write_64bit = [this](const vm_address addr, std::uint64_t value, std::uint64_t expected) {
            return write_exclusive<std::uint64_t>(addr, value, expected);
        };
    }

    void mmu_base::map_to_cpu(const vm_address addr, const std::size_t size, void *ptr, const prot perm) {
        //cpu_->map_backing_mem(addr, size, reinterpret_cast<std::uint8_t *>(ptr), perm);
    }

    void mmu_base::unmap_from_cpu(const vm_address addr, const std::size_t size) {
        const std::uint32_t psize = manager_->page_size();
        vm_address addr_temp = addr;

        for (std::size_t i = 0; i < size / psize; i++) {
            cpu_->dirty_tlb_page(addr_temp);
            addr_temp += psize;
        }
    }

    /// ================== MISCS ====================

    bool mmu_base::read_8bit_data(const vm_address addr, std::uint8_t *data) {
        page_info *inf = manager_->get_page_info(current_addr_space(), addr);
        if (!inf || !inf->host_addr) {
            return false;
        }

        std::uint8_t *ptr = reinterpret_cast<std::uint8_t*>(inf->host_addr) +
            (addr & manager_->offset_mask_);

        *data = *ptr;

        if (conf_->log_read) {
            LOG_TRACE("Read 1 byte from address 0x{:X}", addr);
        }

        cpu_->set_tlb_page(addr & ~manager_->offset_mask_, reinterpret_cast<std::uint8_t*>(inf->host_addr),
            inf->perm);

        return true;
    }

    bool mmu_base::read_16bit_data(const vm_address addr, std::uint16_t *data) {
        page_info *inf = manager_->get_page_info(current_addr_space(), addr);
        if (!inf || !inf->host_addr) {
            return false;
        }

        std::uint16_t *ptr = reinterpret_cast<std::uint16_t*>(reinterpret_cast<std::uint8_t*>(inf->host_addr) +
            (addr & manager_->offset_mask_));

        *data = *ptr;

        if (conf_->log_read) {
            LOG_TRACE("Read 2 bytes from address 0x{:X}", addr);
        }

        cpu_->set_tlb_page(addr & ~manager_->offset_mask_, reinterpret_cast<std::uint8_t*>(inf->host_addr),
            inf->perm);

        return true;
    }

    bool mmu_base::read_32bit_data(const vm_address addr, std::uint32_t *data) {
        page_info *inf = manager_->get_page_info(current_addr_space(), addr);
        if (!inf || !inf->host_addr) {
            return false;
        }

        std::uint32_t *ptr = reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uint8_t*>(inf->host_addr) +
            (addr & manager_->offset_mask_));

        *data = *ptr;

        if (conf_->log_read) {
            LOG_TRACE("Read 4 bytes from address 0x{:X}", addr);
        }

        cpu_->set_tlb_page(addr & ~manager_->offset_mask_, reinterpret_cast<std::uint8_t*>(inf->host_addr),
            inf->perm);

        return true;
    }

    bool mmu_base::read_64bit_data(const vm_address addr, std::uint64_t *data) {
        page_info *inf = manager_->get_page_info(current_addr_space(), addr);
        if (!inf || !inf->host_addr) {
            return false;
        }

        std::uint64_t *ptr = reinterpret_cast<std::uint64_t*>(reinterpret_cast<std::uint8_t*>(inf->host_addr) +
            (addr & manager_->offset_mask_));

        *data = *ptr;

        if (conf_->log_read) {
            LOG_TRACE("Read 8 bytes from address 0x{:X}", addr);
        }

        cpu_->set_tlb_page(addr & ~manager_->offset_mask_, reinterpret_cast<std::uint8_t*>(inf->host_addr),
            inf->perm);

        return true;
    }

    bool mmu_base::write_8bit_data(const vm_address addr, std::uint8_t *data) {
        page_info *inf = manager_->get_page_info(current_addr_space(), addr);
        if (!inf || !inf->host_addr) {
            return false;
        }

        std::uint8_t *ptr = reinterpret_cast<std::uint8_t*>(inf->host_addr) +
            (addr & manager_->offset_mask_);

        *ptr = *data;

        if (conf_->log_write) {
            LOG_TRACE("Write 1 byte to address 0x{:X}", addr);
        }

        cpu_->set_tlb_page(addr & ~manager_->offset_mask_, reinterpret_cast<std::uint8_t*>(inf->host_addr),
            inf->perm);

        return true;
    }

    bool mmu_base::write_16bit_data(const vm_address addr, std::uint16_t *data) {
        page_info *inf = manager_->get_page_info(current_addr_space(), addr);
        if (!inf || !inf->host_addr) {
            return false;
        }

        std::uint16_t *ptr = reinterpret_cast<std::uint16_t*>(reinterpret_cast<std::uint8_t*>(inf->host_addr) +
            (addr & manager_->offset_mask_));

        *ptr = *data;

        if (conf_->log_write) {
            LOG_TRACE("Write 2 bytes to address 0x{:X}", addr);
        }

        cpu_->set_tlb_page(addr & ~manager_->offset_mask_, reinterpret_cast<std::uint8_t*>(inf->host_addr),
            inf->perm);

        return true;
    }

    bool mmu_base::write_32bit_data(const vm_address addr, std::uint32_t *data) {
        page_info *inf = manager_->get_page_info(current_addr_space(), addr);
        if (!inf || !inf->host_addr) {
            return false;
        }

        std::uint32_t *ptr = reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uint8_t*>(inf->host_addr) +
            (addr & manager_->offset_mask_));

        *ptr = *data;

        if (conf_->log_write) {
            LOG_TRACE("Write 4 bytes to address 0x{:X}", addr);
        }

        cpu_->set_tlb_page(addr & ~manager_->offset_mask_, reinterpret_cast<std::uint8_t*>(inf->host_addr),
            inf->perm);

        return true;
    }

    bool mmu_base::write_64bit_data(const vm_address addr, std::uint64_t *data) {
        page_info *inf = manager_->get_page_info(current_addr_space(), addr);
        if (!inf || !inf->host_addr) {
            return false;
        }

        std::uint64_t *ptr = reinterpret_cast<std::uint64_t*>(reinterpret_cast<std::uint8_t*>(inf->host_addr) +
            (addr & manager_->offset_mask_));

        *ptr = *data;

        if (conf_->log_write) {
            LOG_TRACE("Write 8 bytes to address 0x{:X}", addr);
        }

        cpu_->set_tlb_page(addr & ~manager_->offset_mask_, reinterpret_cast<std::uint8_t*>(inf->host_addr),
            inf->perm);

        return true;
    }

    bool mmu_base::read_code(const vm_address addr, std::uint32_t *data) {
        std::uint32_t *code = reinterpret_cast<std::uint32_t*>(manager_->get_host_pointer(
            current_addr_space(), addr));

        if (!code) {
            return false;
        }

        *data = *code;
        return true;
    }
}
