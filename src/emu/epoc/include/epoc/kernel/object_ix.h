#pragma once

#include <common/types.h>
#include <epoc/kernel/kernel_obj.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace eka2l1 {
    using kernel_obj_ptr = kernel::kernel_obj*;
    class kernel_system;

    namespace kernel {
        enum class special_handle_type : uint32_t {
            kernel,
            crr_process = 0xFFFF8000,
            crr_thread = 0xFFFF8001
        };

        struct handle_inspect_info {
            bool handle_array_local;
            bool handle_array_kernel;

            int object_ix_index;
            int object_ix_next_instance;
            bool no_close;

            bool special;
            special_handle_type special_type;
        };

        handle_inspect_info inspect_handle(uint32_t handle);

        enum class handle_array_owner {
            thread,
            process,
            kernel
        };

        struct object_ix_record {
            kernel_obj_ptr object;
            uint32_t associated_handle;
            bool free = true;
        };

        /*! \brief The ultimate object handles holder. */
        class object_ix {
            uint64_t uid;

            size_t next_instance;

            std::array<object_ix_record, 0x100> objects;
            std::vector<std::uint32_t> handles;

            handle_array_owner owner;

            uint32_t make_handle(size_t index);

            kernel_system *kern;

        public:
            object_ix() {}
            object_ix(kernel_system *kern, handle_array_owner owner);

            void do_state(common::chunkyseri &seri);

            /*! \brief Add new object to the ix. 
             * \returns Handle to the object
            */
            std::uint32_t add_object(kernel_obj_ptr obj);

            /*! \brief Duplicate an existing object handle. 
                \returns New handle. 0 if fail to create.
            */
            std::uint32_t duplicate(uint32_t handle);

            /*! \brief Get the kernel object reference by the handle. 
                \returns The kernel object referenced. Nullptr if there is none found.
            */
            kernel_obj_ptr get_object(uint32_t handle);

            int close(uint32_t handle);

            uint64_t unique_id() {
                return uid;
            }

            /*! \brief Get the last handle created. 0 if none left */
            std::uint32_t last_handle();
        };
    }
}
