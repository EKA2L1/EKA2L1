#include <core.h>
#include <epoc/hal.h>

#include <common/e32inc.h>
#include <e32err.h>

#define REGISTER_HAL_FUNC(op, hal_name, func) \
    funcs.emplace(op, std::bind(&##hal_name::##func, this, std::placeholders::_1, std::placeholders::_2))

#define REGISTER_HAL(sys, cage, hal_name) \
    sys->add_new_hal(cage, std::make_shared<##hal_name>(sys))

namespace eka2l1::epoc {
    hal::hal(eka2l1::system *sys)
        : sys(sys) {}

    int hal::do_hal(uint32_t func, int *a1, int *a2) {
        auto func_pair = funcs.find(func);

        if (func_pair == funcs.end()) {
            return false;
        }

        return func_pair->second(a1, a2);
    }

    /*! \brief Kernel HAL cagetory. 
     * Contains HAL of drivers, memory, etc...
     */
    struct kern_hal : public eka2l1::epoc::hal {
        /*! \brief Get the size of a page. 
         *
         * \param a1 The pointer to the integer destination, supposed to
         *           contains the page size.
         * \param a2 Unused.
         */
        int page_size(int *a1, int *a2) {
            *a1 = static_cast<int>(sys->get_memory_system()->get_page_size());
            return KErrNone;
        }

        kern_hal(eka2l1::system *sys)
            : hal(sys) {
            REGISTER_HAL_FUNC(EKernelHalPageSizeInBytes, kern_hal, page_size);
        }
    };

    void init_hal(eka2l1::system *sys) {
        REGISTER_HAL(sys, 0, kern_hal);
    }

    int do_hal(eka2l1::system *sys, uint32_t cage, uint32_t func, int *a1, int *a2) {
        hal_ptr hal_com = sys->get_hal(cage);

        if (!hal_com) {
            return KErrNotFound;
        }

        return hal_com->do_hal(func, a1, a2);
    }
}