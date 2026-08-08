# Solver Parity Harness

> **Status:** harness built and in use. Two bugs found and fixed (half gravity, origin teleport). The GPU matches the CPU on every workload that contains no joints; the remaining divergence is entirely in the joint solver and is open.

## The question

The GPU VBD solver produced unrealistic impulses that the CPU solver did not. Reading the two implementations against each other has diminishing returns — the narrow phase turned out to be a faithful port, and a 36 KB Slang file against a 63 KB C++ one is not a search space worth exhausting by eye. This harness replaces reading with measurement: run the same scripted world through both solvers and report **where** they part company.

## What the harness is

**Per-frame state dump.** `bench_config::state_dump_out` → `--engine-bench-state-dump-out` writes one `state_dump_record` (owner, frame, position, velocity) per body per measured frame through `binary_writer`. The format lives in `Runtime/StateDump.cppm` and is marked `[[= archive_raw{}]]` — see the trap below.

**Comparator**, a Sandbox run mode so it rides the existing build:

- `--compare-states-a/-b` with `--compare-states-threshold` — per-frame max drift, count over threshold, worst body, and the largest single-frame jump.
- `--compare-states-focus-frame/-bodies/-window` — per-frame drift, both solvers' speeds, and position for the top-N diverging bodies. Every finding below came from this.
- `--scan-states` with `--scan-states-speed` — scans a *single* dump for implausible speeds. This is the right instrument for "does it explode"; drift is not, because drift conflates a blowup with ordinary nondeterminism.

**Ladder scenes**, code-built and registered in `world_loader_setup`, no assets:

| Scene | Bodies | Purpose |
|---|---|---|
| `ParityDrop` | 2 | gravity and integration, one body-vs-static contact |
| `ParityPair` | 3 | one dynamic-vs-dynamic contact, no pile |
| `ParityStack` | 9 | contact chains |
| `ParityPile` | 217 | dense pile, spawned with gaps at scene setup |
| `ParityOverlap` | 217 | same but interpenetrating at spawn |

Scenarios are `cpu`/`gpu` pairs sharing a scene, plus `parity_nojoints_gpu` (stress spawn, no joints), `parity_jointsonly_gpu/cpu` (joints, nothing else) and `parity_empty_gpu` (scene only). All carry `.warmup_frames = 0`.

## Rules the measurements depend on

**Warmup must be 0.** The default 120 discards the entire divergence onset. With warmup the CPU-vs-GPU number on `physics_stress` was *smaller* than GPU-vs-GPU, i.e. pure noise.

**Runs must be long enough to reach the mechanism.** The tumbler turns at 0.6 rad/s, so a revolution takes ~10 s. Every run under ~600 frames leaves the drums essentially stationary, and a whole afternoon of "the GPU looks fine" came from 400-frame runs that never tumbled. Use 2400 frames when the tumbler matters.

**Measure the property, not the mechanism you suspect.** Four hypotheses about the colouring fallback were instrumented and refuted in turn. What settled it was measuring the invariant directly — same-colour contact pairs — instead of the mechanism each guess implied.

**Never conclude from one run.** `conflicts` as a high-water mark varies 0–27 across identical runs. It now also reports a per-run total, which is far more stable.

**Instrumentation cost is not free.** A duplicate-contact check added to `collision_build_coloring` cost **35 ms/frame** — a 760x regression on that stage — because it was O(Σcc²) over full `contact_constraint` loads and that stage dispatches as a *single workgroup*. It invalidated every GPU timing measured while it was in. It did not affect correctness results, because the fixed-step clock makes the simulation wall-clock-independent.

## Fixed and verified

**Half gravity.** `build_bodies` uploaded `.accel_weight = 0.f` and no shader ever wrote that field, while `vbd_predict.slang` uses it to scale `gravity_step` into `predicted_position`. Gravity reached bodies only through the solve and converged to about half: on a single falling box the GPU climbed 9.81/120 per frame against the CPU's 9.81/60. Now `airborne ? 1.f : 0.f` driven by `d.body_airborne`, matching the intent of the CPU's `clamp(-accel_y/g, 0, 1)`. Do **not** hardcode 1 — resting bodies then guess into the ground every step.

**Origin teleport.** `latest_snapshot_slot()` returned the in-flight slot. Split into `retired_snapshot_slot()` (`m_dispatch_slot`, for CPU reads, fence-safe) and `render_snapshot_slot()` (`1 - m_dispatch_slot`, for GPU reads). `PhysicsDebugRenderer` had been working around this locally with `1u - latest_snapshot_slot()`. Costs the CPU mirror ~2 frames of lag.

**Contact blowup on spawn.** On a reseed, `id_to_body_index` is rebuilt from motion order so every body index shifts, but `warm_start_buffer` holds contacts naming `body_a`/`body_b` under the *old* numbering — cached lambda and penalty then get applied between unrelated bodies. Fixed by writing a zeroed `contact_constraint` to `warm_start_buffer[0]` in `commit_upload` when `m_apply_all_body_inputs` is set, which trips the lookup's sentinel for that one frame.

## Established by measurement

**Parity holds without joints.** Over 2400 frames: GPU without joints peaks at 15.08 m/s, CPU at 14.84 m/s — 1.6% apart, stable for the whole run. With joints the GPU reaches 210 m/s by frame 9.

**GPU determinism has a boundary between 9 and 217 bodies.** `drop`, `pair`, `stack` return bit-identical hashes across runs; `pile` and `overlap` do not. Nondeterminism and the blowup are *separate* phenomena — pile and overlap are nondeterministic and never explode. Suspect narrow-phase `InterlockedAdd` contact-slot ordering.

**Eliminated:** contact-buffer overflow (36x headroom, zero drops), duplicate contacts (`dup 0` on every run), colouring palette and round exhaustion, colouring conflicts as a blowup cause (~1.7/frame, real but unrelated).

**Warm starts are live**, contrary to an earlier note. `warm_start_lookup`'s `ws_count` parameter was never referenced in its body; the scan is bounded by `max_contacts` and terminated by the zeroed-slot sentinel, so `m_warm_start_count` being 0 gated nothing. Measured 459–516 hits per frame on stress. The dead parameter has been removed.

**The CPU does apply restitution** (bounces 6.533 → 1.332 m/s, ratio 0.20 against a 0.3 setting). An earlier claim that it did not came from a focus window that began after the CPU's impact.

## Open: the joint solver

`parity_jointsonly_gpu` reproduces it in 250 frames with nothing else in the scene — CPU peaks at 10.6 m/s, GPU at 374 m/s, permanently (200–360 m/s from frame 25 through 225, never decaying).

Bisected by editing shaders, which hot-load without a rebuild:

| probe | max speed |
|---|---|
| baseline | 374 m/s |
| joint forces skipped in `solve_body` | 15.1 m/s |
| `pos_lambda` zeroed | 14.3 m/s |
| `ang_lambda` zeroed | 374 m/s |
| `distance` joints skipped | 130 m/s |
| `distance` + `slider` skipped | 16.1 m/s |

So it is the joint **positional lambda**, and `distance` and `slider` are the two types that convert it into flight. But **no joint type actually holds** — with those two skipped the remaining fixed/hinge joints still show 11 m separations and 583 MN lambdas.

The per-joint trace shows `|d|`, `c0` and `penalty` all stable while `lambda` grows *quadratically* (increments 21k → 33k → 45k → 58k → 70k N). Growing increments with a constant penalty mean the per-iteration `c` is itself growing: a feedback loop, not a spike.

Two suspects, both consistent with `distance` and `slider` being the severe cases:

1. **Rotating constraint bases.** Those two derive their directions from current state — `normalize(d)` for distance, and a `cross(axis_w, up)` basis from `predicted_orientation` for slider — whereas `fixed`/`hinge` use constant world axes. `pos_lambda` is retained across frames, is unclamped, and decays only by `gamma = 0.99`, so a direction that rotates between accumulation and application makes the retained force point increasingly wrong.
2. **Slider basis flip.** `perp0 = cross(axis_w, float3(0,1,0))` is guarded only at `length < 1e-6`, and a slider elevator's axis *is* vertical. Near-degenerate axes can flip the basis between frames, which inverts the sign of the retained lambda.

The lambda accumulation formula itself is line-for-line identical to the CPU (`vbd_update_joint_lambda.slang:49` vs `Solver.cppm:1513`), as are the alpha asymmetry, the damping term, and the per-frame `compute_joint_c0` cadence. The divergence is not in the arithmetic.

## Separate finding, not shipped

`vbd_solve_color.slang` has its Gauss-Seidel convergence test hard-wired to `if (false)`, so the GPU never early-outs and always runs all 15 iterations while the CPU breaks at ~4 via `Solver.cppm:696`. Enabling it does **not** fix the blowup (373.6 vs 374.3 m/s) and buys ~6% on the joints scene. Left disabled — `if (false)` reads deliberate, and it is a solver behaviour change that deserves its own decision.

## Traps paid for

**`archive_raw` on the dump record.** Without it, `state_dump_record` is a class with reflected members, so `binary_writer` takes its *schema* path. Schema emission is deduplicated per writer instance, and the dump constructs a fresh writer each frame — so the schema is re-emitted before every frame's records, the reader caches it once, and everything after frame 0 is misparsed into plausible-looking garbage. `read_state_dump` now also validates that the payload is an exact multiple of the record size.

**`--scan-states` exists because drift was the wrong instrument.** Judging "does it blow up" by comparator drift conflates a genuine explosion with ordinary nondeterminism, and led to the wrong conclusion that joints were not required.

**Diagnostics currently degraded.** `conflicts` reads 0 on multi-substep runs, because the `collision_state` readback moved to end-of-dispatch so it could capture joint values written during the solve, and `collision_reset` zeroes the header at the start of substep 1 while colouring only runs on substep 0. The joint `c` slot reports `|d|` rather than constraint error. Both are restorable; neither is load-bearing.

## Next

Scan for the joint with the largest `|pos_lambda|` and report *that* joint's correlated state — the current probe reports joint 0, which is a healthy fixed joint holding at 0.400 m with zero lambda. One shader pass, no C++ change.
