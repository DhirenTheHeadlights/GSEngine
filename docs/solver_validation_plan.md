# Physics Solver Validation — Plan

How to compare the GPU and CPU VBD solvers cleanly, without dragging the comparison into the hot path the way the old inline `compare_solvers` block did.

The old approach (removed during the readback rip-out): an `interval_timer` in `update_vbd_gpu` that every 0.25 s built a fresh `vbd::solver` instance, ran a one-step CPU solve, did a full body+contact+joint readback, computed pairwise deltas, and dumped plain-text "max error / worst body" logs. Heavy, lived in the live solve path, only ran on a 1-step diff, only told you *that* it diverged.

This plan replaces it with three layered tools that share infrastructure and stay out of the hot path.

---

## Components, in landing order

### 1. Always-on health ledger — ~150 LOC

Tiny GPU→CPU summary the solver emits every dispatch. Continuous smoke detector; catches "physics blew up" within a frame at near-zero cost. Aggregate-only — doesn't pinpoint which body diverged.

**GPU side**
- New binding `solver_health` on the existing VBD descriptor set (4-8 uints).
- Reset to 0 in `collision_reset.slang` alongside `grounded_bits`.
- Populated at end of `vbd_finalize.slang`: each body does `InterlockedMax(asuint(value))` on positive-only reductions (magnitudes of position delta, velocity, angular velocity) plus an OR'd NaN flag.

**CPU side**
- New `gpu_solver::read_health() -> std::span<const std::uint32_t>` mirroring `read_grounded()`.
- New `grounded_readback`-shaped host-visible scratch buffer, copy in `vbd_state_copy_stage`.
- In `physics::system::update_vbd_gpu`, decode summary each frame, log/`assert` if any threshold blown: NaN flag set, max-velocity above ceiling, max-position-delta above `dt × max_speed × safety_factor`.

**Optional add behind a `compare_solvers` flag (~50 LOC)**
- CPU runs a 1-step reference solve from the *same uploaded inputs* and emits the same summary locally.
- Diff the two summaries; log when their gap exceeds a tunable threshold.
- Costs a CPU solve every frame when enabled — free in shipping.

**Files touched**
- `Engine/Engine/Source/Physics/VBD/GpuSolver.cppm` — new binding, new buffer in `per_frame_data`, `read_health()` declaration.
- `Engine/Engine/Source/Physics/VBD/GpuSolver.cpp` — buffer creation, descriptor wiring, copy in state-copy stage, `read_health()` impl.
- `Engine/Resources/Shaders/Bodies/VBDPhysics/collision_reset.slang` — clear health uints.
- `Engine/Resources/Shaders/Bodies/VBDPhysics/vbd_finalize.slang` — atomic-reduce summary writes.
- `Engine/Engine/Source/Physics/System.cpp` — decode + threshold check in `update_vbd_gpu` (or a new helper called from there).
- `Engine/Engine/Source/Physics/System.cppm` — health-threshold settings on `system::data`.

---

### 2. Capture + offline replay — ~250 LOC

On-demand deep-dive tool. When the ledger fires or you spot a visible glitch, trigger a capture, then run both solvers from identical seeds offline with per-body diff fidelity. Land it as a `_test.cpp` so CI runs the comparison automatically.

**Capture format** (~80 LOC)
- New `physics::capture_frame(d)` that serializes the current `gpu_upload_payload` — bodies, motors, joints, impulses, solver_cfg, dt, steps — plus the GPU's post-solve snapshot, into a single binary blob (length-prefixed sections, structs are trivially copyable).
- Output path under `Engine/Resources/Captures/` with a frame-numbered or hash-named file.
- Trigger via an action handle (hotkey) or RemoteTrigger. Capture invokes a `physics::capture_request` channel that gets drained in `update_vbd_gpu` once the upload payload is built.

**Replay** (~120 LOC)
- New test TU under `Engine/Engine/Tests/` or wherever the test convention lands.
- Loads a capture, instantiates a `vbd::solver`, feeds it the captured bodies via `begin_frame`, replays the captured joints/motors, runs `solve(dt) × steps`. Reads the captured GPU result from the same file.
- Computes per-body deltas (`max |Δposition|`, `max |Δvelocity|`, `max |Δangular_velocity|`, `max |Δorientation|`) and per-contact deltas (`max |Δlambda|`, `max |Δpenalty|`).
- Asserts under a tunable threshold; on failure dumps the worst N rows as CSV to stdout / a file next to the capture.

**Threshold table + formatter** (~50 LOC)
- `solver_diff_report` struct, prettyprinter for stdout, optional CSV writer for the worst rows.

**Shared with (1)**
- The host-readable scratch buffer pattern, the descriptor binding layout, the `gpu_solver::query_body_snapshot` API for grabbing post-solve state directly.

**Trickiest bit**
- Make `vbd::solver` consume the *exact same* prepared joints — `prepare_joint` is already shared CPU/GPU, so this should be a no-op verification rather than a refactor. Verified by re-running the captured warm-start sequence; drift on first contact frame indicates a real bug.

---

### 3. Scenario / property unit tests — incremental

Hand-authored deterministic scenes that exercise the CPU and GPU solvers from known seeds and assert per-body deltas under a threshold. Run on CI. Catches regression in known scenarios, not emergent drift in live gameplay — so it's a complement, not a substitute.

**Scope:** zero infrastructure cost once (2) lands. Each scenario is a small builder (~30-80 LOC) plus a one-line test that calls into the same diff machinery (2) produces. Start with:

- Mass pyramid (stacking + friction)
- Joint chain (pendulum-like)
- Sphere on slope (rolling friction + restitution)
- Single jenga / hovering block stack (sustained contacts, energy stability)
- Two bodies thrown at each other (high-velocity collision response)

Add scenarios as bugs are found, then keep the scenarios as regression coverage.

---

## Combined cost

Doing (1) and (2) together is cheaper than doing them separately later — they share the host-readable scratch buffer, the descriptor binding pattern, and threshold settings. Estimate **~350 LOC combined** for both, across ~7 files plus one new shader binding and one new compute pass write site.

(3) layers on top of (2)'s infrastructure at near-zero per-scenario cost.

---

## Recommended landing order

1. **Health ledger** — smallest unit, starts buying signal immediately, builds the shared scratch-buffer infrastructure.
2. **Capture + replay test** — uses the same infrastructure, gives the deep-dive tool you'll want the first time the ledger fires.
3. **Scenarios** — populate gradually as the test infrastructure is exercised against real scenes.

Optional CPU reference solve in (1) lands last, behind a setting — it's the most expensive piece and only useful when you want continuous reference comparison rather than just invariant checking.

---

## What we are explicitly **not** building

- A frame-stepper / viewer UI for scrubbing through divergences. Stdout CSV is enough until you actually need to scrub.
- A separate dev binary for replay. Test TU is sufficient and gets CI for free.
- Continuous full-state readback in production. The aggregate ledger covers always-on validation; full readback only happens at explicit capture time.
- An in-engine "compare mode" that runs both solvers in parallel forever. The optional flagged variant in (1) is close, but bounded to a 1-step reference, not a full mirror.
