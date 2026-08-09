# Solver Parity Harness

> **Status:** four bugs found and fixed. `dispatch_indirect` was an empty stub on DX12, so the whole indirect half of the solver — narrow phase, every colour sweep, restitution — silently did nothing on that backend. With it implemented, restitution turned out to apply once per contact point rather than once per impact, injecting energy on every bounce. With that fixed, pile invariants exposed that `sticking` is never computed on the GPU, pinning tangential stiffness at the floor so piles slid apart instead of stacking. The ladder now agrees to millimetres on every scene up to 16 bodies, and joints-only sits at 3 cm once the readback offset is aligned out. The 217-body pile still spreads more than the CPU's.
>
> Read the two-regime section before adding a metric. Per-body drift is the right instrument for scenes that come to rest and the wrong one for piles, and every wrong theory recorded here came from ignoring that.
>
> **Record the GPU and the backend with every measurement.** The earlier findings here were taken on a desktop RTX 5090 running Vulkan. Everything below marked DX12 came from a Galaxy Book 5 Pro (Intel Arc 140V) where Vulkan device creation fails and the engine falls back to DX12. The two are not comparable, and nothing about the joint solver can be re-measured on the DX12 machine until the stub is filled in.

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

## Open, and blocking everything else: indirect dispatches launch nothing

`parity_drop_gpu` is two bodies — one box falling onto one static. At frame 200 the CPU body rests at y = 0.249 with zero speed. The GPU body is **37 m below it and still accelerating**, pinned at 15.081 m/s, which is `max_linear_speed` (15) plus one substep of gravity. It fell straight through the static and never stopped.

Every GPU scenario reports `contacts 0/262144` for its entire run, and every one peaks at exactly 15.082 m/s — drop, nojoints, jointsonly alike. That number is not agreement. It is the terminal speed of a body that nothing ever touches.

Localised by three probes, each a temporary write into a spare `collision_state` slot so it shows up in the `vbd gpu peak` log line (shaders hot-load, so no rebuild):

1. `vbd_prepare_indirect` publishing its `pair_count` → **1**. The broad phase does find the pair and does write `indirect_args[0].x = 1`.
2. A marker at the very top of `collision_narrow_phase`, before every early-out → **never lands**. The narrow phase does not execute a single thread.
3. `vbd_prepare_contact_indirect` — a *direct* dispatch later in the same substep — reading `indirect_args[0].x` back → **1**. The value is correct in memory, and a shader that reads it through the same bindless slot sees it.

So the args are right and nothing consumes them. The cause is not a barrier or a visibility problem:

```cpp
auto gse::dx12::commands::dispatch_indirect(gpu::handle<gpu::buffer>, gpu::device_size) const -> void {}
```

`Dx12/Commands.cppm:467` is an empty stub. Every indirect compute dispatch on the DX12 backend is silently dropped. `draw_indexed_indirect` and `draw_mesh_tasks_indirect` beside it are implemented; only the compute one was never written.

This is why the machine matters. The doc's earlier findings were taken on an RTX 5090 running Vulkan, where `dispatch_indirect` is real. On the Intel Arc 140V laptop the Vulkan device fails to create, the engine logs `vulkan device unavailable; falling back to dx12 backend`, and from there the whole indirect half of the solver quietly does nothing.

**Which dispatches this kills.** Everything indirect: narrow phase, `freeze_jacobians`, `update_lambda`, `apply_restitution`, and — decisively — the per-colour solve sweeps, both the plain and the jointless variants. What survives is everything dispatched directly: grid build, broad phase, adjacency, colouring, predict, `derive_velocities`, `finalize`, the island solve for jointed bodies, and `update_joint_lambda`.

That split explains the whole picture. In a no-joint scene *every* solve dispatch is per-colour and indirect, so nothing is ever solved and every body free-falls to the clamp. In `parity_jointsonly_gpu` the island path is a direct dispatch, so jointed bodies really are solved — which is why joint penalty reads 115 199 N/m there — while the unjointed test sphere in the same scene falls forever.

## Fixed: restitution applied once per contact point instead of once per impact

Once the indirect dispatches came back, `ParityDrop` — a floor and one box — decelerated but never settled. It sank ~2.4 m past the floor and bounced with *growing* amplitude, peak speed cycling 3.9, 5.0, 0.9, 3.2, 8.4, 4.3, 0.2, 3.5, 7.9, 10.0 on a ~75-frame period. Energy going in.

The CPU updates `ba.velocity` in place after each contact (`Solver.cppm:765`), so the second contact of an impact recomputes `v_rel_after` from the corrected velocity, gets `dv <= 0`, and skips. It self-limits after the first contact.

The GPU accumulated into `v_delta` but recomputed `v_self` from the unmodified snapshot every iteration. A box landing on a box makes four contact points — `contacts 4/262144` on this exact scene — so all four saw the same stale velocity, computed the same `dv`, and each applied it. **Four times the restitution impulse, every bounce.** Fixed by reading `body.velocity + v_delta` when forming `v_self`, which restores the CPU's self-limiting behaviour.

The bug was invisible until `dispatch_indirect` was implemented, because `vbd_apply_restitution` is an indirect dispatch and had never executed on this backend.

## The ladder on DX12, both fixes in

Intel Arc 140V, DX12, 250 frames, warmup 0. Drift is max over all bodies at the final frame; "over" counts bodies past a 0.05 m threshold.

| scene | bodies | CPU peak | GPU peak | final drift | over |
|---|---|---|---|---|---|
| `ParityDrop` | 2 | 5.717 | 5.880 | **0.001 m** | 0 |
| `ParityPair` | 3 | 6.533 | 6.696 | **0.003 m** | 0 |
| `ParityStack` | 9 | 8.655 | 8.655 | **0.006 m** | 0 |
| joints only | 16 | 10.615 | 10.615 | 0.134 m | 2 |
| `ParityPile` | 217 | 5.389 | 6.432 | 7.438 m | 212 |
| `ParityOverlap` | 217 | 7.166 | 5.808 | 9.114 m | 216 |

Stack and joints-only now match the CPU's peak speed *exactly*. Nothing anywhere sits on the 15.082 clamp.

**Both solvers are now bit-deterministic, including at 217 bodies.** Run-to-run drift is 0.000000 m for GPU-vs-GPU and CPU-vs-CPU on both pile and overlap. That retires the earlier "GPU determinism has a boundary between 9 and 217 bodies" finding — that nondeterminism was an artifact of the broken pipeline, not the spatial hash.

It also means the remaining pile/overlap divergence is **not** chaos. Two deterministic solvers disagreeing by 7–9 m is a systematic, perfectly reproducible difference, and every bisection step against it is now repeatable. That is the next thing to chase, and it is a much better position than the same number would have been yesterday.

## Bisecting the pile: the metric is most of the problem

`ParityPile` drift is already 0.0068 m at **frame 0**, on 216 of 217 bodies, before any contact exists. Focus-tracing a body through its free fall explains it: the GPU speed sequence *is* the CPU's, shifted. CPU frame 1 reads 0.490 m/s, GPU frame 4 reads 0.490 m/s, and so on down the column.

**The state dump lags the GPU simulation by 3 frames, alternating 3 and 5.** The CPU mirror reads the retired snapshot slot, so `write_state_frame` records GPU state from three frames ago, and the alternation comes from the slot ping-pong. The comparator is therefore matching CPU[N] against GPU[N−3±1]. For a scene in motion that manufactures drift proportional to speed, which is why drop, pair and stack look perfect — they come to rest, and at rest the lag contributes nothing.

The lag does *not* explain the endgame. Over 1200 frames drift plateaus at ~8.3 m from frame 300 onward while no body exceeds 7 m/s, and three frames at 7 m/s is 0.35 m. Something real is different.

But per-body drift still cannot say what. Both solvers are individually bit-deterministic at 217 bodies, so the 8 m is deterministic chaos: two different algorithms, unavoidable float and ordering differences at frame 1, amplified by a dense pile. Reproducible, and uninformative.

**The aggregate profile is informative.** Max speed across the pile, sampled every 100 frames over 1200:

| frame | 100 | 200 | 300 | 400 | 500 | 600 | 700 | 800 | 900 | 1000 | 1100 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| CPU | 0.53 | 0.57 | 0.57 | 0.63 | 0.59 | 0.79 | **5.18** | 0.95 | **2.22** | 1.12 | 0.61 |
| GPU | 3.72 | 4.57 | 2.93 | 1.59 | 1.41 | 0.31 | 0.65 | 0.55 | 0.45 | 0.33 | 0.27 |

Two separate anomalies, and only one of them is the GPU's:

1. **The GPU pile takes about five times longer to settle** — still churning at 3–4.6 m/s where the CPU is already at 0.53, quiet only around frame 600. This is a gross statistic, not chaos, and it is the real parity defect.
2. **The CPU pile erupts from rest at frame 700**, hitting 5.18 m/s after 600 quiet frames, and again at 900. A settled pile does not spontaneously do that. "The CPU is ground truth" is not safe here and this deserves its own investigation.

Ruled out for the GPU's slow settling:

- **Restitution.** Disabling `vbd_apply_restitution` entirely made frame 300 *worse* (6.41 vs 2.93), not better.
- **Capacity and colouring.** 1406 of 262144 contacts, `dropped 0`, `dup 0`, `fallback 0`, `conflicts 0`.

**The ladder has a hole.** It jumps from 9 bodies at millimetre parity to 217 bodies at 8 m, with nothing in between. A rung at 30–60 bodies is the next thing to build: gradual onset means chaos, and per-body drift should then be retired for piles in favour of settling time and aggregate energy; sharp onset points at a threshold or capacity bug that only bites above some density.

## The count sweep: both solvers erupt from rest

Two rungs added to close the 9 → 217 hole. `ParityCluster` is a 3×3×3 grid (28 bodies), `ParityHeap` a 4×4×4 (65), both at `ParityPile`'s exact 0.75 spacing and 40 m floor, so body count is the only variable. 1200 frames.

| scene | bodies | final drift | over | fraction |
|---|---|---|---|---|
| `ParityStack` | 9 | 0.006 m | 0 | 0% |
| `ParityCluster` | 28 | 5.350 m | 23 | 85% |
| `ParityHeap` | 65 | 4.233 m | 62 | 97% |
| `ParityPile` | 217 | 8.3 m | 215 | 99.5% |

The break is between **9 and 28 bodies**, and it is a step rather than a ramp — which rules out the comfortable answer that this is just chaos amplification scaling with body count. Stack also holds parity for the full 1200 frames, so its earlier pass was not a short-run artifact.

The max-speed profiles say what the drift cannot:

| frame | 100 | 200 | 300 | 400 | 500 | 600 | 700 | 800 | 900 | 1000 | 1100 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| cluster CPU | 0.14 | 0.31 | 0.19 | 0.30 | 0.15 | 0.13 | 0.15 | 0.24 | 0.09 | 0.08 | 0.16 |
| cluster GPU | 0.10 | 0.09 | 0.09 | 0.10 | 0.10 | **1.44** | 0.50 | 0.11 | 0.20 | **2.40** | 0.64 |
| heap CPU | 0.32 | 0.26 | 0.29 | 0.36 | 0.30 | 0.40 | 0.47 | 0.41 | 0.47 | 0.27 | 0.36 |
| heap GPU | 0.33 | **3.62** | 0.34 | **1.60** | **2.88** | **2.14** | **2.41** | 0.79 | 0.51 | 0.78 | **1.93** |

### Not eruptions. Boxes falling off the pile.

An earlier revision of this document read those spikes as settled piles spontaneously gaining energy. `--scan-states-body` was added to settle it and the answer is no. Both spikes are a box toppling off the stack and falling, on both solvers.

The CPU pile body at frame 712 — the 6.957 m/s peak:

| frame | speed | step | y | z |
|---|---|---|---|---|
| 60–600 | 0.18–0.79 | 0.003–0.013 | 2.74 | ~1.9 |
| 660 | 1.146 | 0.019 | 2.662 | 2.653 |
| 700 | 5.179 | 0.086 | 1.379 | 3.949 |
| 712 | 6.957 | 0.115 | 0.270 | 4.453 |
| 713 | 2.609 | 0.052 | 0.238 | 4.494 |

It sits in the pile for 600 frames creeping at millimetres per frame, walks to the edge, tips off, falls 2.4 m with speed and step both ramping smoothly, and lands. Constant horizontal velocity, accelerating vertical. Ordinary projectile motion, and the drop height matches the speed.

The GPU cluster body at frame 953 is the same shape — speed 1.36 → 3.97 while y descends 1.139 → 0.430 and x slides steadily outward.

So there is no eruption defect, on either solver. **What the sweep actually measures is that the GPU sheds boxes at a density where the CPU does not.** At 28 bodies the CPU cluster holds at 0.09–0.31 m/s for the whole run while the GPU cluster loses boxes at frames ~600 and ~950; at 217 both shed. That points at resting stability — creep and friction under load — not at an energy source.

### The dump aliases GPU frames

The GPU trace shows the same simulation frame recorded twice, then a double-sized jump:

```
930  1.357 m/s  0.023617 m  (1.366, 1.139, 0.917)
931  1.441 m/s  0.023617 m  (1.386, 1.126, 0.918)
932  1.357 m/s  0.023617 m  (1.366, 1.139, 0.917)   <- 930 again
933  1.441 m/s  0.023617 m  (1.386, 1.126, 0.918)   <- 931 again
934  1.712 m/s  0.079638 m  (1.451, 1.079, 0.921)   <- three frames of motion at once
```

This is the retired-slot readback: the CPU mirror re-reads the same slot, so `write_state_frame` records frame N−3, N−4, N−3, N−4, then catches up. Every GPU-vs-CPU drift number in this document is computed against a dump that repeats and skips frames this way. It does not explain metres of divergence, but it does mean fine-grained drift comparisons are noise below roughly a tenth of a metre, and the dump should record each simulation frame exactly once before anyone tunes against these numbers.

## What this does and does not invalidate

The Vulkan findings stand. They were measured on a backend where the pipeline is whole, and the CPU numbers reproduce across both machines — `parity_jointsonly_cpu` peaks at 10.615 here, matching what was recorded on the desktop. The joint blowup is still open; it just cannot be seen from the DX12 machine.

What is now suspect:

- **"Parity holds without joints" needs re-checking on Vulkan.** GPU 15.08 against CPU 14.84 is the fingerprint of a free-falling body next to a landing one, agreeing to 1.6% by coincidence of drop height. If that pairing was measured on Vulkan it may be real; the number is close enough to the clamp to deserve a second look.
- **Nothing measured on DX12 says anything about the solver.** On this machine `parity_jointsonly_gpu` peaks at 15.082 (twice, frames 96 and 98 on separate runs) and `parity_drop_gpu` sits 37 m below the CPU's resting body, still at the clamp. Both are the stub, not the physics.
- `colors 0/16` in the log is a **diagnostic artifact** on either backend: colouring only runs on substep 0 and `collision_reset` zeroes the header at the start of substep 1.

This is worth having found on its own terms. Backend parity is a goal in its own right, and a silently stubbed command is exactly the failure mode that survives review — no crash, no validation error, just physics that quietly stops happening on one backend.

## Retained hypothesis for the joint blowup, unverified

If the 374 m/s returns once the pipeline is whole, the number itself is worth reading as arithmetic rather than physics. `max_linear_step` is 0.1 m and is enforced per sweep in `vbd_solve_color.slang:490`. With `solver_iterations = 15` and `physics_substeps = 2` (`System.cppm:127`), sub-step time is 1/120 s, and the island path solves each jointed body **twice** per iteration — a forward and a backward sweep at `vbd_solve_color.slang:552` — plus one post-stabilize sweep:

`31 sweeps × 0.1 m × 120 Hz = 372 m/s`, against a measured 374.

Under that reading the body is not exploding, it is marching: the joint Newton step saturates the clamp on every sweep, so displacement per substep is fixed and the reported velocity parks at a ceiling instead of growing without bound — which is what "200–360 m/s, never decaying" describes. It also re-reads the lambda trace: constant displacement per sweep makes `c` grow linearly, so lambda increments grow linearly and lambda grows quadratically. The recorded increments (21k → 33k → 45k → 58k → 70k) have a constant second difference of ~12k, which is that signature. The lambda growth would be downstream of the marching, not its cause.

Falsifiable, one 250-frame run each, once the scenario is meaningful again:

| change | cost | predicted peak |
|---|---|---|
| iterations 15 → 5 | settings | ~125 m/s |
| substeps 2 → 1 | settings | ~190 m/s |
| `max_linear_step` 0.1 → 0.05 | config | ~190 m/s |
| delete the island backward sweep | shader, hot-reloads | ~190 m/s |

If any of these leaves the peak at ~374, drop the ceiling theory. If they scale, the follow-up question is whether the saturated step points toward the constraint or away from it — measure the signed per-sweep change in constraint error plus the pre-clamp `length(delta_x)`. Toward and overshooting is a limit cycle; monotonically away is a direction error in the Newton assembly, which for jointed bodies always takes the Schur-complement branch since `has_cross_terms` is forced true for any joint (`vbd_solve_color.slang:416`).

## Fixed: `sticking` was never computed on the GPU

The invariants found this in one run, after per-body drift had failed to for a day.

| scene | solver | mean height | max height | shed past reach |
|---|---|---|---|---|
| `ParityHeap` (65) | CPU | 0.976 m | 1.751 m | 0 of 65 |
| | GPU | 0.301 m | 0.749 m | **25 of 65** |
| | GPU + fix | 0.895 m | 1.746 m | 4 of 65 |
| `ParityPile` (217) | CPU | 1.426 m | 2.768 m | 8 of 217 |
| | GPU | 0.332 m | 1.248 m | **99 of 217** |
| | GPU + fix | 0.605 m | 2.738 m | 85 of 217 |

The GPU piles were not diverging, they were **pancaking** — collapsing to a third of their height and spreading half their boxes outside the spawn footprint. Boxes were sliding instead of stacking, which is a friction failure, and it is scale-free in a way no drift number expressed.

The cause: on the GPU, `sticking` has exactly one source, `collision_narrow_phase.slang:979`:

```slang
bool sticking = has_cached && cached_ws.sticking != 0;
```

It reads last frame's value and writes it back at line 1063. Nothing ever *computes* it. The CPU derives it in `end_frame` from the solved contact — normal impulse loaded, tangential gap under `stick_threshold`, tangential lambda inside the friction cone (`Solver.cppm:816`). With no GPU equivalent the flag starts false and can never become true.

What it gates is the tangential penalty floor. For a sticking contact the CPU raises the tangent rows to `contact_effective_mass / h_squared`; otherwise they sit at `penalty_min`, 1.0 N/m. Since GPU `sticking` was permanently false, tangential stiffness was pinned orders of magnitude too low for every resting contact in the world.

This is the third instance of the same bug shape as half-gravity: a field that is read, respected, and never written.

Porting the CPU's condition into the narrow phase, evaluated against the cached contact (which is the previous substep's solved state), restores heap to near-parity — max height 1.746 against the CPU's 1.751 — and recovers the pile's structure, max height 2.738 against 2.768. It does not fully fix the pile, which still spreads 85 boxes against the CPU's 8, so either a second factor remains or the placement matters.

**The placement is a probe, not a considered implementation.** The CPU computes this after the solve, from the just-solved contact. The port computes it before the solve, from the previous substep's cached copy. Those are close but not identical, and the proper home is a post-solve pass ahead of the `contact_buffer` → `warm_start_buffer` copy. Verified no regression on the scenes already at parity: drop 0.001360 m unchanged, pair 0.003147 m unchanged, stack 0.005358 → 0.005492 m, joints-only 0.032000 m unchanged.

## Fixed: `accel_weight` was never refreshed after the seed frame

Chasing the resting-velocity floor produced the largest single improvement of the day. The chain, established by probe rather than by reading:

1. The GPU computes grounded bits correctly. For a box at rest on the floor, `|normal.y|` is 1.0, the `c.normal.y < -0.7` test fires, and the body's grounded bit is set.
2. The CPU reads them back and derives `body_airborne` correctly.
3. `accel_weight` never gets back to the GPU. `vbd_apply_body_inputs` copies CPU input for a dynamic body **only** when `apply_all_body_inputs` is set — that is, on a reseed. Otherwise dynamic bodies keep their GPU-resident state.

So `accel_weight` is frozen at whatever it was when the body was seeded, which is 1, for the body's entire life. Probing `1 - accel_weight` and taking the run maximum returned **0.000** over 200 frames: it never once left 1, including the 150 frames the body spent motionless.

`vbd_predict` therefore applies a full gravity step into `predicted_position` every substep for every resting body. The solve cancels it, the body does not move — five microns per frame — but `(predicted - old) / dt` still reports `g * h` = 9.81/120 = **0.082 m/s**. Above the 0.05 m/s `velocity_sleep_threshold`, so `sleep_counter` never accumulates and **nothing on the GPU could ever sleep**.

Forcing the weight to 0 is *not* the fix — it removes the floor but reintroduces half-gravity, peak fall speed dropping to 4.083 m/s against the CPU's 5.717. The weight has to stay adaptive; it just has to actually arrive.

**The first attempt at making it arrive was wrong, and the way it was wrong matters.** Refreshing `accel_weight` from the CPU input in `vbd_apply_body_inputs` fixed the floor and produced bit-exact drop parity — and silently destroyed determinism. Two GPU runs of `ParityMound` that had been identical came back 9.645 m apart, with mean height wandering between 0.866 m and 1.126 m across three runs. Disabling just that branch restored 0.000000 m.

The cause is structural: that route feeds the simulation a value derived from a GPU→CPU readback whose latency depends on scheduling, so *which frame* a given grounded state lands on varies run to run. The value was always being uploaded; it simply had never been consumed, so the nondeterminism was latent.

The correct fix computes the weight on the GPU, in `vbd_predict`, from the grounded bits that `collision_build_adjacency` already wrote **earlier in the same substep**:

```slang
body.accel_weight = ((grounded_bits[bi / 32u] >> (bi % 32u)) & 1u) != 0 ? 0.0 : 1.0;
```

No readback, no CPU round trip, no lag — and strictly fresher than the CPU's own value, which is a frame behind. Determinism returns to 0.000000 m and the resting floor stays gone.

**The lesson worth keeping: a correctness fix that routes per-frame state through a readback buys nondeterminism.** Determinism is not a nice-to-have here; it is what makes every other measurement in this document bisectable. Check it after any change that adds a CPU→GPU dependency.

### What it bought

Aligned drift at frame 200, 250-frame runs:

| scene | before | after |
|---|---|---|
| `ParityDrop` | 0.001360 m | **0.000000 m** |
| `ParityPair` | 0.003147 m | **0.000004 m** |
| `ParityStack` | 0.005358 m | **0.001947 m** |
| joints only | 0.032000 m | 0.032000 m |

Drop is now bit-exact against the CPU.

Invariants over 1200 frames, showing all three shader fixes in sequence:

| | CPU | GPU orig | + sticking | + accel_weight |
|---|---|---|---|---|
| **heap (65)** settled | never | never | never | **frame 1109** |
| mean height | 0.976 m | 0.301 m | 0.895 m | **0.976 m** |
| max height | 1.751 m | 0.749 m | 1.746 m | **1.749 m** |
| shed | 0 of 65 | 25 of 65 | 4 of 65 | **0 of 65** |
| **pile (217)** mean height | 1.426 m | 0.332 m | 0.605 m | 1.074 m |
| max height | 2.768 m | 1.248 m | 2.738 m | 2.751 m |
| shed | 8 of 217 | 99 of 217 | 85 of 217 | 42 of 217 |

`ParityHeap` now matches the CPU on every invariant, and is the first GPU scene ever to report settling. The pile is much improved and still short: mean height 1.074 against 1.426, and 42 boxes shed against 8.

### The stress scene, all four fixes in

The original target: `Sandbox` with the tumblers, 1077 bodies, 2400 frames.

| | CPU | GPU orig | GPU all fixes |
|---|---|---|---|
| mean height | 7.465 m | 6.215 m | 6.750 m |
| max height | 15.593 m | 15.571 m | 15.571 m |
| final max speed | 11.499 m/s | 12.325 m/s | 11.912 m/s |
| p50 frame | 16.9 ms | 27.7 ms | **22.3 ms** |

Max height matches. Mean height closed about 43% of its gap and remains ~10% low. Neither solver settles, which is correct — the tumblers are powered, so the scene is continuously driven and "never settles" is the expected reading rather than a defect.

The GPU also got **20% faster**, 27.7 ms to 22.3 ms p50, purely because bodies can now sleep and be skipped. That had been impossible for the entire life of the GPU path.

Peak speed reads 15.082 m/s on both GPU runs, which is `max_linear_speed` plus a substep of gravity — saturated, so not a usable comparison against the CPU's 14.842.

### Closed: the ramp, with `sticking` post-solve and `accel_weight` GPU-local

Moving the `sticking` computation into its own post-solve pass ahead of the warm-start copy — mirroring the CPU's `end_frame` exactly — together with the GPU-local `accel_weight`, closes the density ramp.

| scene | bodies | CPU mean h | GPU mean h | GPU/CPU | CPU shed | GPU shed |
|---|---|---|---|---|---|---|
| heap | 65 | 0.976 m | 0.976 m | 100% | 0 | 0 |
| mound | 126 | 1.196 m | 1.234 m | 103% | 4 | **0** |
| pile | 217 | 1.426 m | 1.388 m | 97% | 8 | 12 |

Against the pre-fix ramp of 100% / 90% / 75% with shed counts of 0 / 10 / 42. Max height continues to track within 2 cm at every rung.

Aligned drift on the settling scenes at frame 200, all now sub-millimetre:

| scene | original | now |
|---|---|---|
| drop | 0.001360 m | 0.000414 m |
| pair | 0.003147 m | 0.000006 m |
| stack | 0.005358 m | 0.000998 m |
| joints only | 0.367784 m | 0.032000 m |

Stress, 1077 bodies over 2400 frames: max height 15.571 m against 15.593 m, and **p50 frame time 17.9 ms against the CPU's 16.9 ms**, down from 27.7 ms before any of this. Most of that came from bodies finally being able to sleep.

Its mean height reads 6.416 m against 7.465 m, but treat that number carefully: stress is continuously driven by powered tumblers and never settles, so a final-frame mean is a snapshot of a chaotic system rather than a stable statistic. A time-averaged version would be the honest instrument, and the summary mode does not yet compute one.

### Historical: the residual as a ramp

`ParityMound` (5³, 126 bodies) was added to bisect the band between heap and pile. All grids share 0.75 spacing and a 40 m floor, so count is the only variable.

| scene | bodies | CPU mean h | GPU mean h | GPU/CPU | CPU max h | GPU max h | shed CPU | shed GPU |
|---|---|---|---|---|---|---|---|---|
| heap | 65 | 0.976 m | 0.976 m | **100%** | 1.751 m | 1.749 m | 0 | 0 |
| mound | 126 | 1.196 m | 1.072 m | 90% | 2.256 m | 2.249 m | 4 | 10 |
| pile | 217 | 1.426 m | 1.074 m | 75% | 2.768 m | 2.751 m | 8 | 42 |
| stress | 1077 | 7.465 m | 6.750 m | 90% | 15.593 m | 15.571 m | — | — |

**Gradual, not a switch.** The gap opens smoothly from 65 to 217, so this is a per-contact effect that accumulates rather than a threshold or capacity limit. Stress at 1077 bodies scoring better than pile at 217 says the driver is density, not count — the pile is a solid packed block, stress is spread out.

**And it is spreading, not compressing.** Max height tracks the CPU almost exactly at every rung (2.249 vs 2.256, 2.751 vs 2.768), so the core column stands at the right height and the stack is not sinking into itself. What differs is the mean and the shed count: 10 against 4 at mound, 42 against 8 at pile. Outer bodies slide out of the base while the middle holds. The GPU pile is also *quieter* than the CPU's while sitting lower — final max speed 0.177 against 0.713 at mound — so it is settling into a wider, flatter arrangement rather than churning.

That is still a friction signature, which points back at the `sticking` fix being placed approximately. It is currently computed in the narrow phase from the previous substep's cached contact; the CPU computes it after the solve from the just-solved one. Moving it to a post-solve pass ahead of the `contact_buffer` → `warm_start_buffer` copy is the next concrete step, and this ramp is the measurement to re-run against it.

## Superseded: the 0.082 m/s floor

Tracing the worst-drift body in the stress scene turned this up. Both solvers park it on the floor and leave it there, 15.6 m apart — ordinary chaos out of a tumbler. What is not ordinary is the velocity each reports while parked:

| | CPU | GPU |
|---|---|---|
| resting position | (-20.809, 1.008, -5.689) | (-18.149, 0.998, 9.692) |
| resting speed | 0.003 m/s | **0.082 m/s** |
| step per frame | 0.000000 m | 0.000005 m |

The GPU body is genuinely stationary — five microns per frame — yet reports 0.082 m/s. That number is `gravity * substep`: 9.81 / 120 = 0.0817. Every resting body on the GPU carries exactly one substep of gravity in its derived velocity. `ParityDrop` shows the same floor: the CPU settles to 0.000 while the GPU sits at 0.082 forever.

**`velocity_sleep_threshold` is 0.05 m/s** (`System.cpp:582`). A resting GPU body reports 0.082, which is above it, so `sleep_counter` never accumulates and **no body on the GPU can ever go to sleep**. A resting CPU body reports 0.003 and sleeps immediately.

That is a behavioural fork, not a numerical one, and it very likely explains the shedding:

- A CPU pile sleeps and freezes solid. The pyramid scene is the extreme case — perfectly packed, it sleeps within two frames and reports 0.000 m/s for 600 straight frames, never testing the solver at all.
- A GPU pile never sleeps, so it is solved every substep forever, keeps creeping at millimetres per frame, and boxes eventually walk off the stack. That is precisely the mechanism the `--scan-states-body` traces showed at frames 600 and 950 in the cluster.

It also means every GPU body is solved every frame regardless of how long it has been still, which is pure wasted work at scale.

Two things to establish next: whether the residual is `accel_weight` staying 1 for grounded bodies (making `vbd_predict` apply a full gravity step that the solve then cancels, leaving `predicted - old` at `g*h²`), and whether the fix belongs in the derived velocity or in the sleep test. Do not simply raise the sleep threshold above 0.082 — that hides the residual and changes sleeping behaviour for genuinely slow-moving bodies.

## Alignment: what it did and did not absorb

Running the ladder with `--compare-states-align 10`, drift at frame 200:

| scene | naive | aligned | shift histogram |
|---|---|---|---|
| drop | 0.001360 m | 0.001360 m | flat |
| pair | 0.003147 m | 0.003147 m | flat |
| stack | 0.005358 m | 0.005358 m | flat |
| joints only | 0.367784 m | **0.032000 m** | sharp peak at +5 |
| pile | 6.723296 m | 6.722611 m | +0 wins 176 frames |
| overlap | 9.578510 m | 9.578510 m | +0 wins 137 frames |

Three distinct outcomes, and the histogram tells you which you are in.

**Settled scenes**: flat histogram, drift unchanged. Once nothing moves every shift scores the same, so the argmin is arbitrary — exactly what a scene at rest should look like.

**Joints only**: a sharp peak at +5 and an 11.5× drop, 0.368 m to 0.032 m. Its divergence was almost entirely the readback offset. The joint path on DX12 is far closer to parity than the naive number claimed, which fits its peak speed matching the CPU's 10.615 m/s exactly.

**Piles**: shift 0 wins the large majority of frames and the aligned number is unchanged to four decimals. **The pile divergence is genuine.** The earlier worry that dump aliasing was poisoning the pile numbers was wrong — it poisons joints-only, not the piles.

## Two regimes, two metrics

The ladder was built as if one number — per-body drift — answered the question at every rung. It does not, and treating both halves as the same measurement is what produced two confident wrong theories about the pile.

**Scenes that come to rest** (`ParityDrop`, `ParityPair`, `ParityStack`, joints-only) are deterministic and admit exact comparison. Both solvers settle to the same arrangement, the readback lag contributes nothing once nothing is moving, and drift lands in millimetres. Hold these to an exact number and treat any regression as a real defect. This is the regime that found both of today's bugs.

**Scenes that stay in motion** (`ParityCluster`, `ParityHeap`, `ParityPile`, `ParityOverlap`) are chaotic. Two implementations differing in contact ordering, Jacobian freezing and scheduling will disagree in the last bits at frame 1, and a dense pile amplifies that exponentially. Demanding low per-body drift here is chasing Lyapunov amplification: the number will never come down, and every wiggle in it invites a story. Score these on invariants that do not care which box ended up where — settling frame, mean and max height, bodies shed past the spawn reach, final max speed. `--scan-states-summary` reports exactly those.

The empirical case for the split: **the piles have found no bugs.** Both real defects — the `dispatch_indirect` stub and restitution applying per contact point — surfaced on `ParityDrop`, two bodies. The piles produced the half-gravity-style stories twice and cost the most time. What the piles *did* legitimately show, once measured as invariants rather than positions, is that the GPU sheds boxes off a 28-body cluster where the CPU holds. That is a stability finding, and it survives because it does not depend on any particular box.

### Alignment, and what it is covering for

`--compare-states-align N` matches each A frame against the best B frame within N, and prints which shift won per frame. It exists because the GPU dump lags by three frames and records each simulation frame twice before skipping, so a naive frame-to-frame comparison measures the sampling offset rather than the physics. The aligned number is a *lower bound* on true drift, which is the right question for "could these be the same trajectory".

It is a workaround. The proper fix is to stamp each record with the simulation step its state came from, rather than the bench frame counter it happened to be written on — `write_state_frame` currently takes `state.frames_in_phase`, which for the GPU path is unrelated to the step the transforms hold. Until that lands, read the shift histogram: if it is spread across several shifts, the dump is aliasing and sub-decimetre drift means nothing.

### The gold standard, not yet built

Everything above still compares trajectories. The instrument that would end the argument compares **steps**: drive both solvers from the same pre-step state, record the one-step residual per body, then resync so they can never drift apart. Chaos needs time to amplify; deny it time and 217 bodies stops being an amplifier and becomes 217 independent samples of "does one step agree". A per-step residual at 1e-6 across a pile would prove every trajectory divergence in this document is chaos and close the question; anything larger localises a real defect with no amplification in the way.

`physics::data` already holds both `vbd_solver` and `gpu_solver`, and the GPU upload path already has `m_apply_all_body_inputs` for forcing full body state in, so the plumbing exists. It needs a shadow mode that steps the GPU from the CPU's pre-step state each frame, compares, and discards.

## Rules the measurements depend on

**Warmup must be 0.** The default 120 discards the entire divergence onset. With warmup the CPU-vs-GPU number on `physics_stress` was *smaller* than GPU-vs-GPU, i.e. pure noise.

**Runs must be long enough to reach the mechanism.** The tumbler turns at 0.6 rad/s, so a revolution takes ~10 s. Every run under ~600 frames leaves the drums essentially stationary, and a whole afternoon of "the GPU looks fine" came from 400-frame runs that never tumbled. Use 2400 frames when the tumbler matters.

**Measure the property, not the mechanism you suspect.** Four hypotheses about the colouring fallback were instrumented and refuted in turn. What settled it was measuring the invariant directly — same-colour contact pairs — instead of the mechanism each guess implied. The same rule found the indirect-dispatch failure: the question that cracked it was not "why does the narrow phase reject this pair" but "does the narrow phase run at all".

**A clamp value is not a measurement.** Two solvers agreeing at 15.08 m/s when `max_linear_speed` is 15 is a coincidence of saturation. Check whether a matching number is a limit before reading it as parity.

**Never conclude from one run.** `conflicts` as a high-water mark varies 0–27 across identical runs. It now also reports a per-run total, which is far more stable.

**Instrumentation cost is not free.** A duplicate-contact check added to `collision_build_coloring` cost **35 ms/frame** — a 760x regression on that stage — because it was O(Σcc²) over full `contact_constraint` loads and that stage dispatches as a *single workgroup*. It invalidated every GPU timing measured while it was in. It did not affect correctness results, because the fixed-step clock makes the simulation wall-clock-independent.

## Fixed and verified

**Half gravity.** `build_bodies` uploaded `.accel_weight = 0.f` and no shader ever wrote that field, while `vbd_predict.slang` uses it to scale `gravity_step` into `predicted_position`. Gravity reached bodies only through the solve and converged to about half: on a single falling box the GPU climbed 9.81/120 per frame against the CPU's 9.81/60. Now `airborne ? 1.f : 0.f` driven by `d.body_airborne`, matching the intent of the CPU's `clamp(-accel_y/g, 0, 1)`. Do **not** hardcode 1 — resting bodies then guess into the ground every step.

**Origin teleport.** `latest_snapshot_slot()` returned the in-flight slot. Split into `retired_snapshot_slot()` (`m_dispatch_slot`, for CPU reads, fence-safe) and `render_snapshot_slot()` (`1 - m_dispatch_slot`, for GPU reads). `PhysicsDebugRenderer` had been working around this locally with `1u - latest_snapshot_slot()`. Costs the CPU mirror ~2 frames of lag, which shows up in the comparator as a one-frame phase offset and occasional repeated velocity samples — read focus traces with that in mind.

**Contact blowup on spawn.** On a reseed, `id_to_body_index` is rebuilt from motion order so every body index shifts, but `warm_start_buffer` holds contacts naming `body_a`/`body_b` under the *old* numbering — cached lambda and penalty then get applied between unrelated bodies. Fixed by writing a zeroed `contact_constraint` to `warm_start_buffer[0]` in `commit_upload` when `m_apply_all_body_inputs` is set, which trips the lookup's sentinel for that one frame.

## Still standing

**The CPU does apply restitution** (bounces 6.533 → 1.332 m/s, ratio 0.20 against a 0.3 setting). An earlier claim that it did not came from a focus window that began after the CPU's impact.

**Warm starts are live**, contrary to an earlier note. `warm_start_lookup`'s `ws_count` parameter was never referenced in its body; the scan is bounded by `max_contacts` and terminated by the zeroed-slot sentinel, so `m_warm_start_count` being 0 gated nothing. The dead parameter has been removed. The measured hit counts are stale, but the reasoning about the mechanism is not.

## Worth fixing while in here

**Joint colouring conflicts are invisible.** The conflict scan at `collision_build_coloring.slang:210` walks contacts only. Colouring does consider joint adjacency when assigning, but the fallback at line 177 shoves uncoloured bodies into the motor-only colour, which can place two joint-connected bodies in the same colour. One extra loop over joints makes that measurable.

**Jointed bodies take two solver topologies in one substep** — islands in the main loop, plain colours in post-stabilize (`GpuSolver.cpp:1469`). Since colouring already accounts for joints, it is worth asking whether the island path earns its keep; retiring it would remove the double sweep as well.

**Islands run concurrently with no ordering on shared bodies.** Contacts between two islands, or between an island body and a jointless body, are read-modify-written by two dispatches at once. Irrelevant to joints-only, likely relevant to `physics_stress` — and currently masked, since contacts are not being solved at all.

## Traps paid for

**`archive_raw` on the dump record.** Without it, `state_dump_record` is a class with reflected members, so `binary_writer` takes its *schema* path. Schema emission is deduplicated per writer instance, and the dump constructs a fresh writer each frame — so the schema is re-emitted before every frame's records, the reader caches it once, and everything after frame 0 is misparsed into plausible-looking garbage. `read_state_dump` now also validates that the payload is an exact multiple of the record size.

**`--scan-states` exists because drift was the wrong instrument.** Judging "does it blow up" by comparator drift conflates a genuine explosion with ordinary nondeterminism, and led to the wrong conclusion that joints were not required.

**Diagnostics degraded by the substep reset.** `conflicts` and `max_used_color` read 0 on multi-substep runs, because the `collision_state` readback moved to end-of-dispatch so it could capture joint values written during the solve, while `collision_reset` zeroes the header at the start of every substep and colouring only runs on substep 0. Anything written by a pass that runs on substep 0 only is lost. The joint `c` slot reports `|d|` rather than constraint error. All restorable; none load-bearing.

**The joint probe reports joint 0**, which in the joints scene is a healthy fixed joint holding at 0.5 m with zero lambda. Any joint investigation needs it to follow the joint with the largest `|pos_lambda|` instead — pack the magnitude and joint index into one slot with `InterlockedMax`, then hardcode that index for the correlated trace on the next run.

## Next

1. ~~Implement `dispatch_indirect` on DX12.~~ Done — command signature, the `compute_pso_bound` guard, `ExecuteIndirect` count 1.

   **No resource transition is needed, and adding one would be a bug.** Every non-acceleration-structure buffer in this backend is created on an `UPLOAD` or `GPU_UPLOAD` heap (`Device.cpp:1592`), and those are fixed at `GENERIC_READ`/`COMMON` for their lifetime — they cannot be transitioned at all, and buffers in `COMMON` are promoted implicitly. `GENERIC_READ` already includes `INDIRECT_ARGUMENT`. The draw-indirect paths have no state gap for the same reason.
2. ~~`ParityDrop`: find why the bounce gains energy.~~ Done — restitution was applying once per contact point.
3. ~~Add a single-dump body trace.~~ Done — `--scan-states-body <id>`, and it retired the eruption theory in one run.
4. **Stamp the dump with the simulation step, not the bench frame.** `--compare-states-align` covers for the aliasing but does not fix it. `write_state_frame` needs the step index the transforms actually hold, which for the GPU path means plumbing the retired snapshot's generation out of `gpu_solver`.
5. **Resting stability is the open question.** The GPU sheds boxes off a 28-body cluster where the CPU holds. Trace a body that walks off, backwards through its creep phase, and compare its contact tangent forces against the CPU's — friction under load is the first thing to read, not an energy source.
6. **Build the shadow-step harness** described above. It is the only instrument that makes a 217-body scene informative rather than merely expensive.

**Watch it directly.** The parity scenes are all headless, but bench mode loads any registered scene windowed, with the fixed-step clock engaged so frame numbers match the dumps exactly:

```
Sandbox.exe --engine-bench-enabled --engine-bench-scene ParityCluster --engine-bench-warmup-frames 0 --engine-bench-frames 1200 --engine-use-gpu-solver
```

Drop `--engine-use-gpu-solver` for the CPU, and swap the scene for `ParityPile`.
4. Re-run the ladder on Vulkan and record it beside the DX12 column. Both fixes matter there too: the restitution bug is backend-independent and was simply masked on DX12 by a dispatch that never ran.
5. Re-open the joint question. `parity_jointsonly_gpu` now matches the CPU's peak exactly on DX12, so the prediction table needs a Vulkan run to have anything left to explain.

**Turn on the DX12 debug layer for a run.** A stubbed command is invisible to every test that only checks for crashes, and an `ExecuteIndirect` against a resource in the wrong state is exactly what the layer exists to report.
