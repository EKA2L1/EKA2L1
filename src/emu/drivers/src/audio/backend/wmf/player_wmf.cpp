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

#pragma comment(lib, "Mf.lib")
#pragma comment(lib, "Mfplat.lib")
#pragma comment(lib, "Mfreadwrite.lib")
#pragma comment(lib, "Mfuuid.lib")
#pragma comment(lib, "Propsys.lib")

#include <drivers/audio/backend/wmf/player_wmf.h>

#include <common/buffer.h>
#include <common/fileutils.h>
#include <common/path.h>
#include <common/cvt.h>
#include <common/log.h>

#include <propvarutil.h>

namespace eka2l1::drivers {
    template <class T>
    void SafeRelease(T **ppT) {
        if (*ppT) {
            (*ppT)->Release();
            *ppT = NULL;
        }
    }

    template <class T>
    T *GetElement(IMFCollection *collection, const DWORD index) {
        IUnknown *out_unk_obj = nullptr;
        T *out_mt = nullptr;

        HRESULT result = collection->GetElement(index, &out_unk_obj);

        if (result != S_OK) {
            return nullptr;
        }

        result = out_unk_obj->QueryInterface(IID_PPV_ARGS(&out_mt));

        if (result != S_OK) {
            return nullptr;
        }

        return out_mt;
    }

    /**
     * @brief EKA2L1's RW stream wrapper.
     * 
     * Implementation reference from bradenmcd's gist on GitHub. ID link 9106d2e1c46f40bca140.
     */
    class rw_stream_com: public IStream {
    protected:
        common::rw_stream *stream_;
        LONG count_;
    
    public:
        explicit rw_stream_com(common::rw_stream *stream)
            : stream_(stream)
            , count_(1) {
        }

        rw_stream_com(const rw_stream_com &) = delete;
        rw_stream_com & operator=(const rw_stream_com &) = delete;

        virtual HRESULT __stdcall QueryInterface(const IID & iid, void ** ppv) {
            if (!ppv) {
                return E_INVALIDARG;
            }

            if ((iid == __uuidof(IUnknown)) || (iid == __uuidof(IStream)) || (iid == __uuidof(ISequentialStream))) {
                *ppv = static_cast<IStream *>(this);
                AddRef();

                return S_OK;
            }

            return E_NOINTERFACE;
        }

        virtual ULONG __stdcall AddRef() {
            return ::InterlockedIncrement(&count_);
        }

        virtual ULONG __stdcall Release() {
            if (::InterlockedDecrement(&count_) == 0) {
                delete this;
                return 0;
            }

            return count_;
        }

        virtual HRESULT __stdcall Read(void * pv, ULONG cb, ULONG * pcbRead) {
            if (!pv || !pcbRead) {
                return STG_E_INVALIDPOINTER;
            }

            const std::uint64_t size = stream_->read(pv, cb);
            *pcbRead = static_cast<ULONG>(size);

            return (*pcbRead < cb) ? S_FALSE : S_OK;
        }

        virtual HRESULT __stdcall Write(const void * pv, ULONG cb, ULONG * pcbWritten) {
            if (!pv || !pcbWritten) {
                return STG_E_INVALIDPOINTER;
            }
            
            const std::uint64_t size = stream_->write(pv, cb);
            *pcbWritten = static_cast<ULONG>(size);

            return (*pcbWritten < cb) ? S_FALSE : S_OK;
        }

        virtual HRESULT __stdcall Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin,
                                    ULARGE_INTEGER * plibNewPosition) {
            common::seek_where whine = common::seek_where::beg;

            switch (dwOrigin) {
            case STREAM_SEEK_SET:
                whine = common::seek_where::beg;
                break;

            case STREAM_SEEK_CUR:
                whine = common::seek_where::cur;
                break;

            case STREAM_SEEK_END:
                whine = common::seek_where::end;
                break;

            default:
                return STG_E_INVALIDFUNCTION;
            }

            stream_->seek(dlibMove.QuadPart, whine);

            if (plibNewPosition)
                plibNewPosition->QuadPart = stream_->tell();

            return S_OK;
        }

        virtual HRESULT __stdcall SetSize(ULARGE_INTEGER libNewSize) {
            return E_NOTIMPL;
        }

        virtual HRESULT __stdcall CopyTo(IStream * pstm, ULARGE_INTEGER cb,
                                        ULARGE_INTEGER * pcbRead,
                                        ULARGE_INTEGER * pcbWritten) {
            return E_NOTIMPL;
        }

        virtual HRESULT __stdcall Commit(DWORD grfCommitFlags) {
            return E_NOTIMPL;
        }

        virtual HRESULT __stdcall Revert() {
            return E_NOTIMPL;
        }

        virtual HRESULT __stdcall LockRegion(ULARGE_INTEGER libOffset,
                                            ULARGE_INTEGER cb,
                                            DWORD dwLockType) {
            return E_NOTIMPL;
        }

        virtual HRESULT __stdcall UnlockRegion(ULARGE_INTEGER libOffset,
                                            ULARGE_INTEGER cb,
                                            DWORD dwLockType) {
            return E_NOTIMPL;
        }

        virtual HRESULT __stdcall Stat(STATSTG * pstatstg, DWORD grfStatFlag) {
            return E_NOTIMPL;
        }

        virtual HRESULT __stdcall Clone(IStream ** ppstm) {
            return E_NOTIMPL;
        }
    };

    player_wmf_request::~player_wmf_request() {
        if (reader_) {
            SafeRelease(&reader_);
        }

        if (output_supported_list_) {
            SafeRelease(&output_supported_list_);
        }

        if (custom_stream_) {
            SafeRelease(&custom_stream_);
        }
    }
    
    bool player_wmf_request::set_output_type(IMFMediaType *new_output_type) {
        if (!new_output_type) {
            return false;
        }

        if (!SUCCEEDED(new_output_type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &freq_))) {
            return false;
        }

        if (!SUCCEEDED(new_output_type->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels_))) {
            return false;
        }

        output_type_ = new_output_type;
        return true;
    }

    player_wmf::player_wmf(audio_driver *driver)
        : player_shared(driver) {
        // Startup media foundation!
        MFStartup(MF_VERSION);
    }

    void player_wmf::get_more_data(player_request_instance &request) {
        player_wmf_request *request_wmf = reinterpret_cast<player_wmf_request *>(request.get());

        // Data drain, try to get more
        if (request_wmf->flags_ & 1) {
            output_stream_->stop();
            return;
        }

        DWORD stream_flags = 0;
        IMFSample *samples = nullptr;

        // Read from wmf source reader
        HRESULT hr = request_wmf->reader_->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0,
            nullptr, &stream_flags, nullptr, &samples);

        if ((!samples) || (stream_flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
            request_wmf->flags_ |= 1;
        }

        if (samples && SUCCEEDED(hr)) {
            IMFMediaBuffer *media_buffer = nullptr;
            hr = samples->ConvertToContiguousBuffer(&media_buffer);

            if (SUCCEEDED(hr)) {
                DWORD buffer_length = 0;
                BYTE *buffer_pointer = nullptr;

                hr = media_buffer->Lock(&buffer_pointer, nullptr, &buffer_length);

                std::size_t start_to_copy = 0;

                if (request_wmf->use_push_new_data_) {
                    start_to_copy = request_wmf->data_.size();
                    request_wmf->use_push_new_data_ = false;
                }

                request_wmf->data_.resize(start_to_copy + buffer_length);
                std::memcpy(request_wmf->data_.data() + start_to_copy, buffer_pointer, buffer_length);

                hr = media_buffer->Unlock();
                SafeRelease(&media_buffer);
            }

            SafeRelease(&samples);
        }

        request_wmf->data_pointer_ = 0;
    }

    void player_wmf::reset_request(player_request_instance &request) {
        player_wmf_request *request_wmf = reinterpret_cast<player_wmf_request *>(request.get());

        if (!request_wmf) {
            return;
        }

        if (!request_wmf->reader_) {
            if (!make_backend_source(request)) {
                return;
            }
        }

        PROPVARIANT var = { 0 };
        var.vt = VT_I8;
        request_wmf->reader_->SetCurrentPosition(GUID_NULL, var);
    }

    bool player_wmf::is_ready_to_play(player_request_instance &request) {
        player_wmf_request *request_wmf = reinterpret_cast<player_wmf_request *>(request.get());
        if (!request || !request_wmf->reader_) {
            return false;
        }

        return true;
    }

    static bool configure_stream_for_pcm(player_wmf_request &request) {
        IMFMediaType *partial_type = nullptr;

        // We want to set our output type to PCM16
        HRESULT hr = MFCreateMediaType(&partial_type);

        if (!SUCCEEDED(hr)) {
            LOG_ERROR(DRIVER_AUD, "Unable to create new media type to specify output stream to PCM16");
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

    static bool query_wmf_reader_metadata(IMFSourceReader *reader, std::vector<player_metadata> &meta_arr) {
        IMFMetadataProvider *meta_provider = nullptr;
        HRESULT hr = reader->GetServiceForStream(MF_SOURCE_READER_MEDIASOURCE, GUID_NULL,
            __uuidof(meta_provider), reinterpret_cast<LPVOID *>(&meta_provider));

        if (!SUCCEEDED(hr)) {
            LOG_ERROR(DRIVER_AUD, "Unable to get meta provider service for reader stream!");
            return false;
        }

        IMFMetadata *metas = nullptr;
        hr = meta_provider->QueryInterface(IID_PPV_ARGS(&metas));

        SafeRelease(&meta_provider);

        if (!SUCCEEDED(hr)) {
            LOG_ERROR(DRIVER_AUD, "Unable to query metadata interface!");
            return false;
        }

        PROPVARIANT metadata_keys;
        hr = metas->GetAllPropertyNames(&metadata_keys);

        if (!SUCCEEDED(hr)) {
            LOG_ERROR(DRIVER_AUD, "Unable to get all property names from metadata interface");
            SafeRelease(&metas);

            return false;
        }

        PWSTR *metadata_key_strings = nullptr;
        ULONG metadata_key_total = 0;

        hr = PropVariantToStringVectorAlloc(metadata_keys, &metadata_key_strings, &metadata_key_total);

        if (!SUCCEEDED(hr)) {
            LOG_ERROR(DRIVER_AUD, "Unable to convert prop variant to string vector!");
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
                    player_metadata meta;
                    meta.key_ = common::ucs2_to_utf8(std::u16string(reinterpret_cast<const char16_t *>(metadata_key_string)));
                    meta.value_ = common::ucs2_to_utf8(std::u16string(reinterpret_cast<const char16_t *>(metadata_value_string)));

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

    static bool create_source_reader_and_configure(player_wmf_request *request_wmf) {
        const std::u16string url_16 = common::utf8_to_ucs2(request_wmf->url_);

        HRESULT hr = MFCreateSourceReaderFromURL(
            reinterpret_cast<LPCWSTR>(url_16.c_str()), nullptr, &request_wmf->reader_);

        if (!SUCCEEDED(hr)) {
            LOG_ERROR(DRIVER_AUD, "Unable to queue new play URL {} (can't open source reader)", request_wmf->url_);
            return false;
        }

        if (!configure_stream_for_pcm(*request_wmf)) {
            LOG_ERROR(DRIVER_AUD, "Error while configure WMF stream!");
            SafeRelease(&request_wmf->reader_);
            return false;
        }

        return true;
    }

    bool player_wmf::queue_url(const std::string &url) {
        player_request_instance request = std::make_unique<player_wmf_request>();
        player_wmf_request *request_wmf = reinterpret_cast<player_wmf_request *>(request.get());

        request_wmf->data_pointer_ = 0;
        request_wmf->url_ = url;

        const std::lock_guard<std::mutex> guard(lock_);
        requests_.push(std::move(request));

        return true;
    }

    bool player_wmf::queue_custom(common::rw_stream *lower_stream) {
        player_request_instance request = std::make_unique<player_wmf_request>();
        player_wmf_request *request_wmf = reinterpret_cast<player_wmf_request *>(request.get());

        IMFByteStream *stream = NULL;

        // Auto release when ref is to 0.
        request_wmf->custom_stream_ = new rw_stream_com(lower_stream);

        const std::lock_guard<std::mutex> guard(lock_);
        requests_.push(std::move(request));

        return true;
    }

    bool player_wmf::set_position_for_custom_format(player_request_instance &request, const std::uint64_t pos_in_us) {
        player_wmf_request *request_wmf = reinterpret_cast<player_wmf_request *>(request.get());
    
        // Time unit in 100 nanoseconds, convert to unit by mul with 10.
        PROPVARIANT time_to_seek = { pos_in_us * 10 };
        time_to_seek.vt = VT_I8;

        HRESULT res = request_wmf->reader_->SetCurrentPosition(GUID_NULL, time_to_seek);
        if (res == S_OK) {
            request->pos_in_us_ = pos_in_us;
            return true;
        }

        return false;
    }

    void player_wmf::read_and_transcode(player_request_instance &request, const std::uint32_t out_stream_idx, const std::uint64_t time_stamp_source, const std::uint64_t duration_source) {
        player_wmf_request *request_wmf = reinterpret_cast<player_wmf_request*>(request.get());

        // Seek the source stream to time stamp first
        if (!set_position_for_custom_format(request, time_stamp_source)) {
            LOG_ERROR(DRIVER_AUD, "Unable to set position to do reading for transcode!");
            return;
        }

        std::int64_t time_stop_in_wm_tu = static_cast<std::int64_t>((time_stamp_source + duration_source) * 10);
        std::int64_t time_stamp_of_reading_sample_in_wm_tu = 0;

        while (time_stamp_of_reading_sample_in_wm_tu < time_stop_in_wm_tu) {
            IMFSample *samples = nullptr;
            DWORD stream_flags = 0;

            HRESULT result = request_wmf->reader_->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0,
                nullptr, &stream_flags, &time_stamp_of_reading_sample_in_wm_tu, &samples);

            if ((result != S_OK) || !samples || (stream_flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
                LOG_TRACE(DRIVER_AUD, "End of stream sooner then expected, stopping transcode");
                return;
            }

            HRESULT res_err = request_wmf->writer_->WriteSample(static_cast<DWORD>(out_stream_idx), samples);
            SafeRelease(&samples);
        }
    }

    static bool configure_transcode_stream(IMFSinkWriter *writer, IMFSourceReader *reader, IMFMediaType *write_media_type, DWORD *out_stream_idx) {
        const DWORD stream_idx = static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM);

        // Set input audio for sink writer first
        IMFMediaType *source_current_output_reader_type = nullptr;
        HRESULT result = reader->GetCurrentMediaType(stream_idx, &source_current_output_reader_type);

        if (result != S_OK) {
            LOG_ERROR(DRIVER_AUD, "Unable to get current reader media type!");
            return false;
        }

        result = writer->SetInputMediaType(stream_idx, source_current_output_reader_type, nullptr);
        
        if (result != S_OK) {
            LOG_ERROR(DRIVER_AUD, "Unable to set writer input media type!");
            return false;
        }

        SafeRelease(&source_current_output_reader_type);
        result = writer->AddStream(write_media_type, out_stream_idx);

        if (result != S_OK) {
            LOG_ERROR(DRIVER_AUD, "Unable to add new stream to MF sink writer!");
            return false;
        }

        return true;
    }

    bool player_wmf::crop() {
        player_wmf_request *request_wmf = reinterpret_cast<player_wmf_request*>(requests_.front().get());

        // Construct an temporary url to write new samples to
        const std::string url_new = eka2l1::file_directory(request_wmf->url_) + eka2l1::filename(request_wmf->url_) + "_temp" + eka2l1::path_extension(request_wmf->url_);
        const std::u16string url_16_new = common::utf8_to_ucs2(url_new);

        HRESULT result = MFCreateSinkWriterFromURL(reinterpret_cast<LPCWSTR>(url_16_new.c_str()), nullptr, nullptr, &request_wmf->writer_);
        if (result != S_OK) {
            LOG_ERROR(DRIVER_AUD, "Unable to create sink writer to do cropping!");
            return false;
        }

        DWORD out_stream_index = 0;
        bool res = configure_transcode_stream(request_wmf->writer_, request_wmf->reader_, request_wmf->output_type_, &out_stream_index);

        if (!res) {
            LOG_ERROR(DRIVER_AUD, "Unable to configure sink writer for cropping!");
            SafeRelease(&request_wmf->writer_);

            return false;
        }

        read_and_transcode(requests_.front(), out_stream_index, 0, request_wmf->pos_in_us_);
        SafeRelease(&request_wmf->writer_);

        // Recreate reader stream. todo
        common::move_file(request_wmf->url_, url_new);
        SafeRelease(&request_wmf->reader_);

        request_wmf->data_pointer_ = 0;

        return create_source_reader_and_configure(request_wmf);
    }

    bool player_wmf::record() {
        LOG_ERROR(DRIVER_AUD, "Record for WMF unimplemented!");
        return true;
    }

    static const GUID four_cc_codec_to_guid(const std::uint32_t codec) {
        switch (codec) {
        case AUDIO_PCM16_CODEC_4CC:
        case AUDIO_PCM8_CODEC_4CC:
            return MFAudioFormat_PCM;

        default:
            break;
        }

        return GUID_NULL;
    }

    bool player_wmf::set_dest_encoding(const std::uint32_t enc) {
        player_wmf_request *request_wmf = reinterpret_cast<player_wmf_request*>(requests_.front().get());

        if (!request_wmf) {
            return false;
        }

        if (request_wmf->output_supported_list_) {
            SafeRelease(&request_wmf->output_supported_list_);
        }

        if (request_wmf->output_type_) {
            SafeRelease(&request_wmf->output_type_);
        }

        GUID codec_guid = four_cc_codec_to_guid(enc);
        if (codec_guid == GUID_NULL) {
            return false;
        }

        request_wmf->encoding_ = enc;

        if (FAILED(MFTranscodeGetAudioOutputAvailableTypes(codec_guid, MFT_ENUM_FLAG_ALL, nullptr, &request_wmf->output_supported_list_))) {
            // Create an independent output media type. It seems this format supports anykind of encode we throw it in
            if (FAILED(MFCreateMediaType(&request_wmf->output_type_))) {
                return false;
            }

            // Set default properties
            request_wmf->output_type_->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
            request_wmf->output_type_->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 16000);

            return true;
        }

        // Choose the first one
        DWORD elem_count = 0;
        if (FAILED(request_wmf->output_supported_list_->GetElementCount(&elem_count)) || (elem_count == 0)) {
            return false;
        }

        return request_wmf->set_output_type(GetElement<IMFMediaType>(request_wmf->output_supported_list_, 0));
    }

    bool player_wmf::set_dest_freq(const std::uint32_t freq) {
        player_wmf_request *request_wmf = reinterpret_cast<player_wmf_request*>(requests_.front().get());

        if (!request_wmf) {
            return false;
        }

        if (!request_wmf->output_supported_list_) {
            if (request_wmf->output_type_) {
                if (FAILED(request_wmf->output_type_->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, freq))) {
                    return false;
                }

                request_wmf->freq_ = freq;
                return true;
            }

            LOG_ERROR(DRIVER_AUD, "Encoding for destination not set!");
            return false;
        }

        // Iterate through all output media types.
        // If we found exact, break, else we should find the nearest sample rate that also matches channel count
        DWORD count = 0;
        if (!SUCCEEDED(request_wmf->output_supported_list_->GetElementCount(&count))) {
            return false;
        }

        for (DWORD i = 0; i < count; i++) {
            IMFMediaType *type = GetElement<IMFMediaType>(request_wmf->output_supported_list_, i);

            if (type) {
                std::uint32_t output_support_freq = 0;
                std::uint32_t output_support_cn = 0;

                if (SUCCEEDED(type->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &output_support_cn)) &&
                    SUCCEEDED(type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &output_support_freq))) {
                    if ((output_support_freq >= freq) && (output_support_cn == request_wmf->channels_)) {
                        LOG_TRACE(DRIVER_AUD, "Destination sample rate is set to nearest supported: {}", output_support_freq);
                        return request_wmf->set_output_type(type);
                    }
                }
            }

            SafeRelease(&type);
        }

        return false;
    }

    bool player_wmf::set_dest_channel_count(const std::uint32_t cn) {
        player_wmf_request *request_wmf = reinterpret_cast<player_wmf_request*>(requests_.front().get());

        if (!request_wmf) {
            return false;
        }

        if (!request_wmf->output_supported_list_) {
            if (request_wmf->output_type_) {
                if (FAILED(request_wmf->output_type_->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, cn))) {
                    return false;
                }

                request_wmf->channels_ = cn;
                return true;
            }

            LOG_ERROR(DRIVER_AUD, "Encoding for destination not set!");
            return false;
        }

        // Iterate through all output media types.
        // If we found exact, break, else we should find the nearest sample rate that also matches channel count
        DWORD count = 0;
        if (!SUCCEEDED(request_wmf->output_supported_list_->GetElementCount(&count))) {
            return false;
        }

        for (DWORD i = 0; i < count; i++) {
            IMFMediaType *type = GetElement<IMFMediaType>(request_wmf->output_supported_list_, i);

            if (type) {
                std::uint32_t output_support_freq = 0;
                std::uint32_t output_support_cn = 0;

                if (SUCCEEDED(type->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &output_support_cn)) &&
                    SUCCEEDED(type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &output_support_freq))) {
                    if ((output_support_freq == request_wmf->channels_) && (output_support_cn == cn)) {
                        return request_wmf->set_output_type(type);
                    }
                }
            }

            SafeRelease(&type);
        }

        return false;
    }
    
    bool player_wmf::make_backend_source(player_request_instance &request) {
        player_wmf_request *request_wmf = reinterpret_cast<player_wmf_request*>(requests_.front().get());

        if (!request_wmf) {
            return false;
        }

        IMFByteStream *stream = nullptr;

        if (request_wmf->custom_stream_) {
            if (!SUCCEEDED(MFCreateMFByteStreamOnStream(request_wmf->custom_stream_, &stream))) {
                return false;
            }

            HRESULT hr = MFCreateSourceReaderFromByteStream(stream, nullptr, &request_wmf->reader_);

            if (!SUCCEEDED(hr)) {
                LOG_ERROR(DRIVER_AUD, "Unable to queue new custom stream (can't open source reader)");
                return false;
            }
        } else {
            if (!create_source_reader_and_configure(request_wmf)) {
                return false;
            }
        }

        if (!configure_stream_for_pcm(*request_wmf)) {
            LOG_ERROR(DRIVER_AUD, "Error while configure WMF stream!");
            SafeRelease(&request_wmf->reader_);
            return false;
        }

        SafeRelease(&stream);
        query_wmf_reader_metadata(request_wmf->reader_, metadatas_);

        return true;
    }
}

#endif