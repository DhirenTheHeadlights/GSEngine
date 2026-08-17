# Authoring and Running a Scenario

A scenario is a named C++ coroutine that drives the engine through a fixed sequence of world events on a fixed-step clock, then exits with a `.gsprof`, a percentile summary, and a world-state hash. Frame *N* holds the same simulated world every run, so anything measured at frame *N* is comparable across runs, builds, and configurations.

This document is the operating manual. The design rationale is in [script_runner_plan.md](script_runner_plan.md); the traps section there is worth reading once before authoring.

## Adding one

Two files, both in the Sandbox. Nothing central needs editing — the registry is a reflection sweep over `sandbox::scenarios`, so an annotated function in that namespace is discovered automatically.

Declaration, in `Sandbox/Sandbox/Source/Sandbox/Scenarios.cppm`:

```cpp
export namespace sandbox::scenarios {
    [[= gse::scenario::info{ .name = "my_scenario", .scene = "Sandbox", .headless = true }]]
    auto my_scenario(
        gse::scenario::context& ctx
    ) -> gse::async::task<>;
}
```

Body, in `Sandbox/Sandbox/Source/Sandbox/Scenarios.cpp`:

```cpp
auto sandbox::scenarios::my_scenario(gse::scenario::context& ctx) -> gse::async::task<> {
    co_await gse::scenario::wait_settled(ctx);
    ctx.channels().push<spawn_stress_request>({});
}
```

The body **must** live in the `.cpp`. A coroutine body in the partition interface does not survive this toolchain.

## The annotation

| Field | Meaning |
|---|---|
| `name` | Selects the scenario, and names the artifacts. |
| `scene` | Activated before the settle gate. Empty means no scene, which measures an empty world. |
| `headless` | `true` runs with no window or renderer. Only ever *forces* headless — a `false` here leaves the command line's choice alone. |
| `gpu_solver` | `true` forces the GPU VBD solver. Same one-way rule. |
| `warmup_frames` | Discarded frames before measurement. Default 120. |
| `frames` | Measured frames. Default 600. Also sizes the profiler's recording ring. |

Every field is overridable on the command line. `apply_scenario` writes the annotation's value only where the parsed config still holds `bench_config`'s default, so an explicit `--engine-bench-frames 900` survives.

## What a body may do

`context` grants exactly two things: `frame()` and `channels()`. That is deliberate — a scenario is suspended across hundreds of frames, which makes it the worst possible holder of a borrowed reference. There is no `engine&`, no `registry&`, no system state.

So a scenario **acts** by pushing channel requests, which the owning system applies at its own safe point:

```cpp
ctx.channels().push<spawn_stress_request>({});
ctx.channels().push<spawn_joints_request>({});
ctx.channels().push<spawn_character_request>({});
ctx.channels().push<gse::input::synthetic_input_request>({
    .value = gse::input::key_pressed{ .key_code = gse::key::w },
});
```

Synthetic input goes through the same per-frame event set the window feeds, so it drives actions and bindings exactly as a real key would.

A scenario **waits** with:

```cpp
co_await gse::scenario::wait_settled(ctx);
co_await gse::scenario::wait_frames(ctx, 30);
co_await gse::scenario::wait(ctx, gse::seconds(2.f));
```

`wait_settled` is required before the first push. It waits for scheduler quiescence *and* a populated world; a request that arrives before the scene is active is dropped silently.

A scenario cannot **read** world state. `wait_until<State>(predicate)` is specified for that and is not built yet. It is now the main known gap: `wait_settled` covers scheduler quiescence and a populated world, but **not** whether an asset has resolved. Anything spawning from an asset — characters especially — is dropped silently if the request lands before the asset is ready, and the run still produces a valid deterministic hash for an empty world. Until `wait_until` exists, such a scenario has to wait a fixed span before pushing, which is a guess rather than a guarantee.

The cheapest check that a scenario spawned anything at all is to run the same scene with no scenario and compare hashes. Identical hashes mean the scenario did nothing.

Do not end the body with a wait sized to the frame budget. The budget is the sole authority on run length; the body scripts what happens, and `finish_bench` warns if the budget expired while the scenario was still suspended.

## Running

```bash
./Sandbox.exe --engine-bench-scenario my_scenario
```

Run from the Sandbox build directory with the GCC runtime on `PATH`:

```bash
export PATH="/c/Users/Dhiren/.gcc-trunk/current/bin:$PATH"
```

Useful overrides:

```bash
--engine-bench-frames 900          # measured frames
--engine-bench-warmup-frames 30    # warmup frames
--engine-bench-profile-out a.gsprof
--engine-bench-summary-out a.txt
--engine-use-gpu-solver            # if the scenario does not declare it
```

Artifacts land under `%LOCALAPPDATA%/GSE/profile/bench/<scenario>/`, one timestamped `.gsprof` and `.txt` per run.

The run prints one summary line carrying both the timings and the digest:

```
bench: 120 frames measured, p50 9.4 ms p95 12.7 ms p99 16.9 ms, world-state hash 0x6d3576e5fbf1029c
```

The hash covers every transform and motion component, ordered by owner id, digested through `binary_writer`'s reflection walk — so it follows fields added later without anyone maintaining a list.

## Rules that will otherwise cost you a cycle

**Windowed scenarios were unusable until 2026-08-14, for a reason worth knowing.** `step_bench` activated `info.scene` on its first step behind a one-shot latch. In render mode `app_setup` — and therefore every `add_scene` call — is deferred behind the loading screen, so the activation named a scene that did not exist yet, was dropped, and was never reissued. The world stayed empty and the run aborted at the settle cap after ~40 s. Headless was unaffected because it runs `app_setup` inline before the bench loop starts.

Fixed by not latching until `find_scene` confirms the scene is registered. If you are writing a windowed scenario and it aborts, read the settle-cap message: it now reports `registered=`, `activated=`, `populated=`, and `settled=` separately, which is what tells you which of those four stages failed.

Two earlier explanations in this file were wrong and are recorded here so they are not re-derived: the boot gate was never at fault — it fires within a frame of the loading screen drawing — and the claim that successful runs succeeded because a human clicked the window does not survive reading `engine::render`, where `window::show` is not called until `rendered_once()` plus two frames.

**A bench run never shows its window.** `engine::render` skips the show step entirely when `bench.enabled`, so a windowed scenario renders and presents to a hidden window and never steals focus. That is what makes an unattended windowed scenario tolerable to run ten times in a row, and it is why a scenario can record itself with no keypress. One consequence: present-timing feedback does not exist against a hidden window, so do not take present-pacing measurements from a bench run.

Do not trust a windowed hash as a baseline. A run that completes because someone interacted with it took real input into a scripted scenario, and the digest reflects that.

**A scenario can drive capture**, because the capture triggers are channel messages like any other:

```cpp
ctx.channels().push<gse::renderer::capture::toggle_recording_request>({});
```

`record_clip` is the worked example — settle, spawn, toggle on, wait a declared span, toggle off. Recording needs the renderer, so such a scenario must not declare `headless = true`. Clips land in `%LOCALAPPDATA%/GSE/captures/recordings/`.

Under a scenario the encoder stamps presentation timestamps from *content* time, not wall time, so every frame is exactly 1/60 s apart and the clip is constant-rate 60 fps no matter how slowly it rendered. Verify a clip with `ffmpeg -v error -i <file> -f null -` and require zero output — a file playing in a desktop player proves nothing, as VLC and Windows Media Player both happily played a file that decoded zero frames in dav1d.

**A tripped assert does not terminate the process.** It logs `[fatal]` and hangs forever. Always run under an external timeout, and read `%LOCALAPPDATA%/GSE/logs/Sandbox.log` when a run produces no summary line.

**Headless has a narrower asset loader set than render** — `model` only. Anything needing `skinned_model` or `clip_asset`, such as character spawning, asserts headless. This is a known open gap; see the trap entry in the plan for two corrections that were tried and were both worse than the gap.

**Check the machine is quiet before trusting a timing.** A run taken during a compile returned p95 232 ms against a normal 13 ms — with a bit-identical hash, because the fixed-step clock decouples world state from wall time. Determinism cannot detect a contaminated measurement.

```bash
tasklist | grep -E "cc1plus|ninja"
```

**The hash is the regression key, not the timing.** p50 drifts a few percent between builds on an identical world. A hash that moves is only expected when the solver, scene, or a tunable changed; a hash that moves without one is the thing to investigate.

## Detecting a regression

Each scenario has one baseline, `bench/<scenario>/baseline.gsprof`. Record one from a run you trust:

```bash
./Sandbox.exe --engine-bench-scenario my_scenario --engine-bench-update-baseline
```

Every later run of that scenario compares against it automatically, writes `<timestamp>.diff.txt` beside the run, and prints a verdict:

```
bench: frame p50 9.412 ms -> 9.530 ms (1.013x against a 1.100x threshold), p95 ... ; within threshold
```

A regression logs at error level. The threshold is a ratio on frame p50, defaulting to 1.10, overridable with `--engine-bench-regression-threshold 1.05`. Pick it against the measured noise band — p50 drifts a few percent run to run and between builds on an identical world, so a threshold inside that band produces false alarms. p99 is far looser than p50 and is the wrong statistic to gate on.

The per-tag table joins on tag id rather than tag text, so it survives renames and process boundaries, and it marks tags that appear in only one of the two runs as `added` or `removed` instead of silently dropping them. Rows sort by absolute change, so the biggest movers are at the top and improvements at the bottom.

Two things the diff cannot tell you, both of which will otherwise read as regressions:

- A run taken while a compile was in progress. Check the machine is quiet first.
- A run whose world changed. The diff compares timings; the world-state hash on the summary line is what tells you whether you are comparing the same workload at all. A hash change plus a timing change is a different benchmark, not a slower one.

## Comparing two configurations

The intended shape for a CPU-versus-GPU comparison is a declared pair, so each gets its own artifacts instead of overwriting a shared name:

```cpp
[[= gse::scenario::info{ .name = "parity_cpu", .scene = "Sandbox", .headless = true }]]
[[= gse::scenario::info{ .name = "parity_gpu", .scene = "Sandbox", .headless = true, .gpu_solver = true }]]
```

Both should `co_await` one shared workload coroutine declared in the implementation partition, so the only difference between them is the declared configuration. A copied body drifts the first time either changes, and then the comparison is measuring the wrong thing. The helper is invisible to the sweep because it carries no annotation.

Note before relying on a GPU hash: the GPU VBD solver is known non-deterministic, so a `gpu_solver` scenario reproduces its timings but not its world state.
