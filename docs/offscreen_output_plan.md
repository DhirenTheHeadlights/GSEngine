# Offscreen Output Plan

Goal: render a full frame with no OS window, and record it to MP4 from code. Reach that by giving the existing attached-mode machinery one owner, not by adding a second offscreen path. End state: the editor's attached game and a headless recording run go through the same render-target code, and neither creates a hidden window.

Status: approved 2026-10-04. Decisions: attached input goes through a channel; an attach failure exits through the fatal pipe; DX12 gets only the windowless-device fix, and video encode stays Vulkan-only. Phases 1, 2, 3, 5 and 6 are implemented and build. Phase 2 does not yet route attached input through `synthetic_input_request`. Phase 4 (attached without a window) is deferred: it needs the GUI decoupled from `window::data`. The scheduler's starvation rule now drops a system only when every channel it consumes has no active producer; before, any one starved channel dropped it, so a windowless run lost the camera and the whole 3D renderer.

## Current State

What the code does today, with the defects this plan removes. Line numbers are as of 2026-10-04.

### The swapchain owns four facts that are not about presentation

`gpu::swap_chain` owns:

- the presentable images;
- the only depth image in the engine (`Swapchain.cppm:271`, `"swapchain.depth"`);
- the extent every renderer sizes against: `render_graph::extent()` returns `m_swapchain->extent()` (`RenderGraph.cpp:489`), and roughly 25 renderer call sites use it;
- the resize event: `context::on_swap_chain_recreate` (`Context.cpp:106`) has 9 renderer subscribers, and framebuffer images rebuild through `m_swapchain->on_recreate` (`RenderGraph.cpp:56`).

`create_framebuffer_image` (`RenderGraph.cpp:63`) and `render_graph::depth_image()` (`RenderGraph.cppm:344`) dereference `m_swapchain` with no alternative. A render graph without a swapchain cannot size or depth-test anything.

### The offscreen override is a sticky patch over the swapchain

- `set_offscreen_target` (`RenderGraph.cpp:158`) swaps out colour only. Extent, depth and format still come from the swapchain.
- It persists across frames until someone clears it.
- `resolve_color_target` (`RenderGraph.cpp:680`) sends every null-target colour output to the offscreen image, including `.target(window)` outputs for secondary windows.
- The per-target clear (`RenderGraph.cpp:1750`) still clears every acquired swapchain image while the override is active.

### Attached mode is a hidden window kept alive for its side effects

- The game creates a GLFW window. It is cloaked (`Window.cpp:1628`), never shown (`Engine.cpp:573`), and forced to mailbox present mode (`Window.cpp:281`).
- It still acquires and presents every frame. `Frame.cpp:262` and `:610` only skip pacing and present timing for it.
- The window exists to supply:
  - the extent that sizes the shared ring;
  - the colour format (`Engine.cpp:55`);
  - depth;
  - resize, which goes `attached_resize_message` → `window_resize_request` → GLFW resize → swapchain recreate → ring recreate (`Engine.cpp:365`, `:455`);
  - the input queue: `push_attached_input` writes into `window_state->primary.input_events` (`Engine.cpp:604`).
- The ring, semaphores, counters, report clock and revision are 13 members of `class engine` (`Engine.cppm:217-229`). They are not part of a system.

### Two authorities for screen size

- Camera aspect comes from `window::viewport` (`Renderer.cpp:37` → `camera::viewport_update`).
- The pixels being rendered come from the swapchain extent.
- The GUI reads both (`Gui.cpp:188-189`).
- Today the two agree only because the window and the swapchain agree. An offscreen target with its own extent would split them silently. This is the review guide's paired-derivation defect.

### Capture is bound to the renderer and to key bindings

- `renderer::capture` registers only when `config.render` is true (`Engine.cpp:247`).
- Recording starts only from the `Ctrl+Shift+R` action (`CaptureRenderer.cpp:118-130`).
- The pieces it needs already exist:
  - It records from `current_target()`, so it follows the offscreen image.
  - Its pts is `system_clock::content_now`. Under `set_fixed_step_override` that is pure simulation time (`SystemClock.cppm:120`).
  - The muxer derives sample durations from pts deltas (`Mp4Muxer.cpp:955`).
  - The encoder applies back-pressure by blocking on its slot fence. It drops a frame only after a 500 ms timeout (`Vulkan/VideoEncoder.cppm:825`).
- So no new clock and no new encoder plumbing is needed. Only the trigger and the target are missing.

### Leftovers found on the way

- The editor viewport's `targets`/`slots`/`display_slot` (`Viewport.cppm:35-37`) are a 1280x720 navy-clear placeholder. `display_slot` has no consumer outside Viewport.
- DX12 `make_video_encoder_backend` returns `nullptr` (`DeviceDx12Backend.cppm:846`).
- `create_dx12_device_backend` dereferences `*win` unconditionally (`:851`), so DX12 cannot create a device without a window.

## Target Design

Three responsibilities, three owners.

### 1. `render_graph` owns the frame surface

The frame surface is the extent, colour format, depth image and framebuffer images every pass renders against, plus the resize event. It is the single authority for "how big is the frame".

- `extent()` reads it.
- `depth_image()` reads it.
- `create_framebuffer_image` sizes from it.
- Renderers subscribe to `render_graph::on_resize` instead of `context::on_swap_chain_recreate`, which is deleted.
- The swapchain keeps only presentable images, acquire and present. `create_swapchain_depth` moves out and becomes the surface's depth.
- The camera viewport and the GUI's render-side size derive from the surface extent, so camera aspect and pixels share one source. The GUI keeps `window::viewport` only for mapping OS cursor coordinates, which is genuinely a window fact.

### 2. The frame's output is a variant, not an override

Each frame names its output as `std::variant<present_output, offscreen_output>`, resolved with overloads. There is no enum switch and no sticky pointer.

- `present_output` is today's swapchain path, with the surface sized to the window.
- `offscreen_output` is an engine-owned ring of colour images at a configured extent and format. It has an `exportable` flag. When the flag is set, the slots are created through `create_shared_surface`, which both backends implement. The surface is sized to the ring.
- `set_offscreen_target` is deleted.
- `current_target()`, `target_live()` and `resolve_color_target` resolve through the variant, so a secondary window's output can never land in the ring.
- The ring owns only images and slot rotation. It knows nothing about other processes.

### 3. Consumers own their sync and transport

- **Attached link.** Move the 13 `m_attached_*` members and `create/destroy_attached_surface` out of `class engine`, into the existing `Runtime/AttachedLink` unit as a system or owned state. It owns:
  - exporting the ring's handles;
  - the produced/consumed timeline semaphores and the starved-slot rule (`add_graphics_wait` / `add_graphics_signal` already exist on the render graph);
  - the pipe protocol;
  - resize.

  Resize becomes "recreate the offscreen output at the new extent". The GLFW round trip is gone.
- **Capture.** It reads `current_target()` as it does now. Its start/stop trigger becomes a channel request, `start_recording_request{path, duration}` / `stop_recording_request`. The `Ctrl+Shift+R` action pushes the same request, so there is one path. A recording with a duration closes itself after that much `content_now`. Any `dropping capture` timeout while a recording is active is counted and reported when the file closes, instead of being lost in the log.

### Windowless rendering becomes a configuration, not a mode

- `engine_config` already separates `create_window` and `render`. With `create_window=false, render=true`:
  - the window system stays disabled;
  - the renderer, GUI and capture register;
  - `engine::render` uses one path in which `begin_frame` has no present target;
  - the frame uses `offscreen_output`.

  `Frame.cpp` already handles a frame with no acquired targets (plain submit with the fence).
- The offscreen extent comes from a new setting, `Graphics.offscreen_extent`, a `vec2u` defaulting to 1920x1080. The attached link overrides it with the editor's requested size.
- The compute-only headless branch in `engine::update` (`Engine.cpp:382`) remains the `render=false` path. Training and eval are untouched.
- Attached mode becomes `create_window=false` plus an exportable offscreen output plus the attached link. This deletes:
  - the mailbox override (`Window.cpp:281`);
  - the cloak (`Window.cpp:1628`);
  - the forced windowed mode (`Window.cpp:1380`);
  - the attached pacing and timing branches (`Frame.cpp:262`, `:610`);
  - the window-show exemption (`Engine.cpp:563-577`);
  - the `window_resize_request` relay (`Engine.cpp:365`).

### Input needs a home that is not a window

This is the one open design point. Input is drained from `window_surface::input_events` (`Input.cppm:60`), so with no window, attached input has nowhere to land.

Proposed: producers push `input::event` to the input system over a channel. The GLFW window and the attached link both become producers. The `ui_focus` coordinate clamp/flip in `push_attached_input` moves into the attached link, since it is a transport-to-surface mapping.

`Input.cppm` and `InputEvents.cppm` must be read end to end before this is fixed. If the window needs to keep owning the queue, the fallback is a windowless `window_surface`, which is weaker because it keeps a window type for something that is not a window.

### Attach failure

Today a failed attach unhides the hidden window and runs detached. Without a window, the options are:

- (a) create the window late, on failure;
- (b) exit with the error surfaced to the editor's Terminal through the existing fatal pipe.

**Decision needed.** (b) is simpler, and the editor already reports fatals.

## Phases

Each phase leaves the tree building and behaviourally unchanged for every mode it does not target.

1. **Frame surface.**
   - Move depth, extent, framebuffer sizing and the resize event from `swap_chain` to `render_graph`.
   - Migrate the 9 `on_swap_chain_recreate` subscribers.
   - Derive camera viewport and GUI render size from the surface.

   Windowed output should be pixel-identical.
2. **Output variant.**
   - Add `offscreen_output` and the variant.
   - Delete `set_offscreen_target`.
   - Move the attached ring and semaphores into the attached link on top of `offscreen_output(exportable)`.

   Attached mode still has its hidden window in this phase. Only the ownership moves.
3. **Windowless render.**
   - Make `create_window=false, render=true` a valid configuration.
   - Add `Graphics.offscreen_extent`.
   - Fix the DX12 `*win` dereference so DX12 can create a windowless device. DX12 still has no video encode; capture logs it as it does today.
4. **Attached without a window.**
   - Input channel (after the read above).
   - Resize through output recreate.
   - Attach-failure decision.
   - Delete the hidden-window branches listed above.
   - Editor: delete the placeholder `targets`/`slots`/`display_slot`, drawing the navy clear in the panel instead.
5. **Capture trigger.**
   - Add the recording requests with duration.
   - Route the key action through them.
   - Report drops at close.
6. **Project (HumanoidLocomotion).**
   - Add `--play-record <path>` and `--play-record-seconds <time>`.
   - Run the play setup with `create_window=false, render=true`, `set_fixed_step_override(1)`, `viewer_camera::update` in follow mode, and a recording request at autostart.
   - Exit when the recording closes.

## Verification

Builds go through `Tools/gse-build` only. Windowed launches need the owner's go-ahead each time.

- **Phase 1:** screenshot (F9) of the windowed viewer before and after, at the same seed, frame and camera; the images must be byte-identical. Editor attached viewport: open, resize, close, reopen; no device loss in the GPU log.
- **Phase 2:** the same attached checks, plus two attached instances at once (`max_attached_instances = 4`).
- **Phase 3:** a windowless run on Vulkan renders and the F9-equivalent request writes a PNG. A windowless DX12 device boots.
- **Phase 4:** attached checks again, with editor input reaching the game (camera drag, UI click) and resize. The game process has no top-level window (`EnumWindows` by pid).
- **Phase 5:** a windowed `Ctrl+Shift+R` recording still works. A 10 s request yields `frame_count() == 600` at 1/60 s capture with zero reported drops, and the MP4 duration reads 10.0 s.
- **Phase 6:** `--play-record` on the current walker checkpoint produces a clip under 10 MB (bitrate 4.5 Mb/s for 16 s, per the existing setting note).
- **All phases:** the `render=false` training path is untouched. Confirm with the default GPU smoke/hash gates on both backends after phases 1 and 3, the phases that touch `Context`/`Frame`.

## Risks

- Phase 1 touches every renderer's resize subscription. A missed subscriber shows up as a stale-size target after a window resize, not as a compile error. Grep `on_swap_chain_recreate` to zero.
- Other sessions edit the engine tree. A/B against a kept pre-change exe, and check that `.slang` files are untouched. This plan changes no shaders.
- The encoder's extent is fixed at `capture::init`. Recreating the offscreen output at a new extent (attached resize) needs the encoder recreated on the surface's resize event. Today the swapchain resize has the same gap. Phase 4 closes it for both.
