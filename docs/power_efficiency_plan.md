# Editor Power / Battery Efficiency Plan

The editor (`gse.ide`) is a long-running desktop app built on the game engine, so it
runs the engine's game loop for hours on a laptop. A game loop is designed to produce
frames continuously; an editor should sleep until something happens. This plan makes
the engine loop *idle-capable* as a first-class, per-app policy — **without changing
game behavior** — and removes the one background thread that would keep the CPU awake
even after the loop sleeps.

Guiding constraint: **game-engine-first**. Do not special-case the editor in the loop.
Add a cadence *policy* the game leaves at its default (`continuous`) and the editor opts
out of (`reactive`). Same loop body, one flag. Everything here is additive and
backward-compatible for game builds.

Primary sources:
[Bootstrap.cppm](../Engine/Engine/Source/Runtime/Bootstrap.cppm) (the loop),
[Engine.cppm](../Engine/Engine/Source/Runtime/Engine.cppm) (`engine_config`),
[Window.cpp](../Engine/Engine/Source/Os/GLFW/Window.cpp) / [Glfw.cppm](../Engine/Engine/Source/External/Glfw.cppm) (pump + unused wait/wake primitives),
[SearchSystem.cppm](../Editor/Editor/Source/Search/SearchSystem.cppm) / [FileWatcher.cppm](../Engine/Engine/Source/Search/FileWatcher.cppm) (the polling watcher),
[Main.cpp](../Editor/Editor/Source/Main.cpp) (editor entry).

Date drafted: 2026-07-19.

## Diagnosis

Audited six suspected drains. Only two are real; the rest are either already fine or
minor. Recorded here so a cold session need not re-investigate. A seventh, the task
pool's idle spin, was measured and fixed on 2026-09-09; it was larger than either of
the original two and this table previously cleared it.

| Area | Verdict | Evidence |
|------|---------|----------|
| **Render loop** | **Guzzle — dominant** | `Bootstrap.cppm:119` `while (!should_shutdown)` runs `tick_window → update → render` every iteration, unconditionally. No dirty gate, no redraw-on-demand, no FPS cap, no sleep-based pacing. The editor reuses this loop verbatim; `render_world=false` only drops the 3D world, GUI still renders every frame. |
| **Message pump** | Guzzle (CPU never blocks) | GLFW; `poll_events()` = `glfwPollEvents()` (non-blocking) once per frame at `Window.cpp:513,583`. `glfwWaitEvents{,Timeout}` / `glfwPostEmptyEvent` are imported at `Glfw.cppm:99-101` but have **zero call sites** — the blocking/wake path was anticipated and never wired. |
| **Present mode** | OK default, Vulkan footgun | Default **FIFO/vsync** both backends (`Context.cpp:29`). DX12 hardwired `Present(1,0)` (`Dx12/Swapchain.cppm:141`) — cannot free-run. Vulkan honors the persisted setting (`Vulkan/Device.cpp:160`), so Mailbox/Immediate = uncapped. Either way a full frame presents every refresh while idle. |
| **Timer resolution** | Clean | `timeBeginPeriod`/`timeEndPeriod`: zero occurrences. No global tick-rate inflation. |
| **Analysis / diagnostics** | Fine | 500 ms debounce + single-flight, active-doc only (`EditorApp.cppm:1833`). Not per-keystroke. |
| **Symbol / search index** | Fine | Event-driven, worker CV-blocked when idle (`Index.cppm:1061`), cache-incremental. |
| **Task pool idle spin** | **Guzzle — was #1, fixed 2026-09-09** | `worker_loop` burned a fixed `hot_spin_yields = 200` `std::this_thread::yield()` calls before parking on its semaphore. `Scheduler.cpp` `start_frame_tasks` posts a group every frame, so all 15 workers woke, drained, spun and parked at the frame rate. Measured on an idle editor: roughly 0.6 of 16 cores and ~34k context switches/sec, about 70% of the process CPU and ~74% of the machine's non-idle switches. On a busy box the same spin cost 248k switches/sec, because `SwitchToThread` only switches when another thread is runnable. The prior row's "task pool parks on a semaphore" was true but hid the spin in front of the park. |
| **File-watcher** | **Guzzle — #2** | Dedicated jthread: `sleep_for(500ms)` → full recursive `stat()` of the whole workspace tree, forever, even minimized (`SearchSystem.cppm:44-54`, `FileWatcher.cppm:211`). Polling, no OS notifications. |

**Physics of it.** Even vsync-capped, an idle editor burns ~60 full CPU-update +
GPU-render + present cycles per second doing nothing, and the watcher wakes a core
twice a second on top. Between them the CPU package never reaches deep C-states. The
watcher is currently *masked* by the always-on loop; once the loop sleeps (item 1), the
2 Hz stat storm becomes the new floor — so items 1 and 4 are complementary and both
required to see the win.

## Design: cadence policy + frame demand

Two engine additions, both reusable beyond the editor:

1. **`loop_cadence` policy** on `engine_config`. `continuous` (default — game behavior,
   unchanged) always runs the frame body. `reactive` runs the body only when there is
   *frame demand*, otherwise blocks the loop on the OS event queue via
   `glfwWaitEventsTimeout`.

2. **Frame-demand system** — a small engine-side object the reactive loop consults.
   Anything that needs a frame drawn raises demand; the loop renders until demand
   clears, then blocks again. Wake sources:
   - **Input / window events** — wake `WaitEventsTimeout` automatically (OS queue). Free.
   - **Animations** (cursor blink, smooth scroll, drag, fades) — request continuous
     frames for a duration via a token; blink can instead ride the `idle_timeout`
     heartbeat.
   - **Background threads** (diagnostics done, index updated, git status, file reload)
     — call `window::wake()` → `glfwPostEmptyEvent()` when they produce visible output.

The loop body is untouched; game builds never take the blocking branch. The primitives
(`glfwWaitEventsTimeout`, `glfwPostEmptyEvent`) already exist at `Glfw.cppm:99-101`.

Target reactive loop shape (illustrative, not final):

```
while (!should_shutdown.load(std::memory_order_acquire)) {
    if (cadence == loop_cadence::continuous || demand.active()) {
        e.tick_window();
        frame_sync::begin();
        e.update();
        if (config.render) e.render();
        frame_sync::end();
    } else {
        window::wait_events(idle_timeout);
    }
}
```

## Work items (ranked)

### 1. Loop cadence policy (foundational)

- Add `loop_cadence cadence = loop_cadence::continuous;` to `engine_config`
  (`Engine.cppm:30-45`) and a `loop_cadence` enum in the engine namespace.
- Add `window::wait_events(time timeout)` → `glfwWaitEventsTimeout` and `window::wake()`
  → `glfwPostEmptyEvent` next to `poll_events` (`Window.cpp:583`); declare in the window
  module interface. Out-of-line definitions per house rule.
- Branch the loop at `Bootstrap.cppm:119` per the shape above. Keep the trace scope
  guards. In the blocking branch, do **not** run update/render/present.
- Choose `idle_timeout` (heartbeat even with no events) — start ~250 ms so a
  time-driven cursor blink stays responsive; tune later.
- Done when: a `continuous` app is byte-for-byte behaviorally identical, and a
  `reactive` app with nothing happening blocks in `wait_events`.

### 2. Frame-demand system (engine-side, reusable)

- New engine object (e.g. `gse::frame_demand`): `request_redraw()` (one frame),
  `request_frames(time duration)` / a scoped animating token (hold hot while active),
  `active()` (consulted by the loop). Thread-safe — background threads and input
  callbacks both raise it.
- Wire GLFW input/window callbacks (key, mouse, move, resize, focus, scroll) to
  `request_redraw()` so interaction always paints at least one frame.
- `window::wake()` should also raise demand so a posted event actually renders, not just
  unblocks the loop.
- Done when: pressing a key / moving the mouse in a reactive app renders exactly the
  frames needed and then re-idles.

### 3. Editor opt-in + wake wiring (the correctness surface)

- Set `cadence = reactive` in the editor config (`Main.cpp:21-28`).
- Default the editor's present mode to FIFO explicitly (closes the Vulkan
  Mailbox/Immediate footgun; mostly moot once reactive).
- **Wake checklist — this is where reactivity breaks if missed.** Every background
  producer of visible output must `window::wake()` (or raise demand) on completion, or
  its result won't paint until the next input and the editor will look frozen:
  - diagnostics/lint results ready (`DiagnosticsSystem` / `Runner`)
  - symbol index / search index updates (`Index`, `SearchSystem`)
  - git status refresh (`gse.ide.git`)
  - external file change → document reload (`Workspace` watcher)
  - terminal / build-runner output arriving (`Terminal`, `BuildRunner`)
  - IPC / attached-surface messages
- Animations must hold demand for their duration: cursor blink, caret movement,
  smooth scroll, drag operations, tab/panel transitions, loading spinners.
- Done when: typing, scrolling, diagnostics appearing, git refresh, and terminal output
  all update promptly, and the editor idles to ~0 fps when untouched.

### 4. File-watcher → OS notifications (unmask the win)

- Replace the search watcher's `sleep_for(500ms)` + full recursive `stat()`
  (`SearchSystem.cppm:44-54`, `FileWatcher.cppm:211`) with `ReadDirectoryChangesW`
  (event-driven, zero polling). The change event doubles as the `window::wake()` source
  for item 3. No game tension — the game does not run this watcher.
- Interim if the OS-notification port is deferred: back the poll interval off hard when
  the window is unfocused (500 ms → 2–5 s) and skip entirely when minimized.
- Done when: an idle editor issues no periodic filesystem scans, and external edits
  still refresh the index/open docs.

### 5. Minimized + cleanup (mostly falls out of items 1–4)

- Under reactive cadence, minimized + idle already blocks in `wait_events` instead of
  spinning `update()`. Confirm no code path forces demand while minimized.
- Verify the existing undersized-viewport guard still holds when a background `wake()`
  fires while minimized (see Risks).

## Correctness surface / risks

- **Missed wakes (highest risk).** Any background result not paired with `window::wake()`
  will not render until the next input — looks like a hang. Item 3's checklist is the
  mitigation; treat it as the acceptance gate.
- **Frame-count assumptions.** Reactive cadence means variable / zero frame rate. Any
  logic that advances by frame *count* rather than elapsed `time` (dt) will misbehave.
  Audit for it; everything should be dt-driven (the units system makes `time` deltas
  explicit).
- **Input latency for continuous gestures.** Drag, smooth-scroll, and momentum must
  request continuous frames or they stutter. First-key latency is fine (input wakes the
  queue), but sustained motion needs held demand.
- **Minimized / divider-ratio interaction.** There is prior art of the update tier
  running while minimized (viewport → 0) corrupting persisted split ratios, fixed with
  an undersized-viewport guard. Reactive cadence removes most minimized updates, but a
  background `wake()` can still tick one frame while minimized — the guard must remain
  in force. Re-test alt-tab / minimize with dividers after item 1.
- **profiler / trace per-frame ingest.** `finalize_frame` / `ingest_frame`
  (`Bootstrap.cppm:185-189`) currently run every iteration; under reactive idle they run
  far less often. Confirm nothing depends on their steady cadence.

## Validation

- **Present rate at idle** (PresentMon or the in-engine profiler): idle editor should
  drop to ~0 presents/sec, not vsync rate. This is the single headline metric.
- **Deep sleep:** `powercfg /sleepstudy` should show the CPU package reaching deep
  C-states with the editor open and idle.
- **Power draw:** Task Manager "Power usage" column for the editor process idle vs.
  active; ideally before/after battery-drain estimate on the Galaxy Book 5 Pro.
- **Responsiveness regression pass:** run the wake checklist manually (type, scroll,
  trigger diagnostics, save an externally-changed file, run a build, watch git status
  refresh) and confirm each paints promptly.

## Non-goals / already fine — do not "fix" these

- Diagnostics/lint debounce and symbol-index CV-blocking — both already power-friendly.
- Task-pool semaphore parking was listed here as fine until 2026-09-09. The park itself
  is fine; the 200-yield spin in front of it was not, and it outweighed both original
  guzzles. See the diagnosis table. Do not re-clear a wait path by reading only the
  blocking call at the end of it.
- `timeBeginPeriod` — never set; leave it that way.
- DX12 present path — already vsync-locked and cannot free-run.
- The two-tier update/frame model and `frame_scheduler` — the scheduler is a work
  amortizer, not a pacer; leave it.

## Status

**Items 1–3 landed (2026-09-09).** Items 4 and 5 remain. Suggested branch: continue on
`editor` or a dedicated `power` branch off it.

Measured on the settled idle editor (300 s settle, 25–30 s sample, Ryzen 7 7800X3D):

| Build | Cores | Switches/sec | GPU |
|---|---|---|---|
| Before either fix | 1.175 | ~35,000 | 4.2% |
| + task-pool spin fix | 0.82 | 19,926 | — |
| + reactive cadence (items 1–3) | **0.03** | **872** | **0.11%** |

Per-worker switch rate fell from ~1,220/sec to ~35/sec, which is the ECS per-frame group
post dropping from 144 Hz to the 4 Hz floor. The editor's own log corroborates the
cadence directly: the previous continuous run recorded 96,717 presents in ~11.5 min
(~140/sec); the reactive run went 237 → 600 presents over 89.7 s = **4.05 presents/sec**,
i.e. exactly the 250 ms floor with no input.

Two benign consequences worth knowing before item 4:

- `present pacing degraded ... dt now driven by snapped CPU loop timing` now appears
  within a minute of launch. At 4 fps there are too few presentation-feedback samples to
  drive display timing, so `system_clock` falls back to snapped CPU deltas. Correct for a
  dt-driven UI; do not "fix" it.
- Startup `STALL [phase=init]` lines are **not** a reactive regression — they appear with
  the same systems and timing (~2.9 s vs ~3.15 s) in the preceding continuous run.

**One deliberate departure from the spec above: the loop always renders one frame after
the wait returns, rather than looping back to block whenever demand is still clear.** The
spec's shape (`if (!demand) continue;`) makes a missed wake indistinguishable from a
hang — the failure mode the Risks section calls the highest risk. Rendering
unconditionally after `wait_events(250 ms)` turns the same bug into a 4 fps floor: any
producer that forgets to wake is at most 250 ms late, never stuck. That reduces the wake
checklist from an acceptance gate to a latency optimisation, which is what made it safe
to ship items 1–3 together without an interactive responsiveness pass. It costs 4 idle
frames/sec against a theoretical 0.

What landed, against the spec:

- `loop_cadence` enum + `engine_config::cadence` (`Engine.cppm`), `reactive` opted into by
  the editor only.
- `window::wait_events(time)` / `window::wake()` (`Window.cppm` / `Window.cpp`). `wake()`
  raises demand *and* posts, and no-ops the post before the window exists / after it is
  torn down, guarded by `window::event_loop_live`.
- `gse.os:frame_demand` — `request_redraw`, `request_frames(time)`, `request_interaction`,
  `active`, `consume_redraw`. It lives in `gse.os`, not `gse.runtime`, because
  `gse.os:window` cannot import `gse.runtime` without a cycle.
- All 7 GLFW callbacks call `request_interaction()`, which holds demand for 150 ms rather
  than requesting a single frame. This covers the spec's "animations must hold demand"
  bullet generically — hover fades, smooth scroll, drag momentum and press animations all
  get a full-rate window after the last input instead of each needing its own token.
- The blocking branch is additionally gated on `e.all_settled()`, so startup never runs at
  the idle floor while systems are still coming up.
- Wake wiring: `terminal::ring_sink::push` (`Terminal.cpp`) covers every logged line, so
  terminal output, build-runner output and agent output stream at full rate. The
  remaining checklist entries (diagnostics, symbol/search index, git status, workspace
  reload, IPC) are all polled from the update tier and land within the 250 ms floor;
  they are worth waking only if they measure as perceptibly late.
- Present mode needed no change: `window::data::present_mode` already defaults to `fifo`
  and the editor's saved `Editor.ini` has `present_mode = fifo`. Only `d.attached` (the
  game rendering into the editor) selects mailbox.

**Task pool idle spin fixed (2026-09-09).** Independent of items 1–5 and already landed
in `worker_loop`. The per-worker spin budget now halves on each failed spin, floored at
`idle_spin_yields = 4`, and resets to `hot_spin_yields` when a spin finds work, so a hot
pool keeps its full 200-yield latency window and an idle one stops burning for it. Item 1
would also have hidden this in the editor by ending the per-frame wakeups, but the spin
cost applies to every continuous-cadence game and training build too, where the loop must
keep running. The two are complementary, not alternatives.
