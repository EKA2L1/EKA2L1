/*
 * Copyright (c) 2018 EKA2L1 Team.
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

#include <common/algorithm.h>
#include <common/benchmark.h>
#include <common/fileutils.h>
#include <common/log.h>
#include <common/virtualmem.h>

#include <arm/arm_interface.h>
#include <epoc/mem.h>
#include <epoc/ptr.h>

#include <algorithm>

namespace eka2l1 {
    void memory_system::init(arm::jitter &jit, uint32_t code_ram_addr,
        uint32_t ushared_addr, uint32_t ushared_size) {
        page_size = 0x1000;

        codeseg_addr = code_ram_addr;
        shared_addr = ushared_addr;
        shared_size = ushared_size;

        cpu = jit.get();

        page clear_page;
        clear_page.generation = 0;
        clear_page.sts = page_status::free;
        clear_page.page_protection = prot::none;

        std::fill(global_pages.begin(), global_pages.end(), clear_page);
        std::fill(codeseg_pages.begin(), codeseg_pages.end(), clear_page);
    }

    void memory_system::shutdown() {
        if (rom_map) {
            common::unmap_file(rom_map);
        }
    }

    bool memory_system::map_rom(uint32_t addr, const std::string &path) {
        rom_addr = addr;
        rom_map = common::map_file(path);
        rom_size = common::file_size(path);

        LOG_TRACE("Rom mapped to address: 0x{:x}", reinterpret_cast<uint64_t>(rom_map));

        cpu->map_backing_mem(rom_addr, common::align(rom_size, page_size),
            reinterpret_cast<uint8_t *>(rom_map), prot::read_exec);

        return true;
    }

    void *memory_system::get_real_pointer(address addr) {
        // Fallback to slow get if there is no page table
        if (!current_page_table) {
            if (addr >= codeseg_addr && addr < codeseg_addr + 0x10000000) {
                if (!codeseg_pointers[(addr - codeseg_addr) / page_size]) {
                    return nullptr;
                }

                return static_cast<void *>(codeseg_pointers[(addr - codeseg_addr) / page_size] + (addr - codeseg_addr) % page_size);
            }

            if (addr >= rom_addr && addr < rom_addr + 0x10000000) {
                return &reinterpret_cast<uint8_t *>(rom_map)[addr - rom_addr];
            }

            if (addr >= shared_addr && addr < shared_addr + shared_size) {
                if (!global_pointers[(addr - shared_addr) / page_size]) {
                    return nullptr;
                }

                return static_cast<void *>(global_pointers[(addr - shared_addr) / page_size] + (addr - shared_addr) % page_size);
            }

            return nullptr;
        }

        if (!current_page_table->pointers[addr / page_size]) {
            return nullptr;
        }

        return static_cast<void *>(current_page_table->pointers[addr / page_size] + addr % page_size);
    }

    // Create a new chunk with specified address. Return base of chunk
    ptr<void> memory_system::chunk(address addr, uint32_t bottom, uint32_t top, uint32_t max_grow, prot cprot) {
        max_grow -= max_grow % page_size;

        if (bottom % page_size != 0) {
            bottom += bottom % page_size;
        }

        if (top % page_size != 0) {
            top += top % page_size;
        }

        top = common::min(top, max_grow);

        size_t page_begin_off = (addr + page_size - 1) / page_size;
        size_t page_end_off = (addr + max_grow - 1 + page_size) / page_size;

        page *page_begin;
        page *page_end;

        size_t len = common::GB(4);

        if (addr >= shared_addr && addr < shared_addr + shared_size) {
            page_begin = global_pages.data() + (page_begin_off - (shared_addr / page_size));
            page_end = global_pages.data() + (page_end_off - (shared_addr / page_size));
        } else if (addr >= codeseg_addr && addr < codeseg_addr + 0x10000000) {
            page_begin = codeseg_pages.data() + (page_begin_off - ((codeseg_addr) / page_size));
            page_end = codeseg_pages.data() + (page_end_off - ((codeseg_addr) / page_size));
        } else {
            page_begin = current_page_table->pages.data() + page_begin_off;
            page_end = current_page_table->pages.data() + page_end_off;
        }

        const size_t count = page_end_off - page_begin_off;

        for (auto ite = page_begin; ite != page_end; ite++) {
            // If the page is not free, than either it's reserved or commited
            // We can not make a new chunk on those pages
            if (ite->sts != page_status::free) {
                return ptr<void>(0);
            }

            ite->sts = page_status::reserved;
        }

        const gen generation = ++generations;

        // We commit them later, so as well as assigned there protect first
        page new_page = { generation, page_status::reserved, cprot };

        std::fill(page_begin, page_end, new_page);

        if (addr >= shared_addr && addr < shared_addr + shared_size) {
            global_pointers[page_begin_off - (shared_addr / page_size)]
                = static_cast<uint8_t *>(common::map_memory(count * page_size));

            for (size_t i = page_begin_off - (shared_addr / page_size) + 1; i < page_end_off - (shared_addr / page_size); i++) {
                global_pointers[i] = global_pointers[i - 1] + page_size;
            }

            if (current_page_table) {
                std::copy(global_pages.begin() + (page_begin_off - (shared_addr / page_size)),
                    global_pages.begin() + (page_end_off - (shared_addr / page_size)), current_page_table->pages.begin() + page_begin_off);

                std::copy(global_pointers.begin() + (page_begin_off - (shared_addr / page_size)),
                    global_pointers.begin() + (page_end_off - (shared_addr / page_size)), current_page_table->pointers.begin() + page_begin_off);
            }
        } else if (addr >= ram_code_addr && addr < codeseg_addr + 0x10000000) {
            codeseg_pointers[page_begin_off - (ram_code_addr / page_size)]
                = static_cast<uint8_t *>(common::map_memory(count * page_size));

            for (size_t i = page_begin_off - (ram_code_addr / page_size) + 1; i < page_end_off - (ram_code_addr / page_size); i++) {
                codeseg_pointers[i] = codeseg_pointers[i - 1] + page_size;
            }

            if (current_page_table) {
                std::copy(codeseg_pages.begin() + (page_begin_off - (codeseg_addr / page_size)),
                    codeseg_pages.begin() + (page_end_off - (codeseg_addr / page_size)), current_page_table->pages.begin() + page_begin_off);

                std::copy(codeseg_pointers.begin() + (page_begin_off - (codeseg_addr / page_size)),
                    codeseg_pointers.begin() + (page_end_off - (codeseg_addr / page_size)), current_page_table->pointers.begin() + page_begin_off);
            }
        } else {
            current_page_table->pointers[page_begin_off]
                = static_cast<uint8_t *>(common::map_memory(count * page_size));

            for (size_t i = page_begin_off + 1; i < page_end_off; i++) {
                current_page_table->pointers[i] = current_page_table->pointers[i - 1] + page_size;
            }
        }

        commit(addr + static_cast<uint32_t>(bottom), top - bottom);

        return ptr<void>(addr);
    }

    ptr<void> memory_system::chunk_range(address beg_addr, address end_addr, uint32_t bottom, uint32_t top, uint32_t max_grow, prot cprot) {
        // Find the reversed memory with this max grow
        uint32_t page_count = (max_grow + page_size - 1) / page_size;

        uint32_t page_begin_off = (beg_addr + page_size - 1) / page_size;
        uint32_t page_end_off = (end_addr - page_size + 1) / page_size;

        page *page_begin;
        page *page_end;

        if (beg_addr >= shared_addr && beg_addr < shared_addr + shared_size) {
            page_begin = global_pages.data() + (page_begin_off - (shared_addr / page_size));
            page_end = global_pages.data() + (page_end_off - (shared_addr / page_size));
        } else if (beg_addr >= codeseg_addr && beg_addr < codeseg_addr + 0x10000000) {
            page_begin = codeseg_pages.data() + (page_begin_off - (codeseg_addr / page_size));
            page_end = codeseg_pages.data() + (page_end_off - (codeseg_addr / page_size));
        } else {
            page_begin = current_page_table->pages.data() + page_begin_off;
            page_end = current_page_table->pages.data() + page_end_off;
        }

        page holder;

        auto ite = std::search_n(std::reverse_iterator<page *>(page_end),
            std::reverse_iterator<page *>(page_begin), page_count, holder, [](const page &lhs, const page &rhs) {
                return (lhs.sts == page_status::free);
            });

        if (ite == std::reverse_iterator<page *>(page_begin)) {
            return ptr<void>(0);
        }

        if (beg_addr >= shared_addr && beg_addr < shared_addr + shared_size) {
            auto idx = ite.base() - global_pages.data() - page_count;
            return chunk(static_cast<address>(shared_addr + idx * page_size), bottom, top, max_grow, cprot);
        } else if (beg_addr >= ram_code_addr && beg_addr < codeseg_addr + 0x10000000) {
            auto idx = ite.base() - codeseg_pages.data() - page_count;
            return chunk(static_cast<address>(ram_code_addr + idx * page_size), bottom, top, max_grow, cprot);
        }

        auto idx = ite.base() - current_page_table->pages.data() - page_count;
        return chunk(static_cast<address>(idx * page_size), bottom, top, max_grow, cprot);
    }

    // Change the prot of pages
    int memory_system::change_prot(ptr<void> addr, uint32_t size, prot nprot) {
        uint32_t beg = addr.ptr_address() / page_size;
        uint32_t end = (addr.ptr_address() + static_cast<uint32_t>(size) - 1 + page_size) / page_size;

        uint32_t count = end - beg;

        page *page_begin;
        page *page_end;

        size_t len = common::GB(4);

        if (addr.ptr_address() >= shared_addr && addr.ptr_address() < shared_addr + shared_size) {
            page_begin = global_pages.data() + (beg - (shared_addr / page_size));
            page_end = global_pages.data() + (end - (shared_addr / page_size));
        } else if (addr.ptr_address() >= codeseg_addr && addr.ptr_address() < codeseg_addr + 0x10000000) {
            page_begin = codeseg_pages.data() + (beg - ((codeseg_addr) / page_size));
            page_end = codeseg_pages.data() + (end - ((codeseg_addr) / page_size));
        } else {
            page_begin = current_page_table->pages.data() + beg;
            page_end = current_page_table->pages.data() + end;
        }

        auto page_begin_orginal = page_begin;

        for (; page_begin != page_end; page_begin++) {
            // Only a commited region can have a protection
            if (page_begin->sts != page_status::committed) {
                return -1;
            }

            page_begin->page_protection = nprot;
            cpu->unmap_memory(static_cast<address>(addr.ptr_address() + std::distance(page_begin_orginal, page_begin) * page_size),
                page_size);
        }

        if (addr.ptr_address() >= shared_addr && addr.ptr_address() < shared_addr + shared_size) {
            common::change_protection(global_pointers[(beg - (shared_addr / page_size))],
                count * page_size, nprot);

            if (current_page_table) {
                std::copy(global_pages.begin() + (beg - (shared_addr / page_size)),
                    global_pages.begin() + (end - (shared_addr / page_size)), current_page_table->pages.begin() + beg);

                std::copy(global_pointers.begin() + (beg - (shared_addr / page_size)),
                    global_pointers.begin() + (end - (shared_addr / page_size)), current_page_table->pointers.begin() + beg);
            }

            cpu->map_backing_mem(addr.ptr_address(), size, global_pointers[beg - (shared_addr / page_size)], nprot);
        } else if (addr.ptr_address() >= codeseg_addr && addr.ptr_address() < codeseg_addr + 0x10000000) {
            common::change_protection(codeseg_pointers[(beg - (ram_code_addr / page_size))],
                count * page_size, nprot);

            if (current_page_table) {
                std::copy(codeseg_pages.begin() + (beg - (codeseg_addr / page_size)),
                    codeseg_pages.begin() + (end - (codeseg_addr / page_size)), current_page_table->pages.begin() + beg);

                std::copy(codeseg_pointers.begin() + (beg - (codeseg_addr / page_size)),
                    codeseg_pointers.begin() + (end - (codeseg_addr / page_size)), current_page_table->pointers.begin() + beg);
            }

            cpu->map_backing_mem(addr.ptr_address(), size, codeseg_pointers[beg - (codeseg_addr / page_size)], nprot);
        } else {
            common::change_protection(current_page_table->pointers[beg],
                count * page_size, nprot);

            cpu->map_backing_mem(addr.ptr_address(), size, current_page_table->pointers[beg], nprot);
        }

        return 0;
    }

    // Mark a chunk at addr as unusable
    int memory_system::unchunk(ptr<void> addr, uint32_t length) {
        address beg = addr.ptr_address() / page_size;
        address end = (addr.ptr_address() + length - 1 + page_size) / page_size;

        size_t count = end - beg;

        page *page_begin;
        page *page_end;

        size_t len = common::GB(4);

        if (addr.ptr_address() >= shared_addr && addr.ptr_address() < shared_addr + shared_size) {
            page_begin = global_pages.data() + (beg - (shared_addr / page_size));
            page_end = global_pages.data() + (end - (shared_addr / page_size));
        } else if (addr.ptr_address() >= codeseg_addr && addr.ptr_address() < codeseg_addr + 0x10000000) {
            page_begin = codeseg_pages.data() + (beg - ((codeseg_addr) / page_size));
            page_end = codeseg_pages.data() + (end - ((codeseg_addr) / page_size));
        } else {
            page_begin = current_page_table->pages.data() + beg;
            page_end = current_page_table->pages.data() + end;
        }

        for (; page_begin != page_end; page_begin++) {
            page_begin->sts = page_status::free;
            page_begin->page_protection = prot::none;
            page_begin->generation = 0;
        }

        if (addr.ptr_address() >= shared_addr && addr.ptr_address() < shared_addr + shared_size) {
            eka2l1::common::unmap_memory(global_pointers[(beg - (shared_addr / page_size))], eka2l1::common::get_host_page_size());
            global_pointers[(beg - (shared_addr / page_size))] = nullptr;

            if (current_page_table) {
                std::copy(global_pages.begin() + (beg - (shared_addr / page_size)),
                    global_pages.begin() + (end - (shared_addr / page_size)), current_page_table->pages.begin() + beg);

                std::copy(global_pointers.begin() + (beg - (shared_addr / page_size)),
                    global_pointers.begin() + (end - (shared_addr / page_size)), current_page_table->pointers.begin() + beg);
            }
        } else if (addr.ptr_address() >= ram_code_addr && addr.ptr_address() < codeseg_addr + 0x10000000) {
            eka2l1::common::unmap_memory(codeseg_pointers[(beg - (ram_code_addr / page_size))], eka2l1::common::get_host_page_size());
            codeseg_pointers[(beg - (ram_code_addr / page_size))] = nullptr;

            if (current_page_table) {
                std::copy(codeseg_pages.begin() + (beg - (codeseg_addr / page_size)),
                    codeseg_pages.begin() + (end - (codeseg_addr / page_size)), current_page_table->pages.begin() + beg);

                std::copy(codeseg_pointers.begin() + (beg - (codeseg_addr / page_size)),
                    codeseg_pointers.begin() + (end - (codeseg_addr / page_size)), current_page_table->pointers.begin() + beg);
            }
        } else {
            eka2l1::common::unmap_memory(current_page_table->pointers[beg], eka2l1::common::get_host_page_size());
            current_page_table->pointers[beg] = nullptr;
        }

        // cpu->unmap_memory(addr.ptr_address(), count * page_size);

        return 0;
    }

    // Commit to page
    int memory_system::commit(ptr<void> addr, uint32_t size) {
        common::benchmarker marker(__FUNCTION__);

        if (size == 0) {
            return 0;
        }

        address beg = addr.ptr_address() / page_size;
        address end = (addr.ptr_address() + size - 1 + page_size) / page_size;

        size_t count = end - beg;

        page *page_begin;
        page *page_end;

        size_t len = common::GB(4);

        if (addr.ptr_address() >= shared_addr && addr.ptr_address() < shared_addr + shared_size) {
            page_begin = global_pages.data() + (beg - (shared_addr / page_size));
            page_end = global_pages.data() + (end - (shared_addr / page_size));
        } else if (addr.ptr_address() >= codeseg_addr && addr.ptr_address() < codeseg_addr + 0x10000000) {
            page_begin = codeseg_pages.data() + (beg - ((codeseg_addr) / page_size));
            page_end = codeseg_pages.data() + (end - ((codeseg_addr) / page_size));
        } else {
            page_begin = current_page_table->pages.data() + beg;
            page_end = current_page_table->pages.data() + end;
        }

        const auto page_begin_org = page_begin;

        prot nprot = page_begin->page_protection;
        int total_page_committed_so_far = 0;
        page *page_from_count = nullptr;

        for (; page_begin != page_end; page_begin++) {
            // Can commit on commited region or reserved region
            if (page_begin->sts == page_status::free) {
                return -1;
            }

            if (page_begin->sts != page_status::committed) {
                if (total_page_committed_so_far) {
                    cpu->unmap_memory(static_cast<address>(addr.ptr_address() + std::distance(page_from_count, page_begin) * page_size),
                        page_size * total_page_committed_so_far);

                    total_page_committed_so_far = 0;
                }
            } else {
                if (!total_page_committed_so_far) {
                    page_from_count = page_begin;
                }

                total_page_committed_so_far++;
            }

            page_begin->sts = page_status::committed;
        }

        if (addr.ptr_address() >= shared_addr && addr.ptr_address() < shared_addr + shared_size) {
            bool success = common::commit(global_pointers[beg - (shared_addr / page_size)],
                count * page_size, nprot);

            if (!success) {
                LOG_WARN("Host commit failed");
            }

            if (current_page_table) {
                std::copy(global_pages.begin() + (beg - (shared_addr / page_size)),
                    global_pages.begin() + (end - (shared_addr / page_size)), current_page_table->pages.begin() + beg);

                std::copy(global_pointers.begin() + (beg - (shared_addr / page_size)),
                    global_pointers.begin() + (end - (shared_addr / page_size)), current_page_table->pointers.begin() + beg);
            }

            cpu->map_backing_mem(addr.ptr_address(), size, global_pointers[beg - (shared_addr / page_size)],
                global_pages[beg - (shared_addr / page_size)].page_protection);
        } else if (addr.ptr_address() >= codeseg_addr && addr.ptr_address() < codeseg_addr + 0x10000000) {
            common::commit(codeseg_pointers[beg - (ram_code_addr / page_size)],
                count * page_size, nprot);

            if (current_page_table) {
                std::copy(codeseg_pages.begin() + (beg - (codeseg_addr / page_size)),
                    codeseg_pages.begin() + (end - (codeseg_addr / page_size)), current_page_table->pages.begin() + beg);

                std::copy(codeseg_pointers.begin() + (beg - (codeseg_addr / page_size)),
                    codeseg_pointers.begin() + (end - (codeseg_addr / page_size)), current_page_table->pointers.begin() + beg);
            }

            cpu->map_backing_mem(addr.ptr_address(), size, codeseg_pointers[beg - (codeseg_addr / page_size)],
                codeseg_pages[beg - (codeseg_addr / page_size)].page_protection);
        } else {
            common::commit(current_page_table->pointers[beg],
                count * page_size, nprot);

            cpu->map_backing_mem(addr.ptr_address(), size,
                current_page_table->pointers[beg], current_page_table->pages[beg].page_protection);
        }

        return 0;
    }

    // Decommit
    int memory_system::decommit(ptr<void> addr, uint32_t size) {
        if (size == 0) {
            return 0;
        }

        address beg = addr.ptr_address() / page_size;
        address end = (addr.ptr_address() + size - 1 + page_size) / page_size;

        size_t count = end - beg;

        page *page_begin;
        page *page_end;

        size_t len = common::GB(4);

        if (addr.ptr_address() >= shared_addr && addr.ptr_address() < shared_addr + shared_size) {
            page_begin = global_pages.data() + (beg - (shared_addr / page_size));
            page_end = global_pages.data() + (end - (shared_addr / page_size));
        } else if (addr.ptr_address() >= codeseg_addr && addr.ptr_address() < codeseg_addr + 0x10000000) {
            page_begin = codeseg_pages.data() + (beg - ((codeseg_addr) / page_size));
            page_end = codeseg_pages.data() + (end - ((codeseg_addr) / page_size));
        } else {
            page_begin = current_page_table->pages.data() + beg;
            page_end = current_page_table->pages.data() + end;
        }

        prot nprot = page_begin->page_protection;

        for (; page_begin != page_end; page_begin++) {
            if (page_begin->sts == page_status::committed) {
                page_begin->sts = page_status::reserved;
            }
        }

        if (addr.ptr_address() >= shared_addr && addr.ptr_address() < shared_addr + shared_size) {
            common::decommit(global_pointers[(beg - (shared_addr / page_size))],
                count * page_size);

            if (current_page_table) {
                std::copy(global_pages.begin() + beg - (shared_addr / page_size),
                    global_pages.begin() + end - (shared_addr / page_size), current_page_table->pages.begin() + beg);

                std::copy(global_pointers.begin() + beg - (shared_addr / page_size),
                    global_pointers.begin() + end - (shared_addr / page_size), current_page_table->pointers.begin() + beg);
            }
        } else if (addr.ptr_address() >= ram_code_addr && addr.ptr_address() < codeseg_addr + 0x10000000) {
            common::decommit(codeseg_pointers[(beg - (ram_code_addr / page_size))],
                count * page_size);

            if (current_page_table) {
                std::copy(codeseg_pages.begin() + beg - (codeseg_addr / page_size),
                    codeseg_pages.begin() + end - (codeseg_addr / page_size), current_page_table->pages.begin() + beg);

                std::copy(codeseg_pointers.begin() + beg - (codeseg_addr / page_size),
                    codeseg_pointers.begin() + end - (codeseg_addr / page_size), current_page_table->pointers.begin() + beg);
            }
        } else {
            common::decommit(current_page_table->pointers[beg],
                count * page_size);
        }

        cpu->unmap_memory(addr.ptr_address(), size);

        return 0;
    }

    bool memory_system::read(address addr, void *data, uint32_t size) {
        void *fptr = get_real_pointer(addr);

        if (fptr == nullptr) {
            LOG_WARN("Reading invalid address: 0x{:x}", addr);
            return false;
        }

        if (size == 4) {
            *reinterpret_cast<int *>(data) = *reinterpret_cast<int *>(fptr);
            return true;
        }

        memcpy(data, fptr, size);

        return true;
    }

    bool memory_system::write(address addr, void *data, uint32_t size) {
        void *to = get_real_pointer(addr);

        if (to == nullptr) {
            LOG_WARN("Writing invalid address: 0x{:x}", addr);
            return false;
        }

        memcpy(to, data, size);

        return true;
    }

    page_table *memory_system::get_current_page_table() const {
        return current_page_table;
    }

    void memory_system::set_current_page_table(page_table &table) {
        if (current_page_table == &table) {
            return;
        }

        previous_page_table = current_page_table;
        current_page_table = &table;

        std::copy(codeseg_pages.begin(), codeseg_pages.end(), current_page_table->pages.begin() + (codeseg_addr / page_size));
        std::copy(codeseg_pointers.begin(), codeseg_pointers.end(), current_page_table->pointers.begin() + (codeseg_addr / page_size));

        std::copy(global_pages.begin(), global_pages.begin() + (shared_size / page_size), current_page_table->pages.begin() + (shared_addr / page_size));
        std::copy(global_pointers.begin(), global_pointers.begin() + (shared_size / page_size), current_page_table->pointers.begin() + (shared_addr / page_size));

        if (!current_page_table->pointers[rom_addr / page_size]) {
            current_page_table->pointers[rom_addr / page_size] = reinterpret_cast<uint8_t *>(rom_map);

            page rom_page;
            rom_page.sts = page_status::committed;
            rom_page.generation = 250;
            rom_page.page_protection = prot::read_exec;

            current_page_table->pages[rom_addr / page_size] = rom_page;

            for (size_t i = 1; i < common::align(rom_size, page_size) / page_size; i++) {
                current_page_table->pointers[(rom_addr / page_size) + i] = current_page_table->pointers[(rom_addr / page_size) + i - 1] + page_size;
                current_page_table->pages[(rom_addr / page_size) + i] = rom_page;
            }
        }

        if (previous_page_table) {
            const std::uint32_t offset_page_local = local_data / page_size;
            const std::uint32_t offset_page_local_end = shared_data / page_size;

            for (std::uint32_t i = offset_page_local_end; i >= offset_page_local; i--) {
                address beg_addr = (i + 1) * page_size;
                address total_page = 0;

                while (previous_page_table->pointers[i] && previous_page_table->pages[i].sts == page_status::committed
                    && i >= offset_page_local) {
                    total_page++;
                    i--;
                    beg_addr -= page_size;
                }

                if (total_page) {
                    cpu->unmap_memory(beg_addr, total_page * page_size);
                }
            }

            for (std::uint32_t i = offset_page_local_end; i >= offset_page_local; i--) {
                address beg_addr = (i + 1) * page_size;
                address total_page = 0;

                if (current_page_table->pointers[i] && current_page_table->pages[i].sts == page_status::committed) {
                    total_page++;
                    beg_addr -= page_size;

                    while (current_page_table->pointers[i - 1] + page_size == current_page_table->pointers[i]) {
                        total_page++;
                        beg_addr -= page_size;
                        i--;
                    }
                }

                if (total_page) {
                    cpu->map_backing_mem(beg_addr, total_page * page_size, current_page_table->pointers[beg_addr / page_size],
                        current_page_table->pages[beg_addr / page_size].page_protection);
                }
            }
        }

        cpu->page_table_changed();
    }
}
