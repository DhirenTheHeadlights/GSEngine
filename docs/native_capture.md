# Native Capture — Scope of Work (Locked In)

## Overview

Add first-class screenshot, video, and clip capture to the engine using **Vulkan Video** for GPU-resident encoding. The headline feature is a **rolling ring buffer** that continuously holds the last *N* seconds of encoded frames so the user can press a hotkey *after* something interesting happens and save a clip retroactively (Shadowplay-style). Screenshots and continuous recording fall out of the same pipeline.

---

## Goals

- Press F9 → screenshot to PNG (native resolution).
- Press F10 → save the last *N* seconds of gameplay as a video clip.
- Press F11 → toggle continuous recording until pressed again.
- Zero perceptible frame-time impact on the render thread when idle (ring buffer running).
- Encode runs on the GPU through `VK_KHR_video_encode_*`, not CPU.
- Graceful disable on hardware/drivers without video encode support — feature compiles out at the system level, never crashes.
- Style/semantics: lives in `Engine/Engine/Source/Graphics/Capture/`, mirrors existing renderer module conventions, slots into render graph V2.

## Non-Goals (initially)

- Audio capture (engine has no audio subsystem yet).
- Streaming / network output.
- Editing UI beyond "save clip now" / "discard".
- GIF support (modern services accept auto-looping MP4).

---

## Integration Surface

| Concern | Where it lives |
|---|---|
| Render graph | `Platform/Vulkan/RenderGraph.cppm` — `add_pass`, `track`, `color_output`, `record` |
| Final swapchain image | `GpuSwapchain.cppm`; image accessible at `frame::end()` between cmd-buffer end and `presentKHR` (`GpuFrame.cppm:197–250`) |
| Queue families | `Runtime.cppm:156` `queue_family`, `queue_config` (graphics / present / compute today) |
| Per-frame resources | `Core/Utility/Concurrency/PerFrameResource.cppm`, `frames_in_flight = 2` |
| Frame scheduler | `Platform/FrameScheduler.cppm` — saturation-based work spreader |
| Hotkeys | `Platform/GLFW/Actions.cppm`, `Core/Runtime/Api/InputApi.cppm` |
| Module conventions | `Graphics/Renderers/*.cppm` — `state` + `system` pair, `gse::renderer::*` namespace |

No existing screenshot/capture code — clean slate.

---

## Locked-In Decisions

### D1. Codec — AV1 primary, H.265 fallback, codec-agnostic backend

Primary codec is **AV1** (`VK_KHR_video_encode_av1`). Fallback to **H.265** (`VK_KHR_video_encode_h265`) when AV1 encode is unavailable. The encoder module abstracts over codec specifics behind a `codec_backend` interface so swapping is a config change, not a code change.

**What this means architecturally:**
- `CaptureEncoder.cppm` owns a `codec_backend` variant/interface that hides codec-specific session creation, rate control params, reference frame management, and NAL/OBU unit parsing.
- AV1 produces **OBU (Open Bitstream Units)**, not NAL units. The ring buffer stores opaque "coded units" — the codec backend knows how to parse them, the ring doesn't care.
- The muxer must handle both **`av1C`** (AV1 in MP4) and **`hvcC`** (H.265 in MP4) box formats. These are structurally different — `av1C` stores a `configOBUs` blob, `hvcC` stores VPS/SPS/PPS NAL units with length-prefixed arrays. The own-muxer scope grows vs H.264-only but is still bounded (~1500 lines total for both codecs).
- Codec selection happens at `initialize()` time: probe for AV1 encode support, fall back to H.265, fall back to screenshot-only mode.

**Codec backend abstraction (sketch):**
```
codec_backend (variant: av1_backend | h265_backend)
    create_session(device, resolution, bitrate, gop) → session_handle
    encode_frame(session, input_image, is_keyframe) → encoded_unit
    extract_stream_header() → bytes  (configOBUs or VPS/SPS/PPS)
    coded_unit_type() → keyframe | inter | ...
    destroy_session()
```

### D2. Capture source — toggleable, default post-UI

Runtime toggle between pre-UI (clean gameplay) and post-UI (WYSIWYG). Default is post-UI.

The capture UI (recording indicator, "clip saved" toast) checks an `is_capturing` flag and hides itself during capture to avoid recursive capture of the capture UI.

Switching source mode mid-recording is not supported — takes effect on next recording start or ring buffer restart. Exposed as a setting via `save::register_property`.

### D3. Ring buffer storage — encoded bitstream ring

Encoded OBU/NAL units stored in a host-visible ring buffer in RAM, indexed by PTS and keyframe position. Budget: ~37–187 MB for 30–60 seconds of 1080p at 10–25 Mbps.

Ring is flushed on codec or resolution change.

### D4. GOP / keyframe interval — 1 second

Force IDR/key frame every 1 second. Worst-case clip-start error: 1 second at the head. MP4 edit lists trim to the exact requested start PTS so the player skips the pre-roll.

### D5. Container / muxer — roll our own

Custom minimal MP4 muxer supporting:
- AV1 in MP4 (`av1C` sample entry, `av01` codec tag)
- H.265 in MP4 (`hvcC` sample entry, `hvc1` codec tag)
- Variable framerate via per-sample durations in `stts`
- Edit lists for clean clip trimming
- Fragmented MP4 mode for the continuous recording path (F11) — crash-safe, playable up to the last written fragment

Estimated scope: ~1500 lines across both codecs. The muxer is codec-aware (needs to parse stream headers) but not encode-aware (receives opaque coded units + metadata from the codec backend).

### D6. Color conversion — compute shader

Single compute pass: RGBA sRGB → linearize → BT.709 YUV 4:2:0 (NV12 layout). Combined with resolution downscale in the same dispatch. Runs on graphics queue as part of the render graph.

Slang shader at `Engine/Resources/Shaders/Capture/rgba_to_nv12.slang`.

### D7. Capture resolution — configurable cap, default 1080p

Encode at `min(native, cap)`. Default cap is 1080p. Exposed as a setting via `save::register_property`. Changing resolution requires encoder session recreation (takes effect next frame, brief stall).

### D8. Encode queue — dedicated video encode queue

Extend `queue_family` / `queue_config` in `Runtime.cppm` to include an optional `video_encode` queue. If the device doesn't expose a video encode queue family, the video capture feature is disabled (screenshot-only mode, D11).

Queue family ownership transfer barriers required for the capture image (graphics → video encode → graphics). The render graph's `track()` resource system should handle this via its existing sync primitives.

### D9. Threading — render thread submits, worker thread for I/O

Render thread: color convert dispatch + encode submit + fence check (lightweight per-frame).
Worker thread: MP4 mux + disk write when saving a clip (F10) or during continuous recording (F11).

### D10. Frame scheduler — encode unscheduled

Encode runs every frame unconditionally (steady-state cost, not bursty). Clip-save mux + disk write runs on the worker thread, not through frame scheduler.

### D11. Fallback — screenshot-only

When video encode extensions are unavailable:
- F9 works (swapchain readback → PNG via stb_image_write on worker thread).
- F10/F11 are no-ops with a one-time log: "video capture unavailable on this GPU/driver."
- Zero runtime cost from the video path.

### D12. Screenshots — vendor stb_image_write

`stb_image_write.h` vendored into the project. PNG output. Screenshot path is fully independent of the video pipeline.

### D13. GIF — skipped

Modern services (Discord, Twitter/X, Slack, Reddit) accept auto-looping MP4. No GIF path.

### D14. Frame pacing — variable framerate MP4

PTS stamped from `frame_clock`. VFR MP4 via per-sample durations in `stts` box. No frame duplication or dropping.

---

## Pipeline (Final)

```
[render graph produces final image]
        │
        ├─ (post-UI mode) capture after UiRenderer
        ├─ (pre-UI mode)  capture after ForwardRenderer, before UiRenderer
        ▼
[capture blit pass — render graph]
        │   blit/resolve source → capture staging image (RGBA8)
        ▼
[color convert + downscale pass — compute, render graph]
        │   Slang shader: sRGB RGBA → linear → BT.709 NV12 at capture resolution
        ▼
[video encode submission — dedicated video encode queue]
        │   queue family ownership transfer: graphics → video_encode
        │   VkVideoEncodeInfoKHR, GOP managed by codec_backend
        │   queue family ownership transfer: video_encode → graphics
        ▼
[encoded bitstream readback — host-visible buffer]
        │   fence signals 1-2 frames later, polled on render thread
        ▼
[ring buffer of coded units, indexed by PTS + keyframe map]
        │
        ├── F10 → walk back to keyframe, mux (AV1 or H.265 MP4), write on worker thread
        ├── F11 → tee to live fragmented MP4 muxer on worker thread
        └── F9  → separate path: RGBA staging → stb_image_write PNG on worker thread
```

---

## Module Layout

```
Engine/Engine/Source/Graphics/Capture/
    Capture.cppm              — top-level :capture module, state + system
    CaptureRingBuffer.cppm    — coded-unit ring + keyframe index, codec-agnostic
    CaptureEncoder.cppm       — Vulkan Video session, codec_backend variant (AV1 | H.265)
    CaptureColorConvert.cppm  — RGBA→NV12 compute pass + downscale (render graph pass)
    CaptureMuxer.cppm         — MP4 muxer: regular mode (clip save) + fragmented mode (continuous)
    CaptureScreenshot.cppm    — F9 path: staging blit + stb_image_write PNG on worker
Engine/Resources/Shaders/Capture/
    rgba_to_nv12.slang        — sRGB RGBA → BT.709 NV12 compute kernel
```

Public API:

```cpp
namespace gse::renderer::capture {
    enum class codec { av1, h265 };
    enum class source_mode { pre_ui, post_ui };

    struct settings {
        source_mode source = source_mode::post_ui;
        int resolution_cap = 1080;
        float ring_buffer_seconds = 30.f;
        int bitrate_mbps = 15;
    };

    struct state { /* ring buffer, encoder, settings, worker thread handle */ };

    struct system {
        static auto initialize(initialize_phase&, state&) -> void;
        static auto record(render_phase&, state&) -> void;
        static auto save_clip(state&, float seconds) -> void;
        static auto save_screenshot(state&) -> void;
        static auto toggle_continuous(state&) -> void;
        static auto is_capturing() -> bool;  // for UI self-hiding
    };
}
```

Hotkeys registered via `actions::add_action_request` in `initialize`. Settings registered via `save::register_property`.

---

## Phasing

### Phase 0 — Plumbing + screenshots (no Vulkan Video) ✓ COMPLETE

**What was built:**

Platform layer changes:
- `GpuSwapchain.cppm` — added `eTransferSrc` to swapchain image usage; added `is_bgra()` query
- `Runtime.cppm` — added `video_encode_family` to `queue_family`; added optional video encode queue + setter to `queue_config`
- `GpuDevice.cppm` — detects `eVideoEncodeKHR` queue family, conditionally enables `VK_KHR_video_queue` + `VK_KHR_video_encode_queue`, retrieves queue, logs result; added `video_encode_enabled` to `device_creation_result`
- `RenderGraph.cppm` — added `copy_image_to_buffer()` and `capture_swapchain()` to `recording_context` (barriers + copy encapsulated in platform layer, renderers don't touch `vk::` types)
- `ImageLoader.cppm` — added `stb_image_write.h` and `image::write_png()` (merged into existing module, no separate file)
- `SystemClock.cppm` — added `system_clock::timestamp_filename()` using `std::chrono::zoned_time`

Renderer:
- `CaptureRenderer.cppm` (new, in `Graphics/Renderers/`) — `gse::renderer::capture` system with `prepare_render` / `render` / `end_frame` phases
  - Per-frame staging buffers via `per_frame_resource<pending_screenshot>`
  - F9 detection via `input::system_state::key_pressed()`
  - GPU swapchain→staging copy as render graph pass (`.after<ui::state>()`)
  - Async PNG write on TBB worker thread with BGRA→RGBA swizzle + alpha forced to 0xFF (swapchain compositeAlpha is eOpaque, alpha channel is meaningless)
  - `write_in_progress` atomic guard prevents overlapping writes

Registration:
- `Graphics.cppm` — re-exports `:capture_renderer`
- `Engine.cppm` — registers `capture::system` in scheduler after `ui::system`

**Deviations from original plan:**
- Module lives in `Graphics/Renderers/CaptureRenderer.cppm`, not a separate `Graphics/Capture/` folder — matches existing renderer convention
- No separate `CaptureScreenshot.cppm` — screenshot logic fits cleanly in one file
- No F10/F11 hotkeys registered yet — deferred to Phases 2/3
- Uses `input::system_state` directly instead of actions system to avoid `gse.runtime` cyclic dependency

### Phase 1 — Codec backend + encode online
- Implement `codec_backend` abstraction with AV1 backend (+ H.265 fallback).
- Color convert + downscale compute shader (`rgba_to_nv12.slang`).
- Vulkan Video session creation, encode every frame, discard bitstream.
- **Deliverable:** encode running, validated via RenderDoc / Vulkan validation. No output yet.

### Phase 2 — Ring buffer + clip save (F10)
- Coded-unit ring buffer with keyframe index.
- MP4 muxer (regular mode): `av1C`/`hvcC` box, `stts` with per-sample durations, edit lists for trim.
- F10: walk back to keyframe, mux, write on worker thread.
- **Deliverable:** F10 produces a playable MP4 of the last N seconds.

### Phase 3 — Continuous recording (F11)
- Fragmented MP4 muxer mode (crash-safe).
- F11 toggles tee: coded units flow to both ring buffer and live muxer.
- **Deliverable:** F11 produces a growing MP4 file, playable even if the engine crashes mid-recording.

### Phase 4 — Polish
- Settings UI panel (source mode toggle, resolution cap, ring length, bitrate).
- On-screen toast: "clip saved", "recording started/stopped", "video capture unavailable".
- `is_capturing` flag wired to UI for self-hiding.
- Codec selector if both AV1 and H.265 are available.
- Perf measurement: capture frame-time overhead at 1080p and native.

---

## Open Risks

1. **Vulkan Video maturity.** The encode extensions are core-promoted but validation layers and SDK samples are rough. AV1 encode (`VK_KHR_video_encode_av1`) is the newest — expect driver bugs, especially on Intel Arc where AV1 encode is newer than H.264/H.265.
2. **AV1 in MP4 muxing.** The `av1C` box format is simpler than `hvcC` in some ways (no length-prefixed parameter set arrays) but less documented. The ISOBMFF AV1 spec (AV1-ISOBMFF) is the reference.
3. **Queue family ownership transfers.** The render graph tracks resources, but cross-queue-family transfers for the capture image are new territory. May require explicit release/acquire barriers that the current `track()` API doesn't express.
4. **Driver support probe.** Need to enumerate `VK_KHR_video_encode_av1` → `VK_KHR_video_encode_h265` → screenshot-only at init time. Both extensions have different required feature structs and profile queries. The probe logic is non-trivial.
5. **Own-muxer scope.** Rolling our own MP4 muxer for two codecs + VFR + fragmented mode is ~1500 lines. This is bounded but real — budget 4-5 days for the muxer alone, with extensive test playback across VLC, Windows Media Player, Chrome, Discord.
6. ~~**Render graph integration.**~~ Resolved in Phase 0 — capture pass runs `.after<ui::state>()`, barriers + copy are encapsulated in `capture_swapchain()`, swapchain layout is restored to `eColorAttachmentOptimal` before the graph's final present transition. No validation layer errors.

### Resolved in Phase 0
- **Module cycle risk**: `CaptureRenderer` cannot import `gse.runtime` (cyclic via `Engine.cppm` → `Graphics.cppm`). Solved by using `input::system_state` from `gse.platform` directly.
- **Swapchain alpha**: compositeAlpha is eOpaque so swapchain alpha is garbage. Fixed by forcing alpha to 0xFF before PNG write.
- **`gse::system_clock` name collision**: `system_clock::now()` inside `SystemClock.cppm` resolves to the engine's quantity-based clock, not `std::chrono::system_clock::now()`. Fixed by fully qualifying all `std::chrono::` calls in `timestamp_filename()`.

---

## Estimated Effort

| Phase | Scope | Days |
|---|---|---|
| 0 — Plumbing + screenshots | Queue detect, module skeleton, F9 end-to-end | 3–4 |
| 1 — Codec backend + encode | Abstraction, AV1+H.265 backends, color convert shader, session setup | 5–7 |
| 2 — Ring buffer + clip save | Ring, MP4 muxer (regular), F10 end-to-end | 5–6 |
| 3 — Continuous recording | Fragmented MP4, F11 tee mode | 2–3 |
| 4 — Polish | Settings UI, toasts, perf measurement | 2–3 |
| **Total** | | **17–23 days** |

The muxer (~1500 lines, two codecs, two modes) is the largest single piece and the hardest to get right. Phase 2 is the riskiest.
