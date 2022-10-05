#include <drivers/audio/stream.h>
#include <drivers/audio/audio.h>

namespace eka2l1::drivers {
    audio_output_stream::audio_output_stream(audio_driver *driver, const std::uint32_t sample_rate, const std::uint8_t channels)
        : driver_(driver)
        , grand_volume_change_callback_handle_(0)
        , sample_rate(sample_rate)
        , channels(channels) {
        grand_volume_change_callback_handle_ = driver_->add_master_volume_change_callback([this](const std::uint32_t oldvol, const std::uint32_t newvol) {
            set_volume(get_volume());
        });
    }

    audio_output_stream::~audio_output_stream() {
        driver_->remove_master_volume_change_callback(grand_volume_change_callback_handle_);
    }

    audio_input_stream::audio_input_stream(audio_driver *driver, const std::uint32_t sample_rate, const std::uint8_t channels)
        : driver_(driver)
        , sample_rate_(sample_rate)
        , channels_(channels) {

    }
}