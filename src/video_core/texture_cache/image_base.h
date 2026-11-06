// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <optional>
#include <vector>
#include <boost/container/small_vector.hpp>

#include "common/common_funcs.h"
#include "common/common_types.h"
#include "video_core/texture_cache/image_info.h"
#include "video_core/texture_cache/image_view_info.h"
#include "video_core/texture_cache/types.h"

namespace VideoCommon {

enum class ImageFlagBits : u32 {
    AcceleratedUpload = 1 << 0, ///< Upload can be accelerated in the GPU
    Converted = 1 << 1,   ///< Guest format is not supported natively and it has to be converted
    CpuModified = 1 << 2, ///< Contents have been modified from the CPU
    GpuModified = 1 << 3, ///< Contents have been modified from the GPU
    Tracked = 1 << 4,     ///< Writes and reads are being hooked from the CPU JIT
    Strong = 1 << 5,      ///< Exists in the image table, the dimensions are can be trusted
    Registered = 1 << 6,  ///< True when the image is registered
    Picked = 1 << 7,      ///< Temporary flag to mark the image as picked
    Remapped = 1 << 8,    ///< Image has been remapped.
    Sparse = 1 << 9,      ///< Image has non continuous submemory.

    // Garbage Collection Flags
    BadOverlap = 1 << 10, ///< This image overlaps other but doesn't fit, has higher
                          ///< garbage collection priority
    Alias = 1 << 11,      ///< This image has aliases and has priority on garbage
                          ///< collection
    CostlyLoad = 1 << 12, ///< Protected from low-tier GC as it is costly to load back.

    // Rescaler
    Rescaled = 1 << 13,
    CheckingRescalable = 1 << 14,
    IsRescalable = 1 << 15,

    AsynchronousDecode = 1 << 16,
    IsDecoding = 1 << 17, ///< Is currently being decoded asynchronously.
    Unbacked = 1 << 18,   ///< Host image has no memory (too big to back); never upload/download.
    /// The host image keeps only mip levels >= host_level_offset (its decoded copy would not fit
    /// in VRAM); uploads skip the rest, downloads are never done.
    TrimmedLevels = 1 << 19,
    ZeroFilled = 1 << 20, ///< Cleared to zero blocks before its first asynchronous decode.
    /// Over Switch 2 sparse blocks nobody wrote: all zeros, so the host image has no memory
    /// (Unbacked) until the first write, which makes a backed image replace it.
    UnwrittenSparse = 1 << 21,
    /// A large array over Switch 2 sparse blocks: unbacked, each layer gets device memory when
    /// something is written to it (Image::BindLayers) and only those layers are uploaded.
    SparseLayers = 1 << 22,
    /// A large single image over Switch 2 sparse blocks: unbacked, each host sparse block gets
    /// device memory when the guest memory under it can hold data (Image::BindTiles), and only
    /// the parts written are uploaded.
    SparseTiles = 1 << 23,
};
DECLARE_ENUM_FLAG_OPERATORS(ImageFlagBits)

struct ImageViewInfo;

struct AliasedImage {
    std::vector<ImageCopy> copies;
    ImageId id;
};

struct NullImageParams {};

struct ImageBase {
    explicit ImageBase(const ImageInfo& info, GPUVAddr gpu_addr, VAddr cpu_addr);
    explicit ImageBase(const NullImageParams&);

    [[nodiscard]] std::optional<SubresourceBase> TryFindBase(GPUVAddr other_addr) const noexcept;

    [[nodiscard]] ImageViewId FindView(const ImageViewInfo& view_info) const noexcept;

    void InsertView(const ImageViewInfo& view_info, ImageViewId image_view_id);

    [[nodiscard]] bool IsSafeGpuCopy() const noexcept;

    [[nodiscard]] bool Overlaps(VAddr overlap_cpu_addr, size_t overlap_size) const noexcept {
        const VAddr overlap_end = overlap_cpu_addr + overlap_size;
        return cpu_addr < overlap_end && overlap_cpu_addr < cpu_addr_end;
    }

    [[nodiscard]] bool OverlapsGPU(GPUVAddr overlap_gpu_addr, size_t overlap_size) const noexcept {
        const VAddr overlap_end = overlap_gpu_addr + overlap_size;
        const GPUVAddr gpu_addr_end = gpu_addr + guest_size_bytes;
        return gpu_addr < overlap_end && overlap_gpu_addr < gpu_addr_end;
    }

    void CheckBadOverlapState();
    void CheckAliasState();

    bool HasScaled() const {
        return has_scaled;
    }

    ImageInfo info;

    u32 guest_size_bytes = 0;
    u32 unswizzled_size_bytes = 0;
    u32 converted_size_bytes = 0;
    u32 scale_rating = 0;
    u32 host_level_offset = 0; ///< Guest mip level held by host mip 0 (see TrimmedLevels).
    /// Large layered images uploaded by layer: one bit per layer written since the last upload
    /// while CpuModified. Empty while CpuModified means the whole image.
    std::vector<u64> dirty_layers;
    /// SparseTiles images: the guest byte ranges (offset, size) written since the last upload while
    /// CpuModified. Empty while CpuModified means all of it that can hold data.
    std::vector<std::pair<u64, u64>> dirty_ranges;
    u64 scale_tick = 0;
    bool has_scaled = false;

    size_t channel = 0;

    ImageFlagBits flags = ImageFlagBits::CpuModified;

    GPUVAddr gpu_addr = 0;
    VAddr cpu_addr = 0;
    VAddr cpu_addr_end = 0;

    u64 modification_tick = 0;
    /// The texture cache frame the GPU last wrote this image in, and how many frames in a row
    /// (ending there) it did. Render targets the game draws every frame (the scene, shadow maps,
    /// G-buffers) build a streak; atlases and caches drawn once do not. Read by the async
    /// pipeline policy (EDEN_NX2_ASYNC_SKIP) to tell which draws may skip a frame safely.
    u64 last_written_frame = ~0ULL;
    u32 written_frame_streak = 0;
    /// The texture cache frame this image was last cleared whole in. Targets the game redraws
    /// from scratch every frame are cleared (the depth buffer nearly always); a cache filled a
    /// piece at a time over many frames (virtual texture pages) is not.
    u64 last_cleared_frame = ~0ULL;
    size_t lru_index = SIZE_MAX;

    std::array<u32, MAX_MIP_LEVELS> mip_level_offsets{};

    std::vector<ImageViewInfo> image_view_infos;
    std::vector<ImageViewId> image_view_ids;

    boost::container::small_vector<u32, 16> slice_offsets;
    boost::container::small_vector<SubresourceBase, 16> slice_subresources;

    std::vector<AliasedImage> aliased_images;
    std::vector<ImageId> overlapping_images;
    ImageMapId map_view_id{};
};

struct ImageMapView {
    explicit ImageMapView(GPUVAddr gpu_addr, VAddr cpu_addr, size_t size, ImageId image_id);

    [[nodiscard]] bool Overlaps(VAddr overlap_cpu_addr, size_t overlap_size) const noexcept {
        const VAddr overlap_end = overlap_cpu_addr + overlap_size;
        const VAddr cpu_addr_end = cpu_addr + size;
        return cpu_addr < overlap_end && overlap_cpu_addr < cpu_addr_end;
    }

    [[nodiscard]] bool OverlapsGPU(GPUVAddr overlap_gpu_addr, size_t overlap_size) const noexcept {
        const GPUVAddr overlap_end = overlap_gpu_addr + overlap_size;
        const GPUVAddr gpu_addr_end = gpu_addr + size;
        return gpu_addr < overlap_end && overlap_gpu_addr < gpu_addr_end;
    }

    GPUVAddr gpu_addr;
    VAddr cpu_addr;
    size_t size;
    ImageId image_id;
    bool picked{};
};

struct ImageAllocBase {
    std::vector<ImageId> images;
};

bool AddImageAlias(ImageBase& lhs, ImageBase& rhs, ImageId lhs_id, ImageId rhs_id);

/// Records a GPU write to `image` in texture cache frame `frame` (see written_frame_streak).
inline void NoteWrittenInFrame(ImageBase& image, u64 frame) noexcept {
    if (image.last_written_frame == frame) {
        return;
    }
    // ~0 + 1 wraps to 0: an image first written in frame 0 starts its streak at 1 as well.
    image.written_frame_streak =
        image.last_written_frame + 1 == frame ? image.written_frame_streak + 1 : 1;
    image.last_written_frame = frame;
}

/// Whether `image` was cleared whole in texture cache frame `frame` or the one before it.
[[nodiscard]] inline bool ClearedRecently(const ImageBase& image, u64 frame) noexcept {
    return image.last_cleared_frame != ~0ULL && image.last_cleared_frame <= frame &&
           frame - image.last_cleared_frame <= 1;
}

/// An image that takes over from `old` (a join of overlapping images) keeps the frames `old` was
/// written and cleared in, so recreating a render target the game draws every frame does not
/// restart its streak.
inline void InheritWrittenFrames(ImageBase& image, const ImageBase& old) noexcept {
    constexpr u64 Never = ~0ULL;
    if (old.last_written_frame != Never) {
        const bool newer = image.last_written_frame == Never ||
                           old.last_written_frame > image.last_written_frame;
        const bool longer = old.last_written_frame == image.last_written_frame &&
                            old.written_frame_streak > image.written_frame_streak;
        if (newer || longer) {
            image.last_written_frame = old.last_written_frame;
            image.written_frame_streak = old.written_frame_streak;
        }
    }
    if (old.last_cleared_frame != Never &&
        (image.last_cleared_frame == Never || old.last_cleared_frame > image.last_cleared_frame)) {
        image.last_cleared_frame = old.last_cleared_frame;
    }
}

} // namespace VideoCommon
