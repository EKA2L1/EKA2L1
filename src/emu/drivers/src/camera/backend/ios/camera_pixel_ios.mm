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
#include <drivers/camera/camera_collection.h>

namespace eka2l1::drivers::camera {
    // Every built-in iOS camera reads out landscape-right, so a raw buffer sits
    // 90 degrees counter-clockwise from upright in the host's natural (portrait)
    // orientation. The capture connection is pinned to that native readout, so
    // this offset is a constant rather than something to query per frame.
    static constexpr int IOS_SENSOR_CCW_FROM_NATURAL = 90;

    int ios_frame_rotation_ccw() {
        const int total = frame_rotation() - IOS_SENSOR_CCW_FROM_NATURAL;
        return ((total % 360) + 360) % 360;
    }

    bool ios_render_cgimage_to_bgra(CGImageRef image, const int dw, const int dh,
        const int rotation_ccw_deg, std::vector<std::uint8_t> &out) {
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

        const int rotation = ((rotation_ccw_deg % 360) + 360) % 360;

        if (rotation == 0) {
            CGContextDrawImage(context, CGRectMake(0, 0, dw, dh), image);
            CGContextRelease(context);

            return true;
        }

        // A bitmap context puts row 0 at the top and +Y upwards, so the user
        // space is upright and a positive CGContext rotation is counter-
        // clockwise in the picture too.
        const double source_width = static_cast<double>(CGImageGetWidth(image));
        const double source_height = static_cast<double>(CGImageGetHeight(image));
        const bool extents_swapped = ((rotation % 180) != 0);
        const double rotated_width = extents_swapped ? source_height : source_width;
        const double rotated_height = extents_swapped ? source_width : source_height;

        if ((source_width <= 0.0) || (source_height <= 0.0)) {
            CGContextRelease(context);
            return false;
        }

        CGContextTranslateCTM(context, dw * 0.5, dh * 0.5);
        CGContextRotateCTM(context, rotation * M_PI / 180.0);
        CGContextScaleCTM(context, dw / rotated_width, dh / rotated_height);
        CGContextDrawImage(context, CGRectMake(-source_width * 0.5, -source_height * 0.5,
            source_width, source_height), image);
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
