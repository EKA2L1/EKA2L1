/*
 * Copyright (c) 2026 EKA2L1 Team.
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

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>

#include <drivers/camera/backend/ios/camera_pixel_ios.h>

namespace eka2l1::drivers::camera {
    // Formats the backend can synthesize from a BGRA source. Mirrors the
    // Android backend's advertised set.
    const frame_format IOS_SUPPORTED_FORMATS[8] = {
        FRAME_FORMAT_ARGB8888, FRAME_FORMAT_JPEG, FRAME_FORMAT_RGB565,
        FRAME_FORMAT_FBSBMP_COLOR4K, FRAME_FORMAT_FBSBMP_COLOR64K, FRAME_FORMAT_FBSBMP_COLOR16M,
        FRAME_FORMAT_FBSBMP_COLOR16MU, FRAME_FORMAT_EXIF
    };

    bool ios_is_supported_format(const frame_format format) {
        for (const frame_format supported: IOS_SUPPORTED_FORMATS) {
            if (format == supported) {
                return true;
            }
        }

        return false;
    }

    // Repack a top-down BGRA buffer into the guest-facing pixel layout. Byte
    // orders and row alignments follow what the Android backend delivers:
    // FBS bitmap scanlines are 4-byte aligned, raw RGB565 rows are tight, and
    // ARGB8888 means R,G,B,A byte order.
    bool ios_convert_bgra_to_guest(const std::uint8_t *src, const std::size_t src_stride,
        const int width, const int height, const frame_format format, std::vector<std::uint8_t> &dest) {
        switch (format) {
        case FRAME_FORMAT_FBSBMP_COLOR16MU: {
            const std::size_t dest_stride = static_cast<std::size_t>(width) * 4;
            dest.resize(dest_stride * height);

            for (int y = 0; y < height; y++) {
                const std::uint8_t *src_row = src + y * src_stride;
                std::uint8_t *dest_row = dest.data() + y * dest_stride;

                for (int x = 0; x < width; x++) {
                    dest_row[x * 4 + 0] = src_row[x * 4 + 0];
                    dest_row[x * 4 + 1] = src_row[x * 4 + 1];
                    dest_row[x * 4 + 2] = src_row[x * 4 + 2];
                    dest_row[x * 4 + 3] = 0xFF;
                }
            }

            return true;
        }

        case FRAME_FORMAT_ARGB8888: {
            const std::size_t dest_stride = static_cast<std::size_t>(width) * 4;
            dest.resize(dest_stride * height);

            for (int y = 0; y < height; y++) {
                const std::uint8_t *src_row = src + y * src_stride;
                std::uint8_t *dest_row = dest.data() + y * dest_stride;

                for (int x = 0; x < width; x++) {
                    dest_row[x * 4 + 0] = src_row[x * 4 + 2];
                    dest_row[x * 4 + 1] = src_row[x * 4 + 1];
                    dest_row[x * 4 + 2] = src_row[x * 4 + 0];
                    dest_row[x * 4 + 3] = 0xFF;
                }
            }

            return true;
        }

        case FRAME_FORMAT_FBSBMP_COLOR16M: {
            const std::size_t dest_stride = (static_cast<std::size_t>(width) * 3 + 3) / 4 * 4;
            dest.resize(dest_stride * height);

            for (int y = 0; y < height; y++) {
                const std::uint8_t *src_row = src + y * src_stride;
                std::uint8_t *dest_row = dest.data() + y * dest_stride;

                for (int x = 0; x < width; x++) {
                    dest_row[x * 3 + 0] = src_row[x * 4 + 0];
                    dest_row[x * 3 + 1] = src_row[x * 4 + 1];
                    dest_row[x * 3 + 2] = src_row[x * 4 + 2];
                }
            }

            return true;
        }

        case FRAME_FORMAT_FBSBMP_COLOR4K:
        case FRAME_FORMAT_FBSBMP_COLOR64K:
        case FRAME_FORMAT_RGB565: {
            const bool is_fbs = (format == FRAME_FORMAT_FBSBMP_COLOR4K)
                || (format == FRAME_FORMAT_FBSBMP_COLOR64K);
            const std::size_t dest_stride = is_fbs
                ? (static_cast<std::size_t>(width) * 2 + 3) / 4 * 4
                : static_cast<std::size_t>(width) * 2;
            dest.resize(dest_stride * height);

            for (int y = 0; y < height; y++) {
                const std::uint8_t *src_row = src + y * src_stride;
                std::uint8_t *dest_row = dest.data() + y * dest_stride;

                for (int x = 0; x < width; x++) {
                    std::uint16_t pixel = 0;
                    if (format == FRAME_FORMAT_FBSBMP_COLOR4K) {
                        pixel = static_cast<std::uint16_t>(
                            (src_row[x * 4 + 0] >> 4)
                            | (src_row[x * 4 + 1] & 0xF0)
                            | ((src_row[x * 4 + 2] & 0xF0) << 4));
                    } else {
                        pixel = static_cast<std::uint16_t>(
                            ((src_row[x * 4 + 0] & 0xF8) >> 3)
                            | ((src_row[x * 4 + 1] & 0xFC) << 3)
                            | ((src_row[x * 4 + 2] & 0xF8) << 8));
                    }

                    dest_row[x * 2 + 0] = static_cast<std::uint8_t>(pixel & 0xFF);
                    dest_row[x * 2 + 1] = static_cast<std::uint8_t>(pixel >> 8);
                }
            }

            return true;
        }

        default:
            break;
        }

        return false;
    }

    // Draw a CGImage scaled into a top-down BGRX buffer of exactly dw x dh.
    bool ios_render_cgimage_to_bgra(CGImageRef image, const int dw, const int dh,
        std::vector<std::uint8_t> &out) {
        out.resize(static_cast<std::size_t>(dw) * 4 * dh);

        CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
        CGContextRef context = CGBitmapContextCreate(out.data(), dw, dh, 8,
            static_cast<std::size_t>(dw) * 4, color_space,
            kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst);
        CGColorSpaceRelease(color_space);

        if (!context) {
            return false;
        }

        CGContextSetInterpolationQuality(context, kCGInterpolationLow);
        CGContextDrawImage(context, CGRectMake(0, 0, dw, dh), image);
        CGContextRelease(context);

        return true;
    }

    CGImageRef ios_create_cgimage_from_bgra(const std::uint8_t *base, const std::size_t stride,
        const int width, const int height) {
        CGDataProviderRef provider = CGDataProviderCreateWithData(nullptr, base,
            stride * height, nullptr);
        if (!provider) {
            return nullptr;
        }

        CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
        CGImageRef image = CGImageCreate(width, height, 8, 32, stride, color_space,
            kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst, provider, nullptr,
            false, kCGRenderingIntentDefault);

        CGColorSpaceRelease(color_space);
        CGDataProviderRelease(provider);

        return image;
    }

    bool ios_encode_bgra_to_jpeg(const std::uint8_t *bgra, const int width, const int height,
        std::vector<std::uint8_t> &out) {
        CGImageRef image = ios_create_cgimage_from_bgra(bgra,
            static_cast<std::size_t>(width) * 4, width, height);
        if (!image) {
            return false;
        }

        NSMutableData *encoded = [NSMutableData data];
        CGImageDestinationRef destination = CGImageDestinationCreateWithData(
            (__bridge CFMutableDataRef)encoded, CFSTR("public.jpeg"), 1, nullptr);

        bool ok = false;
        if (destination) {
            // Guests ask for FRAME_FORMAT_EXIF as often as plain JPEG, and a
            // Symbian camera app then walks the APP1 segment while saving the
            // shot. A bare JFIF stream has none, so write the usual EXIF/TIFF
            // tags a camera would record. (A device capture that needs no
            // rescaling keeps AVFoundation's own metadata and never gets here.)
            NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
            formatter.dateFormat = @"yyyy:MM:dd HH:mm:ss";
            NSString *timestamp = [formatter stringFromDate:[NSDate date]];

            NSDictionary *properties = @{
                (id)kCGImageDestinationLossyCompressionQuality: @(0.5),
                (id)kCGImagePropertyExifDictionary: @{
                    (id)kCGImagePropertyExifPixelXDimension: @(width),
                    (id)kCGImagePropertyExifPixelYDimension: @(height),
                    (id)kCGImagePropertyExifDateTimeOriginal: timestamp,
                    (id)kCGImagePropertyExifDateTimeDigitized: timestamp,
                    (id)kCGImagePropertyExifColorSpace: @(1)
                },
                (id)kCGImagePropertyTIFFDictionary: @{
                    (id)kCGImagePropertyTIFFMake: @"EKA2L1",
                    (id)kCGImagePropertyTIFFModel: @"EKA2L1 Camera",
                    (id)kCGImagePropertyTIFFOrientation: @(1),
                    (id)kCGImagePropertyTIFFXResolution: @(72),
                    (id)kCGImagePropertyTIFFYResolution: @(72),
                    (id)kCGImagePropertyTIFFResolutionUnit: @(2),
                    (id)kCGImagePropertyTIFFDateTime: timestamp
                }
            };
            CGImageDestinationAddImage(destination, image, (__bridge CFDictionaryRef)properties);
            ok = CGImageDestinationFinalize(destination);
            CFRelease(destination);
        }

        CGImageRelease(image);

        if (ok) {
            const std::uint8_t *base = static_cast<const std::uint8_t *>(encoded.bytes);
            out.assign(base, base + encoded.length);
        }

        return ok;
    }
}
