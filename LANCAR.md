# Lichii "lancar" patches

Changes on top of Lichii build-88 (55532f2) aimed at shader-compile stutter in Switch 2 titles,
measured on The Legend of Zelda: Ocarina of Time (0400486024DEC000) on an i7-13650HX + RTX 4060
Laptop. Pipeline cache keys and the cache file version are unchanged, so existing shader caches
keep working.

## 1. Draws skip a pipeline that is still being built (when that is safe)

`CanDiscardPendingDraw` never discards a native (Switch 2) draw, so with asynchronous shaders on,
every draw whose pipeline was not built yet still recorded a wait on the Vulkan worker
(`build->Wait`). Ocarina logs showed up to 12.9 s of held draws in a 10 s window on new areas and
single frames of 1.6-9.8 s.

Now such a draw returns early (no wait, no error report) when all of these hold
(`CanSkipDrawWhileBuilding`, `async_draw_policy.h`):

- asynchronous shaders are on;
- the pipeline writes no storage buffers or images and has no transform feedback;
- every bound render target was written by the GPU in each of the last 3 frames, and at least one
  of them was cleared whole this frame or the last (`TextureCache::RenderTargetsRedrawnEachFrame`).

Targets drawn rarely or filled a piece at a time (atlases, cached virtual texture pages) are never
cleared and so still wait: the draw is held exactly as before and nothing persistent is lost.
A skipped draw shows for a frame or two, until its pipeline is ready.

Bookkeeping: `ImageBase::last_written_frame`, `written_frame_streak` (updated in
`MarkModification`) and `last_cleared_frame` (set by `TextureCache::NoteRenderTargetsCleared` from
`RasterizerVulkan::Clear`, only for the surfaces the clear covers whole), carried across image
joins by `InheritWrittenFrames`. A title that never clears through the 3D engine never skips.

The 10 s runtime pipeline report now ends with "N draws skipped while their pipeline built".

Switch: `EDEN_NX2_ASYNC_SKIP=0` makes every draw wait again.

## 2. Pipelines are kept longer before eviction

Pipelines unused for 9,000 frames (~5 minutes at 30 fps) were destroyed; building one again
translates its shaders on the GPU thread. Ocarina evicted hundreds at a time and rebuilt them on
each return to an area. The default is now 27,000 frames (~15 minutes at 30 fps). The memory peak
is the boot prebuild of recent sessions' pipelines, which this keeps longer but does not raise.

Switch: `EDEN_NX2_PIPELINE_EVICT_FRAMES=<frames>` (600 or more).

## 3. Build

`release-binaries.yml` on the `lancar` branch builds only the Windows MSVC package, with the
update checker off (so the emulator does not offer upstream builds without these patches).

## Tests

`src/tests/video_core/async_draw_policy.cpp` and `src/tests/video_core/written_frames.cpp`.
