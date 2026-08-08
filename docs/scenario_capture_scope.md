# Phase 3 — Camera and Capture, Scoped

Deliverable from [script_runner_plan.md](script_runner_plan.md): every README clip reproducible from one command. The plan calls this the forcing function for the whole design.

## What already exists

Worth knowing before writing anything, because three of the five pieces are mostly wiring rather than new machinery.

- **`camera::request`** in `Graphics/3D/Camera/CameraData.cppm` already has exactly the shape scripted camera motion needs: a `target` (position, orientation, fov, near and far planes), plus `priority`, `blend_duration`, and `continuous`. It is a channel message, which is what D10 requires of anything a scenario drives.
- **The capture renderer** already screenshots on F9: `screenshot_request{}`, `pending_screenshot`, a `per_frame_resource` ring, and readback.
- **`binary_writer` / `binary_reader`** serialize by reflection walk, which is what D3 wants for the path asset — the same mechanism the world-state digest uses, so a field added later is covered without anyone maintaining a list.
- **The fixed-step clock** is the whole reason this works: every captured frame is exactly 1/60 s of content regardless of how long it took to render, so a scene that renders at 22 fps still yields a smooth clip.

**`camera::request` has no consumer.** Only `camera_yaw_request` is read today. The struct is declared and unused, so "it exists" means the shape is agreed, not that it works. Wiring it is item 2 below and everything else camera-related depends on it.

## The blocker, which is structural

Capture requires the renderer. Windowed scenarios cannot run unattended: the deferred boot waits on `loading::state::rendered_once()`, set from `gui::loading_screen::build()`, so the world never populates unless the window actually presents a frame. Measured at roughly one run in two, aborting cleanly at the settle cap after ~40 s. The runs that succeed succeed because a human clicked the window.

So "one command" is **not currently achievable** for any render scenario, capture included. Three ways out:

1. **Fix the boot gate** so deferred boot does not depend on a presented frame. Smallest of the three, and it also unblocks `render_stress`, which is already written and currently unusable.
2. **Offscreen render mode** — `create_window = false, render = true`. Architecturally plausible already: `gpu::context::init` takes `std::optional<shared_view<window::data>>` and only builds a swapchain when a window is present, so a device without a window is supported at that layer. The unknown is every renderer that targets the swapchain.
3. **Accept human-supervised capture**, and drop "one command" from the deliverable.

Option 1 first. It is the cheapest, it pays for itself immediately on an existing scenario, and it is a prerequisite for judging whether 2 is needed at all.

## Work items

### 1. Windowed boot gate

Make world population independent of a presented frame. Diagnostic first: `loading_screen::build()` calls `mark_rendered()` unconditionally at its top, so the question is why `build()` is not reached on the failing runs — GUI not drawing, or the window not pumping.

### 2. Camera request consumer

`camera::run` consumes `camera::request` and applies it with priority and blend arbitration. Needed by scripted playback *and* by editor authoring, so it is shared and comes before both. Opens one design question worth settling deliberately: how a scripted request arbitrates against `free_camera` and `orbit_camera`, both of which currently write the camera directly.

### 3. Frame dump

New `Scenario/FrameDump.cppm`. One image per virtual frame, written **synchronously**.

The F9 path guards on a single outstanding write (`write_in_progress`) and drops a request when one is in flight. That is correct for screenshots and wrong here — it would silently skip frames, and a clip missing frames 200–260 looks like a stutter in the content rather than a dropped write. Under a virtual clock a slow write costs wall time and nothing else, so synchronous is the correct choice rather than a compromise.

Opt-in, off during every perf run. The per-frame `std::format` for the filename is a sanctioned exception on that basis, per D11 — worth stating at the call site so it is not later "fixed" into a buffer reuse that breaks the naming.

### 4. Camera path asset and playback

```cpp
struct camera_keyframe {
    time at;
    camera::target target;
};

struct camera_path {
    std::vector<camera_keyframe> keys;
};
```

Serialized by `binary_writer`, registered as an asset type. Note this lands in headless automatically now that both branches register one declaration — which is wanted, since a path is data and headless may want to validate one.

Playback is a system consuming `camera_path_play_request { id path; }`, sampling against the **virtual frame index** and pushing a `camera::request` per frame. It must not read `system_clock::now()` (D4). The scenario pushes the play request and waits; because a scenario cannot read the asset to learn the path's duration, either the request carries the frame count or the scenario waits a declared span.

### 5. `ffmpeg` invocation

Offline, outside the engine. A documented command plus a script. Deliberately not engine code: `video_encode_enabled()` is still unvalidated on this hardware, and D11 already chose the image-sequence path over the Vulkan Video encoder for that reason.

### 6. Editor keyframe authoring

Fly the free camera, drop keyframes, save the asset. Largest and least certain piece, and it is last on purpose — a path can be hand-written or recorded from a free-camera session long before there is a UI for editing one.

## Ordering

Each step is independently useful, and a reproducible clip arrives before any editor work:

1. Boot gate — unblocks `render_stress` immediately, independent of capture.
2. Camera request consumer — any scripted camera becomes possible.
3. Frame dump plus `ffmpeg` — clips from a live camera, no authoring yet.
4. Path asset plus playback — reproducible camera motion.
5. Editor authoring — quality-of-life on a working pipeline.

## Risks

- **Disk.** 600 frames of 1080p PNG is roughly 5 GB per take. Opt-in per run, never part of the perf loop.
- **Timing noise is irrelevant here.** Risk 6 warns that windowed runs are noisier, but capture is the one consumer that does not care: the clock is virtual, so render cost affects wall time and not content. Do not carry perf-run assumptions into capture.
- **`camera::request` is unproven.** Declared and unused is not the same as working, and the arbitration question against the existing camera controllers is unresolved.
- **Frame dump interacts with the profiler.** A synchronous per-frame write will dominate any profile taken during a capture run. Capture runs are not perf runs and their `.gsprof` should be treated as meaningless, or recording disabled outright while dumping.
