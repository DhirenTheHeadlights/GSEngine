# Frame demand: generic detection instead of declared demand

Successor to the reactive-cadence work in [power_efficiency_plan.md](power_efficiency_plan.md)
(items 1–3, landed 2026-09-09). That plan made the editor loop idle-capable and left the
question of *who raises demand* to a wake checklist. This plan removes the checklist.

Date drafted: 2026-09-13.

Primary sources:
[Bootstrap.cppm](../Engine/Engine/Source/Runtime/Bootstrap.cppm) (the reactive gate, 148–181),
[FrameDemand.cppm](../Engine/Engine/Source/Os/FrameDemand.cppm) (the demand token),
[Window.cpp](../Engine/Engine/Source/Os/GLFW/Window.cpp) (callbacks 749–870, `wake` 1498),
[Gui.cpp](../Engine/Engine/Source/Graphics/2D/Gui/Gui.cpp) (frame clear 228, submit 357–398),
[Task.cppm](../Engine/Engine/Source/Concurrency/Task.cppm) (`run_job` 911, `lane_loop` 1011, `spawn` 466),
[Viewport.cpp](../Editor/Editor/Source/Viewport/Viewport.cpp) (semaphore poll 273),
[Device.cppm](../Engine/Engine/Source/Gpu/Device/Device.cppm) (`wait_semaphore` 199).

## Symptom

The editor is smooth while the mouse moves and choppy the moment it stops. Mouse motion
refreshes the 150 ms interaction hold; with no input the loop falls to the 250 ms
`wait_events` floor, so anything that animates on its own renders at 4 Hz: the attached
game surface, hover fades, spinners, caret blink, profiler graphs.

## Diagnosis

Demand is raised today by exactly three things: the seven GLFW callbacks
(`request_interaction`, 150 ms hold), `window::wake` (one editor caller,
`Terminal.cpp:438`), and the initial redraw. Nothing in the GUI or any editor panel
raises demand. Every animation works only because the floor ticks four times a second.

The design asked each producer of visible change to declare it. That is the wrong
authority. A frame is `f(state, time)`; a new frame is worth producing only when its output
would differ from the last one. There are exactly three ways that happens, and each has a
detector that lives in infrastructure rather than at the producer:

| Cause | Today | Detector |
|---|---|---|
| An input to `f` changed off the main thread | 7 callbacks + 1 hand-placed wake | the hand-off substrate the write already passes through |
| The last frame was not a fixed point (an animation mid-flight) | 150 ms hold after input | compare this frame's GUI output with the previous one |
| A scheduled time arrives (blink, tooltip, debounce) | 250 ms floor | the earliest registered deadline sets the wait timeout |

The 150 ms hold and the 250 ms floor are both stand-ins for "I do not know when the next
change is". Once the three detectors exist, the hold is redundant and the floor becomes a
safety net rather than a cadence.

## What the substrate looks like (measured, not assumed)

**Off-thread hand-offs.** Every lane job on every lane passes through
`task::run_job` (`Task.cppm:911`): worker loop, io and background lanes, group steal-wait,
`when_all` resumption via `post_range` (`AsyncTask.cpp:217`). That single scope-exit covers
diagnostics (`Runner.cppm:189`), git (`GitStatus.cppm:320,405`), syntax highlight
(`SyntaxProducer.cppm:285`), search ranking (`Engine.cppm:353`), symbol index
(`Index.cpp:1548`) and HTTP/agent completions (`Http/Client.cpp`, `session::run`). Three
producers bypass it because they run on `task::spawn` threads or foreign callbacks: the
build worker (`BuildRunner.cppm:1964`, hand-off at `:1876` and `Spawn.cppm:129`), the
terminal worker (`Terminal.cpp:593`, already wakes), and the file-watcher callback
(`SearchSystem.cpp:66`). Off-thread scheduler hot-adds (`Scheduler.cpp:1098,1280`) flip
`all_settled` without a wake.

**GUI output.** One submit point: the sort-and-push tail of `gui::run`
(`Gui.cpp:357–398`). The buffers are `data::sprite_commands` and `data::text_commands`
(`Gui.cppm:150–151`); element types are `renderer::sprite_command` and
`renderer::text_command` (`UiRenderer.cppm:26–55`). They hold no pointers except
`text_command::text`, a `string_view` into a double-buffered pool (`Gui.cpp:230`), so two
frames compare equal by content, never by address. `quantity`, `vec`, `id` and the handles
already have `==`. The early-out at `Gui.cpp:254–260` submits nothing; it must leave the
settled flag untouched rather than compare against an empty frame.

**Fixed points.** `scroll_axis_advance` already snaps at 0.5 px (`Scroll.cppm:228`).
`animated_color` (`Types.cpp:373`) does not snap and approaches its target
asymptotically for on the order of a hundred frames before bit-equality. Spinner
(`Symbols.cppm:408`) and marquee (`Marquee.cppm:68`) are wall-clock functions and differ
every frame, which is correct: they should hold the loop hot while visible.

**Deadlines.** Two shapes. `interval_timer::tick` (`IntervalTimer.cppm:34`) drives the
profiler, alloc stats, live timers, explorer scan, loading screen and git refresh. Raw
`now` comparisons drive caret blink (`TextInput.cppm:531`, `TextArea.cppm:1408`), key
repeat (`TextInput.cppm:493`), tooltip delay (`Overlay.cpp:120`), hover info
(`CodePanel.cppm:2303`), edit debounce (`CodePanel.cppm:1429,2022`) and search debounce
(`QueryDriver.cppm:120`).

**Attached game surface.** Per game frame there is only the GPU timeline signal
(`Engine.cpp:450`); the pipe carries one handshake per session (`BuildRunner.cppm:2288`).
The editor polls `semaphore_counter_value` once per editor frame (`Viewport.cpp:273`).
`gpu::device::wait_semaphore` exists (`Device.cppm:199`) but has no timeout: Vulkan
hardcodes `UINT64_MAX` (`Vulkan/Device.cpp:2698`) and DX12 reuses the device-wide
`m_idle_event` (`Dx12/Device.cpp:1352`), which is not safe from a second thread.
`directx::wait_fence_for` (`DirectX.cppm:1408`) already creates a per-call event with a
timeout and is the right DX12 path.

**Layering.** `gse.concurrency` imports `gse.core`, `gse.diag`, `gse.log`, `gse.math`; it
cannot import `gse.os`, and `Window.cpp` imports `gse.concurrency`. So the demand token
must sit below concurrency and the OS-side post must be reached through it, not the
other way round. `FrameDemand.cppm` already imports only `gse.math` and `gse.time`.

## Design

### The token moves down and gains a deadline

`gse::frame_demand` moves from `gse.os` to `gse.time` (`:frame_demand` partition; its
imports already permit it). Its state becomes:

- `redraw_pending` — atomic bool, unchanged.
- `next_deadline` — atomic absolute time, folded with **min** (today's `hold_until` folds
  with max; it is deleted along with `request_frames` and `request_interaction`).
- `waker` — an atomic function pointer installed once by the window after creation and
  cleared at teardown. Replaces the `event_loop_live` check inside `window::wake`;
  `window::wake` becomes the installed waker and the only caller of `glfwPostEmptyEvent`.

API:

```
request_redraw()            one frame, from any thread; posts through the waker if installed
request_frame_at(time)      lower next_deadline to this absolute time
active() -> bool            redraw_pending
wait_budget(time floor)     min(next_deadline - now, floor), clamped at zero; consumes the deadline
consume_redraw()            unchanged
```

`wait_budget` is what the loop passes to `window::wait_events` instead of the flat
`idle_timeout`. A deadline in the past yields zero and the loop runs immediately.

### Detector 1: the hand-off substrate wakes

- `task::run_job` scope-exit (`Task.cppm:912`) calls `request_redraw()` when the job has no
  group (`entry.gp == nullptr`). Group jobs are awaited synchronously by the thread that
  posted them, so the main thread is not blocked in `wait_events` while they run; waking
  for them would be a syscall per ECS node per frame for nothing. Ungrouped jobs are
  exactly the fire-and-forget lane work whose results are polled from `frame()`.
- `scheduler` hot-add push (`Scheduler.cpp:1098,1280`) calls `request_redraw()`.
- `spawn::emit` (`Spawn.cppm:129`) calls `request_redraw()`. It is the shared line sink
  under the build worker and covers streaming output as well as completion.
- File-watcher callback (`SearchSystem.cpp:66`) calls `request_redraw()`.
- `Terminal.cpp:438` keeps its call, now spelled the same way.

Four sites, all inside infrastructure that every producer of its kind already flows
through. No panel or system mentions frames.

### Detector 2: GUI output fixed point

`gui::data` gains `previous_sprite_commands` and `previous_text_commands`. At the submit
tail of `gui::run`, after the stable sort and before the channel push:

- `settled = ranges::equal(sprite_commands, previous_sprite_commands) && ranges::equal(text_commands, previous_text_commands, text_equal)` where `text_equal` compares every field and `text` by content.
- If not settled, `frame_demand::request_redraw()`.
- Swap current into previous. Swapping keeps capacity; there is no per-frame allocation
  after warm-up, and the pool double-buffering already keeps the previous frame's views
  alive for exactly one frame, which is the lifetime the comparison needs.

`sprite_command` and `text_command` get defaulted `operator==`; `text_command` excludes
`text` from the default and the comparator supplies it.

The 3D world and the attached surface are both GUI sprites (`sample_scene_snapshot`,
`image_slot`), so an unchanged GUI over a changing world compares equal. That is correct:
world changes arrive through detector 1 or 3, not through this one.

`animated_color` snaps to `target` when every component is within `1/512` of it, so the
fade reaches a fixed point in a bounded number of frames instead of asymptotically.
`scroll_axis_advance` already snaps and needs no change.

### Detector 3: deadlines

- `interval_timer::tick` registers `request_frame_at(now + (m_interval - m_accumulated))`
  when it does not fire. A timer consulted during a frame is a statement that the caller
  wants to be running when it next elapses. Game code that ticks interval timers under
  `continuous` cadence pays one atomic min per tick and nothing else.
- The raw-`now` sites migrate to a `deadline_timer` in `gse.time` beside `interval_timer`:
  `arm(time from_now)`, `due() -> bool` (registers the deadline when not yet due),
  `disarm()`. Blink, key repeat, tooltip delay, hover info, edit debounce and search
  debounce are the seven callers. This is the behavioral-consolidation rule from the
  review guide: seven hand-rolled `now >= t` comparisons are one invariant.

### The attached surface wakes on its own signal

- `gpu::device::wait_semaphore` gains a `time timeout` parameter and returns whether the
  value was reached. Vulkan passes it to `waitSemaphores`; DX12 routes through
  `directx::wait_fence_for`, which owns a per-call event and so is safe off the render
  thread. The two existing callers pass an unbounded timeout.
- `viewport::imported_session` owns a `task::spawn` waiter started after the produced
  semaphore is imported (`Viewport.cpp:221`) and stopped in
  `destroy_imported_session` (`Viewport.cpp:39`). Its loop: wait for `last_seen + 1` with
  a bounded timeout, check the stop token, `request_redraw()` on success. The poll at
  `Viewport.cpp:273` is untouched; it now observes every game frame at most one editor
  frame late.

### The loop

`Bootstrap.cppm:167–170` becomes:

```
if (reactive && e.all_settled() && !frame_demand::active()) {
    window::wait_events(frame_demand::wait_budget(idle_floor));
}
```

`idle_floor` starts at 1 s (up from 250 ms). It remains the missed-wake safety net that
the previous plan chose over an unbounded wait, and the validation step measures whether
any producer still needs it. The `all_settled` gate stays.

The seven GLFW callbacks call `request_redraw()` instead of `request_interaction()`. A
drag or hover fade that outlives the event is caught by detector 2 on the next frame.

## Work items (ranked)

1. **Token.** Move `frame_demand` to `gse.time`; replace `hold_until` with `next_deadline`;
   add `request_frame_at`, `wait_budget`, the installed waker; delete `request_frames`
   and `request_interaction`; rewire `window::wake` and the callbacks; change the loop.
   Done when: the editor behaves exactly as today with a 1 s floor instead of 250 ms.
2. **Detector 2.** Previous-frame buffers, `==` on the commands, the compare-and-request
   at submit, the `animated_color` snap. Done when: a hover fade runs at full rate with
   the mouse still and the editor idles once it converges.
3. **Detector 1.** The four substrate sites. Done when: diagnostics, git status, search
   results, build output and index progress appear within one frame of completion with
   no input.
4. **Detector 3.** `interval_timer` registration, `deadline_timer`, migrate the seven
   sites. Done when: caret blink is exactly periodic and tooltips appear at their delay
   with no input and no floor tick.
5. **Viewport waiter.** `wait_semaphore` timeout, DX12 per-call event, the waiter.
   Done when: the attached game surface presents at the game's rate with the mouse
   still, on both backends.
6. **Floor.** Measure presents per second at idle with each detector on. Raise the floor
   again or remove it once no producer is observed riding it.

Items 2, 3, 4 and 5 are independent of each other and each depends only on 1.

## Review-guide check

- **Machinery with a caller.** Every atomic and every wake site is named above with its
  producer and its frequency. The per-job wake is gated to ungrouped jobs because the
  grouped path was read and found hot. The previous-frame buffers exist because the
  comparison runs every frame in reactive cadence; under `continuous` the gui skips the
  compare entirely.
- **No parallel abstraction.** `request_redraw` and `window::wake` are already the
  vocabulary; this removes two entry points (`request_frames`, `request_interaction`) and
  adds two (`request_frame_at`, `wait_budget`). `deadline_timer` consolidates seven
  hand-rolled copies rather than adding beside them.
- **Paired derivations.** "Is the picture static" is derived once, from the submitted
  commands, and nowhere else. No widget states it.
- **Branches for excluded states.** The gui early-out does not compare because there is
  nothing to compare; it is not a guard. `wait_semaphore` returning false is a real
  boundary (a driver wait) and the waiter checks it.
- **Units.** Every deadline, timeout and interval is `time`. The `1/512` colour epsilon is
  a dimensionless colour-space tolerance, not a physical quantity. The `0.5` scroll snap
  is pre-existing and in pixels.
- **Layering.** The token sits in `gse.time`, below `gse.concurrency`; the OS post is
  reached through an installed function, so no lower module imports a higher one.

## Risks

- **Comparison cost.** A few thousand commands per frame, each a flat struct compare. If
  a profile shows it, compare a running hash accumulated during `queue_sprite` /
  `queue_text` instead, which is the same authority computed incrementally.
- **Wall-clock widgets hold the loop hot.** Spinner and marquee differ every frame by
  design. Any panel that formats live elapsed seconds (`Chrome.cppm:342`,
  `Panel.cpp:491`) does the same while visible. That is the correct behaviour for
  something visibly animating; if it is not wanted, the fix is in the widget, not the
  detector.
- **Jobs that complete with nothing visible.** Detector 1 wakes for every ungrouped lane
  job, including ones whose result is not on screen. The cost is one extra frame, which
  then compares equal and idles. Acceptable; do not add a "visible" flag to jobs.
- **Waiter shutdown.** The waiter is a `jthread` with a stop token checked every timeout;
  it cannot hang on device destruction because `destroy_imported_session` stops it before
  retiring the semaphore.
- **Continuous cadence.** Nothing here runs in the game: the compare is gated on cadence,
  the wake calls are atomic stores whose reader is never waiting, and the callbacks
  already fire. Behaviour under `continuous` must be byte-identical; the locomotion hash
  gates cover it.

## Validation

- Idle editor, mouse still, nothing attached: presents per second falls to the floor and
  no lower; then to zero once the floor is removed.
- Hover a button and hold still: the fade completes at display rate and the loop idles
  afterwards. Same for a caret blink (exactly 2 Hz) and a tooltip (appears at 500 ms).
- Attach a running game, mouse still: editor present rate matches the game's frame rate
  on Vulkan and DX12.
- Trigger diagnostics, a build, a git refresh and a search with the mouse still: each
  paints within one frame of completion.
- Locomotion smoke gates unchanged on both backends.

## Non-goals

- Per-window demand for popouts; the primary window's loop serves all surfaces today.
- Removing the `all_settled` gate.
- Any change to how the game process presents; it already signals per frame.

## Status (2026-09-13)

Landed, editor build green, idle measured at about 0.06 cores with a spinner active and
about 0.03 without one (down from 1.175). Deviations from the text above:

- **Substrate wake discriminator.** Waking on every `run_job` exit produced a
  self-sustaining loop: the ungrouped `post_range` jobs that resume `when_all` coroutines
  in the frame itself woke the next frame. `job_entry::wakes` is set only by
  `submit_async` and `submit_async_to_lane` (the fire-and-forget `post*` family); grouped
  frame jobs and `post_range` never wake.
- **GUI compare is not gated on cadence.** It runs unconditionally in `gui::run`; under
  `continuous` the store it performs is never read, so the gate would have been a
  second copy of the cadence decision. The hash gates still need a rerun to confirm.
- **`window::wake` is gone.** The window installs `window::post_wake` as the waker and the
  seven GLFW callbacks call `frame_demand::request_redraw()` directly. Terminal's ring
  sink does the same.
- **Spinner is quantised.** `spinner_rotation` steps at 15° and registers the next step
  as a deadline (about 37 Hz), so a live spinner costs one frame per step rather than one
  per display refresh. `interval_timer::tick` registers its remaining interval the same
  way.
- **`wait_semaphore` is unchanged.** A separate `wait_semaphore_for(handle, value, time)`
  returning bool was added instead, because the device dispatch table is generated from
  the backend's member names and the timeline concept spells the two-argument form.
  Vulkan passes the timeout in nanoseconds to `waitSemaphores`; DX12 uses
  `directx::wait_fence_for`. The viewport waiter (`imported_session::produced_waiter`)
  waits in 100 ms slices so a stop request is honoured within one slice.
- **`deadline_timer` is non-consuming.** `due()` stays true once the deadline has
  passed until `disarm()` or a new `arm()`, so the tooltip and hover card can test it
  every frame without extra state; blink and key repeat re-arm on the true branch. The
  tooltip no longer accumulates `dt` (which could not advance while the loop was
  parked), and `query_driver::update` lost its `now` parameter. `text_area_state` still
  carries an unused `rpt_active`/`rpt_next` pair that predates this change.
- **Marquee stays wall-clock.** Quantising it to one pixel per step would register a
  deadline at the scroll speed in Hz, which is no cheaper than a display-rate frame; a
  visible marquee keeps the loop hot for exactly as long as it is on screen.
- **Hash gates pass.** 409600 steps, 1024 envs, 15 workers on the game exe built from
  this tree: Vulkan `AC3D5339…`, DX12 `F838905F…`, both equal to the reference.
- **Idle measurements.** Symbol indexing pins one background core for the first few
  minutes after launch (extract 154 s, save 52 s in one run); samples taken inside that
  window read 0.27 cores and are not the loop. After it publishes, the main thread sits
  at about 4 % of a core with the agent spinner live and the process at about 0.08.
- **Still open.** Hands-on checks of hover fade, caret blink, tooltip and an attached
  game on both backends.
