# Phase 3 — Camera and Capture, Scoped

> **Revised 2026-08-13.** The original scope chose an image-sequence-plus-`ffmpeg` capture path because `video_encode_enabled()` was unvalidated on this hardware. It no longer is, and the capture triggers turned out to already be channel messages. Roughly half of the original work items are superseded as a result; they are kept below, marked, with the reason. The camera half is unchanged and is now the whole of the remaining difficulty.

Deliverable from [script_runner_plan.md](script_runner_plan.md): every README clip reproducible from one command. The plan calls this the forcing function for the whole design.

## What the README actually needs

Six slots in `README.md`, five distinct subjects — the hero clip at the top is a re-cut of whichever of the five reads best, not a sixth shoot.

| Slot | Subject | Content exists? |
|---|---|---|
| Hero | re-cut of one below | — |
| `README.md:46` | Atmosphere and volumetric clouds | no scene built for it |
| `README.md:52` | HDR, bloom, AgX tonemap | no scene built for it |
| `README.md:58` | Physics-driven locomotion | yes — `locomotion` scenario |
| `README.md:64` | VBD physics solver | yes — `pyramid_*`, `parity_pile_*` |
| `README.md:70` | Forward+ light culling | partly — `render_stress`, but no lights |

Three of five already have their subject matter written and running. What none of them has is a camera or a recorder, and that is the correct way to read the size of this work: it is not five pieces of content, it is two pieces of machinery and then two pieces of content.

## What already exists

- **`camera::request`** in `Graphics/3D/Camera/CameraData.cppm` has exactly the shape scripted camera motion needs: a `target` (position, orientation, fov, near and far planes), plus `priority`, `blend_duration`, and `continuous`. It is a channel message, which is what D10 requires of anything a scenario drives.
- **The capture renderer** ships F9 screenshots, and F10 clip-save and F11 continuous recording are code-complete against an own-rolled MP4 muxer with a fragmented mode.
- **All three capture triggers are already channel messages** — `screenshot_request`, `save_clip_request`, `toggle_recording_request` in `Graphics/Renderers/CaptureRenderer.cppm`. A scenario can therefore start and stop a recording with no new engine machinery at all. This is the single most important fact in this document and it was not true when the document was first written.
- **Video encode is live on this hardware.** The Editor's 50%-GPU regression in August proved it by accident — `capture::init` was creating a real AV1 session (`Video encoder created: AV1 1951x1157`) and encoding every frame in a tool that wanted none of it. It is now intent-gated on `engine_config::video_encode`. The Intel Arc 140V finding that drove the original decision is stale.
- **`binary_writer` / `binary_reader`** serialize by reflection walk, which is what a path asset would want — the same mechanism the world-state digest uses.
- **The fixed-step clock** is the whole reason this works: every captured frame is exactly 1/60 s of content regardless of how long it took to render, so a scene that renders at 22 fps still yields a smooth clip. **This was aspirational until 2026-08-14** — see the PTS note below. The clock was always fixed-step; the capture pipeline simply ignored it.

**`camera::request` has no consumer.** `camera::run` reads `ui_focus_request`, `viewport_update`, and `camera_yaw_request` and nothing else. The struct is declared and unused, so "it exists" means the shape is agreed, not that it works.

## The blocker — diagnosed 2026-08-14, and it was never the boot gate

Windowed scenarios could not run unattended. This document, and `scenario_authoring.md`, both blamed the boot gate: deferred boot waits on `loading::state::rendered_once()`, so supposedly the GUI often never drew and the world never populated. **That was wrong.** Running `render_stress` and reading the log shows the boot gate working perfectly — every marker fires, in order, in milliseconds:

```
15:38:24.343  bench: requested scene 'Sandbox'
15:38:24.410  boot: loading_screen first build()
15:38:24.410  boot: deferred boot begin (loading screen rendered)
15:38:24.410  boot: app_setup begin / end
15:38:26.379  boot: loading.mark_finished (all settled + rendered)
```

Read the first two lines together. **The bench requests the scene 67 ms before the scene exists.** `step_bench` calls `activate_scene` on its first step behind a one-shot `state.scene_requested` latch that never retries, and the scenes are registered by `WorldLoader`'s `add_scene` calls inside `app_setup` — which the render branch defers behind the loading screen. The request names a scene id nothing has registered, is dropped, and is never reissued. The world stays empty until the settle cap.

Headless works for exactly the mirrored reason: `engine::initialize`'s headless branch runs `register_deferred()` and `app_setup()` inline, so scenes exist before the bench loop starts. Measured contrast on the same binary — headless `physics_stress` logs `requested scene` and `world settled` in the *same millisecond*; windowed `render_stress` never settles at all.

So the deferral is not the bug. The bug is that the bench assumed a registration ordering that only holds on one of the two branches.

**Fixed 2026-08-14** in `Runtime/Bench.cpp`: the latch is no longer set until `find_scene` confirms the scene is registered, so the activation is issued on the first step where it can actually take effect. Headless behaviour is unchanged — the scene is already there on step one — and the windowed path now waits the few frames until `app_setup` has run. The settle-cap abort message also gained `registered=` and `settled=` fields, because the old message reported only `populated=false`, which is the symptom shared by every possible cause and is what made this look like a boot-gate problem for months.

**Offscreen render mode is not needed.** It was listed here as escape hatch 2 on the assumption that the windowed path was structurally unfixable. It was a one-line ordering bug.

**Confirmed 2026-08-14.** `render_stress` now completes unattended **10 runs out of 10**, every one reporting a world-state hash of `0x6464871876713c93`. No further render-mode obstacle sat behind this one. Windowed scenarios are reproducible in the same sense headless ones are, which is the property the whole capture plan depends on — and the identical hash across ten windowed runs is the first evidence that determinism survives the renderer at all.

## Work items

### 0. Validate the capture output — **failed, root-caused, and fixed 2026-08-14**

A 27 s clip recorded with Ctrl+Shift+R plays in VLC and does not play in a browser: the play button does nothing, with no console error. Probing it localises the fault precisely, and it is not where this document previously guessed.

**The MP4 layer is correct.** Verified against the file directly:

- `av1` / `av01` / `yuv420p`, 1920x1080, 1744 samples, container duration 27.18 s.
- Per-sample durations sum to exactly the declared duration, at ~64 fps. The `live_muxer` close-time patch of `mvhd`/`tkhd`/`mdhd` works.
- Every sample's OBU chain is `[temporal_delimiter(size 0), OBU_FRAME(size N)]` and consumes exactly its `trun` `sample_size`, byte for byte, across the samples checked.
- `av1C` is well formed — marker/version correct, `seq_profile=0`, `seq_level_idx_0=8`, and a 13-byte `configOBUs` holding a genuine `OBU_SEQUENCE_HEADER` with a valid size field.

**The elementary bitstream is rejected.** `ffmpeg -f null -` reports `Error parsing OBU data` on **1744 of 1744** samples; zero frames decode; the decode error rate is 1.0. The rejecting decoder is **libdav1d**, which is exactly the AV1 decoder Chrome and Firefox use. That is the whole explanation of the symptom: the browser has nothing to show, so `play()` is a no-op, and because the file is structurally valid there is nothing for it to log.

**Root cause: the muxer never writes the sequence header in-band.** The encoder is fine — every one of the 1744 frame OBUs decodes perfectly once a sequence header precedes them. Comparing our first sample against a known-good file built by `ffmpeg` from the *identical* frame bytes:

```
ffmpeg  0a 0b <11-byte sequence header> 32 a5 5e        10 20 8e 40 ...
gse     12 00                           32 a5 de 80 00  10 20 8e 40 ...
        ^^ temporal delimiter                           ^^ identical payload
```

`ffmpeg` puts an `OBU_SEQUENCE_HEADER` at the head of each keyframe sample. We put only a temporal delimiter. `find_sequence_header_obu` pulls the header out of `stream_header` at `open()`, spends it on the `av1C` box, and drops it. Hardware decoders (VLC, Windows Media Player) seed themselves from `av1C` and were happy; dav1d — and therefore Chrome and Firefox — needs the in-band copy.

Confirmed by three measurements, not inference: our MP4 fails 1744/1744; `ffmpeg`'s MP4 from the same frames passes; and **our exact samples** — temporal delimiters, non-minimal 4-byte LEB128 sizes and all — pass with a sequence header spliced in. That last one also clears two red herrings: dav1d does not care about the TD OBU, and it does not care about the padded `obu_size` encoding.

**Fixed 2026-08-14** in `Mp4Muxer.{cppm,cpp}`: `live_muxer` retains the sequence header in `m_sequence_header` and writes it after the temporal delimiter on every keyframe sample; the one-shot `mux()` does the same on the F10 path, which had the identical defect. The per-sample prefix is now a `std::span<const std::uint32_t>` computed by `sample_prefix_sizes` rather than one uniform `std::uint32_t`, because the prefix is keyframe-dependent — `trun` sample sizes, `stsz`, and the `mdat` totals all consume it. Scoped to AV1; H.265 keeps a zero prefix, since `hvc1` carries its parameter sets in `hvcC` by design and there is no H.265 capture to test against.

Verified by rebuilding the existing 27 s recording the way the patched muxer writes it: **0 parse errors, 1744/1744 frames decoded.** The rebuilt file is at `%LOCALAPPDATA%/GSE/captures/recordings/recording_20260814_151042_FIXED.mp4`.

**It also removed the escape hatch, while it lasted.** Item 5 assumed an offline `ffmpeg` transcode could always rescue a browser-hostile file. It could not rescue this one — `ffmpeg` could not decode a single frame either. Worth remembering as a shape: a file no decoder accepts has no publishing-side workaround.

### 0b. Presentation timestamps came from wall time — fixed 2026-08-14

This document claimed the fixed-step clock guaranteed smooth clips. It did not, because nothing in the capture path consulted it. `vulkan::video_encoder::encode_frame` stamped `slot.last_pts = m_clock.elapsed()` from a `gse::clock`, i.e. `steady_clock`. Presentation timestamps were **wall time**, visible in the first recording as sample durations of 2654, 3112, 2686, 2916, 2506 µs — jittering with render cost where fixed-step content should be a constant 16667.

Interactively that is defensible. Under a scenario it is wrong twice over. 600 frames at 1/60 s is 10 s of content; if each frame takes 24 ms to render, wall time is 14.4 s, so the clip is 14.4 s long showing 10 s of content — slow motion, unevenly paced, and the playback speed varies with whatever else the machine was doing.

Fixed by making the PTS *content* time rather than wall time:

- `system_clock` accumulates `content_time` from `delta_time` in both `update()` branches, exposed as `content_now<Q>()`. Under a fixed-step override `delta_time` is `const_update_time * fixed_steps_count`, so content time advances in exact 1/60 s units.
- `encode_frame` takes the PTS as a parameter across all three layers (`vulkan::video_encoder`, the `gse.gpu` facade, and the `capture` caller), and the encoder's `m_clock` member is gone. An encoder owning a clock was the layering mistake that made this easy to miss.

Under a scenario every sample duration is now exactly 16667 µs — a true constant-rate 60 fps clip regardless of render cost. Interactively the PTS becomes the snapped delta the world actually advanced by, which is more faithful than raw `steady_clock` anyway.

### 0c. Bench runs no longer show a window — 2026-08-14

`engine::render` showed the window once loading finished, and GLFW's `focus_on_show` hint is `true`, so every unattended scenario stole keyboard focus. The window is already *created* hidden; only the show step was the problem. It is now skipped entirely when `m_config.bench.enabled`, logging `boot: window kept hidden (bench run)` in its place.

This is what makes "headless" recording achievable without true offscreen rendering: the window exists and the swapchain presents to it, but it is never shown and never takes focus. Attached mode has always relied on the same property. Note the consequence recorded in the pacing work — present-timing feedback is structurally dead against a hidden window — which does not matter under a fixed-step clock but does mean these runs should not be trusted for present-pacing measurements.

### 0d. Recorded video was double sRGB-encoded — fixed and verified 2026-08-14

**Shader changes need no C++ rebuild.** `.slang` files are compiled at runtime from the source tree — nothing is copied into the build directory and there is no compiled-shader cache (the one under `%LOCALAPPDATA%/GSE/cache` is the Editor's symbol index). Edit and re-run.


The first scripted-camera clip came out milky: lifted blacks, crushed contrast, desaturated. Applying a single sRGB *decode* to an extracted frame restored a correctly exposed image, which pins it at exactly one gamma encode too many.

`rgba_to_nv12.slang` sampled the capture image and called `linear_to_srgb` on it, assuming linear input. It is not linear. The chain is: tonemap writes display-encoded values to the swapchain → `blit_swapchain_to_image` copies them into an `r8g8b8a8_unorm` capture image → the shader samples a UNORM view, which performs no linearization. So the values arriving are already sRGB-encoded and the shader encoded them again.

The confirming argument is the screenshot path: F9 uses a raw `copy_image_to_buffer` with no conversion anywhere, and its PNGs have always looked correct — which is only possible if the swapchain already holds display-encoded bytes.

Fixed by deleting the conversion. This is also the semantically correct pipeline: BT.709 YUV is defined on gamma-encoded R'G'B', so the right input to the matrix is display-ready values, and the linearize step was conceptual overreach rather than a missing piece.

Worth knowing if this ever regresses on other hardware: the fix assumes the blit performs no sRGB conversion, which holds because source and destination are both effectively UNORM. A swapchain that genuinely reported an sRGB format would linearize on blit read and the old shader would have been right — so if a future backend produces dark, over-saturated clips, this is the first place to look.

### 1. Windowed boot gate

Make world population independent of a drawn loading screen. See the phase scope below — this is the immediate next piece of work and is specified there in full.

### 2. Camera request consumer — **implemented 2026-08-14**

`camera::run` now consumes `camera::request`. The arbitration question this document flagged turned out to have an obvious answer, because the machinery was already there: `camera::data` already carried `blend_from` / `blend_to` / `blend_elapsed` / `blend_duration` / `blending` and an `active_controller_entity` + `active_priority` pair, and `run` already ran a priority contest across `follow_component`s that blends whenever the winner changes.

**A scripted request therefore joins that contest rather than bypassing it.** The latched request is one more candidate; if its priority beats the best `follow_component`, it supplies `best_target`, `best_controller`, and `best_blend_duration`, and every existing code path downstream — blend on takeover, track directly while it keeps winning, blend back when something outbids it — works unchanged. No second camera authority, no ordering hazard between the two.

Decisions worth knowing:

- **`continuous` means the request latches.** A `continuous = true` request holds the camera until outbid or replaced by its own requester. `continuous = false` is a one-shot: once its blend completes it clears itself and control falls back to the normal contest. This is why per-frame scripted paths work — each frame's request replaces the previous one from the same requester, which the existing "same winner" branch already treats as direct tracking rather than a fresh blend.
- **A missing `requester_id` is filled in.** A scenario naturally writes `.target` and `.priority` and leaves `requester_id` default, and a default `id` does not `exists()`, so the whole contest would have silently skipped it. Requests without one are assigned `scripted_requester_id()`, a stable id from `find_or_generate_id`. Silent no-ops are the wrong failure mode for something a scenario drives blind.
- **`blend_duration = 0` is a hard cut**, guarded explicitly rather than left to divide by zero. This is the setting a scripted path wants — exact positions per frame, no easing.
- Sandbox `follow_component` priorities are 50–60, so scripted requests use 100.

### 3. ~~Frame dump~~ — SUPERSEDED

~~New `Scenario/FrameDump.cppm`, one image per virtual frame, written synchronously.~~

Superseded by item 0. The reason this was an image sequence at all was that the hardware could not encode; it can. A scenario now records by pushing `toggle_recording_request`, waiting, and pushing it again — no new module, no per-frame `std::format`, and none of the ~5 GB per take that made the original item need an opt-in flag and a warning about the profiler.

Keep the image-sequence path in mind as the fallback if item 0 comes back with a broken muxer. It was a sound design; it is simply no longer the cheap one. `script_runner_plan.md:296` still lists `FrameDump.cppm` as a Phase 3 TODO and should be read against this section.

### 4. ~~Camera path asset and playback~~ — DEFERRED, not on the critical path

~~A `camera_path` asset of `camera_keyframe { time at; camera::target target; }`, serialized by `binary_writer`, played back by a system consuming `camera_path_play_request`.~~

A scenario body is a coroutine that is resumed every frame with a monotonic `frame()` count. It can hold its keyframes as local `constexpr` data, interpolate them itself, and push one `camera::request` per frame in about fifteen lines. That is reproducible, deterministic against the virtual frame index, and needs no asset type, no registration, no serialization, and no play-request plumbing — and it sidesteps the open question the original item ended on, which was how a scenario learns a path's duration when it cannot read the asset.

Build the asset when hand-scripting five camera moves demonstrably hurts. It probably will not, and if it does, the keyframe data written for the scripted version is exactly what the asset would serialize.

### 5. ~~`ffmpeg` invocation~~ — SUPERSEDED, with a likely residue

~~Offline image-sequence-to-video, a documented command plus a script.~~

Superseded along with item 3. The residue: GitHub renders README video through the browser's `<video>` element, so playback depends on the viewer's browser rather than on the file being valid. H.265 in MP4 plays almost nowhere in browsers, and AV1 is well supported in Chrome and Firefox but not universally. If item 0 confirms that, a one-line offline transcode to H.264 stays in the pipeline for README-bound clips only — a distribution step, not an engine one, and much smaller than the original item.

### 6. ~~Editor keyframe authoring~~ — DEFERRED

Unchanged in substance and now further out, since item 4 no longer sits between it and a working clip. A path can be hand-written or recorded from a free-camera session long before there is a UI for editing one.

### 7. Sun arc driver

`sun_elevation` and `sun_azimuth` are settings on `renderer::atmosphere::data`, consumed by `compute_sun_direction`. Settings are fixed for the duration of a run, so a dawn-to-dusk arc needs something that animates them — a channel request the atmosphere system consumes, in the same shape as every other scenario-driven change. Small, and needed only by the atmosphere clip.

### 8. Light spawn request

`dev_spawn` offers `spawn_stress_request`, `spawn_joints_request`, `spawn_character_request`, and `spawn_pyramid_request`. Forward+ culling needs many lights, and there is no request for that. Small, and needed only by that clip.

## Ordering

Each step is independently useful, and a reproducible clip arrives before any authoring work:

1. ~~**Capture output validation**~~ — failed on first test, root-caused to a missing in-band sequence header, **fixed 2026-08-14** in `Mp4Muxer`. Needs one confirming recording from a build carrying the fix. Items 3 and 5 stay superseded.
2. ~~**Boot gate**~~ — misdiagnosis; the real defect was the bench activating its scene before `app_setup` registered it. **Fixed and gated 2026-08-14** — `render_stress` 10/10 unattended, one hash across all ten.
3. ~~**A scenario that records itself**~~ — `record_clip`, added and **verified 2026-08-14**: settle, spawn the stress workload, toggle recording on, wait 5 s of content, toggle off. No camera, no keypress, no window.

   Result: 262 frames, **0 decode errors**, and **every sample duration exactly 16666 µs**. The run took 94 s of wall time to produce 4.37 s of content — roughly 21× slower than realtime — and the clip is still constant-rate 60 fps, which is the whole point of stamping PTS from content time. It also confirms the muxer fix in the live pipeline rather than in a reconstruction.

   `warmup_frames = 0, frames = 420` — the budget is sized to end just after the recording stops. Warmup is pointless for a capture run, and every surplus frame costs ~21× its own duration in wall time.
4. ~~**Camera request consumer**~~ — implemented 2026-08-14; `record_clip` now drives a 70° orbit at 48 m, pushing one `camera::request` per frame with `blend_duration = 0`. Needs a build to confirm.
5. **First real clip**, on the solver, reusing a scene and a workload that already exist.
5. **Locomotion and Forward+ clips** — the remaining two whose subject matter exists.
6. **Atmosphere and tonemap clips** — the two that need scenery built for them.
7. Path asset and editor authoring, if and only if 4 through 6 make the case for them.

The honest risk in this ordering is step 6. Steps 4 and 5 are bounded — the content exists and the work is framing it. "Make the sky look good" is not bounded the same way, and it is where the time will actually go.

---

## Phase scope: unattended windowed scenarios

### What the two branches actually differ on

`engine::initialize` takes two paths, and the difference that matters is *when `app_setup` runs*, not the loading screen.

The **headless** branch runs `register_deferred()`, `app_setup()`, `scheduler.initialize()`, `enter_running()`, and `mark_finished()` inline, with no gate. Scenes are registered before the bench loop starts.

The **render** branch pushes a `gui::loading_screen` and parks `register_deferred()` plus `app_setup()` inside `m_deferred_boot`, fired from `engine::update` once `loading::state::rendered_once()` is true. The gate itself works — it fires within a frame of the loading screen drawing. What it changes is that scene registration now happens *after* the bench has already taken its first step.

### Why the deferral is not the thing to remove

The deferral exists so the loading screen reaches the swapchain before boot work blocks the main thread. That mattered when boot ran an eager, single-threaded `compile_non_boot_critical()` pass; per the boot-async work it no longer does, so it protects less than it used to. But it is doing its job correctly, and removing it would be treating a symptom. The bench's assumption about registration ordering was the defect, and that is what was fixed.

### Done when

`render_stress` completes unattended, ten runs out of ten, under an external timeout, printing its summary line. That scenario was written before any of this and was already unusable, which makes it a free and honest gate: nothing about it was authored to make this test pass.

Run it as:

```bash
./Sandbox.exe --engine-bench-scenario render_stress
```

with `~/.gcc-trunk/current/bin` on `PATH`, from the Sandbox build directory, under a timeout — a tripped assert logs `[fatal]` and hangs rather than exiting.

### If it still fails

Read the new abort line first. `registered=false` means the scene was never added and the fix did not take; `registered=true populated=false` means activation happened and the scene spawned nothing; `settled=false` alone means the scheduler never quiesced and the problem is elsewhere entirely. The old message could not distinguish these, which is why this took months to see.

## Risks

- ~~**The AV1 bitstream is malformed.**~~ **Closed 2026-08-14** — the bitstream was always fine; the muxer omitted the in-band sequence header. Fixed and verified at 1744/1744 frames. See item 0.
- ~~**Browser playback is a distribution problem with an `ffmpeg` workaround.**~~ **Refuted 2026-08-14** — `ffmpeg` cannot decode the file either, so there is no transcode that rescues it. Do not plan around a publishing-side fix for this failure.
- **A tolerant player is not validation.** VLC played a file that no software decoder accepts. Any future "capture works" claim needs a decode check (`ffmpeg -v error -i <file> -f null -`) and not a visual one.
- **Timing noise is irrelevant here.** Risk 6 in the plan warns that windowed runs are noisier, but capture is the one consumer that does not care: the clock is virtual, so render cost affects wall time and not content. Do not carry perf-run assumptions into capture.
- **`camera::request` is unproven.** Declared and unused is not the same as working, and the arbitration question against the existing camera controllers is unresolved.
- **The last two clips are unbounded.** Atmosphere and tonemap need scenery that does not exist, and "looks good" has no completion test. Schedule them last and expect them to dominate.
