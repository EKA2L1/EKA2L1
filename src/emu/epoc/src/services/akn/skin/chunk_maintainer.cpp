#include <epoc/services/akn/skin/chunk_maintainer.h>
#include <epoc/kernel/chunk.h>

namespace eka2l1::epoc {
    constexpr std::int64_t AKNS_CHUNK_ITEM_DEF_HASH_BASE_SIZE_GRAN = -4;
    constexpr std::int64_t AKNS_CHUNK_ITEM_DEF_AREA_BASE_SIZE_GRAN = 11;
    constexpr std::int64_t AKNS_CHUNK_DATA_AREA_BASE_SIZE_GRAN = 20;
    constexpr std::int64_t AKNS_CHUNK_FILENAME_AREA_BASE_SIZE_GRAN = 1;
    constexpr std::int64_t AKNS_CHUNK_SCALEABLE_GFX_AREA_BASE_SIZE_GRAN = 1;

    akn_skin_chunk_maintainer::akn_skin_chunk_maintainer(kernel::chunk *shared_chunk, const std::size_t granularity)
        : shared_chunk_(shared_chunk)
        , current_granularity_off_(0)
        , max_size_gran_(0)
        , granularity_(granularity) {
        // Calculate max size this chunk can hold (of course, in granularity meters)
        max_size_gran_ = shared_chunk_->max_size() / granularity;

        // Add areas according to normal configuration
        add_area(akn_skin_chunk_area_base_offset::item_def_hash_base, AKNS_CHUNK_ITEM_DEF_HASH_BASE_SIZE_GRAN);
        add_area(akn_skin_chunk_area_base_offset::item_def_area_base, AKNS_CHUNK_ITEM_DEF_AREA_BASE_SIZE_GRAN);
        add_area(akn_skin_chunk_area_base_offset::data_area_base, AKNS_CHUNK_DATA_AREA_BASE_SIZE_GRAN);
        add_area(akn_skin_chunk_area_base_offset::gfx_area_base, AKNS_CHUNK_SCALEABLE_GFX_AREA_BASE_SIZE_GRAN);
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

    akn_skin_chunk_maintainer::akn_skin_chunk_area *akn_skin_chunk_maintainer::get_area_info(const akn_skin_chunk_area_base_offset area_type) {
        if (static_cast<int>(area_type) % 3 != 0) {
            // Don't satisfy the condition
            return nullptr;
        }

        // Search for the area.
        const auto search_result = std::lower_bound(areas_.begin(), areas_.end(), area_type,
            [](const akn_skin_chunk_area &lhs, const akn_skin_chunk_area_base_offset off) {
                return lhs.base_ < off;
            });

        if (search_result == areas_.end()) {
            return nullptr;
        }

        return &(*search_result);
    }
    
    const std::size_t akn_skin_chunk_maintainer::get_area_size(const akn_skin_chunk_area_base_offset area_type) {
        akn_skin_chunk_area *area = get_area_info(area_type);

        if (!area) {
            // We can't get the area. Abort!!!
            return static_cast<std::size_t>(-1);
        }

        if (area->gran_size_ < 0) {
            return granularity_ / area->gran_size_;
        }

        return granularity_ * area->gran_size_;
    }

    void *akn_skin_chunk_maintainer::get_area_base(const akn_skin_chunk_area_base_offset area_type, 
        std::uint64_t *offset_from_begin) {
         akn_skin_chunk_area *area = get_area_info(area_type);

        if (!area) {
            // We can't get the area. Abort!!!
            return nullptr;
        }

        // Check for the optional pointer, if valid, write the offset of the area from the beginning of the chunk
        // to it
        if (offset_from_begin) {
            *offset_from_begin = area->gran_off_ * granularity_;
        }

        return reinterpret_cast<std::uint8_t*>(shared_chunk_->host_base()) + (area->gran_off_ * granularity_);
    }
}