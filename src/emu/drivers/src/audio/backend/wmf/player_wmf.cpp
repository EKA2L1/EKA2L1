/*
 * Copyright (c) 2020 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <common/platform.h>

#if EKA2L1_PLATFORM(WIN32)

#pragma comment(lib, "Mfplat.lib")
#pragma comment(lib, "Mfreadwrite.lib")
#pragma comment(lib, "Mfuuid.lib")
#pragma comment(lib, "Propsys.lib")

#include <drivers/audio/backend/wmf/player_wmf.h>
#include <drivers/audio/audio.h>

#include <common/log.h>
#include <common/cvt.h>

#include <propvarutil.h>

namespace eka2l1::drivers {
    template <class T>
    void SafeRelease(T **ppT) {
        if (*ppT) {
            (*ppT)->Release();
            *ppT = NULL;
        }
    }

    player_wmf::player_wmf(audio_driver *driver)
        : aud_(driver) {
        HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);

        // Startup media foundation!
        hr = MFStartup(MF_VERSION);
    }

    void player_wmf::get_more_data(player_wmf_request &request) {
        // Data drain, try to get more
        if (request.type_ == player_wmf_request_format) {
            if (request.flags_ & 1) {
                output_stream_->stop();
                return;
            }

            DWORD stream_flags = 0;
            IMFSample *samples = nullptr;

            // Read from wmf source reader
            HRESULT hr = request.reader_->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0,
                nullptr, &stream_flags, nullptr, &samples);

            if ((!samples) || (stream_flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
                request.flags_ |= 1;
            }

            if (samples && SUCCEEDED(hr)) {
                IMFMediaBuffer *media_buffer = nullptr;
                hr = samples->ConvertToContiguousBuffer(&media_buffer);

                if (SUCCEEDED(hr)) {
                    DWORD buffer_length = 0;
                    BYTE *buffer_pointer = nullptr;

                    hr = media_buffer->Lock(&buffer_pointer, nullptr, &buffer_length);

                    std::size_t start_to_copy = 0;

                    if (request.use_push_new_data_) {
                        start_to_copy = request.data_.size();
                        request.use_push_new_data_ = false;
                    }

                    request.data_.resize(start_to_copy + buffer_length);
                    std::memcpy(request.data_.data() + start_to_copy, buffer_pointer, buffer_length);

                    hr = media_buffer->Unlock();
                    SafeRelease(&media_buffer);
                }

                SafeRelease(&samples);
            }
        }

        request.data_pointer_ = 0;
    }
    
    std::size_t player_wmf::data_supply_callback(std::int16_t *data, std::size_t size) {
        // Get the oldest request
        const std::lock_guard<std::mutex> guard(request_queue_lock_);
        player_wmf_request &request = requests_.front();

        std::size_t frame_copied = 0;

        auto supply_stuff = [&]() {
            while ((frame_copied < size) && (!(request.flags_ & 1))) {
                if (request.data_.size() == request.data_pointer_)
                    get_more_data(request);

                const std::size_t total_frame_left = (request.data_.size() - request.data_pointer_) / request.channels_ / sizeof(std::uint16_t);
                const std::size_t frame_to_copy = std::min<std::size_t>(total_frame_left, size);

                // Copy the frames first
                if (request.channels_ == 1) {
                    const std::uint16_t *original_data_u16 = reinterpret_cast<std::uint16_t*>(request.data_.data() + request.data_pointer_);

                    // We have to do something!
                    for (std::size_t frame_ite = 0; frame_ite < frame_to_copy; frame_ite++) {
                        data[2 * frame_ite] = original_data_u16[frame_ite];
                        data[2 * frame_ite + 1] = original_data_u16[frame_ite];
                    }
                } else {
                    // Well, just copy straight up
                    std::memcpy(data, request.data_.data() + request.data_pointer_, frame_to_copy * 2 * sizeof(std::uint16_t));
                }

                request.data_pointer_ += frame_to_copy * request.channels_ * sizeof(std::uint16_t);
                frame_copied = frame_to_copy;
            }
        };

        supply_stuff();
        
        if ((frame_copied < size) || (request.flags_ & 1)) {
            bool no_more_way = false;

            // There is no more data for us! Either repeat or kill
            if (request.repeat_left_ == 0) {
                no_more_way = true;
            } else {
                // Seek back to do a loop. Intentionally left this so that negative repeat can do infinite loop
                if (request.repeat_left_ > 0) {
                    request.repeat_left_ -= 1;
                }

                // Reset the stream if we are the custom format guy!
                if (request.type_ == player_wmf_request_format) {
                    PROPVARIANT var = { 0 };
                    var.vt = VT_I8;
                    request.reader_->SetCurrentPosition(GUID_NULL, var);
                }

                request.data_pointer_ = 0;
                request.flags_ = 0;

                // We want to supply silence samples
                // 1s = 1ms
                const std::size_t silence_samples = request.freq_ * request.silence_micros_ / 1000000;
                request.data_.resize(silence_samples * sizeof(std::uint16_t) * request.channels_);
                std::fill(request.data_.begin(), request.data_.end(), 0);

                request.use_push_new_data_ = true;
                supply_stuff();
            }

            // We are drained (out of frame)
            // Call the finish callback
            if (no_more_way && callback_)
                callback_(userdata_.data());
        }

        return frame_copied;
    }

    static bool configure_stream_for_pcm(player_wmf_request &request) {
        IMFMediaType *partial_type = nullptr;

        // We want to set our output type to PCM16
        HRESULT hr = MFCreateMediaType(&partial_type);

        if (!SUCCEEDED(hr)) {
            LOG_ERROR("Unable to create new media type to specify output stream to PCM16");
            return false;
        }

        hr = partial_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);

        if (SUCCEEDED(hr)) {
            hr = partial_type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
        }

        if (SUCCEEDED(hr)) {
            hr = request.reader_->SetCurrentMediaType(
                (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, partial_type);
        }

        // Set the stream selection to be first one, onlyyyyy
        hr = request.reader_->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, false);

        if (SUCCEEDED(hr)) {
            hr = request.reader_->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, true);
        }

        IMFMediaType *media_type_on_stream = nullptr;
        request.reader_->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &media_type_on_stream);

        media_type_on_stream->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &request.channels_);
        media_type_on_stream->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &request.freq_);

        SafeRelease(&partial_type);
        SafeRelease(&media_type_on_stream);

        return true;
    }

    static bool query_wmf_reader_metadata(IMFSourceReader *reader, std::vector<player_wmf_metadata> &meta_arr) {
        IMFMetadataProvider *meta_provider = nullptr;
        HRESULT hr = reader->GetServiceForStream(MF_SOURCE_READER_MEDIASOURCE, GUID_NULL,
            __uuidof(meta_provider), reinterpret_cast<LPVOID*>(&meta_provider));

        if (!SUCCEEDED(hr)) {
            LOG_ERROR("Unable to get meta provider service for reader stream!");
            return false;
        }

        IMFMetadata *metas = nullptr;
        hr = meta_provider->QueryInterface(IID_PPV_ARGS(&metas));

        SafeRelease(&meta_provider);

        if (!SUCCEEDED(hr)) {
            LOG_ERROR("Unable to query metadata interface!");
            return false;
        }

        PROPVARIANT metadata_keys;
        hr = metas->GetAllPropertyNames(&metadata_keys);

        if (!SUCCEEDED(hr)) {
            LOG_ERROR("Unable to get all property names from metadata interface");
            SafeRelease(&metas);

            return false;
        }

        PWSTR *metadata_key_strings = nullptr;
        ULONG metadata_key_total = 0;

        hr = PropVariantToStringVectorAlloc(metadata_keys, &metadata_key_strings, &metadata_key_total);

        if (!SUCCEEDED(hr)) {
            LOG_ERROR("Unable to convert prop variant to string vector!");
            SafeRelease(&metas);
            return false;
        }

        for (ULONG i = 0; i < metadata_key_total; i++) {
            PWSTR metadata_key_string = metadata_key_strings[i];
            PROPVARIANT metadata_value;

            hr = metas->GetProperty(metadata_key_string, &metadata_value);

            if (SUCCEEDED(hr)) {
                PWSTR metadata_value_string = nullptr;
                PropVariantToStringAlloc(metadata_value, &metadata_value_string);

                if (metadata_value_string) {
                    player_wmf_metadata meta;
                    meta.key_ = common::ucs2_to_utf8(std::u16string(reinterpret_cast<const char16_t*>(metadata_key_string)));
                    meta.value_ = common::ucs2_to_utf8(std::u16string(reinterpret_cast<const char16_t*>(metadata_value_string)));

                    meta_arr.push_back(meta);
                }

                CoTaskMemFree(metadata_value_string);
                PropVariantClear(&metadata_value);
            }
        }

        CoTaskMemFree(metadata_key_strings);
        PropVariantClear(&metadata_keys);

        return true;
    }

    bool player_wmf::queue_url(const std::string &url) {
        player_wmf_request request;
        request.type_ = player_wmf_request_format;
        request.data_pointer_ = 0;
        request.url_ = url;

        const std::u16string url_16 = common::utf8_to_ucs2(url);

        HRESULT hr = MFCreateSourceReaderFromURL(
            reinterpret_cast<LPCWSTR>(url_16.c_str()), nullptr, &request.reader_);

        if (!SUCCEEDED(hr)) {
            LOG_ERROR("Unable to queue new play URL {} (can't open source reader)", url);
            return false;
        }
        
        if (!configure_stream_for_pcm(request)) {
            LOG_ERROR("Error while configure WMF stream!");
            SafeRelease(&request.reader_);
            return false;
        }

        query_wmf_reader_metadata(request.reader_, metadatas_);
        
        const std::lock_guard<std::mutex> guard(request_queue_lock_);
        requests_.push(request);

        return true;
    }

    bool player_wmf::queue_data(const char *raw_data, const std::size_t data_size,
        const std::uint32_t encoding_type, const std::uint32_t frequency,
        const std::uint32_t channels) {
        player_wmf_request request;
        request.type_ = player_wmf_request_raw_pcm;
        request.data_pointer_ = 0;

        if (encoding_type == player_audio_encoding_custom) {
            request.type_ = player_wmf_request_format;

            IMFByteStream *stream = NULL;
            
            HRESULT hr = MFCreateTempFile(MF_ACCESSMODE_READWRITE, MF_OPENMODE_DELETE_IF_EXIST,
                MF_FILEFLAGS_NONE, &stream);

            ULONG bytes_written = 0;
            stream->Write(reinterpret_cast<const BYTE*>(raw_data), static_cast<ULONG>(data_size),
                &bytes_written);

            stream->SetCurrentPosition(0);

            hr = MFCreateSourceReaderFromByteStream(stream, nullptr, &request.reader_);

            if (!SUCCEEDED(hr)) {
                LOG_ERROR("Unable to queue new custom data {} (can't open source reader)");
                return false;
            }
            
            if (!configure_stream_for_pcm(request)) {
                LOG_ERROR("Error while configure WMF stream!");
                SafeRelease(&request.reader_);
                return false;
            }

            SafeRelease(&stream);    
            query_wmf_reader_metadata(request.reader_, metadatas_);
        }

        const std::lock_guard<std::mutex> guard(request_queue_lock_);
        requests_.push(request);

        if (encoding_type != player_audio_encoding_custom) {
            player_wmf_request &request_ref = requests_.back();
            request_ref.channels_ = channels;
            request_ref.encoding_ = encoding_type;
            request_ref.freq_ = frequency;
            request_ref.reader_ = nullptr;

            request_ref.data_.resize(data_size);

            std::memcpy(request_ref.data_.data(), raw_data, data_size);
        }

        return true;
    }

    bool player_wmf::play() {
        // Stop previous session
        if (output_stream_)
            output_stream_->stop();

        // Reset the request
        {
            const std::lock_guard<std::mutex> guard(request_queue_lock_);
            player_wmf_request &request = requests_.front();

            PROPVARIANT var = { 0 };
            var.vt = VT_I8;
            request.reader_->SetCurrentPosition(GUID_NULL, var);

            request.data_pointer_ = 0;
            request.flags_ = 0;
            request.data_.clear();
            
            // New stream to restart everything     
            output_stream_ = aud_->new_output_stream(request.freq_, [this](std::int16_t *u1, std::size_t u2) {
                return data_supply_callback(u1, u2);
            });

            output_stream_->set_volume(static_cast<float>(volume_) / 100.0f);
        }

        return output_stream_->start();
    }

    bool player_wmf::stop() {
        if (output_stream_)
            return output_stream_->stop();

        return true;
    }

    bool player_wmf::notify_any_done(finish_callback callback, std::uint8_t *data, const std::size_t data_size) {
        const std::lock_guard<std::mutex> guard(request_queue_lock_);
        return player::notify_any_done(callback, data, data_size);
    }
    
    void player_wmf::clear_notify_done() {
        std::unique_lock<std::mutex> guard(request_queue_lock_, std::try_to_lock);
        return player::clear_notify_done();
    }

    void player_wmf::set_repeat(const std::int32_t repeat_times, const std::uint64_t silence_intervals_micros) {
        const std::lock_guard<std::mutex> guard(request_queue_lock_);
        player_wmf_request &request = requests_.front();

        request.repeat_left_ = repeat_times;
        request.silence_micros_ = silence_intervals_micros;
    }
}

#endif