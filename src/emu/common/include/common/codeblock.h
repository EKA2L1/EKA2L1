// Copyright 2013 Dolphin Emulator Project, PPSSPP Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <cstdint>

#include <common/log.h>
#include <common/virtualmem.h>

namespace eka2l1::common {
    // Everything that needs to generate code should inherit from this.
    // You get memory management for free, plus, you can use all emitter functions without
    // having to prefix them with gen-> or something similar.
    // Example implementation:
    // class JIT : public CodeBlock<ARMXEmitter>, public JitInterface {}

    class codeblock_common {
    public:
        codeblock_common()
            : region(nullptr)
            , region_size(0) {}
        virtual ~codeblock_common() {}

        bool is_in_space(const std::uint8_t *ptr) {
            return (ptr >= region) && (ptr < (region + region_size));
        }

        virtual void set_code_ptr(std::uint8_t *ptr) = 0;
        virtual const std::uint8_t *get_code_ptr() const = 0;

        std::uint8_t *get_base_ptr() {
            return region;
        }

        size_t get_offset(const std::uint8_t *ptr) const {
            return ptr - region;
        }

    protected:
        std::uint8_t *region;
        size_t region_size;
    };

    template <class T>
    class codeblock : public codeblock_common, public T {
    private:
        codeblock(const codeblock &) = delete;
        void operator=(const codeblock &) = delete;

        // A privately used function to set the executable RAM space to something invalid.
        // For debugging usefulness it should be used to set the RAM to a host specific breakpoint instruction
        virtual void poision_memory(int offset) = 0;

    public:
        codeblock()
            : writeStart_(nullptr) {}
        virtual ~codeblock() {
            if (region) {
                free_codespace();
            }
        }

        // Call this before you generate any code.
        void alloc_codespace(int size) {
            region_size = size;
            // The protection will be set to RW if PlatformIsWXExclusive.
            region = (std::uint8_t *)map_memory(region_size);
            T::set_code_pointer(region);
        }

        // Always clear code space with breakpoints, so that if someone accidentally executes
        // uninitialized, it just breaks into the debugger.
        void clear_codespace(int offset) {
            if (is_memory_wx_exclusive()) {
                change_protection(region, region_size, prot_read_write);
            }

            // If not WX Exclusive, no need to call ProtectMemoryPages because we never change the protection from RWX.
            poision_memory(offset);
            reset_codeptr(offset);

            if (is_memory_wx_exclusive()) {
                // Need to re-protect the part we didn't clear.
                change_protection(region, offset, prot_read_exec);
            }
        }

        // BeginWrite/EndWrite assume that we keep appending.
        // If you don't specify a size and we later encounter an executable non-writable block, we're screwed.
        // These CANNOT be nested. We rely on the memory protection starting at READ|WRITE after start and reset.
        void begin_write(size_t sizeEstimate = 1) {
            if (writeStart_) {
                LOG_ERROR(COMMON, "Can't do nested code block write");
            }

            // In case the last block made the current page exec/no-write, let's fix that.
            if (is_memory_wx_exclusive()) {
                writeStart_ = get_writeable_code_ptr();
                change_protection(writeStart_, sizeEstimate, prot_read_write);
            }
        }

        void end_write() {
            // OK, we're done. Re-protect the memory we touched.
            if (is_memory_wx_exclusive() && writeStart_ != nullptr) {
                const uint8_t *end = get_code_ptr();
                change_protection(writeStart_, end - writeStart_, prot_read_exec);
                writeStart_ = nullptr;
            }
        }

        // Call this when shutting down. Don't rely on the destructor, even though it'll do the job.
        void free_codespace() {
            change_protection(region, region_size, prot_read_write);
            unmap_memory(region, region_size);

            region = nullptr;
            region_size = 0;
        }

        void set_code_ptr(std::uint8_t *ptr) override {
            T::set_code_pointer(ptr);
        }

        const std::uint8_t *get_code_ptr() const override {
            return T::get_code_pointer();
        }

        void reset_codeptr(int offset) {
            T::set_code_pointer(region + offset);
        }

        size_t get_space_left() const {
            return region_size - (T::get_code_pointer() - region);
        }

    private:
        std::uint8_t *writeStart_;
    };
}