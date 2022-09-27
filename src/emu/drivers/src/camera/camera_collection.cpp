#include <drivers/camera/camera_collection.h>
#include <drivers/camera/backend/null/camera_collection_null.h>

#include <common/platform.h>

#if EKA2L1_PLATFORM(ANDROID)
#include <drivers/camera/backend/android/camera_collection_android.h>
#endif

namespace eka2l1::drivers::camera {
    std::unique_ptr<collection> collection_detail = nullptr;

    collection *get_collection() {
        if (collection_detail == nullptr) {
#if EKA2L1_PLATFORM(ANDROID)
            collection_detail = std::make_unique<collection_android>();
#else
            collection_detail = std::make_unique<collection_null>();
#endif
        }

        return collection_detail.get();
    }
}