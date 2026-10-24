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
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <catch2/catch.hpp>
#include <drivers/video/video.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <vector>

#ifdef EKA2L1_HAS_FFMPEG
TEST_CASE("video_pause_seek_and_range_completion_follow_media_time", "[video]") {
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<std::uint64_t> positions;
    int completions = 0;
    int error = 0;
    bool packed_frames = true;
    auto player = eka2l1::drivers::new_best_video_player(nullptr);
    REQUIRE(player);
    REQUIRE(player->open_file("videoassets/seek.mp4"));
    REQUIRE(player->duration() == 2000000);

    player->set_image_frame_available_callback([&](void *, const std::uint8_t *, std::size_t size) {
        const std::lock_guard<std::mutex> guard(mutex);
        packed_frames = packed_frames && size == 18 * 16 * 4;
        positions.push_back(player->position());
        changed.notify_all();
    }, nullptr);
    player->set_play_complete_callback([&](void *, int result) {
        const std::lock_guard<std::mutex> guard(mutex);
        completions++;
        error = result;
        changed.notify_all();
    }, nullptr);

    player->play(nullptr);
    bool rendered;
    {
        std::unique_lock<std::mutex> guard(mutex);
        rendered = changed.wait_for(guard, std::chrono::seconds(3), [&] { return positions.size() >= 2; });
    }
    player->pause();
    REQUIRE(rendered);
    REQUIRE(completions == 0);
    REQUIRE(player->position() >= 100000);
    const auto paused = player->position();

    positions.clear();
    player->play(nullptr);
    {
        std::unique_lock<std::mutex> guard(mutex);
        rendered = changed.wait_for(guard, std::chrono::seconds(3), [&] { return !positions.empty(); });
    }
    player->pause();
    REQUIRE(rendered);
    REQUIRE(completions == 0);
    REQUIRE(positions.front() >= paused);

    player->set_position(1200000);
    REQUIRE(player->position() == 1200000);
    positions.clear();
    const std::uint64_t range[] = { 1200000, 1500000 };
    player->play(range);
    bool completed;
    {
        std::unique_lock<std::mutex> guard(mutex);
        completed = changed.wait_for(guard, std::chrono::seconds(3), [&] { return completions != 0; });
    }
    player->stop();
    REQUIRE(completed);
    REQUIRE(completions == 1);
    REQUIRE(error == 0);
    REQUIRE_FALSE(positions.empty());
    REQUIRE(positions.front() == 1200000);
    REQUIRE(positions.back() < 1500000);
    REQUIRE(player->position() == 1500000);
    REQUIRE(packed_frames);
}
#endif
