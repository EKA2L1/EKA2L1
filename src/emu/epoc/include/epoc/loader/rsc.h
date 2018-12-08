#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <common/buffer.h>

namespace eka2l1 {
    class file;
    using symfile = std::shared_ptr<file>;

    class rsc_file_read_stream {
        std::vector<std::uint8_t> buffer;
        common::ro_buf_stream stream;

	protected:
        bool do_decompress(symfile f);

    public:
        explicit rsc_file_read_stream(symfile f, bool *success = nullptr);

        template <typename T>
        std::optional<T> read();

        /*! \brief Read to the specified buffer with given length. 
		 *
		 *  \returns False if stream is end, fail.
		 */
        bool read_unsafe(std::uint8_t *buffer, const std::size_t len);

        void rewind(const std::size_t len);

        void advance(const std::size_t len);
    };
}