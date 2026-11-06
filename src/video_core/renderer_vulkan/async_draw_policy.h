// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>

#include "common/common_types.h"

namespace Vulkan {

/// Whether a draw whose graphics pipeline is still building may be skipped (asynchronous
/// shaders) instead of waiting for the build.
/// @param native         The draw comes from a Switch 2 (native Turing macro) program.
/// @param depth_enabled  The depth test is on for the draw.
/// @param index_count    Index count of the draw.
/// @param vertex_count   Vertex count of the draw.
/// @param discard_native Let native draws follow the heuristic too (EDEN_NX2_ASYNC_DISCARD=1).
/// @return True when the draw may be skipped.
constexpr bool CanDiscardPendingDraw(bool native, bool depth_enabled, u32 index_count,
                                    u32 vertex_count, bool discard_native = false) {
    // Native draws can produce persistent resources, even with depth enabled. They are only
    // skipped when the user opts in (EDEN_NX2_ASYNC_DISCARD=1), trading a possible missing
    // effect for no stall while a new pipeline builds.
    if (native && !discard_native) {
        return false;
    }
    // Preserve the legacy async-shader heuristic.
    return depth_enabled || (index_count > 6 && vertex_count > 6);
}

/// Frames in a row every render target of a draw must have been written in (this one included)
/// before the draw may skip its unbuilt pipeline.
constexpr u32 SkipWhileBuildingTargetFrames = 3;

/// Whether a native draw that CanDiscardPendingDraw keeps (EDEN_NX2_ASYNC_DISCARD off) may still
/// skip its pipeline while it builds instead of holding the Vulkan worker: it writes no storage
/// and has no transform feedback, and its render targets are redrawn from scratch every frame
/// (each written in the last SkipWhileBuildingTargetFrames frames, one cleared this frame or the
/// last). Targets drawn rarely or filled a piece at a time still wait, so nothing is lost.
constexpr bool CanSkipDrawWhileBuilding(bool async_shaders, bool writes_storage,
                                        bool transform_feedback,
                                        bool targets_redrawn_each_frame) {
    return async_shaders && !writes_storage && !transform_feedback && targets_redrawn_each_frame;
}

/// Whether a native pipeline's SPIR-V emission is deferred to a worker: only while fewer than
/// `capacity` emissions are pending.
constexpr bool DefersShaderEmission(bool native, size_t pending, size_t capacity) {
    return native && pending < capacity;
}

} // namespace Vulkan
