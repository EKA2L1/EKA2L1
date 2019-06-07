#include <epoc/services/akn/skin/chunk_maintainer.h>
#include <epoc/kernel/chunk.h>

namespace eka2l1::epoc {
    akn_skin_chunk_maintainer::akn_skin_chunk_maintainer(kernel::chunk *shared_chunk, const std::size_t granularity)
        : shared_chunk_(shared_chunk)
        , current_granularity_off_(0)
        , max_size_gran_(0)
        , granularity_(granularity) {
        // Calculate max size this chunk can hold (of course, in granularity meters)
        max_size_gran_ = shared_chunk_->max_size() / granularity;
    }

    bool akn_skin_chunk_maintainer::add_area(const akn_skin_chunk_area_base_offset offset_type, 
        const std::int64_t allocated_size_gran) {
        // Enum value "*_base" always aligned with 3
        if (static_cast<int>(offset_type) % 3 != 0) {
            return false;
        }

        akn_skin_chunk_area area;
        area.base_ = offset_type;
        area.gran_off_ = current_granularity_off_;
        area.gran_size_ = allocated_size_gran;

        // Check if the area exists before
        if (std::binary_search(areas_.begin(), areas_.end(), area, [](const akn_skin_chunk_area &lhs, const akn_skin_chunk_area &rhs) {
            return lhs.base_ == rhs.base_; })) {
            // Cancel
            return false;
        }

        // Check if memory is sufficient enough for this area.
        if (current_granularity_off_ + allocated_size_gran > max_size_gran_) {
            return false;
        }

        // Get chunk base
        std::uint32_t *base = reinterpret_cast<std::uint32_t*>(shared_chunk_->host_base());

        if (!areas_.empty() && base[static_cast<int>(areas_[0].base_) + 1] <= 4 * 3) {
            // We can eat more first area memory for the header. Abort!
            return false;
        }

        // Set area base offset
        base[static_cast<int>(offset_type)] = static_cast<std::uint32_t>(current_granularity_off_ *
            granularity_);
        
        // Set area allocated size
        if (allocated_size_gran < 0) {
            base[static_cast<int>(offset_type) + 1] = static_cast<std::uint32_t>(granularity_ / 
                allocated_size_gran);
        } else {    
            base[static_cast<int>(offset_type) + 1] = static_cast<std::uint32_t>(allocated_size_gran * 
                granularity_);
        }

        // Clear area current size to 0
        base[static_cast<int>(offset_type) + 2] = 0;

        // Add the area to area list, then sort
        areas_.emplace_back(area);
        std::sort(areas_.begin(), areas_.end(), [](const akn_skin_chunk_area &lhs, const akn_skin_chunk_area &rhs) {
            return static_cast<int>(lhs.base_) < static_cast<int>(rhs.base_);
        });

        // We need to make offset align (for faster memory access)
        // Modify the first area offset to be after the header
        // Each area header contains 3 fields: offset, allocated size, current size, each field is 4 bytes
        // We also need to modify the size too.
        base[static_cast<int>(areas_[0].base_)] = static_cast<std::uint32_t>(areas_.size() * 3 * 4);
        base[static_cast<int>(areas_[0].base_) + 1] -= static_cast<std::uint32_t>(areas_.size() * 3 * 4);

        return true;
    }
}