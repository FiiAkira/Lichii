// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "video_core/texture_cache/image_base.h"

namespace {
void Write(VideoCommon::ImageBase& image, u64 frame) {
    VideoCommon::NoteWrittenInFrame(image, frame);
}

VideoCommon::ImageBase MakeImage() {
    return VideoCommon::ImageBase{VideoCommon::NullImageParams{}};
}
} // Anonymous namespace

TEST_CASE("Written frame streaks count consecutive frames", "[video_core][written_frames]") {
    auto image = MakeImage();
    REQUIRE(image.written_frame_streak == 0);
    Write(image, 0);
    REQUIRE(image.written_frame_streak == 1);
    Write(image, 0); // many writes in one frame count once
    REQUIRE(image.written_frame_streak == 1);
    Write(image, 1);
    Write(image, 2);
    REQUIRE(image.written_frame_streak == 3);
    Write(image, 4); // a skipped frame restarts the streak
    REQUIRE(image.written_frame_streak == 1);
}

TEST_CASE("A replacement image keeps the newer streak", "[video_core][written_frames]") {
    auto old_image = MakeImage();
    auto new_image = MakeImage();
    for (u64 frame = 10; frame < 15; ++frame) {
        Write(old_image, frame);
    }
    VideoCommon::InheritWrittenFrames(new_image, old_image);
    REQUIRE(new_image.last_written_frame == 14);
    REQUIRE(new_image.written_frame_streak == 5);

    auto never_written = MakeImage();
    VideoCommon::InheritWrittenFrames(new_image, never_written);
    REQUIRE(new_image.written_frame_streak == 5);

    auto older = MakeImage();
    Write(older, 3);
    VideoCommon::InheritWrittenFrames(new_image, older);
    REQUIRE(new_image.last_written_frame == 14);
}

TEST_CASE("Recent clears cover this frame and the previous one", "[video_core][written_frames]") {
    auto image = MakeImage();
    REQUIRE_FALSE(VideoCommon::ClearedRecently(image, 0));
    REQUIRE_FALSE(VideoCommon::ClearedRecently(image, 100));
    image.last_cleared_frame = 10;
    REQUIRE(VideoCommon::ClearedRecently(image, 10));
    REQUIRE(VideoCommon::ClearedRecently(image, 11));
    REQUIRE_FALSE(VideoCommon::ClearedRecently(image, 12));
    REQUIRE_FALSE(VideoCommon::ClearedRecently(image, 9));

    auto replacement = MakeImage();
    VideoCommon::InheritWrittenFrames(replacement, image);
    REQUIRE(replacement.last_cleared_frame == 10);
}
