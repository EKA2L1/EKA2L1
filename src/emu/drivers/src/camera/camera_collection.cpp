#include <drivers/camera/camera_collection.h>
#include <drivers/camera/backend/null/camera_collection_null.h>

#include <common/platform.h>

#include <type_traits>

#if EKA2L1_PLATFORM(ANDROID)
#include <drivers/camera/backend/android/camera_collection_android.h>
#elif EKA2L1_PLATFORM(IOS)
#include <TargetConditionals.h>

#include <drivers/camera/backend/ios/camera_ios.h>

#if TARGET_OS_SIMULATOR
#include <drivers/camera/backend/ios/camera_simulator.h>
#endif
#endif

namespace eka2l1::drivers::camera {
    // [expr.delete]/3: deleting a derived object through a base pointer without
    // a virtual destructor is undefined.
    static_assert(std::has_virtual_destructor_v<collection>,
        "camera::collection is owned polymorphically and must be destroyed polymorphically");

    std::unique_ptr<collection> collection_detail = nullptr;

    collection *get_collection() {
        if (collection_detail == nullptr) {
#if EKA2L1_PLATFORM(ANDROID)
            collection_detail = std::make_unique<collection_android>();
#elif EKA2L1_PLATFORM(IOS)
            collection_detail = std::make_unique<collection_ios>();

#if TARGET_OS_SIMULATOR
            // AVFoundation exposes no capture device in the simulator, so the
            // real backend would report zero cameras and no guest would ever
            // reach the ECam path. Fall back to the test-pattern backend. The
            // check is not simply "we are in the simulator": if a future
            // simulator ever does surface a device, the real one still wins.
            if (collection_detail->count() == 0) {
                collection_detail = std::make_unique<collection_simulator>();
            }
#endif
#else
            collection_detail = std::make_unique<collection_null>();
#endif
        }

        return collection_detail.get();
    }
}