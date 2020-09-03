#pragma once

#include <common/types.h>
#include <kernel/kernel_obj.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace eka2l1 {
    using kernel_obj_ptr = kernel::kernel_obj *;
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
            size_t totals;

            uint32_t make_handle(size_t index);

            kernel_system *kern;

        public:
            explicit object_ix() {}
            explicit object_ix(kernel_system *kern, handle_array_owner owner);

            void do_state(common::chunkyseri &seri);

            /**
             * @brief Reset the container state to deref all opened objects.
             */
            void reset();

            /**
             * @brief   Add new object to the container.
             * 
             * @param   obj     Pointer to the kernel object.
             * @returns Handle to the object
            */
            std::uint32_t add_object(kernel_obj_ptr obj);

            /**
             * @brief   Duplicate an existing object handle. 
             * 
             * @param   handle The existing handle to duplicate.
             * 
             * @returns New handle. 0 if fail to create.
            */
            std::uint32_t duplicate(uint32_t handle);

            /**
             * @brief   Get the kernel object reference by the handle.
             * 
             * @param   handle  The handle to extract kernel object from.
             * @returns The kernel object referenced. Nullptr if there is none found.
            */
            kernel_obj_ptr get_object(uint32_t handle);

            int close(uint32_t handle);

            std::uint64_t unique_id() const {
                return uid;
            }

            std::size_t total_open() const {
                return totals;
            }

            /*! \brief Get the last handle created. 0 if none left */
            std::uint32_t last_handle();

            /**
             * @brief   Check if the container currently has this object opened.
             * 
             * @param   obj         Pointer to this kernel object.
             * @returns True if this container has given object.
             */
            bool has(kernel_obj_ptr obj);

            /**
             * @brief   Count the number of times this object appeared in the handle list.
             * 
             * This function counts the number of time a kernel object has been opened in this object container.
             * Operates by iterating through all the container and searchs if this kernel object is associated with an index.
             * 
             * This function returns 0 if the object does not appear at all.
             * 
             * This function differs from the "has" function by searching through all index non-stop, while if "has" has found
             * the object, it stops.
             * 
             * @param   obj     Pointer to the kernel object to be counted.
             * @returns Number of times this object has appeared in this container
             * 
             * @see     has
             */
            std::uint32_t count(kernel_obj_ptr obj);
        };
    }
}
