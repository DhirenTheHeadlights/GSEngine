# Scenario Runner — Scope of Work

> **Status:** Phases 0 and 1 complete and verified by running them. The determinism gate passed on 2026-08-06 — five runs, one hash, and a long run that diverges (see Phase 2). Design reviewed against [CODE_REVIEW_GUIDE.md](CODE_REVIEW_GUIDE.md). Two consumers drive this: repeatable performance measurement (replacing "play for a bit, then read the profile"), and deterministic footage capture for the README clips listed in `README.md`.
>
> The review pass added D9 (recording capacity derives from the frame budget) and D10 (the context grants no authority beyond a channel writer), and tightened D2, D7, and D8. D10 is the load-bearing one: it removes direct engine access from the scenario API, which is what kept the plan from reintroducing the spawn race that `scene_command` deferral was written to fix.

Authoring and running scenarios is documented separately in [scenario_authoring.md](scenario_authoring.md). This document keeps the design rationale and the decision record.

## Current State

**Done and verified by execution:**

- The bench harness: `bench_config` on `engine_config`, and `begin_bench`/`step_bench`/`finish_bench` in `Runtime/Bootstrap.cppm`. Scene activation → populated-world settle gate → warmup → measurement → `.gsprof` + text summary → clean exit. Runs headless in under a second. A 6000-frame settle cap logs at error level and shuts down rather than hanging.
- `profile::build_report_file`, `set_recording_capacity`, and a `reset()` that also clears the recorded ring (it had no call sites before, so nothing else changed behaviour).
- `Diag/ProfileSummary.cppm`: percentile analysis over a `report_file`, one authority shared by the in-process summary and any future editor diff.
- `engine::world_state_hash()` and `engine::world_populated()` in `Runtime/Engine.cpp`, digesting transform and motion components through `binary_writer`'s reflection walk.
- `Scenario/Scenario.cppm` — the `gse.scenario` module: `info` annotation, `context`, wait gate and awaitables, reflection sweep, `find`.
- `parse_args` warns on unrecognized arguments instead of ignoring them silently.

**Phase 1, built and run:**

- `bench_config` carries `scenario` (the name, so artifacts are named after it) and `scenario_body` (the resolved fn-ptr). The fn-ptr has no `parser<>` specialization, so `parse_args` skips it and no phantom flag appears for it.
- `drive_scenario` in `Bootstrap.cppm`: starts the coroutine on the first bench step, advances `context::frame()` once per loop iteration, and services the wait gate. Every resume — including the first — goes through `async::track_frame` + `async::resume_checked`, and a refused resume is logged at error level rather than silently ending the script.
- `bench_world_ready` is now the single settle predicate, shared by the settling phase and `wait_settled`. It was previously written out twice, once in `step_bench` and once (implicitly) in the trap note below.
- `sandbox::scenarios::physics_stress` in `Sandbox/Source/Sandbox/Scenarios.cppm` — the first scenario body.
- `Sandbox/Main.cpp` runs the sweep application-side and resolves the name before `gse::start`.

**Working invocation** (headless, from the Sandbox build directory, with the GCC runtime on `PATH`):

```
./Sandbox.exe --engine-bench-scenario physics_stress
```

The scenario's annotation supplies the scene, the render mode, and both frame budgets, so nothing else is required. Every one of those is still overridable on the command line — `apply_scenario` writes the annotation's value only where the parsed config still holds `bench_config`'s default, so an explicit `--engine-bench-frames 900` survives. The Phase 0 invocation still works unchanged for scenario-free runs:

```
./Sandbox.exe --no-engine-create-window --no-engine-render --engine-bench-enabled --engine-bench-scene Sandbox --engine-bench-warmup-frames 30 --engine-bench-frames 240
```

A run takes 5.1 s wall at 120 measured frames, most of which is the ~1000-body workload; the settle gate clears in about half a second.

**Next:** Phase 2 proper — `ScenarioTuning` state (D2) and the VBD / Forward+ / locomotion scenarios. A GPU-solver scenario is the one that closes risk 3.

## Traps Already Paid For

Each of these cost a build-and-run cycle. Do not rediscover them.

- **Negative boolean flags read backwards.** `build_arg_flag` prefixes the *fully qualified* name, so headless is `--no-engine-create-window`, never `--engine-no-create-window`. The natural spelling silently does nothing.
- **`all_settled()` is not "the world exists."** It reports scheduler quiescence and goes true long before scene content spawns. The gate requires `all_settled() && world_populated()`. Both the settling phase and `wait_settled` now route through `bench_world_ready`, so there is one copy of that predicate — do not re-spell it at a third call site.
- **`world_loader_setup` registers a scene; it does not activate one.** Nothing in a headless or bench run activates it, so without `--engine-bench-scene` the registry stays empty and every measurement is of nothing.
- **Headless disables `gui::data` and `window::data`.** Any app-side system reading them must be registered behind the render flag or the closed-graph assert fires at boot. `Sandbox/Main.cpp` now guards `crosshair`, `client_ui`, `pause_menu`, and `popout_system`.
- **`report_frame::span` is not the frame duration.** It spans from the earliest node start, including long-lived open spans, and reported ~1000× high. Frame percentiles derive from the frame's root node instead (`profile::frame_duration`).
- **The coroutine frame allocator is safe.** `frame_arena` is a thread-local free-list pool, not a per-frame bump allocator; see risk 1. Resume only through `async::resume_checked`.
- **An annotation reflection is not spliceable; `[:ann:]` is an error.** Read the value with `[:std::meta::constant_of(ann):]`, the form `Annotations.cppm` uses. This survived a full build undetected because `registry<Ns>` is a template and nothing had instantiated it — "compiles clean" for an uninstantiated template means only that it parsed.
- **Entity identity is the hash of the name, so a spawn helper with fixed names can only ever build one instance.** `registry::create(name)` goes to `generate_id(name)` which is `stable_id(name)` — FNV over the string. `spawn_character` used `"Character"`, `"Character.Proxy"`, `"Character.{bone}"`, so spawning it eight times put every character on the same ids, each stomping the last. The symptoms pointed everywhere except the cause: p50 stayed flat because only one character's worth of entities existed, `count=1` and `count=2` produced *identical* world hashes, and eight overlapping spawns went non-deterministic. Any spawn helper called more than once needs an index in its names, as `spawn_tumbler` already does. The bisect that found it needed no rebuild — the count is a project-scoped tunable, so `Sandbox/Config/settings.ini` plus a re-run walks it.
- **`wait_settled` does not cover asset residency.** It waits for scheduler quiescence and a populated world, neither of which implies a queued asset has resolved. `spawn_character` opens with `if (!model.valid()) return {};`, so a scenario that pushes a spawn the instant the world settles gets a silent no-op, and the run then measures an empty scene while producing a perfectly valid deterministic hash. F7 hides this in the real game because a human presses it seconds after boot. Proven by comparing against a no-scenario run of the same scene: identical hash meant nothing spawned; a two-second wait before the push changed the hash and the character appeared. The wait is a stopgap for a missing mechanism — D10's `wait_until` is what lets a scenario state the precondition instead of guessing a delay.
- **The headless/render asset split is authoring versus consuming, not a different loader set.** Headless used to register `model` only while render registered `graphics::asset_types + audio::asset_types`, so `spawn_character` asserted headless and `locomotion` could not run. Two narrower corrections failed first and are worth not repeating: the **whole** render pack worked but put 162–401 ms `slow frame` spikes inside the measurement window, and a `model, clip_asset, skinned_model` subset broke `physics_stress` outright, because `gse::material` holds a `handle<texture>` and both model types gate readiness on `material().textures_ready()` — the model family reaches textures, so a pack containing models without textures cannot load. The loader *set* was never the difference. `register_loaders` also installed the stale-recompile pre-load hook that the hand-rolled `add_loader<model>` never had, and baking a stale asset mid-run is what the spikes were. Both branches now register the same pack; render adds `install_recompile_fns`, headless adds `install_stale_checks`, which fails the load rather than baking. Registering a loader is just a factory in a map, so the wide set costs nothing — authoring is what costs, and headless does not author.
- **A renderer setting can overwrite a programmatic one.** `renderer::run` pushed `profile::set_enabled` and `set_frame_recording` from its own state *every frame*, so it clobbered `begin_bench` on frame one and a windowed scenario recorded nothing. Headless never noticed because it has no renderer. Both now push only on change, matching the hot-reload toggle immediately above them. Any other per-frame `set_*` from system state has the same hazard.
- **A windowed scenario cannot run unattended.** Render-mode world systems are deferred behind `m_loading.rendered_once()`, so the window must actually present a frame before the scene populates. Launched back-to-back with no one touching them, roughly half the runs never presented: the settle cap aborted cleanly at 6000 frames with `populated=false` after about 40 s. Confirmed by the opposite case — the runs that *did* complete completed because the window was clicked into focus. That makes a windowed scenario unusable as an unattended benchmark on two counts at once: it may not boot without a human, and the click that makes it boot is real input into a scenario that is supposed to be scripted, so the resulting hash is not a baseline. Headless has no such gate and cannot be touched. Moving a scenario to windowed to dodge the headless asset gap therefore does not work; the asset gap has to be fixed.
- **A windowed scenario accepts real input.** Clicking or moving the mouse over the window feeds the same event stream the scenario drives, so it can perturb the world. Headless runs are immune. Do not touch the window during a `render_stress` measurement.
- **A valid hash does not mean valid timings.** A run taken while a compile was in flight returned p95 232 ms and p99 531 ms against a normal 13 ms / 17 ms, and its world-state hash was bit-identical to the quiet-machine runs. That is the fixed-step clock working as designed — world state keys off frame index, not wall time — but it means the determinism check cannot detect a contaminated measurement. Confirm no build or editor compile is running before recording any number as a baseline; `tasklist | grep -E "cc1plus|ninja"` settles it.
- **A scenario body cannot live in the partition interface.** The annotated declaration stays in `Scenarios.cppm`; the coroutine body goes in `Scenarios.cpp` (`module sandbox:scenarios_impl;`), matching the existing `Client.cppm` / `Client.cpp` split. The sweep only reads the declaration, so nothing is lost.
- **A tripped assert does not terminate the process.** It logs `[fatal]` and hangs. Any unattended bench run needs an external timeout until that is fixed.

## Overview

A **scenario** is a named C++ coroutine that drives the engine through a fixed, reproducible sequence of world events on a virtual clock. Run the same scenario twice and frame *N* holds the same world state both times, so the wall-clock cost measured at frame *N* is comparable across runs, builds, and machines.

The same mechanism produces trailer footage: because simulated time advances by a fixed step per frame regardless of how long the frame took to render, a scenario recorded at a locked virtual 60 fps yields smooth 60 fps video even when the renderer is delivering 22.

One feature, two consumers. Nothing here is specific to either.

---

## Goals

- `Sandbox.exe --engine-bench-scenario vbd_ragdolls` runs a fixed workload and exits with a `.gsprof` and a percentile summary, named after the scenario.
- Rerunning a scenario produces the same world state at the same frame indices.
- A scenario declares whether it needs a window and renderer, or runs headless like the existing `--physics-parity` path.
- Scenarios are selectable and runnable from the editor, with the resulting profile loaded into the Profile tab and diffed against a stored baseline.
- A scenario can emit a lockstep image sequence suitable for encoding into a README clip.
- Adding a scenario requires no edit to any central dispatcher.

## Non-Goals

- A scripting language. See D1.
- Cross-machine performance comparison. Baselines are per-machine.
- Determinism of GPU floating-point results across vendors or drivers.
- Replacing the existing `--physics-parity` mode. Scenarios generalize it; parity can migrate later or not at all.
- Live cross-process profile streaming. The transport is a file, as it is today.

---

## The Determinism Argument

`system_clock::set_fixed_step_override(steps)` ([SystemClock.cppm:258](../Engine/Engine/Source/Time/SystemClock.cppm:258)) replaces `delta_time` with `const_update_time * steps` and discards wall time entirely. With `steps == 1`, frame *N* contains exactly *N* × 16.667 ms of simulated world no matter how long the frame took.

This is the whole foundation:

- **For benchmarks** it makes the *inputs* deterministic while leaving *timing* free-running. The world state under measurement is identical run to run; only the cost of producing it varies, which is the quantity of interest. Physics substep counts stop varying with frame cost, removing the largest source of run-to-run noise.
- **For capture** it decouples content rate from render rate. Every captured frame is exactly 1/60 s of content, so a scene that renders at 22 fps still yields a smooth 60 fps clip. Without this, README footage of the heavy scenes (VBD ragdolls, Forward+ with hundreds of lights) is unusable.

`--physics-parity` already relies on this ([Main.cpp:50](../Sandbox/Sandbox/Source/Main.cpp:50)). The scenario runner generalizes it.

---

## Integration Surface

| Concern | Where it lives today |
|---|---|
| Deterministic clock | `system_clock::set_fixed_step_override` |
| Reflection system sweep to copy | `register_systems<^^ns>`, `system_manifest<...>::register_with` ([SystemManifest.cppm](../Engine/Engine/Source/Ecs/SystemManifest.cppm)) |
| POD fn-ptr thunk table precedent | `settings::settings_field` ([Settings.cppm:90](../Engine/Engine/Source/Ecs/Settings.cppm:90)) |
| Tunables → settings page + ini + hot reload | `[[= describe]]` on a system state field; `push_annotated_field_change` ([SystemManifest.cppm:453](../Engine/Engine/Source/Ecs/SystemManifest.cppm:453)) |
| Deferred safe-point mutation | `channel_writer`, `annotated_change_request<State>` |
| CLI args by reflection | `gse::parse_args<Config>` ([Args.cppm](../Engine/Engine/Source/Meta/Args.cppm)) |
| "run, emit artifact, exit" precedent | `--dump-system-graph-path` ([Bootstrap.cppm:189](../Engine/Engine/Source/Runtime/Bootstrap.cppm:189)) |
| Perf artifact + reader | `profile::dump_report` / `load_report` → `.gsprof` |
| Editor consuming a game profile | `profile_report_request`, `profile_source::game` ([Profile.cppm:90](../Editor/Editor/Source/Profile/Profile.cppm:90)) |
| Editor spawning the game | `build_runner` + `gse.ide.build:spawn`, shared-surface viewport attach |
| Synthetic input | `input::event` variant; `engine::push_attached_input` already fed by the editor pipe |
| World-ready signal | `engine::all_settled()` |
| Binary asset serialization | `binary_writer` / `binary_reader` ([Archive.cppm](../Engine/Engine/Source/Containers/Archive.cppm)) |
| Screenshot path | `capture` renderer F9, validated |

---

## Locked-In Decisions

### D1. Scenarios are C++ coroutines, not a scripting language

A scenario is `auto run(scenario_ctx&) -> async::task<>`, sequenced with `co_await`.

A text DSL was considered and rejected. The rejection turns on separating **control flow** from **data**:

- Control flow in C++ keeps the debugger, compile-time errors when a settings field is renamed, and the `gse.math` unit system. A DSL forces physical quantities through strings, which the style guide's "never represent physical quantities with raw float/double/int — this applies everywhere" rule exists specifically to prevent.
- The iteration-speed argument for a DSL was really an argument about *data* — camera paths and tuning values — which D2 and D3 handle without a language.
- The cost of a DSL is not the parser. It is parse diagnostics with source locations, editor completion, syntax highlighting, grammar versioning, and documentation: a permanent product surface for a single user.

Coroutines are already the engine's async idiom, so this adds no new concept.

### D2. World tunables ride the existing settings registry

A scenario must not hardcode magic numbers. **World** parameters — what the scenario builds and how the simulation is configured — live on an annotated struct:

```cpp
struct [[= system_state<"ScenarioTuning">{}]] data {
    [[= describe("Ragdolls spawned by the VBD stress scenario"), = range<1, 512>{}]]
    int ragdoll_count = 128;

    [[= describe("Seconds of sun arc covered by the atmosphere scenario")]]
    time sun_arc_span = seconds(20.f);
};
```

That annotation alone yields a settings page, `.ini` persistence, hot reload, and a `category.key` string handle — all through machinery that already exists.

**The scenario does not read the value; the owning system does.** This clause originally said the scenario reads it, which D10 then made impossible — a scenario suspended across hundreds of frames may not hold a reference to any system's state, and reading one inside the coroutine is exactly the borrowed-reference hazard D10 exists to forbid. The resolution keeps both decisions intact: the tunable lives on the state of whichever system applies it, the scenario pushes a request that names *what* to build, and the owning system reads its own tunable for *how much*. So `ScenarioTuning` is not a new system state; `ragdoll_count` belongs on the spawner that consumes `spawn_ragdolls_request`.

This is better than a scenario-side read on its own merits. The tunable is then also reachable from the settings page during ordinary play, one authority serves both, and a hot reload mid-run cannot silently change what is being measured — the value is read where and when it is applied.

**Reading settings without writing them.** A scenario run sets `persist_settings = false`, which previously suppressed the project settings path as well as the user `.ini` auto-save, so a scenario read no settings at all and its tunables were inert. Those are now separate: the project path is always set, and `persist_settings` gates only auto-save. Workload sizes are annotated `scope<scope_kind::project>`, because a workload size is project configuration that should be checked in and shared, not a personal preference.

The path itself is one derivation. `config::project_settings_path()` answers it for the running process, and both it and the editor — which resolves an *opened* project rather than the process one — build it through `config::project_settings_path_for(root)`, so the `Config/settings.ini` layout is stated once. Before this, the editor computed that string itself and a standalone game had no project settings at all, which meant one binary on one project was configured differently depending on who launched it.

The write side needed the same separation and did not have it. `engine::shutdown` calls `save_now()` unconditionally, and `save_all` wrote the project file whenever a project path was set — so merely setting the path would have made every bench and parity run rewrite the project's checked-in settings. `m_auto_save` is now the single gate on writing, consulted by `save_all` itself rather than by the destructor alone, so every writer (shutdown, restart, the GPU backend fallback) inherits it. Setting a path means "read this"; persistence is what means "write it".

**Runner** parameters are not tunables. Frame budget, warmup length, and render mode are properties of the scenario's identity, not user preferences, so they live on the scenario annotation (D7) with the command line as the only override. The boundary is: if changing it changes *what is measured*, it is a world tunable; if it changes *how the measurement is taken*, it belongs to the scenario.

### D3. Camera paths are data, authored in-editor

A camera path is a reflection-serialized asset (`binary_writer`), not source. Authoring flow: fly the free camera in the editor, drop keyframes, save. A scenario names the path asset and `co_await`s its playback.

This is the piece that actually motivated wanting a DSL, and it is better served by live authoring than by editing text and re-running.

### D4. Virtual frame index is the only time base

Inside a scenario, `system_clock::now()` and wall time are off limits. Everything keys off `scenario_ctx::frame()`, a monotonic counter incremented once per engine update after the scenario system starts. `co_await wait_frames(n)` and `co_await wait(seconds(2.f))` (converted to frames at `fixed_dt`) are the only waits.

The runner asserts `set_fixed_step_override` is active for its whole lifetime.

### D5. Measurement starts after an explicit warmup gate

Cold-start cost varies enormously — shader compilation, asset baking, allocator warmup. The sequence is fixed:

1. Wait for `engine::all_settled()`.
2. Run `warmup_frames` (default 120) with profiling active but discarded.
3. `profile::reset()`.
4. Run the scenario body.
5. `profile::dump_report(path)`, emit the percentile summary, exit.

`profile::set_frame_recording` is currently enabled only in attached mode ([Bootstrap.cppm:100](../Engine/Engine/Source/Runtime/Bootstrap.cppm:100)); a bench run must force it on.

### D6. Render mode is declared per scenario, read before `engine_config` is built

Sim-only scenarios (solver throughput, broad-phase scaling) run `create_window = false, render = false` like `--physics-parity`. Render scenarios run windowed. The scenario annotation carries the flag; because the registry is a compile-time table, the selected name is resolved *before* `gse::start`, and its declared mode populates `engine_config`.

The selection flag is `--engine-bench-scenario`, not `--scenario`, because the name lives on `bench_config` and every bench flag is derived from that struct by reflection. A hand-written `--scenario` on the application config would be a second home for one fact and would need its own plumbing into `bench_config` anyway. `headless` only ever *forces* headless: a scenario that declares `headless = false` leaves `create_window` / `render` at whatever the command line said, so `--no-engine-render` on a render scenario is still honoured.

### D7. The registry is a POD fn-ptr table built by reflection sweep

Scenarios are discovered by sweeping a namespace for functions carrying the annotation, exactly as `hook_fns_in_namespace` does for system hooks. The resulting table stores **plain function pointers and captureless thunks only**.

This is a hard constraint, not a preference: `std::function` / `std::move_only_function` in a module partition that another module loads produces "Bad file data" on this toolchain. `settings_field`'s thunk members are the pattern to copy.

The annotation is a plain aggregate with a `char` array, matching `view_label` in [Profile.cppm:7](../Editor/Editor/Source/Profile/Profile.cppm:7) — not an NTTP-parameterized class template, which would give each scenario a distinct type and put the `gse.meta` helpers out of reach:

```cpp
namespace gse::scenario {
    struct info {
        char name[64];
        char scene[64] = "";
        bool headless = false;
        int warmup_frames = 120;
        int frames = 600;
    };
}
```

`scene` was added during Phase 1. Without it, running a scenario also requires `--engine-bench-scene Sandbox` on the command line, which is the second independently-declared copy of a fact the scenario already knows — exactly the defect D9 names one paragraph later, and it makes the goal "`--engine-bench-scenario physics_stress` runs a fixed workload" unreachable. Which scene a scenario measures is part of its identity, so it belongs on the annotation with the command line as the override.

The annotation is `gse::scenario::info`, not `gse::scenario`. Earlier drafts of this document wrote `[[= gse::scenario{...}]]` alongside `gse::scenario::context`, which cannot both exist — one name cannot be a struct and a namespace. The namespace wins, matching `settings::describe` and `system_state`.

The sweep reads annotations **only through the `gse.meta` helpers** (`has_annotation`, `first_annotation_of_type`). A hand-rolled `template for` over `annotations_of` is a reimplementation of those helpers and is rejected regardless of whether it works; `std::define_static_array(std::meta::annotations_of(...))` is banned outright, because two entities sharing an unqualified name and annotation shape collide in that static storage and silently return each other's data.

`entry` (below) is **derived only** — populated from the annotation at table construction and never written elsewhere. It is a projection of one source of truth, not a second copy of it.

The table is static, dense, and never removes, so its indices are themselves stable — a plain `std::span<const entry>` is correct and `id_mapped_collection` would be machinery with nothing in it. Runtime identity is `gse::id`: the CLI resolves `--scenario=<name>` to an `id` once at startup, and nothing string-compares after that.

### D8. Perf artifact is `.gsprof`; the metric is percentiles, not EMA

`report_entry::ema` and `peak` are too noisy for regression detection. `report_file::recorded` already holds per-frame node DAGs, so p50/p95/p99 of any tagged span is computable from a completed run. The runner emits both the raw `.gsprof` and a text summary.

Percentile selection stays **inside `time_t`** end to end. Quantities are ordered, so `std::ranges::nth_element` operates on them directly, and a ratio of two same-dimension quantities is already dimensionless. There is no external contract at any point in this path, so there is no legitimate `.as<Unit>()` in it. Output goes through the quantity format spec — `{:>10.2f:us}` paired with a `{:>13}` heading, exactly as the existing profile dump does — never a hand conversion with the unit baked into the surrounding text.

Runs land at `bench/<scenario>/<timestamp>.gsprof`, with `bench/<scenario>/baseline.gsprof` as the comparison target. That tree is rooted at `config::profile_dir()`, not the project, which is a deliberate departure from this decision's first draft: baselines are per-machine by the non-goals above, and timestamping a file into a version-controlled tree on every run would churn `git status` for data that must never be shared anyway.

**Built.** `profile::diff_summaries` joins on `id` and `write_diff` emits the table; `compare_against_baseline` runs it from `finish_bench`, and `--engine-bench-update-baseline` records the current run as the baseline. A regression logs at error level against `--engine-bench-regression-threshold` (default 1.10 on frame p50). Ratios are quantity-over-quantity throughout, so they are dimensionless without any conversion, and `id::tag()` appears only in the printed table — never in the join. **Diffing joins on `gse::id`, not on strings.** `stable_id` is FNV-1a over the tag ([ID.cppm:416](../Engine/Engine/Source/Core/ID.cppm:416)), so an `id` is stable across processes, runs, and builds; the loader recovers it from the report's tag table once and every comparison after that is an `id` comparison. `id::tag()` is a sanctioned escape hatch, which is exactly why reaching for it here would read as care while quietly ending the checking.

### D9. Recording capacity derives from the frame budget

`max_recorded_frames = 600` and `max_recorded_nodes = 400000` ([ProfileAggregator.cppm:158](../Engine/Engine/Source/Diag/ProfileAggregator.cppm:158)) are compile-time constants that happen to equal the default scenario budget. Left as two independently-declared numbers, a scenario that outgrows the recorder truncates silently and its percentiles are computed over a partial run that reads as complete.

The resolved frame budget is the single authority: the recorder is sized from it when the scenario starts. Where a hard ceiling genuinely binds, the runner clamps and logs the truncation conspicuously — a loud failure beats a consistent one.

### D10. The scenario context grants no authority beyond a channel writer

A scenario is the longest-lived deferred thing in the engine — suspended across hundreds of frames — which makes it the worst possible holder of a borrowed reference. Two rules follow, and both are structural rather than conventional:

**No direct engine access.** `context` exposes `frame()` and `channels()`. It does not hand out `engine&`, a `registry&`, or any system's state. A scenario that could mutate the world directly would race readers exactly the way direct spawning raced them before `scene_command` was deferred to a safe point — the fix that already exists for this class of bug. Scene activation, spawning, and camera control are channel requests, applied by their owning systems.

**Nothing borrowed survives a suspension point.** `context` holds a `channel_writer` **by value**; it is a thin wrapper over a `channel_registry*` that outlives every scenario ([Registries.cppm:75](../Engine/Engine/Source/Ecs/Registries.cppm:75)), so it is safe to carry across `co_await`. No shared view, no system state reference, and no raw pointer obtained from either is ever stored in a coroutine frame.

World *reads* therefore cannot happen inside the coroutine. `co_await wait_until<State>(predicate)` evaluates its predicate inside the scenario system's own `run`, under the access the scheduler granted it, and resumes the coroutine only with the boolean result. The predicate is passed inline and must stay captureless — a stored capturing closure in a module interface body corrupts the partition's BMI on this toolchain.

### D11. Video is a lockstep image sequence, encoded offline

The primary capture path writes one image per virtual frame and hands the sequence to `ffmpeg`. Rationale:

- It is how trailer footage is actually made, and it is exactly deterministic.
- It does not depend on `video_encode_enabled()`, which per [native_capture.md](native_capture.md) is still unvalidated — the Arc 140V exposes video *decode* only.
- Write stalls are harmless: the clock is virtual, so a slow write costs wall time and nothing else.

The existing F9 screenshot path guards on a single outstanding write (`write_in_progress`), which is correct for screenshots and wrong for per-frame dumps. The dump path writes synchronously, which under a virtual clock is the *correct* choice, not a compromise.

The dump path formats a filename per frame, which is the per-frame allocation pattern that is normally a defect. It is a sanctioned exception here on two grounds: the path is opt-in and off during every perf run, and under a virtual clock its cost is invisible to the measurement by construction. Naming the exception is the point — an unexplained `std::format` in a per-frame path should still be challenged everywhere else.

The Vulkan Video encoder (F10/F11) stays as a convenience path. If it is ever validated, it needs a locked-cadence mode stamping PTS from virtual time rather than `frame_clock` — D14 in the capture plan specifies wall-clock VFR, which would reintroduce exactly the judder this design exists to remove.

### D12. Synthetic input reuses `input::event`

Input verbs construct `input::event` values that reach the same per-frame event set the window feeds. No parallel input path; the editor already proves this channel works.

**Delivery is a channel, not `push_attached_input`.** That function writes into `window::data`, which headless disables outright, so it can never serve a headless scenario — and D10 forbids the scenario from holding the engine needed to call it anyway. Instead `input::run` reads a `synthetic_input_request` channel and appends to the same `drained` vector it fills from the window. The two sources converge before any state is touched, so the input state machine stays the single authority and there is still exactly one path through it. Being a channel also means no scheduler dependency edge, so this does not disturb the headless activation cascade that gates systems by their `shared_view` deps.

`physics_stress_via_input` exists to keep that honest. It presses and releases F5 — the dev spawn binding — instead of pushing `spawn_stress_request`, so it exercises synthetic input all the way through `input::run` into `actions` and out the other side as the same workload `physics_stress` builds directly. A scenario that silently pressed nothing would otherwise look exactly like one that worked, since the settle gate and the profile artifact do not care whether anything spawned. Comparing its p50 and hash against `physics_stress` is what makes the difference visible.

### D13. `gse.scenario` is its own module, outside `gse.runtime`

`gse.runtime` re-exports `gse.graphics`, so a scenario module living inside it and reaching for graphics or input would cycle. This is the same constraint `CaptureRenderer` hit — resolved there by importing `gse.platform` directly instead of `gse.runtime`.

`gse.scenario` therefore imports `gse.ecs`, `gse.time`, `gse.diag`, `gse.os`, and `gse.math` only. The application registers scenario systems from its `setup` lambda, the same way the Sandbox registers everything else ([Main.cpp:32](../Sandbox/Sandbox/Source/Main.cpp:32)). Scenario bodies that need graphics or scene access live in the application, not the engine module.

---

## Module Layout

This is the layout as built, which is flatter than the original sketch. Three of the planned files were not needed: the warmup gate and frame budget live next to the existing pacing state in `Bootstrap.cppm`, and percentile analysis belongs with the profiler rather than in a scenario module.

```
Engine/Engine/Source/Scenario/
    Scenario.cppm        — DONE: info annotation, context, wait gate, awaitables,
                           reflection sweep, find. Module gse.scenario, standalone;
                           deliberately not re-exported from Import/, so it never
                           depends on engine (keeps D13's cycle closed).
    FrameDump.cppm       — DROPPED (Phase 3): a lockstep image-sequence writer was
                           only ever needed because video encode was unavailable on
                           this hardware. It is available, and the capture triggers
                           are already channel messages a scenario can push, so a
                           scenario records by pushing toggle_recording_request.
                           See scenario_capture_scope.md item 3; that item also
                           records what would bring this file back.
Engine/Engine/Source/Runtime/
    Bootstrap.cppm       — DONE: begin/step/finish_bench, bench_world_ready and
                           drive_scenario. The scenario driver lives here because it
                           already owns the phase state and has engine&.
    Engine.cppm          — DONE: bench_config carries the scenario name and body.
Engine/Engine/Source/Diag/
    ProfileSummary.cppm  — DONE: percentiles over a report_file, shared by the
                           in-process summary and any future editor diff.
Sandbox/Sandbox/Source/Sandbox/
    Scenarios.cppm       — DONE: sandbox:scenarios — declarations and annotations
                           only, the namespace the sweep scans.
    Scenarios.cpp        — DONE: sandbox:scenarios_impl — the coroutine bodies.
                           Split for the same reason Client.cpp is split from
                           Client.cppm: a coroutine body in a partition INTERFACE
                           does not survive this toolchain. Scenario bodies live in
                           the application, not the engine (D13).
Editor/Editor/Source/Scenario/
    ScenarioPanel.cppm   — TODO (Phase 4): list scenarios, Run, load .gsprof, baseline diff
```

## Public API Sketch

This is the surface as built. The awaitables take the `context` explicitly rather than reading a thread-local, so there is nothing ambient to get wrong when more than one scenario type exists later.

```cpp
export namespace gse::scenario {
    class context {
    public:
        auto frame() const -> std::uint64_t;

        auto channels() -> channel_writer&;
    };

    auto wait_frames(
        context& ctx,
        std::uint64_t count
    ) -> wait_awaitable;

    auto wait(
        context& ctx,
        time duration
    ) -> wait_awaitable;

    auto wait_settled(
        context& ctx
    ) -> wait_awaitable;

    using body_fn = auto (*)(context&) -> async::task<>;

    struct entry {
        id id;
        info info;
        body_fn body = nullptr;
    };

    template <std::meta::info Ns>
    auto registry() -> std::span<const entry>;

    auto find(
        std::span<const entry> table,
        std::string_view name
    ) -> const entry*;
}
```

`wait_until<State>` is not built. It is the only piece of D10 with no call site yet, and adding it before a scenario needs a world read would be machinery with nothing in it.

A scenario, in application code. The declaration carries the annotation and stays in the partition interface, where the sweep can see it; the body goes in the implementation partition, because a coroutine body in a partition interface does not survive this toolchain — the same split `Client.cppm` / `Client.cpp` already uses.

```cpp
// Scenarios.cppm — export module sandbox:scenarios;
export namespace sandbox::scenarios {
    [[= gse::scenario::info{ .name = "physics_stress", .scene = "Sandbox", .headless = true }]]
    auto physics_stress(
        gse::scenario::context& ctx
    ) -> gse::async::task<>;
}

// Scenarios.cpp — module sandbox:scenarios_impl;
auto sandbox::scenarios::physics_stress(gse::scenario::context& ctx) -> gse::async::task<> {
    co_await gse::scenario::wait_settled(ctx);
    ctx.channels().push<spawn_stress_request>({});
    co_await gse::scenario::wait_frames(ctx, 1);
    ctx.channels().push<spawn_joints_request>({});
}
```

**The body does not declare how long the run is.** Earlier drafts ended it with `co_await wait_frames(600)` next to a `.frames = 600` annotation, which is the same number written twice — change one and the measurement quietly stops matching the recorder capacity D9 derives from the other. The frame budget is the sole authority: the body scripts *what happens*, `step_bench` decides *when it stops*, and `finish_bench` warns if the budget expired while the scenario was still suspended, because that means part of the scripted workload never ran and the measured world is not the intended one.

The `wait_frames(ctx, 1)` is not a tuning value. It is the minimum separation that keeps two spawn bursts off the same frame, so the one-frame spike of building ~1000 bodies does not land on top of the joint spawn and blur what the percentiles describe. Anything that is genuinely tuned — wave counts, delays between waves, body counts — is a D2 world tunable and waits for Phase 2's `ScenarioTuning` state.

---

## Phasing

### Phase 0 — Determinism harness, no scenarios — **DONE**

Implemented as a nested `bench_config` on `engine_config`, so `parse_args` derives the flags by reflection: `--engine-bench-enabled`, `--engine-bench-scene`, `--engine-bench-warmup-frames`, `--engine-bench-frames`, `--engine-bench-profile-out`, `--engine-bench-summary-out`. Fixed-step clock, scene activation, populated-world settle gate, forced frame recording, percentile computation, text summary.

**Deliverable:** a repeatable number from a fixed workload, with no scenario machinery at all. Independently useful; validates D4/D5/D8 before anything depends on them. Achieved — though the *workload* is still empty until Phase 2, so the numbers currently describe an idle world.

**Validation gate: moved to Phase 2.** This originally sat here, and that was a sequencing error found by running it. The gate diffs a world-state hash across repeated runs, which only means something if the world *changes* between the start and end of a run. `sandbox_scene_setup` builds static geometry; every dynamic body in the Sandbox comes from `dev_spawn`, which is input-driven. A bench run sends no input, so the world is motionless and the hash is trivially identical — confirmed by identical hashes at 30, 240, and 900 frames (~15 s of simulated time). Five matching runs against a motionless world is a tautology, not a determinism result.

The gate therefore depends on Phase 2's spawn helpers and cannot run before them. What Phase 0 *can* establish, and did, is that the harness itself works: scene activation, settle gate, warmup, measurement, artifact write, clean exit, and a real reproducible non-empty digest.

The world-state digest serializes transform and motion components through `binary_writer`'s reflection member walk and hashes the resulting bytes with `stable_id` (already FNV-1a over a byte range). It is deliberately not a hand-rolled per-field fold: a digest that names its fields explicitly stops covering any field added later, and nothing reports the gap — a determinism harness with a silent blind spot is worse than none. This required widening `binary_writer` to accept `std::ostream&` rather than `std::ofstream&`, which is a pure widening: every existing call site passes an `ofstream`, and the writer only ever uses `write`/`tellp`/`seekp`/`good`.

Known limitation: the byte digest cannot normalize `-0.0` to `+0.0`, so a path that produces negative zero in one run and positive zero in another reports a mismatch despite being numerically identical. That is the first thing to check if the gate fails.

This gate requires building and running the engine, which the repository owner performs. It is a hard gate on Phase 1, not a background task: if world state diverges, the correct response is to stop and fix determinism, not to proceed with a weaker guarantee.

### Phase 1 — Registry and runner — **DONE**

Annotation, reflection sweep, fn-ptr table, name selection before `engine_config` construction, the driving system, `wait_frames` / `wait` / `wait_settled`.

`Engine/Engine/Source/Scenario/Scenario.cppm` holds the `info` annotation, `context`, `wait_gate`, the awaitables, `registry<^^Ns>()`, and `find`. It is not re-exported from any `Import/*.cppm` — it is a standalone module, deliberately, so it never depends on `engine`. `gse.runtime:engine` and `gse.runtime:bootstrap` import it directly, which is the known-good shape: a re-exported partition importing a *standalone* module does not form the collator diamond that a re-exported-partition import does.

1. The sweep runs **application-side**, in `Sandbox/Main.cpp`. This is a toolchain constraint, not a preference: `members_of` on a namespace segfaults `cc1plus` when the namespace's members live in a partition of the module *currently being compiled*. `Main.cpp` is a plain TU that imports `sandbox`, so `sandbox::scenarios` is a cross-module imported namespace — the case that is proven to scan fine.
2. `apply_scenario` writes the selected entry's `scene` / `headless` / `warmup_frames` / `frames` into `engine_config` before `gse::start` (D6), reusing Phase 0's `bench_config` fields. A name with no matching entry prints the available names and exits non-zero rather than falling through to an ordinary game session.
3. `drive_scenario` is called from `step_bench` in `Bootstrap.cppm`, which already owns the settling → warmup → measuring machine and already has `engine&`. The `async::task<>` lives in `bench_state` so the frame stays alive, and every resume goes through `async::resume_checked`.

Channel latency is worth knowing when reading a scenario's timeline: `channel<T>` flips on `frame_sync::on_end`, and `step_bench` runs *after* `frame_sync::end()`, so a push from a scenario body is visible to systems two frames later. It is constant and deterministic, so it costs nothing but it is not one frame.

**Deliverable:** named scenarios run to completion and emit a `.gsprof`.

### Phase 2 — Scenario surface

Tuning state (D2); scene activation; synthetic input (D10); spawn helpers in the Sandbox.

**Deliverable:** the VBD, Forward+, and locomotion perf scenarios exist and produce stable numbers.

**Status.** Four scenarios exist, all CPU-solver only (risk 3): `physics_stress`, `render_stress` (the same workload windowed, for renderer cost), `physics_stress_via_input`, and `locomotion`. Scene activation and the spawn helpers were already satisfied by Phase 1 — the bench activates `info.scene`, and `dev_spawn` consumes the spawn requests.

`physics_stress` and `render_stress` differ only in their declared render mode, so the workload itself is one coroutine both `co_await`. Copying the body instead would have made the pair drift the first time either changed, and the whole point of running them together is that the *only* difference is whether the renderer is in the frame. The shared helper is declared in the implementation partition, so it is invisible to the sweep and cannot be mistaken for a scenario in its own right.

`dev_spawn` gained `spawn_character_request` alongside its F7 binding, matching the stress and joints requests: one authority applies the spawn, and key and script are equal-footing producers into it.

`locomotion` spawns the character and then holds shift+W for the rest of the run, so the measurement covers a steady-state walk rather than a start transient. It waits a second of simulated time between the spawn and the first keypress, which is not a tuning value — it is the character reaching the ground before anything drives it. The keys are never released: a scenario ending does not clear input state, so the walk continues through the whole measurement window.

This is the scenario most likely to fail quietly. The character spawn needs its skinned model and clips resolved, and if that fails `spawn_character` returns early; the settle gate and the profile artifact would both look completely normal. Until `wait_until` exists (D10), a scenario cannot assert the character is there, so the check is external: a locomotion run whose p50 sits at the idle-world floor did not spawn anything.

**The spawn path already exists — do not build a second one.** `dev_spawn::run` already consumes `spawn_stress_request` and `spawn_joints_request` from the channel, on equal footing with the F5/F6 key bindings, and guards both on the active scene being non-null. Input and scripted requests therefore already resolve through one authority. A scenario spawns its workload by pushing that existing request through `ctx.channels()`; nothing new is required on the Sandbox side, and adding a parallel spawn entry point would be the duplicated-derivation defect this plan exists to avoid.

The one open question is *when* the push happens: the request is dropped if it arrives on a frame where no scene is active, so the scenario must `co_await wait_settled()` — which now also waits for the world to be populated — before pushing. That ordering is the scenario body's responsibility, not the runner's.

**Validation gate (moved here from Phase 0).** `physics_stress` is the scenario that satisfies it: `spawn_physics_stress` drops pyramids, dominoes, a funnel of boxes, sphere stacks and two 432-cube tumblers into the world, and the tumblers and elevators keep moving for the whole run, so the world is never motionless. The two runs the gate needs are

```
./Sandbox.exe --engine-bench-scenario physics_stress --engine-bench-frames 120
./Sandbox.exe --engine-bench-scenario physics_stress --engine-bench-frames 900
```

with the first repeated five times. The hash is on the `bench:` summary line. Two conditions must both hold, and the second is what Phase 0 could not satisfy:

1. The hash is identical across five runs.
2. The hash *differs* between a short run and a long one — proving the world evolved and the check has something to detect. Without this, condition 1 is vacuous.

**Result: both conditions hold.** Current baseline, on the Jacobi solver:

| Run | p50 | p95 | p99 | world-state hash |
|---|---|---|---|---|
| 120 frames ×5 | 8.67–9.14 ms | 11.1–13.3 ms | 11.7–23.5 ms | `0x6d3576e5fbf1029c` (all five) |
| 900 frames | 8.03 ms | 10.45 ms | 11.72 ms | `0x91762aee956b20e9` |

The gate was first passed on the Gauss-Seidel solver (`0xdac6153788620802` / `0xf9e03294d51f43df`, p50 ~16.4 ms) and re-passed unchanged after the switch to Jacobi. A solver change moves the hash and the cost by construction, so those numbers are superseded, not a regression — the point of recording the baseline is that the distinction is decidable. **A hash that moves without an accompanying change to solver, scene, or tunables is the thing to investigate.**

Condition 1 has held more strongly than the gate asks on every run of it so far: the same hash comes back across separate builds of the binary, so world state is reproducible across compilations and not merely across invocations of one image. Condition 2 holds — the long run diverges, so the digest has something to detect and the five matching hashes are not the tautology Phase 0's motionless world produced.

`physics_stress_via_input` sits at the same p50 with hash `0x809060cfa642c105`. Its hash differs from the direct scenario by design: routing the spawn through `input::run` and `actions` delays it a frame or two of simulated time, so the world is at a different point 120 frames later. Matching p50 with a differing hash is the expected signature of that pair, and it is what proves the keypress landed rather than silently doing nothing.

Timing noise is the other output. p50 spanned 8.67–9.14 ms across five identical runs, roughly a ±3% band on an otherwise idle machine; a later build measured 9.41–9.53 ms at the same hash, so p50 drifts a few percent between builds while the world stays bit-identical — one more reason the hash, not the timing, is what a regression check keys on. That band is the floor a regression threshold has to clear, and it is measured rather than guessed. p99 is much looser (11.7–23.5 ms) — a single outlier frame moves it, so it is the wrong statistic to gate on.

**What this does not cover.** `use_gpu_solver` defaults false and the scenario does not set it, so the gate exercised the CPU solver only. Risk 3's GPU nondeterminism is untouched by this result and needs its own scenario before any GPU-solver number is trusted.

Prime suspects if it ever regresses: unseeded RNG, and system execution order under work-stealing (the ECS schedule serializes RAW/WAW/WAR conflicts, so this *should* be order-independent by construction — this result is that being proven rather than assumed).

### Phase 3 — Camera and capture

**Rescoped 2026-08-13 — see [scenario_capture_scope.md](scenario_capture_scope.md), which is the authoritative version.** The original list here was camera path asset format, in-editor keyframe authoring, playback awaitable, lockstep frame dump, and `ffmpeg` invocation. Half of it is gone: the last two are superseded now that video encode is validated on this hardware and the capture triggers turned out to already be channel messages, and the first two are deferred because a scenario body can interpolate keyframes and push a `camera::request` per frame without an asset type.

What remains is the windowed boot gate, a consumer for `camera::request`, and two small per-clip additions — a sun-arc driver and a light-spawn request.

**Deliverable:** every README clip is reproducible from one command. This is the forcing function for the whole plan.

### Phase 4 — Editor panel

Scenario list from the reflection table, Run via `build_runner` spawn, auto-load the resulting `.gsprof` into the Profile tab, baseline diff with a regression threshold.

Two constraints on the spawn, both already paid for once:

- It must go through `gse.ide.build:spawn`. A child launched with `bInheritHandles = TRUE` inherits the editor's GPU handle table, which previously corrupted `cc1plus` nondeterministically; the explicit `PROC_THREAD_ATTRIBUTE_HANDLE_LIST` path is the fix and every new editor spawn uses it.
- The child needs the GCC runtime directory on `PATH` or it exits 127 before reaching `main`. Build that environment for the child specifically — do not mutate the editor's own process environment to arrange it.

---

## Open Risks

1. **Long-lived coroutine lifetime.** ~~The scenario frame must not be allocated from the frame arena.~~ **Resolved by inspection during Phase 1.** `frame_arena` is not a per-frame bump allocator that resets at frame boundaries — it is a thread-local free-list pool of size-bucketed blocks obtained from `::operator new` and returned only on explicit `deallocate`, i.e. when the coroutine frame is destroyed. The name misleads: it is an arena *for coroutine frames*, not an arena reset *every frame*. A scenario coroutine suspended for hundreds of frames is therefore safe provided its `async::task<>` is kept alive by the owner.

	The residual hazard is address reuse after the task is destroyed, and the engine already ships the guard: `async::track_frame` stamps a generation, and `async::resume_checked` refuses to resume when the generation at that address no longer matches. **Scenario resumption must go through `resume_checked`, never a bare `handle.resume()`** — that is the invariant that keeps the original `execute at 0x0` crash from returning. Combined with D10's ban on borrowed references, both halves of this risk are now closed by construction.

2. **Determinism may not hold on first attempt.** ~~Unseeded RNG is the likely first offender.~~ **Closed 2026-08-06 by the Phase 2 gate**, which passed on the first attempt and across two different builds of the binary. No tolerance was needed and the `-0.0` hazard never fired. This covers the CPU solver only.

3. **GPU nondeterminism — not a risk, a known property. Scenarios are CPU-solver only.** The GPU VBD solver is known to be non-deterministic; that is established, documented in `docs/LOCOMOTION.md` and the physics work that preceded it, and not something this plan re-litigates. No scenario declares the GPU solver, and `--engine-use-gpu-solver` alongside a scenario produces a world state that does not reproduce, so it is not a valid way to run one. If a GPU throughput number is ever wanted, it is a timing measurement with no determinism claim attached, and it needs its own framing rather than borrowing this harness's.

4. **Reflection sweep across module boundaries.** ~~`members_of` has previously ICE'd on namespaces outside the same module partition.~~ **Addressed by construction in Phase 1, pending a build.** The ICE is narrow: it fires when the scanned namespace's members live in a partition of the module being compiled *and* the scan is instantiated from an impl unit of that same module. `registry<^^Ns>` takes the namespace as a template parameter the way `register_systems<^^ns>` does, and the only caller is `Sandbox/Main.cpp` — a plain TU that imports `sandbox`, which is the cross-module case proven to scan fine. A scenario namespace declared inside the *engine* would reintroduce the hazard; D13 already forbids that for a different reason.

5. **Frame dump disk cost.** 600 frames of 1080p PNG is roughly 5 GB per take. Acceptable for one-shot README captures, not for a routine part of the perf loop. The dump is opt-in per scenario.

6. **Headless render scenarios.** Sim-only scenarios are proven by `--physics-parity`. A *windowed* scenario is subject to compositor and VRR behavior that varies with focus state, which will show up as run-to-run variance in GPU timings even with a perfectly deterministic sim. Expect windowed benchmarks to be noisier than headless ones and set regression thresholds accordingly.
