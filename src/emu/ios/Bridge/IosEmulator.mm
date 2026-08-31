// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later

#import "IosEmulator.h"

#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>


#include <array>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <iomanip>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include <sys/stat.h>

#include <common/algorithm.h>
#include <common/buffer.h>
#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/language.h>
#include <common/log.h>
#include <common/path.h>
#include <common/thread.h>
#include <common/version.h>
#include <config/app_settings.h>
#include <config/config.h>
#include <cpu/arm_factory.h>
#include <dispatch/dispatcher.h>
#include <drivers/audio/audio.h>
#include <drivers/audio/dsp.h>
#include <drivers/audio/player.h>
#include <drivers/graphics/backend/emu_window_ios.h>
#include <drivers/graphics/graphics.h>
#include <drivers/input/common.h>
#include <drivers/itc.h>
#include <drivers/hwrm/vibration.h>
#include <drivers/camera/camera_collection.h>
#include <drivers/sensor/sensor.h>
#include <services/window/screen.h>
#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <loader/mbm.h>
#include <loader/mif.h>
#include <package/manager.h>
#include <services/applist/applist.h>
#include <services/fbs/bitmap.h>
#include <services/fbs/fbs.h>
#include <services/window/window.h>
#include <system/devices.h>
#include <system/epoc.h>
#include <system/installation/archive.h>
#include <system/installation/common.h>
#include <system/installation/firmware.h>
#include <system/installation/rpkg.h>
#include <system/software.h>
#include <utils/apacmd.h>
#include <utils/locale.h>
#include <utils/panic.h>
#include <utils/system.h>
#include <vfs/vfs.h>

#include <lunasvg.h>

@implementation EKA2L1AppEntry
@end

@implementation EKA2L1DeviceEntry
@end

@implementation EKA2L1NGageInstallReport
@end

@implementation EKA2L1LanguageEntry
@end

namespace eka2l1::ios {
    static dispatch_queue_t emulator_control_queue() {
        static dispatch_queue_t queue;
        static dispatch_once_t once;
        dispatch_once(&once, ^{
            queue = dispatch_queue_create("com.eka2l1.emulator.control", DISPATCH_QUEUE_SERIAL);
        });
        return queue;
    }

    static EKA2L1InstallResult map_install_result(eka2l1::device_installation_error err) {
        switch (err) {
            case eka2l1::device_installation_none: return EKA2L1InstallResultSuccess;
            case eka2l1::device_installation_not_exist: return EKA2L1InstallResultNotExist;
            case eka2l1::device_installation_insufficent: return EKA2L1InstallResultInsufficient;
            case eka2l1::device_installation_rpkg_corrupt: return EKA2L1InstallResultRpkgCorrupt;
            case eka2l1::device_installation_determine_product_failure: return EKA2L1InstallResultDetermineProductFailure;
            case eka2l1::device_installation_already_exist: return EKA2L1InstallResultAlreadyExist;
            case eka2l1::device_installation_general_failure: return EKA2L1InstallResultGeneralFailure;
            case eka2l1::device_installation_rom_fail_to_copy: return EKA2L1InstallResultRomFailToCopy;
            case eka2l1::device_installation_vpl_file_invalid: return EKA2L1InstallResultVplInvalid;
            case eka2l1::device_installation_rofs_corrupt: return EKA2L1InstallResultRofsCorrupt;
            case eka2l1::device_installation_rom_file_corrupt: return EKA2L1InstallResultRomCorrupt;
            case eka2l1::device_installation_fpsx_corrupt: return EKA2L1InstallResultFpsxCorrupt;
            case eka2l1::device_installation_rpkg_missing: return EKA2L1InstallResultNeedRpkg;
            case eka2l1::device_installation_archive_corrupt: return EKA2L1InstallResultArchiveCorrupt;
            case eka2l1::device_installation_archive_no_device: return EKA2L1InstallResultArchiveNoDevice;
            default: return EKA2L1InstallResultGeneralFailure;
        }
    }
}

namespace eka2l1::ios {
    static NSString *guest_fatal_detail(kernel::process *pr) {
        if (!pr) {
            return nil;
        }

        const auto exit_type = pr->get_exit_type();
        const std::int32_t reason = pr->get_exit_reason();
        if (exit_type != kernel::entity_exit_type::panic
            && !(exit_type == kernel::entity_exit_type::terminate && reason != 0)) {
            return nil;
        }

        const std::string process_name = pr->raw_name();
        const std::string category = common::ucs2_to_utf8(pr->get_exit_category());
        const std::optional<std::string> description = epoc::get_panic_description(category, reason);

        const char *exit_type_name = "unknown";
        switch (exit_type) {
            case kernel::entity_exit_type::panic: exit_type_name = "panic"; break;
            case kernel::entity_exit_type::terminate: exit_type_name = "terminate"; break;
            case kernel::entity_exit_type::kill: exit_type_name = "kill"; break;
            case kernel::entity_exit_type::pending: exit_type_name = "pending"; break;
        }

        std::ostringstream detail;
        detail << "Process: " << (process_name.empty() ? "<unknown>" : process_name) << "\n"
               << "Exit type: " << exit_type_name << "\n"
               << "Category: " << (category.empty() ? "<none>" : category) << "\n"
               << "Reason: " << reason;
        if (description && !description->empty()) {
            detail << "\nDescription: " << *description;
        }

        const std::string message = detail.str();
        return [NSString stringWithUTF8String:message.c_str()];
    }

    // Icon decoder. Mirrors src/emu/android/.../launcher.cpp::get_app_icon
    // but writes the rendered RGBA buffer straight into a CFData → CGImage →
    // UIImage → PNG round-trip so SwiftUI can consume it as plain Data.
    //
    // Order of attempts matches the Android side:
    //   .mif → lunasvg (after svgb / nvg debinarization, cached to disk)
    //   .mbm → epoc::convert_to_rgba8888 against sbm header 0
    //   anything else → applist_server::get_icon -> bitwise_bitmap pair
    static NSData *encode_rgba_to_png(const std::uint8_t *pixels,
                                      std::size_t width, std::size_t height,
                                      std::size_t requested_side) {
        if (!pixels || width == 0 || height == 0) {
            return nil;
        }
        CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
        CGBitmapInfo bitmap_info = kCGBitmapByteOrder32Big | kCGImageAlphaPremultipliedLast;
        const std::size_t bpr = width * 4;
        CGContextRef src_ctx = CGBitmapContextCreate(const_cast<std::uint8_t *>(pixels),
            width, height, 8, bpr, color_space, bitmap_info);
        if (!src_ctx) {
            CGColorSpaceRelease(color_space);
            return nil;
        }
        CGImageRef src_image = CGBitmapContextCreateImage(src_ctx);
        CGContextRelease(src_ctx);
        if (!src_image) {
            CGColorSpaceRelease(color_space);
            return nil;
        }

        // Rescale into a square `requested_side` × `requested_side` so the
        // SwiftUI list cells get a predictable canvas; lunasvg renders at the
        // SVG's intrinsic size (often 88×88 or 176×176) and MBM dimensions
        // vary by app. Skip rescale if it already matches to save one draw.
        UIImage *image = nil;
        if (requested_side == width && requested_side == height) {
            image = [UIImage imageWithCGImage:src_image];
        } else {
            CGContextRef dst_ctx = CGBitmapContextCreate(nullptr,
                requested_side, requested_side, 8, requested_side * 4,
                color_space, bitmap_info);
            if (dst_ctx) {
                CGContextSetInterpolationQuality(dst_ctx, kCGInterpolationHigh);
                CGContextDrawImage(dst_ctx, CGRectMake(0, 0, requested_side, requested_side), src_image);
                CGImageRef dst_image = CGBitmapContextCreateImage(dst_ctx);
                CGContextRelease(dst_ctx);
                if (dst_image) {
                    image = [UIImage imageWithCGImage:dst_image];
                    CGImageRelease(dst_image);
                }
            }
        }
        CGImageRelease(src_image);
        CGColorSpaceRelease(color_space);
        if (!image) {
            return nil;
        }
        return UIImagePNGRepresentation(image);
    }

    static NSData *decode_mif_icon(eka2l1::apa_app_registry *reg,
                                   eka2l1::io_system *io,
                                   const std::string &cache_dir,
                                   std::size_t side) {
        eka2l1::symfile file_route = io->open_file(reg->icon_file_path, READ_MODE | BIN_MODE);
        if (!file_route) return nil;
        eka2l1::common::create_directories(cache_dir);

        std::ostringstream cached_path_builder;
        cached_path_builder << cache_dir << "/debinarized_"
                            << std::hex << std::uppercase << std::setfill('0') << std::setw(8)
                            << reg->mandatory_info.uid << ".svg";
        const std::string cached_path = cached_path_builder.str();
        const std::uint64_t mif_last_modified = file_route->last_modify_since_0ad();

        std::unique_ptr<lunasvg::Document> document;
        if (eka2l1::common::exists(cached_path)) {
            const std::u16string cached_u16 = eka2l1::common::utf8_to_ucs2(cached_path);
            if (eka2l1::common::get_last_modifiy_since_ad(cached_u16) >= mif_last_modified) {
                document = lunasvg::Document::loadFromFile(cached_path.c_str());
            }
        }

        if (!document) {
            eka2l1::ro_file_stream rfs(file_route.get());
            eka2l1::loader::mif_file parser(reinterpret_cast<eka2l1::common::ro_stream *>(&rfs));
            if (parser.do_parse()) {
                int dest_size = 0;
                if (parser.read_mif_entry(0, nullptr, dest_size) && dest_size > 0) {
                    std::vector<std::uint8_t> data(dest_size);
                    parser.read_mif_entry(0, data.data(), dest_size);

                    auto outfile = std::make_unique<eka2l1::common::wo_std_file_stream>(cached_path, true);
                    const bool converted = eka2l1::loader::convert_mif_icon_to_svg(data.data(),
                        data.size(), *outfile);
                    outfile.reset();

                    if (converted) {
                        document = lunasvg::Document::loadFromFile(cached_path.c_str());
                    }

                    if (!document) {
                        // Don't leave a half-written cache entry behind: the mtime check
                        // would happily serve it back on the next launch.
                        eka2l1::common::remove(cached_path);
                    }
                }
            }
        }

        if (!document) return nil;
        const std::uint32_t w = static_cast<std::uint32_t>(document->width());
        const std::uint32_t h = static_cast<std::uint32_t>(document->height());
        if (w == 0 || h == 0) return nil;

        std::vector<std::uint8_t> rgba(w * h * 4);
        auto bitmap = lunasvg::Bitmap(rgba.data(), w, h, w * 4);
        document->render(bitmap, lunasvg::Matrix{ 1, 0, 0, 1, 0, 0 });
        bitmap.convertToRGBA();
        return encode_rgba_to_png(rgba.data(), w, h, side);
    }

    static NSData *decode_mbm_icon(eka2l1::apa_app_registry *reg,
                                   eka2l1::fbs_server *fbsserv,
                                   eka2l1::io_system *io,
                                   std::size_t side) {
        eka2l1::symfile file_route = io->open_file(reg->icon_file_path, READ_MODE | BIN_MODE);
        if (!file_route) return nil;
        eka2l1::ro_file_stream rfs(file_route.get());
        eka2l1::loader::mbm_file parser(reinterpret_cast<eka2l1::common::ro_stream *>(&rfs));
        if (!parser.do_read_headers() || parser.sbm_headers.empty()) return nil;

        const auto &hdr = parser.sbm_headers[0];
        const std::size_t w = hdr.size_pixels.x;
        const std::size_t h = hdr.size_pixels.y;
        if (w == 0 || h == 0) return nil;

        std::vector<std::uint8_t> rgba(w * h * 4);
        eka2l1::common::wo_buf_stream dst(rgba.data(), rgba.size());
        if (!eka2l1::epoc::convert_to_rgba8888(fbsserv, parser, 0, dst)) {
            return nil;
        }

        // Symbian app-icon MBMs store the icon at index 0 and its paired mask at
        // index 1. Apply it when present and dimension-matched.
        if (parser.sbm_headers.size() > 1) {
            const auto &mask_hdr = parser.sbm_headers[1];
            if (static_cast<std::size_t>(mask_hdr.size_pixels.x) == w
                && static_cast<std::size_t>(mask_hdr.size_pixels.y) == h) {
                std::vector<std::uint8_t> mask_rgba(w * h * 4);
                eka2l1::common::wo_buf_stream mask_dst(mask_rgba.data(), mask_rgba.size());
                if (eka2l1::epoc::convert_to_rgba8888(fbsserv, parser, 1, mask_dst, true)) {
                    eka2l1::epoc::apply_icon_mask_alpha(rgba.data(), mask_rgba.data(), w, h,
                        mask_hdr.bit_per_pixels);
                }
            }
        }
        return encode_rgba_to_png(rgba.data(), w, h, side);
    }

    static NSData *decode_bitwise_icon(eka2l1::apa_app_registry *reg,
                                       eka2l1::applist_server *alserv,
                                       eka2l1::fbs_server *fbsserv,
                                       std::size_t side) {
        auto icon_pair = alserv->get_icon(*reg, 0);
        if (!icon_pair.has_value() || !icon_pair->first) return nil;
        auto *bitmap = icon_pair->first;
        const std::size_t w = bitmap->header_.size_pixels.x;
        const std::size_t h = bitmap->header_.size_pixels.y;
        if (w == 0 || h == 0) return nil;

        std::vector<std::uint8_t> rgba(w * h * 4);
        eka2l1::common::wo_buf_stream dst(rgba.data(), rgba.size());
        if (!eka2l1::epoc::convert_to_rgba8888(fbsserv, bitmap, dst)) return nil;

        // The applist server hands back the paired mask as the second bitmap.
        if (auto *mask_bitmap = icon_pair->second) {
            if (static_cast<std::size_t>(mask_bitmap->header_.size_pixels.x) == w
                && static_cast<std::size_t>(mask_bitmap->header_.size_pixels.y) == h) {
                std::vector<std::uint8_t> mask_rgba(w * h * 4);
                eka2l1::common::wo_buf_stream mask_dst(mask_rgba.data(), mask_rgba.size());
                if (eka2l1::epoc::convert_to_rgba8888(fbsserv, mask_bitmap, mask_dst, true)) {
                    eka2l1::epoc::apply_icon_mask_alpha(rgba.data(), mask_rgba.data(), w, h,
                        mask_bitmap->header_.bit_per_pixels);
                }
            }
        }
        return encode_rgba_to_png(rgba.data(), w, h, side);
    }
}

namespace eka2l1::ios {
    // C++ side of the iOS emulator state. Lives behind the Obj-C facade
    // EKA2L1Emulator so SwiftUI never sees the C++ types directly.
    //
    // Owns everything the app needs for one emulator instance: the system, its
    // drivers, and the two threads behind the frame loop.
    struct emulator {
        std::unique_ptr<eka2l1::system> symsys;
        std::unique_ptr<config::app_settings> settings;
        std::unique_ptr<drivers::emu_window_ios> window;
        drivers::graphics_driver_ptr graphics_driver;
        std::unique_ptr<drivers::audio_driver> audio_driver;
        std::unique_ptr<drivers::sensor_driver> sensor_driver;

        config::state conf;
        window_server *winserv = nullptr;
        std::string documents_root;
        std::string caches_root;

        std::atomic<bool> running{false};
        std::atomic<bool> paused{false};
        // symsys->loop() must not run before bootDeviceAtIndex: completes —
        // kernel_system::crr_thread() dereferences a null thread_scheduler
        // otherwise. The os_thread idles until this flips true.
        std::atomic<bool> mounted{false};

        // Frame loop / lifecycle. Two threads sit behind the
        // singleton — one feeds drivers::graphics_driver::run() (must own
        // the EAGL context), the other ticks symsys->loop().
        std::unique_ptr<std::thread> os_thread;
        std::unique_ptr<std::thread> graphics_thread;

        // Held by os_thread around each symsys->loop() tick. Device install /
        // switch rebuilds symsys (or mutates device_manager) and must not race
        // a loop in flight, so those paths grab this between ticks.
        std::mutex loop_mutex;

        // Serialises bridge entry points that walk symsys internals from
        // arbitrary threads (app rescan spins up the applist worker pool)
        // against the paths that rebuild or destroy symsys (boot / install /
        // shutdown). loop_mutex only covers the os_thread tick, not these.
        // Recursive because launch -> reboot nests two writer sections.
        // Main-thread readers must try_lock only: while a boot holds this
        // lock the graphics thread dispatch_syncs onto the main queue, so a
        // blocked main thread would deadlock the boot.
        std::recursive_mutex session_mutex;

        std::mutex layer_mutex;
        std::condition_variable layer_cv;
        bool layer_dirty = false;
        void *pending_layer = nullptr;
        std::uint32_t pending_width = 0;
        std::uint32_t pending_height = 0;
        float pending_scale = 1.0f;

        std::mutex icon_mutex;
        std::vector<std::size_t> screen_redraw_handles;
        // Double-buffered present fences. Each frame uses one slot and only
        // blocks on the slot it reused two frames ago, so the guest CPU can run
        // one frame ahead while the graphics thread is still doing the (vsync-
        // gated) swap of the previous frame instead of serialising against it.
        // present_mutex serialises submit_screen_frame itself: besides the os
        // thread's redraw callback, the launch/resize paths kick presents from
        // dispatch queues, and two concurrent submits would race the slot
        // fences (both waiting on the same in-flight slot — a deadlock).
        std::mutex present_mutex;
        int present_status[2] = { 0, 0 };
        int present_slot = 0;
        std::atomic<std::uint64_t> rendered_frame_count{0};

        // Vertical anchor for the presented guest picture, in surface pixels.
        // -1 centres it (default). >= 0 pins the picture's top edge at that
        // offset (clamped so it stays on screen) — the frontend uses this to
        // top-align the picture when a keypad overlays the bottom of the view,
        // so the keys cover letterbox instead of gameplay.
        std::atomic<int> display_anchor_top_px{-1};

        // CCW rotation of on-screen content relative to the iPhone's natural
        // (portrait) orientation: 0 portrait, 90 landscapeLeft, 180 upside
        // down, 270 landscapeRight. Combined with the guest screen mode's
        // rotation on every present to orient accelerometer samples.
        std::atomic<int> host_interface_rotation_deg{0};

        // Whether the booted device has a touch screen, published by
        // bootDeviceAtIndex:. The emulator screen asks for this while it
        // appears — on the main thread, without the session lock — and the
        // launch path may be rebuilding symsys on the control queue at that
        // very moment, so the answer must not come from walking symsys.
        std::atomic<bool> device_is_touch_screen{false};

        // Primary-thread id of the app launched by launchAppWithUID:. Used to
        // kill that process when the frontend closes the emulator screen, and
        // cleared once the process exits (so closeRunningApp no-ops afterwards).
        eka2l1::kernel::uid running_thread_id = 0;

        // Set once an app has exited / been killed in this booted session. The
        // window + view servers don't fully reset between app instances (the
        // guest logs "Can't remove active view!" on exit), so relaunching into
        // the same session renders a blank screen. Mirror the desktop frontend,
        // which reboots the device on app exit: the next launchAppWithUID:
        // rebuilds the system first. Cleared by that rebuild.
        bool needs_reboot_before_launch = false;

        // Bumped on every launch. The process exit logon is registered with the
        // generation it belongs to; a stale logon (e.g. the previous app's
        // callback re-fired while its process is torn down during the reboot of
        // the next launch — logon_requests_emu isn't cleared after firing) is
        // then ignored instead of closing the freshly-launched screen.
        std::uint64_t launch_generation = 0;

        // Firmware code of the device a previous run of the app was still
        // bringing up when the process died (see the boot-attempt marker
        // below). Read once at start, handed to the frontend, and used to keep
        // the auto-boot away from that device.
        std::string failed_boot_firmware;

        // Firmware code of the device the current boot attempt is on, empty
        // once the boot is confirmed good. Mirrors the on-disk marker.
        std::string pending_boot_firmware;

        // Set while a boot that owns the marker above is still running, so a
        // confirmation arriving from the frontend in the meantime (an app-list
        // rescan racing a device switch) doesn't clear a marker the boot still
        // needs.
        std::atomic<bool> boot_in_flight{false};
    };

    // A ROM or RPKG dump that is damaged in a way the installer doesn't catch
    // only fails once the device is actually brought up — and it fails hard
    // (guest memory is mapped straight from the image, so a bad one takes the
    // host process down rather than returning an error). Persisting the device
    // selection before that point is what turns a one-off crash into a
    // permanent one: every later launch auto-boots the same broken device and
    // dies again before the UI can offer a way out.
    //
    // So the selection is written to config.yml only after a boot has proven
    // itself, and the attempt itself is recorded in this marker file
    // beforehand. Finding the marker at start means the previous run died
    // mid-boot; that device is then skipped by the auto-boot and reported to
    // the frontend, leaving the user with a working app they can delete or
    // reinstall the bad dump from.
    static std::string boot_attempt_marker_path(emulator *state) {
        return eka2l1::add_path(state->conf.storage, "boot_attempt.txt");
    }

    static void mark_boot_attempt(emulator *state, const std::string &firmware_code) {
        state->pending_boot_firmware = firmware_code;
        state->boot_in_flight = true;
        FILE *f = common::open_c_file(boot_attempt_marker_path(state), "wb");
        if (!f) {
            LOG_WARN(eka2l1::FRONTEND_CMDLINE, "Unable to record the device boot attempt marker");
            return;
        }
        fwrite(firmware_code.data(), 1, firmware_code.length(), f);
        fflush(f);
        fclose(f);
    }

    static void clear_boot_attempt(emulator *state) {
        state->boot_in_flight = false;
        if (state->pending_boot_firmware.empty()) {
            return;
        }
        state->pending_boot_firmware.clear();
        common::remove(boot_attempt_marker_path(state));
    }

    // Read the marker left by a previous run and delete it, so a device is only
    // ever held against one launch: the user can still boot it by hand, and a
    // dump that works again (reinstalled, or the crash was unrelated) is not
    // locked out.
    static std::string take_boot_attempt(emulator *state) {
        const std::string marker = boot_attempt_marker_path(state);
        std::string firmware_code;
        if (FILE *f = common::open_c_file(marker, "rb")) {
            char buf[128] = { 0 };
            const std::size_t read = fread(buf, 1, sizeof(buf) - 1, f);
            fclose(f);
            firmware_code.assign(buf, read);
        }
        common::remove(marker);
        return firmware_code;
    }

    // Typed service-server accessors. The window / applist / fbs servers are
    // all looked up by their epoc-version-specific name, so wrap the verbose
    // get_by_name<server>(name_by_epocver(...)) + reinterpret_cast that would
    // otherwise be repeated at every call site. All return nullptr for a null
    // kernel so callers can chain without a separate guard.
    static eka2l1::window_server *get_window_server(eka2l1::kernel_system *kern) {
        if (!kern) {
            return nullptr;
        }
        return reinterpret_cast<eka2l1::window_server *>(
            kern->get_by_name<eka2l1::service::server>(
                eka2l1::get_winserv_name_by_epocver(kern->get_epoc_version())));
    }

    static eka2l1::applist_server *get_applist_server(eka2l1::kernel_system *kern) {
        if (!kern) {
            return nullptr;
        }
        return reinterpret_cast<eka2l1::applist_server *>(
            kern->get_by_name<eka2l1::service::server>(
                eka2l1::get_app_list_server_name_by_epocver(kern->get_epoc_version())));
    }

    static eka2l1::fbs_server *get_fbs_server(eka2l1::kernel_system *kern) {
        if (!kern) {
            return nullptr;
        }
        return reinterpret_cast<eka2l1::fbs_server *>(
            kern->get_by_name<eka2l1::service::server>(
                eka2l1::epoc::get_fbs_server_name_by_epocver(kern->get_epoc_version())));
    }

    // The component bundle handed to each new eka2l1::system. iOS never feeds a
    // graphics driver through here (it is bound later, once the EAGL context is
    // live); audio / conf / settings live for the emulator's whole lifetime.
    static eka2l1::system_create_components make_system_components(emulator *state) {
        eka2l1::system_create_components comp;
        comp.audio_ = state->audio_driver.get();
        comp.graphics_ = nullptr;
        comp.conf_ = &state->conf;
        comp.settings_ = state->settings.get();
        comp.cache_root_ = state->caches_root;
        return comp;
    }

    static bool wait_for_graphics_driver(emulator *state, const std::chrono::milliseconds timeout) {
        if (!state) {
            return false;
        }
        std::unique_lock<std::mutex> lock(state->layer_mutex);
        return state->layer_cv.wait_for(lock, timeout, [state]() {
            return !state->running || state->graphics_driver != nullptr;
        }) && state->graphics_driver != nullptr;
    }

    // With cpu_load_save enabled the scheduler parks the os_thread inside
    // symsys->loop() (idle_event.wait) whenever no guest thread is ready, so it
    // stops burning a host core when the guest is idle. The flip side is that a
    // parked loop still holds loop_mutex and won't observe running/paused/mounted
    // flips until the next timer wakes it. Mirror Qt's kill_emulator: poke the
    // scheduler awake so the in-flight tick returns promptly. Safe to call when
    // idle is disabled (stop_cores_idling is a no-op then) or before boot.
    static void break_core_idling(emulator *state) {
        if (!state || !state->symsys) {
            return;
        }
        if (auto *kern = state->symsys->get_kernel_system()) {
            kern->stop_cores_idling();
        }
    }

    // Stop the os_thread from ticking symsys->loop() and wait out any tick in
    // flight, returning the held loop lock so the caller can mutate symsys /
    // kernel state exclusively. Mirrors the prologue used by device install,
    // boot and app-close. The caller owns `mounted` afterwards: boot flips it
    // back true, a failed install restores the previous value, app-close leaves
    // it false.
    [[nodiscard]] static std::unique_lock<std::mutex> pause_loop_and_lock(emulator *state) {
        state->mounted = false;
        break_core_idling(state);
        return std::unique_lock<std::mutex>(state->loop_mutex);
    }

    static bool bind_graphics_driver(emulator *state) {
        if (!state || !state->symsys || !state->graphics_driver) {
            return false;
        }

        state->symsys->set_graphics_driver(state->graphics_driver.get());
        auto *kern = state->symsys->get_kernel_system();
        if (!state->winserv) {
            state->winserv = get_window_server(kern);
        }
        if (state->winserv) {
            for (eka2l1::epoc::screen *scr = state->winserv->get_screens(); scr; scr = scr->next) {
                if (!scr->screen_texture) {
                    scr->set_screen_mode(state->winserv, state->graphics_driver.get(), scr->crr_mode);
                }
            }
        }
        return true;
    }

    static void submit_screen_frame(emulator *state, eka2l1::epoc::screen *scr) {
        if (!state || !state->graphics_driver || !state->window) {
            return;
        }
        if (!scr || !scr->screen_texture) {
            return;
        }

        std::lock_guard<std::mutex> present_lock(state->present_mutex);

        // wait_for blocks while the slot is -100 (in-flight) and returns
        // immediately once the driver thread has called finish() (or for the
        // initial 0). With two ping-ponging slots this blocks on the present
        // from two frames ago, so at most two frames are ever in flight: the
        // guest computes the next frame while the graphics thread does the
        // (synchronous, vsync-gated) swap of the previous one.
        const int slot = state->present_slot;
        state->graphics_driver->wait_for(&state->present_status[slot]);

        eka2l1::drivers::graphics_command_builder builder;
        const eka2l1::vec2 swapchain_size = state->window->window_fb_size();
        builder.set_swapchain_size(swapchain_size);
        builder.backup_state();
        builder.bind_bitmap(0);
        builder.set_feature(eka2l1::drivers::graphics_feature::cull, false);
        builder.set_feature(eka2l1::drivers::graphics_feature::depth_test, false);
        builder.set_feature(eka2l1::drivers::graphics_feature::blend, false);
        builder.set_feature(eka2l1::drivers::graphics_feature::clipping, false);
        builder.set_feature(eka2l1::drivers::graphics_feature::stencil_test, false);

        eka2l1::rect viewport;
        viewport.size = swapchain_size;
        builder.set_viewport(viewport);
        builder.clear({ 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f },
            eka2l1::drivers::draw_buffer_bit_color_buffer);

        // The guest's own orientation.
        const int rotation = scr->ui_rotation % 360;

        auto &mode = scr->current_mode();

        // Keep accelerometer samples aligned with what the player sees: the
        // emulated device's natural orientation sits (mode.rotation -
        // natural_mode.rotation) CCW from the guest picture, and the picture
        // sits host_interface_rotation_deg CCW from the iPhone's natural
        // orientation. wsini SCR_ROTATION is framebuffer-to-panel, so the
        // first mode (the orientation the device is normally held in, where
        // Symbian defines the sensor axes) must be subtracted as the panel
        // mount angle: S60v5 nHD panels are landscape-native (portrait mode
        // rot=270), unlike Symbian^3 (rot=0). Refreshing here tracks both
        // guest screen-mode switches and host rotations (a host rotation
        // re-attaches the surface, which re-presents even a static screen).
        {
            const eka2l1::epoc::config::screen_mode *natural_mode = scr->mode_info(0);
            const int panel_mount = natural_mode ? natural_mode->rotation : 0;
            const int picture_rotation = mode.rotation - panel_mount
                + state->host_interface_rotation_deg.load(std::memory_order_relaxed);

            if (state->sensor_driver) {
                state->sensor_driver->set_motion_rotation(picture_rotation);
            }

            // A camera is bolted to the device body, so it needs the mirror of the
            // accelerometer's angle for the host term: turning the phone counter-
            // clockwise spins the scene clockwise inside the sensor buffer, while
            // the interface counter-rotates the picture to stay upright for the
            // viewer. The guest term keeps its sign -- an app that composes for a
            // rotated panel already lays the frame out for it.
            eka2l1::drivers::camera::set_frame_rotation(mode.rotation - panel_mount
                - state->host_interface_rotation_deg.load(std::memory_order_relaxed));
        }
        eka2l1::rect src;
        src.size = mode.size;

        eka2l1::vec2 display_size = mode.size;
        if (rotation % 180 != 0) {
            std::swap(display_size.x, display_size.y);
        }

        float scale = std::min(
            static_cast<float>(swapchain_size.x) / static_cast<float>(display_size.x),
            static_cast<float>(swapchain_size.y) / static_cast<float>(display_size.y));
        if (scale <= 0.0f) {
            return;
        }
        const float width = display_size.x * scale;
        const float height = display_size.y * scale;

        eka2l1::rect dest;
        dest.top.x = static_cast<int>((swapchain_size.x - width) / 2.0f);
        dest.top.y = static_cast<int>((swapchain_size.y - height) / 2.0f);
        dest.size.x = static_cast<int>(width);
        dest.size.y = static_cast<int>(height);

        const int anchor_top = state->display_anchor_top_px.load(std::memory_order_relaxed);
        if (anchor_top >= 0) {
            const int max_top = std::max(0, swapchain_size.y - static_cast<int>(height));
            dest.top.y = std::min(anchor_top, max_top);
        }

        scr->set_native_scale_factor(state->graphics_driver.get(), scale, scale);
        scr->absolute_pos = dest.top;

        eka2l1::drivers::advance_draw_pos_around_origin(dest, rotation);
        if (rotation % 180 != 0) {
            std::swap(dest.size.x, dest.size.y);
        }
        src.size *= scr->display_scale_factor;

        std::uint32_t flags = 0;
        if (scr->flags_ & eka2l1::epoc::screen::FLAG_SCREEN_UPSCALE_FACTOR_LOCK) {
            flags |= eka2l1::drivers::bitmap_draw_flag_use_upscale_shader;
        }

        builder.set_texture_filter(scr->screen_texture, true, eka2l1::drivers::filter_option::linear);
        builder.set_texture_filter(scr->screen_texture, false, eka2l1::drivers::filter_option::linear);
        builder.draw_bitmap(scr->screen_texture, 0, dest, src, eka2l1::vec2(0, 0),
            static_cast<float>(rotation), flags);

        builder.load_backup_state();
        state->present_status[slot] = -100;
        builder.present(&state->present_status[slot]);
        eka2l1::drivers::command_list commands = builder.retrieve_command_list();
        state->graphics_driver->submit_command_list(commands);
        state->present_slot ^= 1;
        state->rendered_frame_count.fetch_add(1, std::memory_order_relaxed);
    }

    // Re-present the primary screen's current texture without touching guest
    // window-server state — safe to call while the guest loop is running. Used
    // after a surface resize so a static screen re-composes at the new size.
    static void re_present_screen(emulator *state) {
        eka2l1::epoc::screen *scr = state->winserv ? state->winserv->get_screens() : nullptr;
        submit_screen_frame(state, scr);
    }

    // Force the primary screen to (re)create its texture and present a frame.
    // The first frames after launch can land before the guest has drawn, so the
    // launch path schedules a couple of these to kick a stale/black screen.
    static void kick_screen_redraw(emulator *state) {
        eka2l1::epoc::screen *scr = state->winserv ? state->winserv->get_screens() : nullptr;
        if (scr && state->graphics_driver && state->symsys) {
            // scr->redraw() walks the guest window tree and rasterises text
            // through the (non-reentrant) FreeType font atlas. The window
            // server's own animation_scheduler drives the same redraw from the
            // ntimer thread under kern->lock() + screen_mutex, so this launch
            // kick — dispatched onto a global queue — must take the identical
            // locks in the same order, or the two redraws race inside FreeType
            // and corrupt the shared glyph cache (SIGSEGV in get_glyph_atlas).
            if (auto *kern = state->symsys->get_kernel_system()) {
                kern->lock();
                {
                    const std::lock_guard<std::mutex> guard(scr->screen_mutex);
                    if (!scr->screen_texture) {
                        scr->set_screen_mode(state->winserv, state->graphics_driver.get(), scr->crr_mode);
                    }
                    scr->redraw(state->graphics_driver.get());
                }
                kern->unlock();
            }
        }
        submit_screen_frame(state, scr);
    }

    static void install_required_rom_patches(emulator *state) {
        if (!state || !state->symsys) {
            return;
        }

        auto *io = state->symsys->get_io_system();
        // Same folder lib_manager::load_patch_libraries scans, which the
        // frontend redirects into the read-only app bundle at startup.
        const std::string patch_dir = eka2l1::runtime_resource_path("patch");
        const std::vector<std::tuple<std::u16string, std::string, epocver>> dlls_need_to_copy = {
            { u"Z:\\sys\\bin\\goommonitor.dll", "goommonitor_general.dll", epocver::epoc94 },
            { u"Z:\\sys\\bin\\avkonfep.dll", "avkonfep_general.dll", epocver::epoc93fp1 }
        };

        for (const auto &entry : dlls_need_to_copy) {
            if (state->symsys->get_symbian_version_use() < std::get<2>(entry)) {
                continue;
            }

            auto destination = io->get_raw_path(std::get<0>(entry));
            if (!destination.has_value()) {
                continue;
            }

            const std::string source = eka2l1::add_path(patch_dir, std::get<1>(entry));
            const std::string dest = common::ucs2_to_utf8(destination.value());
            if (!common::exists(source)) {
                continue;
            }

            const std::string backup = dest + ".bak";
            if (common::exists(dest) && !common::exists(backup)) {
                common::move_file(dest, backup);
            }
            common::copy_file(source, dest, true);
        }
    }
}

@interface EKA2L1Emulator ()
// Main-queue bounce target for the launched process' exit logon. `generation`
// identifies the launch the logon belonged to, so a stale re-fire is dropped.
- (void)handleRunningAppExitedForGeneration:(std::uint64_t)generation
                               fatalDetails:(nullable NSString *)fatalDetails;
// Synchronous launch body, run off the main thread by launchAppWithUID:completion:.
- (BOOL)runLaunchAppWithUID:(uint32_t)uid;
// Uninstall path for apps with no package registry (N-Gage game cards).
- (BOOL)removeUnpackagedAppWithUID:(uint32_t)uid;
// Post-uninstall cleanup for a registration the package did not own.
- (void)dropLeftoverRegistrationOf:(eka2l1::apa_app_registry *)reg;
@end

@implementation EKA2L1Emulator {
    std::unique_ptr<eka2l1::ios::emulator> _state;
    // Host pointer identity (UITouch address) → guest pointer number. The guest
    // event's ptr_num is a uint8_t indexed pointer slot on Symbian^3 (advanced
    // pointers), so raw UITouch identities must be mapped to small stable
    // indices with down/up pairing — mirrors Qt's map_mouse_id_to_touch_index.
    std::array<uintptr_t, 8> _touchSlots;
    std::mutex _touchSlotsLock;
    // Last successful rescanApps result, returned as-is when a system rebuild
    // holds session_mutex (rescanApps must not block the main thread).
    NSArray<EKA2L1AppEntry *> *_lastAppList;
}

+ (instancetype)shared {
    static EKA2L1Emulator *instance;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        instance = [[EKA2L1Emulator alloc] init];
    });
    return instance;
}

- (BOOL)startWithDocumentsPath:(NSString *)documentsPath {
    if (_state && _state->running) {
        return YES;
    }

    _state = std::make_unique<eka2l1::ios::emulator>();
    _state->documents_root = documentsPath.UTF8String;

    // Build the sandbox layout up front so later steps can rely on it.
    NSFileManager *fm = NSFileManager.defaultManager;
    NSURL *cachesURL = [fm URLForDirectory:NSCachesDirectory
                                  inDomain:NSUserDomainMask
                         appropriateForURL:nil
                                    create:YES
                                     error:nil];
    if (cachesURL) {
        _state->caches_root = cachesURL.path.UTF8String;
    }
    // Drive letters mirror Symbian's uppercase convention. iOS app data
    // containers are case-sensitive (despite the host APFS volume being
    // case-insensitive), so rescan_devices()'s "drives/Z/" probe and the
    // mount() calls below have to find these exact names on disk.
    // Use lowercase drive letters and firmcode dir names throughout. The
    // system code paths that read the ROM / Z drive build their paths via
    // common::lowercase_string(firmcode) and lowercase drive letters, and
    // the iOS sim's runtime presents the app with a case-sensitive view of
    // the host APFS volume (which itself is case-insensitive, so we can
    // only ever have one entry per case-insensitive name on disk). Naming
    // everything lowercase ensures both views agree.
    // Only the emulator's own tree is created here. roms/ is made on demand by
    // the device install, and ROM/RPKG/SIS files are all read straight from the
    // picked URL, so nothing else needs to sit empty in the user-visible
    // Documents root.
    NSArray<NSString *> *subdirs = @[@"data",
                                      @"data/drives/c", @"data/drives/d",
                                      @"data/drives/e", @"data/drives/z",
                                      @"data/compat"];
    for (NSString *sub in subdirs) {
        NSString *path = [documentsPath stringByAppendingPathComponent:sub];
        [fm createDirectoryAtPath:path withIntermediateDirectories:YES attributes:nil error:nil];
    }

    // Run the emulator with cwd = Documents/data so the config / drive paths
    // resolve into the sandbox rather than the (read-only) bundle.
    NSString *dataRoot = [documentsPath stringByAppendingPathComponent:@"data"];

    // The emulator opens its shipped resources through cwd-relative paths:
    // "resources/*.vert|frag" plus "resources/upscale/*.frag" (ogl_shader_module),
    // "resources/defaultbank.sf2" (player_tsf), and ".//patch//" scanned by
    // lib_manager::load_patch_libraries. All of those readers are read-only, so
    // point them at the app bundle instead of staging a copy into the sandbox:
    // the working directory below stays the writable data tree (config, drives,
    // logs), and set_runtime_resource_root redirects just the shipped ones.
    // Documents is user-visible through the Files app, so keeping the resources
    // out of it also keeps them out of reach of an accidental edit or delete.
    //
    // The bundle subfolder is "emures", not "resources": a top-level "resources"
    // would collide with the reserved "Resources" bundle dir on the
    // case-insensitive build host and break codesign, so the names the emulator
    // expects live one level down (see src/emu/ios/CMakeLists.txt).
    NSString *bundleResourceRoot = [NSBundle.mainBundle.resourcePath
        stringByAppendingPathComponent:@"emures"];
    eka2l1::set_runtime_resource_root(bundleResourceRoot.UTF8String);

    // Earlier builds copied those resources into the sandbox instead of reading
    // them from the bundle. Nothing reads the copies anymore, so drop them rather
    // than leaving ~1MB of dead weight in the user's backed-up Documents.
    for (NSString *stale in @[@"resources", @"patch"]) {
        [fm removeItemAtPath:[dataRoot stringByAppendingPathComponent:stale] error:nil];
    }
    // Cache contents are disposable and belong under Library/Caches. Remove
    // the complete cache tree left in Documents/data by older builds.
    [fm removeItemAtPath:[dataRoot stringByAppendingPathComponent:@"cache"] error:nil];

    chdir(dataRoot.UTF8String);

    eka2l1::log::setup_log(nullptr);
    LOG_INFO(eka2l1::FRONTEND_CMDLINE, "EKA2L1 iOS v26.7.0 ({}-{})", GIT_BRANCH, GIT_COMMIT_HASH);

    // A bundle missing this folder means the copy-files build phases did not
    // run: no shaders (null FILE* in the ogl driver) and an unpatched guest.
    // Say so once here instead of leaving a pile of confusing follow-up errors.
    if (![fm fileExistsAtPath:bundleResourceRoot]) {
        LOG_ERROR(eka2l1::FRONTEND_CMDLINE, "Bundle resource folder {} is missing!",
            bundleResourceRoot.UTF8String);
    }


    _state->conf.deserialize();
    _state->conf.storage = dataRoot.UTF8String;

    // Pick up (and clear) the marker a previous run left behind if it died
    // while bringing a device up — a damaged ROM/RPKG only shows itself there,
    // and it takes the process down instead of failing gracefully. The device
    // it names is kept out of the auto-boot below and reported to the frontend.
    _state->failed_boot_firmware = eka2l1::ios::take_boot_attempt(_state.get());
    if (!_state->failed_boot_firmware.empty()) {
        LOG_ERROR(eka2l1::FRONTEND_CMDLINE, "Previous run did not survive booting device {}; skipping it",
            _state->failed_boot_firmware);
    }
    // Force cpu_load_save on, matching desktop/Android's default: when the guest
    // has no ready thread the scheduler parks the os_thread on idle_event instead
    // of spinning symsys->loop(), so an idle screen (e.g. the N97 home screen)
    // stops pinning a host core at 100%. We override here rather than just
    // trusting the default because earlier builds hard-set this false (a leftover
    // from launch-path debugging) and saveSettings persisted that into config.yml;
    // deserialize() above would otherwise keep loading the stale false. There is
    // no iOS UI toggle for this, so the default must always win.
    // shutdown/pause/boot call break_core_idling() so a parked loop still tears
    // down promptly.
    _state->conf.cpu_load_save = true;

    // Apply the configured log filter, the way Qt does it in state.cpp — the iOS
    // frontend used to skip this step, leaving the log_filterings ctor's
    // every-class-at-trace default in place. The filter string is taken as
    // written: an earlier version here rewrote it whenever it equalled the
    // "*:trace" debug preset, which made a filter the user had set explicitly
    // indistinguishable from an unconfigured one (that preset is also this
    // build's default, since the app is not a BUILD_FOR_USER build). What that
    // rewrite really guarded against was dyncom's per-op VFP trace on the
    // interpreter's hot path, and those sites are now compiled out unless
    // EKA2L1_DYNCOM_VFP_TRACE is turned on (see src/emu/cpu/CMakeLists.txt).
    if (eka2l1::log::filterings && !_state->conf.log_filter.empty()) {
        eka2l1::log::filterings->parse_filter_string(_state->conf.log_filter);
    }

    _state->settings = std::make_unique<eka2l1::config::app_settings>(&_state->conf);

    // Instantiate the audio driver up front, so services that fan out
    // audio_driver at startup (KeySound, MediaClient, DSP shared streams) get a
    // real instance rather than a null one. On iOS the cubeb backend selector
    // resolves to the native AURemoteIO driver, which configures AVAudioSession
    // (Playback category, mix-with-others) on the first call.
    eka2l1::drivers::player_type midi_be = eka2l1::drivers::player_type_tsf;
    _state->audio_driver = eka2l1::drivers::make_audio_driver(
        eka2l1::drivers::audio_driver_backend::cubeb,
        _state->conf.audio_master_volume,
        midi_be);
    if (_state->audio_driver) {
        // The configured paths are the shared "resources/defaultbank.*"
        // defaults unless the user pointed them somewhere else, so resolve them
        // against the bundle; an absolute user path passes through untouched.
        _state->audio_driver->set_bank_path(eka2l1::drivers::MIDI_BANK_TYPE_HSB,
            eka2l1::runtime_resource_path(_state->conf.hsb_bank_path));
        _state->audio_driver->set_bank_path(eka2l1::drivers::MIDI_BANK_TYPE_SF2,
            eka2l1::runtime_resource_path(_state->conf.sf2_bank_path));
    } else {
        LOG_WARN(eka2l1::FRONTEND_CMDLINE,
            "iOS audio: cubeb_audio_driver instance is null; services will fall back to silence");
    }

    // CoreMotion-backed accelerometer for the guest Sensor Framework, same
    // wiring as the Qt/Android frontends. Bound to each booted system in
    // bootDeviceAtIndex: alongside the audio driver.
    _state->sensor_driver = eka2l1::drivers::sensor_driver::instantiate();
    if (!_state->sensor_driver) {
        LOG_WARN(eka2l1::FRONTEND_CMDLINE, "Failed to create sensor driver");
    }

    auto comp = eka2l1::ios::make_system_components(_state.get());
    _state->symsys = std::make_unique<eka2l1::system>(comp);
    _state->window = std::make_unique<eka2l1::drivers::emu_window_ios>();

    _state->running = true;

    auto *state = _state.get();
    _state->graphics_thread = std::make_unique<std::thread>([state]() {
        eka2l1::common::set_thread_name("Graphics thread");
        eka2l1::common::set_thread_priority(eka2l1::common::thread_priority_high);

        // Wait for the EAGLView to publish its CAEAGLLayer; the EAGL context
        // can't be created without a drawable. attachLayer:pixelSize:scale:
        // flips layer_dirty under layer_mutex.
        std::unique_lock<std::mutex> lock(state->layer_mutex);
        state->layer_cv.wait(lock, [state]() {
            return !state->running || state->pending_layer != nullptr;
        });
        if (!state->running) {
            return;
        }
        state->window->surface_changed(state->pending_layer, state->pending_width,
            state->pending_height, state->pending_scale);
        state->layer_dirty = false;
        lock.unlock();

        // Build the graphics driver on this thread so the EAGL context is
        // current here. The OGL driver's run() loop owns the thread until
        // the symsys / driver tear down.
        auto graphics_driver = eka2l1::drivers::create_graphics_driver(
            eka2l1::drivers::graphic_api::opengl,
            state->window->get_window_system_info());
        if (!graphics_driver) {
            LOG_ERROR(eka2l1::DRIVER_GRAPHICS, "iOS graphics driver creation failed");
            state->layer_cv.notify_all();
            return;
        }
        {
            std::lock_guard<std::mutex> publish_lock(state->layer_mutex);
            // Install the surface hook before publishing the driver, under the
            // same lock. attachLayer: treats a visible driver as "the hook is
            // ready" and then calls it without holding this lock, so assigning
            // the std::function afterwards let the main thread observe a
            // half-constructed callable and jump through its vptr.
            state->window->surface_change_hook = [state](void *new_surface) {
                state->graphics_driver->update_surface(new_surface);
            };
            state->graphics_driver = std::move(graphics_driver);
        }
        state->layer_cv.notify_all();

        state->graphics_driver->set_display_hook([]() {
            // CAEAGLLayer presentation is implicit in gl_context_eagl::
            // swap_buffers; nothing extra to poll here. iOS lifecycle hooks
            // gate pause/resume via context::pause()/resume().
        });

        state->graphics_driver->run();
    });

    _state->os_thread = std::make_unique<std::thread>([state]() {
        eka2l1::common::set_thread_name("Symbian OS thread");
        eka2l1::common::set_thread_priority(eka2l1::common::thread_priority_high);

        while (state->running) {
            if (state->paused || !state->mounted.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
                continue;
            }
            try {
                std::lock_guard<std::mutex> loop_lock(state->loop_mutex);
                state->symsys->loop();
            } catch (std::exception &exc) {
                LOG_ERROR(eka2l1::FRONTEND_CMDLINE, "Emu loop exception: {}", exc.what());
                state->running = false;
                break;
            }
        }
    });

    // If a device was previously installed, boot straight into it (restoring
    // the last-selected one from conf.device) so the frontend lands on the
    // app list rather than the empty-state import prompt.
    auto *dvc = _state->symsys->get_device_manager();
    if (dvc && dvc->total() > 0) {
        std::size_t want = static_cast<std::size_t>(std::max(0, _state->conf.device));
        if (want >= dvc->total()) {
            want = 0;
        }
        // A device the previous run died on must not be auto-booted again, or
        // a broken dump crashes the app on every launch with no chance to
        // reach the UI and remove it. Fall back to the first device that isn't
        // the suspect one; if it is the only one, come up device-less and let
        // the frontend report it.
        if (!_state->failed_boot_firmware.empty()) {
            std::lock_guard<std::mutex> dvc_lock(dvc->lock);
            auto &devices = dvc->get_devices();
            const auto is_suspect = [&](const std::size_t idx) {
                return eka2l1::common::compare_ignore_case(devices[idx].firmware_code.c_str(),
                    _state->failed_boot_firmware.c_str()) == 0;
            };
            if (is_suspect(want)) {
                want = devices.size();
                for (std::size_t i = 0; i < devices.size(); i++) {
                    if (!is_suspect(i)) {
                        want = i;
                        break;
                    }
                }
            }
        }
        if (want < dvc->total()) {
            [self bootDeviceAtIndex:want];
        }
    }

    return YES;
}

- (NSString *)takeFailedBootDeviceCode {
    if (!_state || _state->failed_boot_firmware.empty()) {
        return nil;
    }
    NSString *code = [NSString stringWithUTF8String:_state->failed_boot_firmware.c_str()];
    _state->failed_boot_firmware.clear();
    return code;
}

- (void)shutdown {
    if (!_state) {
        return;
    }
    // Wait out any in-flight rescan/boot before tearing the whole state down.
    // Released just before _state.reset() — the mutex lives inside _state.
    std::unique_lock<std::recursive_mutex> session_lock(_state->session_mutex);
    // Quiesce sensor callbacks (pause doubles as an in-flight barrier) before
    // the kernel they complete into goes away below.
    if (_state->sensor_driver) {
        _state->sensor_driver->pause();
    }
    _state->running = false;
    // Wake the os_thread if it's parked in the scheduler's idle wait, otherwise
    // the join below blocks until the next guest timer happens to fire.
    break_core_idling(_state.get());
    {
        std::lock_guard<std::mutex> lk(_state->layer_mutex);
        _state->layer_cv.notify_all();
    }
    if (_state->graphics_driver) {
        _state->graphics_driver->abort();
    }
    if (_state->os_thread && _state->os_thread->joinable()) {
        _state->os_thread->join();
    }
    if (_state->graphics_thread && _state->graphics_thread->joinable()) {
        _state->graphics_thread->join();
    }
    _state->graphics_driver.reset();
    _state->symsys.reset();
    _state->audio_driver.reset();
    _state->sensor_driver.reset();
    _state->window.reset();
    _state->settings.reset();
    session_lock.unlock();
    _state.reset();
}

- (NSArray<EKA2L1DeviceEntry *> *)installedDevices {
    NSMutableArray<EKA2L1DeviceEntry *> *out = [NSMutableArray array];
    if (!_state || !_state->symsys) {
        return out;
    }
    auto *dvc = _state->symsys->get_device_manager();
    if (!dvc) {
        return out;
    }
    std::lock_guard<std::mutex> dvc_lock(dvc->lock);
    auto &devices = dvc->get_devices();
    for (std::size_t i = 0; i < devices.size(); i++) {
        EKA2L1DeviceEntry *entry = [[EKA2L1DeviceEntry alloc] init];
        entry.index = i;
        entry.firmwareCode = [NSString stringWithUTF8String:devices[i].firmware_code.c_str()];
        entry.manufacturer = [NSString stringWithUTF8String:devices[i].manufacturer.c_str()];
        entry.model = [NSString stringWithUTF8String:devices[i].model.c_str()];
        [out addObject:entry];
    }
    return out;
}

- (NSInteger)currentDeviceIndex {
    if (!_state || !_state->mounted || !_state->symsys) {
        return -1;
    }
    auto *dvc = _state->symsys->get_device_manager();
    if (!dvc) {
        return -1;
    }
    return static_cast<NSInteger>(dvc->get_current_index());
}

// Shared body of the two install entry points below. `installer` is handed the
// device manager plus the two storage folders every installer writes into, and
// does the format-specific work; everything around it (freezing the emulator,
// progress plumbing, persisting devices.yml) is the same either way.
- (EKA2L1InstallResult)runDeviceInstall:(eka2l1::device_installation_error (^)(eka2l1::device_manager *dvc,
                                            const std::string &romResidentPath, const std::string &rootZPath,
                                            progress_changed_callback progressCb,
                                            cancel_requested_callback cancelCb))installer
                               progress:(void (^)(double))progress
                            cancelCheck:(BOOL (^)(void))cancelCheck {
    if (!_state || !_state->symsys) {
        return EKA2L1InstallResultGeneralFailure;
    }

    // Installing mutates device_manager + the sandbox storage tree; stop the
    // os_thread from ticking symsys->loop() while we work, then wait out any
    // in-flight tick before touching shared state. On a failed install the
    // installers revert their own changes, so restore the previous run state
    // (a device may already be booted) and let it keep ticking. On success the
    // frontend boots the new device, which rebuilds the system and remounts.
    const bool was_mounted = _state->mounted;
    std::lock_guard<std::recursive_mutex> session_lock(_state->session_mutex);
    auto loop_lock = eka2l1::ios::pause_loop_and_lock(_state.get());

    auto *sys = _state->symsys.get();
    auto *dvc = sys->get_device_manager();
    if (!dvc) {
        _state->mounted = was_mounted;
        return EKA2L1InstallResultGeneralFailure;
    }

    const std::string storage = _state->conf.storage;
    const std::string root_z_path = eka2l1::add_path(storage, "drives/z/");
    const std::string rom_resident_path = eka2l1::add_path(storage, "roms/");
    eka2l1::common::create_directories(rom_resident_path);

    // The installers report (done, total) pairs whose scale changes between
    // phases but whose ratio stays monotonic, so hand the frontend the ratio.
    // The last reported value is kept so a per-chunk callback doesn't wake the
    // frontend thousands of times for sub-percent moves.
    auto last_reported = std::make_shared<std::atomic<double>>(-1.0);
    progress_changed_callback progress_cb = nullptr;
    if (progress) {
        progress_cb = [progress, last_reported](const std::size_t done, const std::size_t total) {
            if (!total) {
                return;
            }
            const double fraction = std::clamp(static_cast<double>(done) / static_cast<double>(total), 0.0, 1.0);
            if ((fraction < last_reported->load() + 0.005) && (fraction < 1.0)) {
                return;
            }
            last_reported->store(fraction);
            progress(fraction);
        };
    }

    cancel_requested_callback cancel_cb = nullptr;
    if (cancelCheck) {
        cancel_cb = [cancelCheck]() -> bool { return cancelCheck() == YES; };
    }

    const eka2l1::device_installation_error result = installer(dvc, rom_resident_path, root_z_path,
        progress_cb, cancel_cb);

    if (result != eka2l1::device_installation_none) {
        _state->mounted = was_mounted;
        // A cancel surfaces as whatever error the aborted step happened to
        // return (install_rom turns it into "ROM file corrupt"), so answer from
        // the flag the user actually set instead of from the error code.
        if (cancel_cb && cancel_cb()) {
            return EKA2L1InstallResultCancelled;
        }
        return eka2l1::ios::map_install_result(result);
    }

    dvc->save_devices();

    if (progress) {
        progress(1.0);
    }
    return EKA2L1InstallResultSuccess;
}

- (EKA2L1InstallResult)installDeviceWithRomPath:(NSString *)romPath
                                       rpkgPath:(NSString *)rpkgPath
                                       progress:(void (^)(double))progress
                                    cancelCheck:(BOOL (^)(void))cancelCheck {
    if (![NSFileManager.defaultManager fileExistsAtPath:romPath]) {
        return EKA2L1InstallResultNotExist;
    }

    const std::string rom_std = romPath.UTF8String;
    const std::string rpkg_std = rpkgPath ? std::string(rpkgPath.UTF8String) : std::string();

    return [self runDeviceInstall:^(eka2l1::device_manager *dvc, const std::string &rom_resident_path,
                                     const std::string &root_z_path, progress_changed_callback progress_cb,
                                     cancel_requested_callback cancel_cb) {
        return eka2l1::loader::install_rom_with_optional_rpkg(dvc, rom_std, rpkg_std, rom_resident_path,
            root_z_path, progress_cb, cancel_cb);
    } progress:progress cancelCheck:cancelCheck];
}

- (EKA2L1InstallResult)installDeviceWithArchivePath:(NSString *)archivePath
                                           progress:(void (^)(double))progress
                                        cancelCheck:(BOOL (^)(void))cancelCheck {
    if (![NSFileManager.defaultManager fileExistsAtPath:archivePath]) {
        return EKA2L1InstallResultNotExist;
    }

    const std::string archive_std = archivePath.UTF8String;

    return [self runDeviceInstall:^(eka2l1::device_manager *dvc, const std::string &rom_resident_path,
                                     const std::string &root_z_path, progress_changed_callback progress_cb,
                                     cancel_requested_callback cancel_cb) {
        return eka2l1::loader::install_archive(dvc, archive_std, rom_resident_path, root_z_path, progress_cb,
            cancel_cb);
    } progress:progress cancelCheck:cancelCheck];
}

- (BOOL)bootDeviceAtIndex:(NSUInteger)index {
    return [self bootDeviceAtIndex:index trackAttempt:YES];
}

// trackAttempt drops the boot-attempt marker that guards against a dump which
// only turns out to be broken at boot time. Only user-driven boots need it: the
// relaunch reboot re-runs a device that already came up in this session, and
// leaving a marker behind there would quarantine a healthy device on the next
// launch (nothing rescans the app list afterwards to clear it).
- (BOOL)bootDeviceAtIndex:(NSUInteger)index trackAttempt:(BOOL)trackAttempt {
    if (!_state) {
        return NO;
    }
    const std::string storage = _state->conf.storage;

    // Rebuild the system so device_manager reloads devices.yml fresh and the
    // kernel comes up clean for the selected device. device_manager only
    // reads devices.yml on construction. Take the session lock so an app
    // rescan (worker pool inside the old symsys) finishes before the rebuild
    // frees the state under it, then stop + drain the os_thread so the symsys
    // reset below doesn't race a loop in flight.
    std::lock_guard<std::recursive_mutex> session_lock(_state->session_mutex);
    auto loop_lock = eka2l1::ios::pause_loop_and_lock(_state.get());
    // Quiesce the sensor pump before the old system goes away: its data
    // callbacks complete guest IPC and must not land in a dying kernel.
    // pause() doubles as a barrier for a callback already in flight. Only
    // resume below if this pause flipped the state — a lifecycle pause
    // (backgrounded app) must stay paused.
    const bool sensor_paused_for_reboot = _state->sensor_driver && _state->sensor_driver->pause();
    struct sensor_resume_guard {
        eka2l1::drivers::sensor_driver *drv_;
        ~sensor_resume_guard() {
            if (drv_) {
                drv_->resume();
            }
        }
    } sensor_resume{ sensor_paused_for_reboot ? _state->sensor_driver.get() : nullptr };
    _state->winserv = nullptr;
    _state->screen_redraw_handles.clear();
    _state->rendered_frame_count.store(0, std::memory_order_relaxed);
    // Drop any present fences left in-flight by the previous session so the
    // first frame of the new one doesn't block waiting on a finish() that the
    // torn-down graphics driver will never deliver.
    _state->present_status[0] = 0;
    _state->present_status[1] = 0;
    _state->present_slot = 0;

    auto comp = eka2l1::ios::make_system_components(_state.get());
    _state->symsys = std::make_unique<eka2l1::system>(comp);
    auto *sys = _state->symsys.get();

    sys->startup();
    auto *dvc = sys->get_device_manager();
    if (!dvc || index >= dvc->total()) {
        return NO;
    }
    if (!sys->set_device(static_cast<std::uint8_t>(index))) {
        return NO;
    }
    _state->conf.device = static_cast<int>(index);
    // Publish for the main thread, which must not touch symsys itself. Only
    // update it once the device is known good, so a failed boot keeps the last
    // answer instead of flipping the emulator screen to the wrong input model.
    _state->device_is_touch_screen.store(sys->get_symbian_version_use() >= epocver::epoc94,
        std::memory_order_relaxed);

    // Mirror the Android frontend's device-switch behavior: when the
    // configured system language isn't shipped by this device's ROM (or was
    // never set), fall back to the device's default language. Services boot
    // below (setup_outsider) reads conf.language, so fix it up first.
    std::string firmware_code;
    {
        std::lock_guard<std::mutex> dvc_lock(dvc->lock);
        const auto &device = dvc->get_devices()[index];
        firmware_code = device.firmware_code;
        const auto &device_langs = device.languages;
        if (std::find(device_langs.begin(), device_langs.end(), _state->conf.language) == device_langs.end()) {
            _state->conf.language = device.default_language_code;
        }
    }

    // Everything below reads the device's own image. conf keeps the new
    // selection in memory from here on, but it is only written to disk once the
    // frontend reaches the app list on the other side of this boot
    // (confirmDeviceBoot) — the point at which the device has proven it can
    // carry the process.
    if (trackAttempt) {
        eka2l1::ios::mark_boot_attempt(_state.get(), firmware_code);
    }

    sys->mount(drive_c, drive_media::physical, eka2l1::add_path(storage, "/drives/c/"), io_attrib_internal);
    sys->mount(drive_d, drive_media::physical, eka2l1::add_path(storage, "/drives/d/"), io_attrib_internal);
    sys->mount(drive_e, drive_media::physical, eka2l1::add_path(storage, "/drives/e/"), io_attrib_removeable);
    sys->mount(drive_z, drive_media::rom, eka2l1::add_path(storage, "/drives/z/"),
        io_attrib_internal | io_attrib_write_protected);

    // Bind the drivers before initialize_user_parties so the services it
    // boots (window server, key sound, ...) come up against a live driver.
    // The graphics driver is re-bound via bind_graphics_driver() below once
    // the device is mounted, so it does not need setting again here.
    if (_state->graphics_driver) {
        sys->set_graphics_driver(_state->graphics_driver.get());
    }
    if (_state->audio_driver) {
        sys->set_audio_driver(_state->audio_driver.get());
    }
    if (_state->sensor_driver) {
        sys->set_sensor_driver(_state->sensor_driver.get());
    }
    sys->initialize_user_parties();
    eka2l1::ios::install_required_rom_patches(_state.get());
    sys->get_packages()->load_registries();
    sys->get_packages()->migrate_legacy_registries();

    _state->winserv = eka2l1::ios::get_window_server(sys->get_kernel_system());

    _state->mounted = true;
    eka2l1::ios::bind_graphics_driver(_state.get());

    // Register a per-screen redraw callback so each frame produced by the
    // Symbian window server triggers a swap on the EAGL context.
    if (_state->winserv) {
        auto *state = _state.get();
        eka2l1::epoc::screen *screens = _state->winserv->get_screens();
        while (screens) {
            std::size_t handle = screens->add_screen_redraw_callback(state,
                [](void *userdata, eka2l1::epoc::screen *scr, const bool /*is_dsa*/) {
                    auto *st = reinterpret_cast<eka2l1::ios::emulator *>(userdata);
                    if (!st->graphics_driver) {
                        return;
                    }
                    eka2l1::ios::submit_screen_frame(st, scr);
                });
            _state->screen_redraw_handles.push_back(handle);
            screens = screens->next;
        }
    }

    // The device is up; the marker stays until the frontend confirms, but the
    // boot itself is no longer in flight, so that confirmation may now land.
    _state->boot_in_flight = false;
    return YES;
}

- (BOOL)deleteDeviceAtIndex:(NSUInteger)index {
    if (!_state || !_state->symsys) {
        return NO;
    }
    // Mutating device_manager + deleting the sandbox storage tree must not race
    // the os loop or an app rescan. Mirror the install/boot prologue: take the
    // session lock, then stop and drain the loop. The caller reboots afterwards
    // (or drops to the empty state), so preserve the prior mounted flag.
    const bool was_mounted = _state->mounted;
    std::lock_guard<std::recursive_mutex> session_lock(_state->session_mutex);
    auto loop_lock = eka2l1::ios::pause_loop_and_lock(_state.get());

    auto *dvc = _state->symsys->get_device_manager();
    if (!dvc) {
        _state->mounted = was_mounted;
        return NO;
    }

    std::string firmcode;
    {
        std::lock_guard<std::mutex> dvc_lock(dvc->lock);
        auto &devices = dvc->get_devices();
        if (index >= devices.size()) {
            _state->mounted = was_mounted;
            return NO;
        }
        firmcode = devices[index].firmware_code;
    }

    // delete_device takes dvc->lock itself, so it must be called without the
    // scoped lock above held.
    const bool removed = dvc->delete_device(firmcode);

    // Drop everything on disk that belongs to this device alone: its ROM filesystem
    // and image, plus the folders the central repository and the message store keep
    // per device on the shared C, D and E drives. Leaving the latter behind is what
    // makes "delete the device and install it again" fail to repair anything.
    const std::string firmcode_low = eka2l1::common::lowercase_string(firmcode);
    const std::string storage = _state->conf.storage;

    for (const std::string &per_device : eka2l1::per_device_storage_paths(firmcode)) {
        eka2l1::common::delete_folder(eka2l1::add_path(storage, per_device));
    }

    // The debinarized-SVG icon cache is keyed by firmware code as well (the same app
    // UID ships different art per device), so it would otherwise feed stale icons to
    // a reinstalled device.
    if (!_state->caches_root.empty()) {
        eka2l1::common::delete_folder(eka2l1::add_path(_state->caches_root, "icons/" + firmcode_low + "/"));
    }

    // Keep conf.device in step with device_manager's adjusted current index so
    // a later boot (or a fresh launch) targets the surviving device.
    _state->conf.device = dvc->get_current_index();
    _state->conf.serialize();

    _state->mounted = was_mounted;
    return removed ? YES : NO;
}

- (BOOL)renameDeviceAtIndex:(NSUInteger)index toName:(NSString *)name {
    if (!_state || !_state->symsys) {
        return NO;
    }
    auto *dvc = _state->symsys->get_device_manager();
    if (!dvc) {
        return NO;
    }
    // A rename only rewrites the in-memory model string and serialises
    // devices.yml — it touches neither symsys memory nor the drive tree, so
    // (like installedDevices) the device_manager lock alone guards it against a
    // concurrent list read. save_devices() does not take the lock itself.
    std::lock_guard<std::mutex> dvc_lock(dvc->lock);
    auto &devices = dvc->get_devices();
    if (index >= devices.size()) {
        return NO;
    }
    devices[index].model = name.UTF8String ? name.UTF8String : "";
    dvc->save_devices();
    return YES;
}

- (BOOL)rescanDevices {
    if (!_state) {
        return NO;
    }
    // Mirrors the Android/Qt launcher's rescan_devices call. It clears +
    // repopulates device_manager from what's found on drive Z, then (if
    // anything was found) resets the current symsys to index 0 as a side
    // effect via set_device()/reset() — that call chain reaches down into
    // kernel_system::install_memory(), which on iOS is only ever exercised
    // right after bootDeviceAtIndex:'s fresh symsys + startup() prologue.
    // Calling it on the long-lived _state->symsys (which may have never been
    // through set_device if no device was booted yet) crashes there. Run the
    // same rebuild prologue bootDeviceAtIndex: uses so rescan_devices() always
    // sees a freshly-started system, exactly like a real boot would. The
    // caller must not treat the device as booted afterward — call
    // bootDeviceAtIndex: next (like the install flow), which rebuilds symsys
    // again and rereads the devices.yml this call just saved.
    std::lock_guard<std::recursive_mutex> session_lock(_state->session_mutex);
    auto loop_lock = eka2l1::ios::pause_loop_and_lock(_state.get());
    const bool sensor_paused_for_reboot = _state->sensor_driver && _state->sensor_driver->pause();
    struct sensor_resume_guard {
        eka2l1::drivers::sensor_driver *drv_;
        ~sensor_resume_guard() {
            if (drv_) {
                drv_->resume();
            }
        }
    } sensor_resume{ sensor_paused_for_reboot ? _state->sensor_driver.get() : nullptr };
    _state->winserv = nullptr;
    _state->screen_redraw_handles.clear();
    _state->present_status[0] = 0;
    _state->present_status[1] = 0;
    _state->present_slot = 0;

    auto comp = eka2l1::ios::make_system_components(_state.get());
    _state->symsys = std::make_unique<eka2l1::system>(comp);
    _state->symsys->startup();

    const bool found = _state->symsys->rescan_devices(drive_z);
    return found ? YES : NO;
}

- (NSArray<EKA2L1AppEntry *> *)rescanApps {
    NSMutableArray<EKA2L1AppEntry *> *out = [NSMutableArray array];
    if (!_state || !_state->symsys) {
        return out;
    }
    // The rescan fans registry/icon loading out to the applist worker pool,
    // which pokes fbs/io state — it must never overlap a system rebuild.
    // try_lock only: this is called from the main thread, and blocking main
    // while a boot is in flight deadlocks (boot -> graphics thread ->
    // dispatch_sync(main)). Hand back the previous list instead.
    std::unique_lock<std::recursive_mutex> session_lock(_state->session_mutex, std::try_to_lock);
    if (!session_lock.owns_lock()) {
        return _lastAppList ?: out;
    }
    if (!_state->symsys) {
        return _lastAppList ?: out;
    }
    auto *alserv = eka2l1::ios::get_applist_server(_state->symsys->get_kernel_system());
    if (!alserv) {
        return out;
    }
    // applist_server binds its FBS/FS dependencies lazily from
    // get_registerations().  EKA2 registry scans happened not to need FBS,
    // but EKA1 AIF icon loading creates bitmaps during the scan and crashes if
    // rescan_registries() is called before that lazy initialization.
    auto &registrations = alserv->get_registerations();
    alserv->rescan_registries(_state->symsys->get_io_system());
    for (auto &reg : registrations) {
        if (reg.caps.is_hidden) {
            continue;
        }
        EKA2L1AppEntry *entry = [[EKA2L1AppEntry alloc] init];
        entry.uid = reg.mandatory_info.uid;
        std::string name = eka2l1::common::ucs2_to_utf8(reg.mandatory_info.long_caption.to_std_string(nullptr));
        entry.name = [NSString stringWithUTF8String:name.c_str()];
        // Anything staged on the ROM drive (Z) ships with the firmware, so it is
        // a built-in/system app. User-installed packages land on a writable
        // drive (C/E). The iOS home screen hides system apps by default and only
        // offers uninstall for user apps, so treat the land drive as the sole
        // signal — the Qt UID-range check let ROM apps with high UIDs slip
        // through the "hide system apps" filter.
        entry.system = (reg.land_drive == drive_z);
        [out addObject:entry];
    }
    _lastAppList = out;
    return out;
}

- (void)confirmDeviceBoot {
    // The frontend reached the app list, so the device it just booted brought
    // the system up and kept it up: retire the crash marker and only now write
    // the selection to config.yml. Skipped while a boot is still running, so a
    // rescan that overlaps one doesn't sign off on a device that hasn't
    // finished coming up yet.
    if (!_state || _state->boot_in_flight.load() || _state->pending_boot_firmware.empty()) {
        return;
    }
    eka2l1::ios::clear_boot_attempt(_state.get());
    _state->conf.serialize();
}

- (void)launchAppWithUID:(uint32_t)uid completion:(void (^)(BOOL))completion {
    // Serialize launches and keep them off the main thread. The launch path
    // issues synchronous graphics commands (bind_graphics_driver ->
    // set_screen_mode -> create_bitmap) while the graphics worker thread bounces
    // the CAEAGLLayer attach back onto the main queue via dispatch_sync. If the
    // launch ran on the main thread the two would deadlock: main blocks on the
    // graphics thread, which blocks on a main queue that will never drain. Off
    // the main thread the main run loop stays free to service that bounce.
    dispatch_async(eka2l1::ios::emulator_control_queue(), ^{
        BOOL ok = [self runLaunchAppWithUID:uid];
        if (completion) {
            dispatch_async(dispatch_get_main_queue(), ^{
                completion(ok);
            });
        }
    });
}

- (BOOL)runLaunchAppWithUID:(uint32_t)uid {
    if (!_state || !_state->symsys) {
        return NO;
    }
    // Runs on the control queue (never main), so a blocking lock is safe. This
    // keeps a main-thread rescan's applist worker pool from overlapping both
    // the reboot below and launch_app's registration lookups.
    std::lock_guard<std::recursive_mutex> session_lock(_state->session_mutex);
    if (!eka2l1::ios::wait_for_graphics_driver(_state.get(), std::chrono::seconds(5))) {
        LOG_ERROR(eka2l1::DRIVER_GRAPHICS, "iOS graphics driver was not ready before app launch");
        return NO;
    }

    // A prior app left the booted session dirty (the window/view servers don't
    // reset cleanly between app instances). Rebuild the system for a clean
    // launch, mirroring the desktop frontend's reboot-on-exit. Only the second
    // and later launches pay this; the screen is black during launch anyway.
    if (_state->needs_reboot_before_launch) {
        _state->needs_reboot_before_launch = false;
        NSInteger reboot_index = [self currentDeviceIndex];
        if (reboot_index < 0) {
            reboot_index = std::max(0, _state->conf.device);
        }
        if (![self bootDeviceAtIndex:static_cast<NSUInteger>(reboot_index) trackAttempt:NO]) {
            LOG_ERROR(eka2l1::FRONTEND_CMDLINE, "iOS device reboot before relaunch failed");
            return NO;
        }
    }

    eka2l1::ios::bind_graphics_driver(_state.get());

    auto *kern = _state->symsys->get_kernel_system();
    auto *alserv = eka2l1::ios::get_applist_server(kern);
    if (!alserv) {
        return NO;
    }
    auto *reg = alserv->get_registration(uid);
    if (!reg) {
        return NO;
    }
    eka2l1::epoc::apa::command_line cmdline;
    cmdline.launch_cmd_ = eka2l1::epoc::apa::command_create;

    eka2l1::kernel::uid launched_thread_id = 0;
    const std::uint64_t generation = ++_state->launch_generation;
    kern->lock();
    // The exit callback fires from the kernel loop thread (process logon) when
    // the app leaves — normal exit, Exit soft key, kill or panic. Bounce to the
    // main queue so the SwiftUI frontend can close the emulator screen. self is
    // a singleton so a weak capture is just defensive. The generation guards
    // against a stale logon from a superseded launch closing this screen.
    __weak EKA2L1Emulator *weakSelf = self;
    bool launched = alserv->launch_app(*reg, cmdline, &launched_thread_id,
        [weakSelf, generation](eka2l1::kernel::process *pr) {
            NSString *fatalDetails = eka2l1::ios::guest_fatal_detail(pr);
            dispatch_async(dispatch_get_main_queue(), ^{
                [weakSelf handleRunningAppExitedForGeneration:generation
                                                 fatalDetails:fatalDetails];
            });
        });
    if (launched) {
        _state->running_thread_id = launched_thread_id;
        kern->stop_cores_idling();
    }
    kern->unlock();
    if (!_state->winserv) {
        _state->winserv = eka2l1::ios::get_window_server(kern);
    }
    if (launched && _state->winserv) {
        auto *state = _state.get();
        eka2l1::epoc::screen *immediate_scr = state->winserv ? state->winserv->get_screens() : nullptr;
        if (immediate_scr && !immediate_scr->screen_texture && state->graphics_driver) {
            immediate_scr->set_screen_mode(state->winserv, state->graphics_driver.get(), immediate_scr->crr_mode);
        }
        eka2l1::ios::submit_screen_frame(state, immediate_scr);
        // The first guest frame can lag the launch, so re-kick the screen a
        // couple of times to flush any leftover black frame from the swapchain.
        for (double delay : { 0.5, 1.5 }) {
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(delay * NSEC_PER_SEC)),
                dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
                    eka2l1::ios::kick_screen_redraw(state);
                });
        }
    }
    return launched ? YES : NO;
}

- (void)handleRunningAppExitedForGeneration:(std::uint64_t)generation
                               fatalDetails:(NSString *)fatalDetails {
    // Ignore a logon that belongs to a superseded launch: a previous app's
    // callback can re-fire while its process is torn down during the next
    // launch's reboot, and must not close the screen that just opened.
    if (!_state || generation != _state->launch_generation) {
        return;
    }
    // The tracked process is gone; drop its id so a later closeRunningApp from
    // the screen-teardown path doesn't try to kill a stale (or recycled) id,
    // and flag the session for a rebuild before the next launch.
    _state->running_thread_id = 0;
    _state->needs_reboot_before_launch = true;
    if (self.appExitHandler) {
        self.appExitHandler(fatalDetails);
    }
}

- (void)closeRunningApp {
    if (!_state || !_state->symsys) {
        return;
    }
    const eka2l1::kernel::uid tid = _state->running_thread_id;
    if (tid == 0) {
        return;
    }
    _state->running_thread_id = 0;
    // The killed app leaves the session dirty; rebuild before the next launch.
    _state->needs_reboot_before_launch = true;

    // Runs on the main thread, so only try the session lock (see rescanApps).
    // Losing it means a system rebuild is in flight: the old session — and the
    // guest process with it — is being torn down anyway, and the reboot flag
    // set above already forces a clean rebuild before the next launch.
    std::unique_lock<std::recursive_mutex> session_lock(_state->session_mutex, std::try_to_lock);
    if (!session_lock.owns_lock()) {
        return;
    }

    // Stop and drain the OS loop before killing the guest process. Otherwise
    // the loop thread can still be executing this process in HLE/active
    // scheduler code while the UI thread tears it down.
    auto loop_lock = eka2l1::ios::pause_loop_and_lock(_state.get());

    auto *kern = _state->symsys->get_kernel_system();
    if (!kern) {
        return;
    }

    // Kill the process under the kernel lock, the same guard launch uses to
    // mutate kernel state. The process logon still fires
    // (handleRunningAppExited); callers tearing down the screen clear
    // appExitHandler first so that bounce is a no-op.
    kern->lock();
    auto *thr = kern->get_by_id<eka2l1::kernel::thread>(tid);
    eka2l1::kernel::process *pr = thr ? thr->owning_process() : nullptr;
    if (pr && pr->get_exit_type() == eka2l1::kernel::entity_exit_type::pending) {
        pr->kill(eka2l1::kernel::entity_exit_type::kill, u"Closed", 0);
    }
    kern->stop_cores_idling();
    kern->unlock();

    // Killing the process orphans the HLE objects it owned (audio players above all), and
    // those are destroyed by the emulation loop — which is parked for good once the app list
    // is back, so an orphaned player would keep playing forever. Destroy them here instead,
    // now that the kernel lock is released: an audio teardown waits out the render callback
    // in flight, and that callback needs the kernel lock to finish guest notifications.
    if (auto *dispatcher = _state->symsys->get_dispatcher()) {
        dispatcher->flush_pending_teardown();
    }
}

- (BOOL)installSisAtPath:(NSString *)sisPath {
    if (!_state || !_state->symsys) {
        return NO;
    }
    std::u16string upath = eka2l1::common::utf8_to_ucs2(sisPath.UTF8String);
    drive_number install_drive = _state->symsys->is_s80_device_active()
        ? drive_number::drive_d
        : drive_number::drive_e;
    auto result = static_cast<eka2l1::package::installation_result>(
        _state->symsys->install_package(upath, install_drive));
    return result == eka2l1::package::installation_result_success ? YES : NO;
}

- (EKA2L1NGageInstallReport *)installNGageGameAtFolderPath:(NSString *)folderPath {
    EKA2L1NGageInstallReport *report = [[EKA2L1NGageInstallReport alloc] init];
    report.result = eka2l1::ngage_game_card_general_error;
    report.gameName = @"";

    if (!_state || !_state->symsys) {
        return report;
    }

    std::string gameName;
    eka2l1::ngage_game_card_install_error result = eka2l1::ngage_game_card_general_error;
    {
        // symsys->loop() may be parked in the scheduler's idle wait while it
        // still owns loop_mutex. A plain lock here can therefore wait forever.
        // Use the same writer prologue as device install/switch: protect the
        // symsys lifetime, stop new ticks, wake the parked one, then take the
        // loop lock between ticks.
        std::lock_guard<std::recursive_mutex> session_lock(_state->session_mutex);
        const bool was_mounted = _state->mounted;
        auto loop_lock = eka2l1::ios::pause_loop_and_lock(_state.get());
        result = _state->symsys->install_ngage_game_card(folderPath.UTF8String, [&](std::string name) {
            gameName = std::move(name);
        });
        _state->mounted = was_mounted;
    }

    report.result = result;
    report.gameName = gameName.empty() ? @"" : [NSString stringWithUTF8String:gameName.c_str()];
    return report;
}

- (BOOL)uninstallAppWithUID:(uint32_t)uid {
    if (!_state || !_state->symsys) {
        return NO;
    }
    auto *manager = _state->symsys->get_packages();
    if (!manager) {
        return NO;
    }
    auto *alserv = eka2l1::ios::get_applist_server(_state->symsys->get_kernel_system());
    eka2l1::apa_app_registry *reg = alserv ? alserv->get_registration(uid) : nullptr;

    // App UID3 == package UID for most single-app SIS packages, but Symbian does
    // not require it (Opera Mobile registers app 0x2002AA96 from package
    // 0x2002AA97). When the UID finds nothing, ask which package owns the app's
    // executable: by its secure ID, which is the app UID3, or — for registries
    // written before those were resolved — by the binary's path. ROM apps belong
    // to no package, so every lookup comes up empty for them and the unpackaged
    // path below takes over.
    std::uint32_t package_uid = uid;
    if (!manager->package(uid, 0) && manager->augmentations(uid).empty()) {
        eka2l1::package::object *owner = manager->package_owning_executable(uid);
        if (!owner && reg) {
            owner = manager->package_owning_file(reg->mandatory_info.app_path.to_std_string(nullptr));
        }
        if (owner) {
            package_uid = owner->uid;
        }
    }

    // Remove the base install (index 0) plus any augmentations sharing the UID so
    // nothing is left dangling.
    bool removed = false;
    for (eka2l1::package::object *aug : manager->augmentations(package_uid)) {
        if (aug && manager->uninstall_package(*aug)) {
            removed = true;
        }
    }
    if (eka2l1::package::object *base = manager->package(package_uid, 0)) {
        if (manager->uninstall_package(*base)) {
            removed = true;
        }
    }
    if (removed) {
        [self dropLeftoverRegistrationOf:reg];
        return YES;
    }
    return [self removeUnpackagedAppWithUID:uid] ? YES : NO;
}

// A package installed before conditional install blocks were registered (or one
// whose registration resource its script writes itself) can leave the app's
// _reg.rsc on disk after uninstall, which keeps a launchable-looking entry in the
// app list pointing at binaries that are gone. Drop that leftover, but only when
// the package really did take the app binary away.
- (void)dropLeftoverRegistrationOf:(eka2l1::apa_app_registry *)reg {
    if (!reg || reg->rsc_path.empty()) {
        return;
    }
    auto *io = _state->symsys->get_io_system();
    if (!io || !io->exist(reg->rsc_path)) {
        return;
    }
    const std::u16string app_path = reg->mandatory_info.app_path.to_std_string(nullptr);
    if (app_path.empty() || io->exist(app_path)) {
        return;
    }

    const std::u16string rsc_path = reg->rsc_path;
    io->delete_entry(rsc_path);

    auto *alserv = eka2l1::ios::get_applist_server(_state->symsys->get_kernel_system());
    if (alserv) {
        alserv->delete_registry(rsc_path);
    }
}

// Fallback for apps that were never installed through the package manager, the
// N-Gage game card installer being the usual source: it just copies the card
// content onto drive E, so there is no SIS registry to uninstall and the
// package path above finds nothing. Such an app is fully described by its own
// EKA1 registration folder (<drive>:\System\Apps\<app>\ holds the .aif, .app
// and the game data), so deleting that folder unregisters it on the next
// rescan. Shared drops the card makes outside of it (System\Libs,
// System\Programs) are deliberately left alone — other games link against them.
- (BOOL)removeUnpackagedAppWithUID:(uint32_t)uid {
    auto *alserv = eka2l1::ios::get_applist_server(_state->symsys->get_kernel_system());
    if (!alserv) {
        return NO;
    }
    eka2l1::apa_app_registry *reg = alserv->get_registration(uid);
    if (!reg || reg->land_drive == drive_z || reg->rsc_path.empty()) {
        return NO;
    }
    const std::u16string rsc_path = reg->rsc_path;
    const std::u16string app_dir = eka2l1::file_directory(rsc_path, true);

    // Only ever delete a per-app folder. A registration sitting directly in the
    // Apps root (or anywhere else) is not ours to remove wholesale.
    const std::string app_dir_utf8 = eka2l1::common::lowercase_string(
        eka2l1::common::ucs2_to_utf8(app_dir));
    const std::string apps_root = "\\system\\apps\\";
    const std::size_t apps_root_pos = app_dir_utf8.find(apps_root);
    if ((apps_root_pos == std::string::npos)
        || (app_dir_utf8.find_first_not_of("\\/", apps_root_pos + apps_root.length()) == std::string::npos)) {
        return NO;
    }

    auto *io = _state->symsys->get_io_system();
    std::optional<std::u16string> real_dir = io ? io->get_raw_path(app_dir) : std::nullopt;
    if (!real_dir || real_dir->empty()) {
        return NO;
    }
    const std::string real_dir_utf8 = eka2l1::common::ucs2_to_utf8(*real_dir);
    if (!eka2l1::common::exists(real_dir_utf8) || !eka2l1::common::delete_folder(real_dir_utf8)) {
        return NO;
    }
    // Drop the stale registration right away so a launch attempt between here
    // and the frontend's rescan can't resolve to the deleted app.
    alserv->delete_registry(rsc_path);
    return YES;
}

- (void)attachLayer:(CAEAGLLayer *)layer
         pixelSize:(CGSize)pixelSize
              scale:(CGFloat)scale {
    if (!_state) {
        return;
    }

    // The render surface handed to the graphics driver. For EAGL it is the
    CALayer *renderLayer = layer;

    bool driver_ready = false;
    {
        std::lock_guard<std::mutex> lk(_state->layer_mutex);
        _state->pending_layer = (__bridge void *)renderLayer;
        _state->pending_width = static_cast<std::uint32_t>(pixelSize.width);
        _state->pending_height = static_cast<std::uint32_t>(pixelSize.height);
        _state->pending_scale = static_cast<float>(scale);
        _state->layer_dirty = true;
        _state->layer_cv.notify_all();
        // Read the driver under the same lock the graphics thread publishes it
        // (and the surface hook) under. That ordering is what makes the hook
        // safe to call below without holding the lock.
        driver_ready = (_state->graphics_driver != nullptr);
    }
    // Once the graphics thread has consumed the first layer, subsequent
    // changes flow through surface_change_hook → driver->update_surface.
    if (_state->window && driver_ready) {
        _state->window->surface_changed((__bridge void *)renderLayer,
            static_cast<int>(pixelSize.width),
            static_cast<int>(pixelSize.height),
            static_cast<float>(scale));
        // update_surface alone is not enough when the layer object stays the
        // same but its bounds changed (keypad overlay toggles, rotation): the
        // EAGL context early-returns on an identical layer and keeps the old
        // renderbuffer storage, which CoreAnimation then stretches to the new
        // layer bounds — the classic distorted-frame symptom. Pushing the new
        // size makes the context re-create its renderbuffer storage on the
        // next swapchain bind.
        _state->graphics_driver->update_surface_size(eka2l1::vec2(
            static_cast<int>(pixelSize.width),
            static_cast<int>(pixelSize.height)));
        // A static guest screen (menus, paused games) won't produce a frame on
        // its own, so the stale stretched frame would linger until the next
        // guest redraw. Re-present the current texture at the new size.
        auto *state = _state.get();
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(0.1 * NSEC_PER_SEC)),
            dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
                eka2l1::ios::re_present_screen(state);
            });
    }
}


- (void)detachLayer {
    if (!_state) {
        return;
    }
    {
        std::lock_guard<std::mutex> lk(_state->layer_mutex);
        _state->pending_layer = nullptr;
        _state->layer_dirty = true;
        _state->layer_cv.notify_all();
    }
    if (_state->window && _state->graphics_driver) {
        _state->window->surface_changed(nullptr, 0, 0, 0.0f);
    }
}

- (void)pause {
    if (!_state) return;
    _state->paused = true;
    // Break the idle wait so a parked loop returns and the os_thread settles on
    // its paused sleep promptly instead of after the next guest timer.
    break_core_idling(_state.get());
    // Stop CoreMotion updates in the background (battery + no point feeding a
    // paused guest); mirrors the Android frontend's lifecycle handling.
    if (_state->sensor_driver) {
        _state->sensor_driver->pause();
    }
    if (_state->audio_driver) {
        _state->audio_driver->suspend();
    }
    // Drop the AVAudioSession activation while we're in the background so
    // the system can route audio to whatever is actually frontmost.
    NSError *err = nil;
    [[AVAudioSession sharedInstance] setActive:NO
        withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
              error:&err];
    if (err) {
        LOG_WARN(eka2l1::FRONTEND_CMDLINE, "iOS audio: setActive:NO failed: {}",
            err.localizedDescription.UTF8String ?: "unknown");
    }
}

- (void)resume {
    if (!_state) return;
    NSError *err = nil;
    const BOOL audio_session_active =
        [[AVAudioSession sharedInstance] setActive:YES error:&err];
    if (err) {
        LOG_WARN(eka2l1::FRONTEND_CMDLINE, "iOS audio: setActive:YES failed: {}",
            err.localizedDescription.UTF8String ?: "unknown");
    }
    if (audio_session_active && _state->audio_driver) {
        _state->audio_driver->resume();
    }
    if (_state->sensor_driver) {
        _state->sensor_driver->resume();
    }
    // Resume guest execution only after host devices have been restored, so
    // guest stream start/stop calls cannot race the AudioUnit restart above.
    _state->paused = false;
}

- (void)submitPointerEventAtX:(CGFloat)x
                            y:(CGFloat)y
                        phase:(EKA2L1PointerPhase)phase
                    pointerId:(uintptr_t)pointerId {
    if (!_state || !_state->winserv) {
        return;
    }
    eka2l1::drivers::input_event evt;
    evt.type_ = eka2l1::drivers::input_event_type::touch;
    evt.time_ = 0;
    evt.mouse_.pos_z_ = 0;
    evt.mouse_.button_ = eka2l1::drivers::mouse_button_left;

    // Feed swapchain/layer pixels (touch point * contentScale) and let
    // window_server map them through the screen's absolute_pos + logic scale,
    // matching the Qt/Android frontends. raw_screen_pos_ must be false here;
    // feeding raw device pixels as guest coords lands every tap far outside
    // the guest screen.
    evt.mouse_.pos_x_ = static_cast<int>(x);
    evt.mouse_.pos_y_ = static_cast<int>(y);
    evt.mouse_.raw_screen_pos_ = false;
    switch (phase) {
        case EKA2L1PointerPhaseBegan:     evt.mouse_.action_ = eka2l1::drivers::mouse_action_press; break;
        case EKA2L1PointerPhaseMoved:     evt.mouse_.action_ = eka2l1::drivers::mouse_action_repeat; break;
        case EKA2L1PointerPhaseEnded:     evt.mouse_.action_ = eka2l1::drivers::mouse_action_release; break;
        case EKA2L1PointerPhaseCancelled: evt.mouse_.action_ = eka2l1::drivers::mouse_action_release; break;
    }

    // The guest reads ptr_num as a small pointer-slot index (uint8_t, 0-based;
    // Symbian^3 advanced pointers track per-slot down/up state). Map the
    // host UITouch identity to the lowest free slot on press and free it on
    // release so every guest slot sees strictly paired down/drag/up.
    {
        std::lock_guard<std::mutex> lk(_touchSlotsLock);
        std::size_t slot = _touchSlots.size();
        for (std::size_t i = 0; i < _touchSlots.size(); i++) {
            if (_touchSlots[i] == pointerId) {
                slot = i;
                break;
            }
        }

        if (slot == _touchSlots.size()) {
            // Unknown pointer: a release with no tracked down has nothing to
            // pair with in the guest — drop it. Press (or a move that lost its
            // press) claims the lowest free slot.
            if (evt.mouse_.action_ == eka2l1::drivers::mouse_action_release) {
                return;
            }
            for (std::size_t i = 0; i < _touchSlots.size(); i++) {
                if (_touchSlots[i] == 0) {
                    slot = i;
                    _touchSlots[i] = pointerId;
                    break;
                }
            }
            if (slot == _touchSlots.size()) {
                return; // more concurrent touches than guest slots
            }
        } else if (evt.mouse_.action_ == eka2l1::drivers::mouse_action_release) {
            _touchSlots[slot] = 0;
        }

        evt.mouse_.mouse_id = static_cast<std::uint32_t>(slot);
    }
    _state->winserv->queue_input_from_driver(evt);
}

- (BOOL)currentDeviceIsTouchScreen {
    if (!_state) {
        return NO;
    }
    // Cached at boot time: see emulator::device_is_touch_screen.
    return _state->device_is_touch_screen.load(std::memory_order_relaxed) ? YES : NO;
}

- (void)setDisplayAnchorTopPixels:(NSInteger)anchorTop {
    if (!_state) {
        return;
    }
    _state->display_anchor_top_px.store(static_cast<int>(anchorTop), std::memory_order_relaxed);
}

- (NSDictionary<NSString *, id> *)guestScreenModeSnapshot {
    if (!_state) {
        return @{ @"modes": @[], @"current": @(-1) };
    }

    NSMutableArray<NSNumber *> *modes = [NSMutableArray array];
    NSInteger current = -1;
    std::unique_lock<std::recursive_mutex> session_lock(_state->session_mutex, std::try_to_lock);
    if (session_lock.owns_lock() && _state->winserv) {
        if (eka2l1::epoc::screen *screen = _state->winserv->get_current_focus_screen()) {
            current = screen->crr_mode;
            for (int mode = 0; mode < screen->total_screen_mode(); ++mode) {
                [modes addObject:@(mode)];
            }
        }
    }
    return @{ @"modes": modes, @"current": @(current) };
}

- (void)setGuestScreenModeForAppUID:(uint32_t)uid
                               mode:(NSInteger)mode
                         completion:(void (^)(NSInteger mode))completion {
    if (!_state) {
        return;
    }
    dispatch_async(eka2l1::ios::emulator_control_queue(), ^{
        NSInteger selected_mode = -1;
        {
            std::lock_guard<std::recursive_mutex> session_lock(self->_state->session_mutex);
            if (self->_state->winserv && self->_state->graphics_driver) {
                eka2l1::epoc::screen *screen = self->_state->winserv->get_current_focus_screen();
                if (screen && mode >= 0 && mode < screen->total_screen_mode()) {
                    selected_mode = mode;
                    screen->ui_rotation = 0;
                    screen->set_screen_mode(self->_state->winserv,
                        self->_state->graphics_driver.get(), static_cast<int>(selected_mode));

                    if (self->_state->settings) {
                        eka2l1::config::app_setting setting;
                        if (eka2l1::config::app_setting *existing =
                                self->_state->settings->get_setting(uid)) {
                            setting = *existing;
                        }
                        screen->store_to_config(self->_state->graphics_driver.get(), setting);
                        setting.screen_mode = static_cast<int>(selected_mode);
                        self->_state->settings->add_or_replace_setting(uid, setting);
                    }
                }
            }
        }
        if (completion) {
            dispatch_async(dispatch_get_main_queue(), ^{
                completion(selected_mode);
            });
        }
    });
}

- (void)setHostInterfaceRotationDegrees:(NSInteger)degrees {
    if (!_state) {
        return;
    }
    _state->host_interface_rotation_deg.store(static_cast<int>(degrees), std::memory_order_relaxed);
}

// The screen's vsync divides 1s by refresh_rate, so 0 would divide-by-zero;
// "unlimited" is represented as a high cap the 60Hz-native guests never reach.
static constexpr std::uint8_t k_unlimited_refresh_rate = 240;

- (void)setGuestFrameLimitForAppUID:(uint32_t)uid limit:(NSInteger)limit {
    if (!_state) {
        return;
    }
    const std::uint8_t refresh_rate = (limit <= 0)
        ? k_unlimited_refresh_rate
        : static_cast<std::uint8_t>(std::min<NSInteger>(limit, k_unlimited_refresh_rate));
    dispatch_async(eka2l1::ios::emulator_control_queue(), ^{
        std::lock_guard<std::recursive_mutex> session_lock(self->_state->session_mutex);
        eka2l1::epoc::screen *scr = nullptr;
        if (self->_state->winserv) {
            scr = self->_state->winserv->get_current_focus_screen();
            if (scr) {
                scr->refresh_rate = refresh_rate;
            }
        }
        if (self->_state->settings) {
            eka2l1::config::app_setting setting;
            if (eka2l1::config::app_setting *existing = self->_state->settings->get_setting(uid)) {
                setting = *existing;
            }
            if (scr) {
                scr->store_to_config(self->_state->graphics_driver.get(), setting);
            }
            setting.fps = refresh_rate;
            self->_state->settings->add_or_replace_setting(uid, setting);
        }
    });
}

- (NSInteger)guestFrameLimitForAppUID:(uint32_t)uid {
    if (!_state) {
        return 0;
    }
    std::uint8_t refresh_rate = 60;
    std::unique_lock<std::recursive_mutex> session_lock(_state->session_mutex, std::try_to_lock);
    if (session_lock.owns_lock() && _state->winserv) {
        if (eka2l1::epoc::screen *scr = _state->winserv->get_current_focus_screen()) {
            refresh_rate = scr->refresh_rate;
        }
    } else if (_state->settings) {
        if (eka2l1::config::app_setting *existing = _state->settings->get_setting(uid)) {
            refresh_rate = static_cast<std::uint8_t>(existing->fps);
        }
    }
    return (refresh_rate == 0 || refresh_rate > 60) ? 0 : static_cast<NSInteger>(refresh_rate);
}

- (void)submitRawKey:(uint32_t)scanCode pressed:(BOOL)pressed {
    if (!_state || !_state->winserv) {
        return;
    }
    eka2l1::drivers::input_event evt;
    evt.type_ = eka2l1::drivers::input_event_type::key_raw;
    evt.time_ = 0;
    evt.key_.code_ = static_cast<int>(scanCode);
    evt.key_.state_ = pressed ? eka2l1::drivers::key_state::pressed : eka2l1::drivers::key_state::released;
    _state->winserv->queue_input_from_driver(evt);
}

- (void)tapRawKey:(uint32_t)scanCode {
    [self submitRawKey:scanCode pressed:YES];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(60 * NSEC_PER_MSEC)),
                   dispatch_get_main_queue(), ^{
        [self submitRawKey:scanCode pressed:NO];
    });
}

- (NSDictionary<NSString *, id> *)currentConfigSnapshot {
    if (!_state) {
        return @{};
    }
    NSMutableArray<NSDictionary *> *friends = [NSMutableArray array];
    for (const eka2l1::config::friend_address &address: _state->conf.friend_addresses) {
        [friends addObject:@{
            @"addr": [NSString stringWithUTF8String:address.addr_.c_str()],
            @"port": @(address.port_)
        }];
    }
    return @{
        @"audioMasterVolume": @(_state->conf.audio_master_volume),
        @"integerScaling": @(_state->conf.integer_scaling),
        @"nearestNeighborFiltering": @(_state->conf.nearest_neighbor_filtering),
        @"hideSystemApps": @(_state->conf.hide_system_apps),
        @"extensiveLogging": @(_state->conf.extensive_logging),
        @"cpuBackend": [NSString stringWithUTF8String:_state->conf.cpu_backend.c_str()],
        @"jitEnabled": @(_state->conf.ios_use_jit),
        @"deviceDisplayName": [NSString stringWithUTF8String:_state->conf.device_display_name.c_str()],
        @"logFilter": [NSString stringWithUTF8String:_state->conf.log_filter.c_str()],
        @"btnetDiscoveryMode": @(_state->conf.btnet_discovery_mode),
        @"btnetListenPort": @(_state->conf.internet_bluetooth_port),
        @"btnetPassword": [NSString stringWithUTF8String:_state->conf.btnet_password.c_str()],
        @"btCentralServerUrl": [NSString stringWithUTF8String:_state->conf.bt_central_server_url.c_str()],
        @"btnetFriendAddresses": friends
    };
}

- (BOOL)applyConfigSnapshot:(NSDictionary<NSString *, id> *)snapshot {
    if (!_state) {
        return NO;
    }

    NSNumber *volume = snapshot[@"audioMasterVolume"];
    if (volume) {
        _state->conf.audio_master_volume = std::clamp(volume.intValue, 0, 100);
        if (_state->audio_driver) {
            _state->audio_driver->master_volume(_state->conf.audio_master_volume);
        }
    }
    NSNumber *integerScaling = snapshot[@"integerScaling"];
    if (integerScaling) {
        _state->conf.integer_scaling = integerScaling.boolValue;
    }
    NSNumber *nearest = snapshot[@"nearestNeighborFiltering"];
    if (nearest) {
        _state->conf.nearest_neighbor_filtering = nearest.boolValue;
    }
    NSNumber *hideSystemApps = snapshot[@"hideSystemApps"];
    if (hideSystemApps) {
        _state->conf.hide_system_apps = hideSystemApps.boolValue;
    }
    NSNumber *extensive = snapshot[@"extensiveLogging"];
    if (extensive) {
        _state->conf.extensive_logging = extensive.boolValue;
    }
    NSString *cpuBackend = snapshot[@"cpuBackend"];
    if ([cpuBackend isKindOfClass:NSString.class]) {
        _state->conf.cpu_backend = cpuBackend.UTF8String;
    }
    NSNumber *jitEnabled = snapshot[@"jitEnabled"];
    if (jitEnabled && jitEnabled.boolValue != _state->conf.ios_use_jit) {
        _state->conf.ios_use_jit = jitEnabled.boolValue;
        // The CPU core is instantiated when the system is (re)built, so a
        // backend switch takes effect on the next app launch's rebuild.
        _state->needs_reboot_before_launch = true;
    }
    NSString *deviceDisplayName = snapshot[@"deviceDisplayName"];
    if ([deviceDisplayName isKindOfClass:NSString.class]) {
        _state->conf.device_display_name = deviceDisplayName.UTF8String;
    }
    NSString *logFilter = snapshot[@"logFilter"];
    if ([logFilter isKindOfClass:NSString.class]) {
        _state->conf.log_filter = logFilter.UTF8String;
        // Re-apply live so the change takes effect without a restart.
        if (eka2l1::log::filterings && !_state->conf.log_filter.empty()) {
            eka2l1::log::filterings->parse_filter_string(_state->conf.log_filter);
        }
    }

    // BT netplay settings. The bluetooth midman is constructed with the
    // config on device boot, so changes here apply from the next app launch
    // (which rebuilds the system).
    NSNumber *btnetDiscoveryMode = snapshot[@"btnetDiscoveryMode"];
    if (btnetDiscoveryMode) {
        _state->conf.btnet_discovery_mode = btnetDiscoveryMode.unsignedIntValue;
    }
    // The host port range the guest's virtual bluetooth ports map onto
    // (btnet-port-offset) is not exposed: it only needs changing when several
    // emulator instances share one machine, which iOS never does. Edit
    // config.yml directly for that case.
    //
    // Discovery port for direct IP mode (the other modes bind the fixed
    // harbour port instead). Clamped so a stray value can never make the
    // midman bind an out-of-range port at boot.
    NSNumber *btnetListenPort = snapshot[@"btnetListenPort"];
    if (btnetListenPort) {
        _state->conf.internet_bluetooth_port = std::clamp(btnetListenPort.intValue, 1, 65535);
    }
    NSString *btnetPassword = snapshot[@"btnetPassword"];
    if ([btnetPassword isKindOfClass:NSString.class]) {
        _state->conf.btnet_password = btnetPassword.UTF8String;
    }
    NSString *btCentralServerUrl = snapshot[@"btCentralServerUrl"];
    if ([btCentralServerUrl isKindOfClass:NSString.class] && (btCentralServerUrl.length > 0)) {
        _state->conf.bt_central_server_url = btCentralServerUrl.UTF8String;
    }
    // UPnP port forwarding (enable-upnp) is not exposed either: it defaults on
    // and only ever runs in central server mode, where NAT traversal is the
    // whole point of the mode.
    NSArray *friendAddresses = snapshot[@"btnetFriendAddresses"];
    if ([friendAddresses isKindOfClass:NSArray.class]) {
        _state->conf.friend_addresses.clear();
        for (NSDictionary *entry in friendAddresses) {
            if (![entry isKindOfClass:NSDictionary.class]) {
                continue;
            }
            NSString *addr = entry[@"addr"];
            NSNumber *port = entry[@"port"];
            if (![addr isKindOfClass:NSString.class] || (addr.length == 0) || !port) {
                continue;
            }
            eka2l1::config::friend_address address;
            address.addr_ = addr.UTF8String;
            address.port_ = port.unsignedIntValue;
            _state->conf.friend_addresses.push_back(address);
        }
    }

    _state->conf.serialize();
    return YES;
}

- (NSArray<EKA2L1LanguageEntry *> *)availableLanguages {
    NSMutableArray<EKA2L1LanguageEntry *> *out = [NSMutableArray array];
    if (!_state || !_state->symsys) {
        return out;
    }
    auto *dvc = _state->symsys->get_device_manager();
    if (!dvc) {
        return out;
    }
    std::lock_guard<std::mutex> dvc_lock(dvc->lock);
    auto &devices = dvc->get_devices();
    const int device_index = _state->conf.device;
    if ((device_index < 0) || (device_index >= static_cast<int>(devices.size()))) {
        return out;
    }
    for (const int code: devices[device_index].languages) {
        EKA2L1LanguageEntry *entry = [[EKA2L1LanguageEntry alloc] init];
        entry.code = code;
        entry.name = [NSString stringWithUTF8String:eka2l1::common::get_language_name_by_code(code).c_str()];
        [out addObject:entry];
    }
    return out;
}

- (NSInteger)currentLanguageCode {
    return _state ? _state->conf.language : -1;
}

- (void)setSystemLanguageCode:(NSInteger)code {
    if (!_state) {
        return;
    }

    _state->conf.language = static_cast<int>(code);
    _state->conf.serialize();

    if (!_state->symsys || !_state->mounted) {
        // Not booted yet — setup_outsider applies conf.language on boot.
        return;
    }

    // Mirror the Android frontend's set_language_current: flip the kernel's
    // current language (drives .rXX resource selection on next app launch)
    // and rewrite the live locale property so running guests see the change.
    _state->symsys->set_system_language(static_cast<language>(code));

    eka2l1::kernel_system *kern = _state->symsys->get_kernel_system();
    if (!kern) {
        return;
    }

    eka2l1::property_ptr lang_prop = kern->get_prop(eka2l1::epoc::SYS_CATEGORY, eka2l1::epoc::LOCALE_LANG_KEY);
    if (!lang_prop) {
        return;
    }

    auto locale_lang = lang_prop->get_pkg<eka2l1::epoc::locale_language>();
    if (!locale_lang) {
        return;
    }

    locale_lang->language = static_cast<eka2l1::epoc::language>(code);
    lang_prop->set<eka2l1::epoc::locale_language>(locale_lang.value());

    // The applist caches each registration's caption in the language it was
    // first loaded in, and rescan_registries() skips reloading a registry whose
    // .rsc mtime is unchanged — so a language switch alone leaves the app list
    // showing the old-language names. Drop the cached registrations so the next
    // rescanApps re-reads every caption with the new current language.
    if (auto *alserv = eka2l1::ios::get_applist_server(kern)) {
        alserv->get_registerations().clear();
    }
}

- (BOOL)jitCompiledIn {
#if EKA2L1_IOS_DYNARMIC
    return YES;
#else
    return NO;
#endif
}

- (BOOL)jitAvailable {
    return eka2l1::arm::host_can_jit() ? YES : NO;
}

- (uint64_t)renderedFrameCount {
    if (!_state) {
        return 0;
    }
    return _state->rendered_frame_count.load(std::memory_order_relaxed);
}

- (nullable NSData *)iconPNGDataForUID:(uint32_t)uid sizePx:(NSUInteger)sizePx {
    if (!_state || !_state->symsys || sizePx == 0) {
        return nil;
    }
    std::lock_guard<std::mutex> icon_lock(_state->icon_mutex);
    // Decoding walks the applist / fbs servers of the booted system. The
    // frontend runs it off a background queue, so block on the session lock
    // (never main: a boot holding it dispatch_syncs onto the main queue) to
    // keep a reboot from freeing symsys under the decode.
    std::lock_guard<std::recursive_mutex> session_lock(_state->session_mutex);
    if (!_state->symsys) {
        return nil;
    }
    auto *kern = _state->symsys->get_kernel_system();
    if (!kern) return nil;
    auto *alserv = eka2l1::ios::get_applist_server(kern);
    auto *fbsserv = eka2l1::ios::get_fbs_server(kern);
    if (!alserv) return nil;
    auto *reg = alserv->get_registration(uid);
    if (!reg) return nil;

    auto *io = _state->symsys->get_io_system();
    const std::u16string ext = eka2l1::common::lowercase_ucs2_string(
        eka2l1::path_extension(reg->icon_file_path));

    // Key the debinarized-SVG cache by firmware code and app UID: the same UID
    // can ship different assets per device, while different apps may share a
    // caption. Keep these disposable files in the iOS system cache directory.
    if (_state->caches_root.empty()) return nil;
    std::string cache_dir = eka2l1::add_path(_state->caches_root, "icons");
    auto *dvc_mngr = _state->symsys->get_device_manager();
    if (eka2l1::device *crr_dvc = dvc_mngr ? dvc_mngr->get_current() : nullptr) {
        cache_dir = eka2l1::add_path(cache_dir,
            eka2l1::common::lowercase_string(crr_dvc->firmware_code));
    }
    const std::size_t side = static_cast<std::size_t>(sizePx);

    NSData *out = nil;
    if (ext == u".mif") {
        out = eka2l1::ios::decode_mif_icon(reg, io, cache_dir, side);
    } else if (ext == u".mbm") {
        if (fbsserv) out = eka2l1::ios::decode_mbm_icon(reg, fbsserv, io, side);
    } else {
        if (fbsserv) out = eka2l1::ios::decode_bitwise_icon(reg, alserv, fbsserv, side);
    }
    // If the registered icon type didn't yield anything, try the bitwise
    // fallback too — some apps point .mif at corrupt blobs but still expose a
    // bitwise icon via the applist server.
    if (!out && fbsserv && (ext == u".mif" || ext == u".mbm")) {
        out = eka2l1::ios::decode_bitwise_icon(reg, alserv, fbsserv, side);
    }
    return out;
}

@end
