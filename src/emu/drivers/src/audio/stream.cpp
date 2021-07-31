#include <drivers/audio/stream.h>
#include <drivers/audio/audio.h>

namespace eka2l1::drivers {
    audio_output_stream::audio_output_stream(audio_driver *driver)
        : driver_(driver)
        , grand_volume_change_callback_handle_(0) {
        grand_volume_change_callback_handle_ = driver_->add_master_volume_change_callback([this](const std::uint32_t oldvol, const std::uint32_t newvol) {
            set_volume(get_volume());
        });
    }

    audio_output_stream::~audio_output_stream() {
        driver_->remove_master_volume_change_callback(grand_volume_change_callback_handle_);
    }
}