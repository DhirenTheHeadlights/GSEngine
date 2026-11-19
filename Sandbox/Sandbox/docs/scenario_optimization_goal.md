# Goal: Optimize every scenario without changing fidelity

## Aim
Make every scenario in `Sandbox/Sandbox/Source/Scenarios/Scenarios.cppm` cheaper to run, CPU and GPU, without changing what it simulates or renders. The work is driven by evidence: profiles, the generated x86 assembly of hot functions, and the compiled GPU code (Slang to SPIR-V/DXIL, with register, occupancy and instruction counts where the tools expose them). Major architectural decisions go to the owner.

## Scenario tiers
- **Perf tier (measured for speed):** `physics_stress`, `physics_grow`, `grass_sleep_bench`, `grass_render`, `grass_walk`, `grass_rigid_render`, `render_stress`, `sky_render_bench`, `light_hall_bench`, `carousel_bench`, `machines_bench`, `dominoes_bench`, `swings_bench`, `character_gpu_bench`, `character_gpu_bench_headless`, `pyramid`, `pyramid16k_cpu`.
- **Correctness tier (must keep passing and keep their hashes):** all `rollback_*` pairs, all `parity_*`, `scene_reset`, `scene_reset_cpu`, `lighting_parity`, `textured_parity`, `locomotion`, `physics_stress_via_input`.
- **Showcase/probe tier** (`*_showcase`, `record_clip`, `*_horizon_probe`, `grass_look/pan/stride_look`, `web_*`, `relaunch_probe`, `net_walk`): these must still run to completion. Speed is reported, never optimized against.

## Pass criteria
1. **Baseline recorded:** the first steering task records p50/p95 and the world-state hash for every perf-tier scenario, over 3 runs on an idle machine, in the ledger.
2. **Speed:** every perf-tier scenario reaches p50 ≥ 15% below its baseline, holding over 3 consecutive runs, with p95 no worse than baseline. A scenario is exempt when the ledger shows a profile in which its top 5 self-time CPU entries and top 5 GPU passes have each been inspected (assembly or GPU code) and recorded as fidelity-bound or needing an owner decision.
3. **Fidelity, hashes:** every correctness-tier scenario passes and every world-state hash matches the baseline. A hash may change only with owner approval, recorded in the ledger with the reason (for example, an approved float reassociation).
4. **Fidelity, settings:** no scenario pin, quality setting, resolution, entity count, iteration count, present mode or feature toggle is changed to reach a number.
5. **Fidelity, visuals:** `lighting_parity` and `textured_parity` keep their hashes. For renderer changes the owner signs off a showcase clip. This criterion is owner-judged.
6. **Builds:** the release build is clean, and the correctness tier passes on the final tree.

## Charter (no need to ask)
- **Profile and inspect freely:**
  - Run scenarios and turn diagnostics on with `settings` overrides only.
  - Disassemble built binaries with the toolchain's objdump/addr2line, read GCC `-S`/opt-report output, and read the Slang compiler's SPIR-V/DXIL output, run outside the editor's build directory and module cache. These are read-only inspections.
  - Use Nsight or GPU timestamp queries where they are available.
- **Local optimizations anywhere in `Engine/` and `Sandbox/`:**
  - Hoisting, removing redundant work and copies, cache-friendlier layouts inside one system, avoiding allocations in hot loops, devirtualization, reserve/reuse, SIMD-friendly loops, and removing dead passes or work whose result is unused.
  - Barrier and dispatch tightening, shader ALU and memory cuts, reducing register pressure, LDS/shared-memory use, and push-constant and descriptor churn.
  - Every change must leave all correctness-tier hashes unchanged.
- **Compiler flags:** per-target flag changes, such as LTO or `-march` within the owner's machine baseline, may be tried as experiments. They are adopted only with owner approval, because they affect the whole tree.
- **Process:**
  - Each task is one hotspot with before and after numbers over 3 runs each.
  - Experiments that don't pay off are reverted, and every attempt is recorded in the ledger.
  - Follow `docs/CODE_REVIEW_GUIDE.md`, `docs/STYLEGUIDE.md` and `docs/GRAPHICS_REVIEW.md`. Use the units system throughout. No comments in code.

**Off limits without asking (escalate):**
- **Major architecture:**
  - new or removed systems;
  - changing the frame graph's `inline_all` resume policy, `gse.gpu_record` round semantics, or task-graph scheduling policy;
  - ECS storage model changes;
  - render-graph structure changes beyond merging or reordering passes;
  - new threads or queues;
  - replacing an algorithm with a different one (for example, a broad-phase scheme or shadow technique);
  - cross-system data-ownership moves.
- Any change to a world-state hash, solver math ordering that alters results, precision changes (fp32 to fp16 and the like), or approximations.
- Editing `settings.ini`, scenario pins, or expected hashes.
- Whole-tree build flags, toolchain changes, or new dependencies.
- Commits.

## Steering order
1. Baseline every perf-tier scenario and record it (criterion 1).
2. Rank scenarios by absolute cost and rank shared hotspots: code paths that appear in several scenario profiles, such as the geometry collector, the VBD solver, the forward pass and RT shadows, come first.
3. Iterate one hotspot per task: profile, inspect the assembly or GPU code, change it, measure, check hashes.
4. Escalate architectural options as they come up, with measured potential gains.
5. Check fidelity at the end and get the owner's clip sign-off.

## Assumptions
- `gse_run` runs on the owner's machine, and performance is judged there.
- Steering and apply phases have shell access for read-only disassembly tools. If not, steering records a capability gap and falls back to profiles plus source-level reasoning.
- The baseline hashes are deterministic across 3 runs. Any scenario that is not deterministic is listed in the ledger and excluded from criterion 3's hash check rather than "fixed".
- Each scenario's hashes are recorded at baseline.

## Ledger

- 2026-11-09 — Steering run 1. Goal record relocated to `Sandbox/Sandbox/docs/` (task 110); no code changed, no measurements yet. Verdict: setup done; next task is the criterion-1 baseline (perf tier p50/p95 + hashes ×3, correctness-tier hashes ×3).
- 2026-11-09 — Task: criterion-1 baseline. No code changed. Binary `.generated/out/build/x64-mingw-gcc-Release/Sandbox/Sandbox.exe` (built 2026-11-09 15:05Z), unchanged for every run below. Runs used `gse_run scenario=<name>`, attached to the editor, with no `settings` overrides, between 16:14Z and 19:13Z.
  - **Method.** Sweep 1 (16:14–17:46Z) gave the timings and the profiles. The log directory keeps only about 43 run logs, and the world-state hash is printed only in the run log: `Bench.cpp` `finish_bench` writes no hash to the `.txt`/`.gsprof`. So hashes were taken from sweep 2 (17:49–19:13Z), harvested from the logs after every batch. Sweep 2 timings are listed too. The p50/p95 values come from `%LOCALAPPDATA%/GSE/profile/bench/<scenario>/<stamp>.txt`.
  - **`pyramid` is not a scenario name.** The catalogue has `pyramid_cpu` and `pyramid_gpu`. `gse_run scenario=pyramid` exits 1 with "unknown scenario". Both variants are baselined below in its place.

  **Perf tier.** Timings are p50 / p95 in ms for each run. The hash was identical in all 3 sweep-2 runs unless noted.

  | scenario | sweep 1 runs 1, 2, 3 | sweep 2 runs 1, 2, 3 | hash |
  |---|---|---|---|
  | physics_stress | 18.61/22.74, 18.33/22.66, 19.30/23.28 | 17.62/21.64, 18.28/22.19, 19.16/22.26 | 0x455de8965ccf67bc |
  | physics_grow | 32.00/41.81, 31.00/42.08, 31.99/42.04 | 31.82/42.52, 31.98/42.34, 31.92/42.77 | 0x1e4a89cda6843dc3 |
  | grass_sleep_bench | 6.83/7.59, 6.57/7.52, 6.82/7.88 | 6.59/7.60, 6.63/7.60, 6.69/7.95 | 0x969384b052c1d56e |
  | grass_render | 125.59/129.06, 124.89/128.86, 125.48/129.07 | 125.10/128.79, 125.88/129.26, 125.38/129.25 | 0xdac13405ea41e3f8 |
  | grass_walk | 176.98/192.28, 175.96/191.27, 175.71/190.34 | 175.97/191.04, 176.46/190.70, 176.35/191.48 | 0x146fc40d0016fd69 |
  | grass_rigid_render | 71.27/98.12, 74.15/106.09, 74.19/103.05 | 76.79/104.70, 72.71/94.89, 78.77/106.16 | 0x969384b052c1d56e |
  | render_stress | 47.73/61.60, 50.37/62.65, 50.16/66.90 | 43.01/63.30, 45.47/59.05, 44.12/66.60 | 0x2745a999bec5f71d |
  | sky_render_bench | 15.82/27.70, 18.03/29.32, 15.45/19.16 | 15.40/18.61, 15.66/20.46, 16.42/20.96 | 0x7c3c5142ab7c6f94 |
  | light_hall_bench | 243.94/306.84, 246.88/276.01, 242.57/255.74 | 242.52/254.01, 242.61/253.97, 242.20/253.84 | 0x709fdf1d9d9ff152 |
  | carousel_bench | 46.06/55.40, 49.90/58.26, 52.84/65.30 | 49.70/61.25, 52.78/65.51, 52.55/66.32 | 0xe6d4e9bf9aeb2a33 |
  | machines_bench | 0.548/1.037, 0.535/0.966, 0.535/0.980 | 0.554/0.940, 0.551/0.930, 0.609/1.096 | 0x6e1f5fc2568f02d5 |
  | dominoes_bench | 1.505/2.444, 1.512/2.356, 1.469/2.529 | 1.468/2.408, 1.366/2.355, 1.375/2.285 | 0x0345f78bd394d786 |
  | swings_bench | 0.996/1.487, 1.083/1.545, 0.975/1.497 | 1.043/1.524, 1.003/1.536, 1.068/1.659 | 0x98ed405f0dfd1f15 |
  | character_gpu_bench | 23.16/26.27, 23.17/26.21, 23.29/26.24 | 22.90/25.89, 24.10/32.91, 26.35/31.08 | 0x838486fe3dbffc2a |
  | character_gpu_bench_headless | 2.237/2.579, 2.253/2.649, 2.253/2.638 | 2.262/2.629, 2.231/2.633, 2.268/2.607 | 0x432e034fc18d7ab3 ×3; sweep 1 run 1 was 0x3d8f136157fa1443 |
  | pyramid_cpu | 5.99/64.26, 6.28/68.99, 6.40/65.05 | 5.36/65.78, 6.44/69.56, 6.42/68.95 | 0x1db83c6c0972dc68 |
  | pyramid_gpu | 0.380/20.04, 0.326/20.06, 0.322/21.73 | 0.442/20.80, 0.470/20.50, 0.383/19.27 | 0xf4b01fb73cd4afc7 |
  | pyramid16k_cpu | 83.70/1596.4, 84.10/1601.3, 84.14/1603.5 | 83.89/1604.9, 83.48/1601.9, 83.94/1643.8 | 0x828fe2ae85bc729f |

  Sweep-1 hashes seen before the logs rotated matched sweep 2 for physics_stress, machines_bench, dominoes_bench, swings_bench and pyramid_gpu. `grass_sleep_bench` and `grass_rigid_render` share a hash, so the world-state hash apparently does not see the grass or rigid-body difference between those two scenes; this is not yet investigated. Timing spread between sweeps is large enough to matter for criterion 2: render_stress p50 moved about 10% (sweep 1 at 48–50 ms, sweep 2 at 43–45 ms), and carousel_bench and character_gpu_bench moved 5–10%. Judge criterion 2 against both sweeps, not one number.

  **Perf tier, top 5 entries from sweep 1 run 1.** CPU values are self-time p50 in ms; GPU values are timestamp-query EMA in ms. "—" means the report has no GPU pass section.

  | scenario | top 5 CPU self time | top 5 GPU passes |
  |---|---|---|
  | physics_stress | vbd::gs_color_iter 17.9, sched::run_wait 16.9, task::fanout_wait 14.3, vbd_cpu::broad_phase::pair_loop 12.8, vbd::contact_dual_update 12.4 | — |
  | physics_grow | begin_frame::wait_fence::compute 27.5, vbd_solve_iterations_stage 16.2, vbd_narrow_phase_stage 4.7, record_pass::acquire 2.3, vbd_broad_phase_stage 1.6 | vbd_solve_iterations 3.65, vbd_narrow_phase 2.66, vbd_broad_phase 0.84, vbd_build_coloring 0.63, vbd_build_adjacency 0.57 |
  | grass_sleep_bench | sched::run_wait 6.2, task_graph::resume 3.5, physics::tick_gpu 2.4, physics::prepare::specs::scan 0.56, scheduler::render 0.20 | — |
  | grass_render | present::timed 123.0, grass forward_pass 87.4, grass depth_pass 15.7, forward::frame 7.8, taa::frame 3.0 | grass forward_pass 87.4, grass depth_pass 15.7, forward 7.7, taa 3.0, sdf_grid 0.78 |
  | grass_walk | present::timed 171.8, grass forward_pass 121.8, grass depth_pass 24.5, vbd_solve_iterations_stage 12.0, forward::frame 11.3 | grass forward_pass 121.7, grass depth_pass 25.0, forward 11.2, vbd_narrow_phase 2.5, taa 2.3 |
  | grass_rigid_render | geom_collect::collect_chunk 60.6, present::timed 36.8, sched::run_wait 28.1, task::fanout_wait 20.9, forward::frame 17.8 | forward 9.5, taa 2.1, depth_prepass 1.6, physics_transform 1.3, sdf_grid 0.80 |
  | render_stress | present::timed 28.5, vbd::gs_color_iter 16.7, sched::run_wait 16.6, task::fanout_wait 13.8, vbd_cpu::broad_phase::pair_loop 13.3 | forward 4.5, taa 2.0, oit accumulate_pass 0.77, light_culling 0.75, sdf_grid 0.66 |
  | sky_render_bench | present::timed 11.4, forward::frame 1.9, taa::frame 1.7, engine::render 1.5, record_pass::acquire 1.1 | forward 1.9, taa 1.7, cloud_raymarch 1.1, cloud_shadow 0.92, light_culling 0.77 |
  | light_hall_bench | present::timed 238.3, forward::frame 223.1, gi_probe::frame 4.0, taa::frame 2.0, engine::render 1.8 | forward 223.3, gi_probe 4.9, taa 2.0, light_culling 1.4, bloom downsample 0.51 |
  | carousel_bench | vbd::gs_color_iter 49.7, sched::run_wait 44.3, task::fanout_wait 36.7, vbd_cpu::broad_phase::pair_loop 34.5, vbd::contact_dual_update 33.3 | — |
  | machines_bench | sched::run_wait 0.28, task_graph::resume 0.16, async::when_all::resume 0.16, frame::finalize_trace 0.09, vbd_cpu::broad_phase::pair_loop 0.08 | — |
  | dominoes_bench | sched::run_wait 1.02, vbd_cpu::broad_phase::pair_loop 0.36, task::fanout_wait 0.30, vbd::gs_color_serial 0.30, vbd::iterations 0.24 | — |
  | swings_bench | sched::run_wait 0.53, vbd::gs_island_iter 0.46, task::fanout_wait 0.32, frame::finalize_trace 0.15, task_graph::resume 0.15 | — |
  | character_gpu_bench | present::timed 16.3, vbd_solve_iterations_stage 10.7, forward::frame 6.9, record_pass::acquire 3.7, record_pass::body 1.9 | forward 6.8, vbd_solve_iterations 2.7, taa 1.9, sdf_grid 0.78, light_culling 0.77 |
  | character_gpu_bench_headless | record_pass::body 0.75, record_pass::acquire 0.69, begin_frame::wait_fence::compute 0.65, vbd_solve_iterations_stage 0.56, task::fanout_wait 0.47 | vbd_solve_iterations 0.14, vbd_build_coloring 0.12, vbd_broad_phase 0.05, vbd_collision_reset 0.04, vbd_narrow_phase 0.04 |
  | pyramid_cpu | vbd_cpu::broad_phase::pair_loop 6.2, sched::run_wait 5.3, task::fanout_wait 1.9, vbd::solve 1.6, vbd_cpu::broad_phase::emit 0.51 | — |
  | pyramid_gpu | task_graph::resume 0.13, async::when_all::resume 0.11, sched::run_wait 0.08, frame::finalize_trace 0.06, scheduler::render 0.03 | vbd_build_coloring 0.49, vbd_build_adjacency 0.46, vbd_solve_iterations 0.33, vbd_narrow_phase 0.29, vbd_broad_phase 0.11 |
  | pyramid16k_cpu | vbd_cpu::broad_phase::pair_loop 58.8, vbd::solve 29.4, task::fanout_wait 19.1, vbd_cpu::broad_phase::pair_scan 8.8, vbd_cpu::broad_phase::emit 7.5 | — |

  The profiler reports self time. `present::timed`, `sched::run_wait`, `task::fanout_wait` and `begin_frame::wait_fence::compute` are waits on other work, not hotspots in their own right. Shared hotspots for step 2:
  - **Grass forward/depth passes:** 103–146 ms GPU in grass_render and grass_walk.
  - **Forward pass:** 223 ms in light_hall_bench; also top of the GPU list in render_stress, sky_render_bench, character_gpu_bench and grass_rigid_render.
  - **CPU VBD:** gs_color_iter, broad_phase::pair_loop and contact_dual_update in physics_stress, render_stress, carousel_bench, pyramid_cpu and pyramid16k_cpu.
  - **Geometry collector:** `geom_collect::collect_chunk` at 60.6 ms in grass_rigid_render.

  **Correctness tier.** Every scenario passed 3 runs with exit 0 and `bench: PASSED`. Hashes come from these runs:

  | scenario | hash (all 3 runs unless noted) |
  |---|---|
  | physics_stress_via_input | 0x2bdfebd23b3f1cf9 |
  | lighting_parity | 0x350203be5257f128 |
  | textured_parity | 0x265fb1ac45c30181 |
  | scene_reset | 0xe3ccc22b2b6c4eb8 |
  | scene_reset_cpu | 0x51ceab5d23e54919 |
  | locomotion | **non-deterministic:** 0xe989d51d914c7914, 0xe36877263dbc73cd, 0x41372e711f98230c |
  | rollback_reference / rollback_replay | 0x724c9cdd0b160dc9 / 0x724c9cdd0b160dc9 |
  | rollback_reference_gpu / rollback_replay_gpu | 0xee083a719d754925 / 0xee083a719d754925 |
  | rollback_impulse_reference / rollback_impulse_replay | 0x4a12ef062cf9cb93 / 0x4a12ef062cf9cb93 |
  | rollback_impulse_reference_gpu / rollback_impulse_replay_gpu | 0xc3bf0eeb016e0091 / 0xc3bf0eeb016e0091 |
  | rollback_reference_character / rollback_replay_character | **non-deterministic:** ref 0x7a340c18aca77da0, 0x634de4f22a5d8dd6, 0x3b07f7469422b7da; replay 0x484e0bb6ac31b906, 0xad7225cb888950ea, 0x9b189e47bd38f3dd. Reference and replay never matched, yet both report PASSED. |
  | rollback_reference_stress / rollback_replay_stress | 0x7671f90047246e1b / 0x7671f90047246e1b |
  | parity_stress_cpu / _gpu | 0xb649b9e5cfbd2a8c / 0x70e4e12d52f2f8e0 |
  | parity_nojoints_gpu | 0x889f0a389100e483 |
  | parity_empty_gpu | 0xec8d55b2e20d1024 |
  | parity_jointsonly_cpu / _gpu | 0x0ae023e7da0c7eea / 0xe0b5a4f34f7b4fd4 |
  | parity_drop_cpu / _gpu | 0xd006ec5cacb4d539 / 0x18fddd236faa77c7 |
  | parity_pair_cpu / _gpu | 0x473fee7806be05f6 / 0x6c7cda8b70a30735 |
  | parity_stack_cpu / _gpu | 0xc6d70d7d5c136b88 / 0x6bcc80614d6dd7f0 |
  | parity_cluster_cpu / _gpu | 0xae6eb0faaadbf621 / 0xb7d75842146f6168 |
  | parity_heap_cpu / _gpu | 0xd40875f857890b6d / 0x25d54651a96e8165 |
  | parity_mound_cpu / _gpu | 0x2bed85c8d4739dd8 / 0x23d734eac23e9156 |
  | parity_pile_cpu / _gpu | 0xd041bfcfa235de93 / 0x05dde25b81d34844 |
  | parity_hull_pile_cpu / _gpu | 0xc7a9cf8a810cc505 / 0x24fee43e5fff47aa |
  | parity_overlap_cpu / _gpu | 0x25c377db8e0b15b1 / 0x0dc1fc30140854a0 |
  | parity_shapes_cpu / _gpu | 0xbd12f54b33e60219 / 0x58d6c1611c709ccb |
  | parity_domino_cpu / _gpu | 0x863f82766d4d5a2e / 0x76fe50108be6ced5 |

  Every parity scenario also passed a 3-run timing sweep in sweep 1. Its hashes partly rotated out of the logs; every hash that survived matches the table.

  **Non-deterministic, excluded from criterion 3's hash check (not fixed):**
  - `locomotion`.
  - `rollback_reference_character` and `rollback_replay_character`.
  - `character_gpu_bench_headless`: 1 of 4 observed runs differed. The perf-tier hash for it is advisory only.

  **Failed launches.** These are early aborts, not test failures. `textured_parity` exited 1 on 7 of 13 launches, and `lighting_parity` on 1 of 7. Each failed launch's log stops 0.3–2 s in, after "Built-in assets verified", with no crash record and no `bench:` result line. One of them, run `20261109_185157`, was launched alone with nothing else running. The cause is not investigated; these two should be relaunched on failure until that is fixed. Two earlier `pyramid` launches also exited 1, because the name is not in the catalogue.

  Verdict: criterion 1 is met for the 17 named perf-tier scenarios, with `pyramid` recorded as `pyramid_cpu` and `pyramid_gpu`. Next, per steering order step 2: start with the shared hotspots — grass forward/depth GPU passes, the forward pass in light_hall_bench, CPU VBD gs_color_iter/pair_loop, and geom_collect::collect_chunk.
- 2026-11-09 — Steering run 2. Judged the criterion-1 baseline task: met as recorded (pyramid → pyramid_cpu/pyramid_gpu). No code changed. Ranking by absolute cost and sharing: the grass forward+depth GPU passes (103–146 ms in grass_render and grass_walk) are the largest shared hotspot. Verdict: next task is the grass forward/depth GPU passes; light_hall_bench forward (223 ms) follows.
- 2026-11-09 — Task: grass forward/depth GPU passes (grass_render). **No net code change; tree reverted.**
  - Before (grass_render, gse_run, no overrides): p50/p95 126.07/129.80 ms, forward_pass 87.4 ms, depth_pass 15.6 ms, hash 0xdac13405ea41e3f8.
  - Attribution (diagnostic only): `Graphics.ao_quality=off` → 125.95/129.75, forward 88.0 (AO costs nothing here). AO+shadow+reflection off → 109.64/112.62, forward 74.0 ms. So rays are ~14 ms of the 88 ms forward pass; ~74 ms is non-ray shading of thin blades (vertex + BRDF/GI/aerial perspective + 2×2 quad helper lanes).
  - Static BLAS fast-trace idea dropped without editing: the log shows `Grass RT: static BLAS 3760 MiB exceeds the 2048 MiB budget; resting tiles cast no RT shadow`, so static grass BLASes are never built in this scenario.
  - Tried: bit-exact vertex cut (rest curve computed only up to the highest node read; skipped for awake blades) in grass_shape/grass_geometry. After ×2: 129.83/138.73 and 128.60/138.70; forward 88.8/88.9, depth 16.2/16.3; hash unchanged. No gain and p95 worse → reverted. Vertex ALU is not the bound.
  - Conclusion: the remaining cost is fragment shading of sub-pixel-wide blades (quad occupancy). The local fixes left are an architectural choice for the owner: a cheaper grass-specific shading path, visibility-buffer/deferred shading for grass, or variable-rate shading. Escalate those with this split. grass_walk was not re-measured because nothing changed.
- 2026-11-09 — Steering run 3. Judged the grass forward/depth task: no change kept, evidence thin (1 before / 2 after runs, grass_walk unmeasured), but the attribution (~74 ms non-ray shading of sub-pixel blades) is enough to stop local work there. Grass shading-path options (grass-specific cheap shading, visibility-buffer/deferred grass, VRS) are architectural; they are parked for a batched escalation with other architectural findings rather than halting the loop now. Verdict: next task is the light_hall_bench forward pass (223 ms GPU).
- 2026-11-09 — Task: light_hall_bench forward pass. **Kept.** Change: in `Engine/Resources/Shaders/Standard3D/forward_shading.slang` `shade_surface`, the per-light loop now `continue`s when `attenuation <= 0` or `NdotL <= 0`, before the shadow, cloud-shadow and BRDF work. Those pairs contributed exactly 0 before (the window term is 0 past `l.radius`, and BRDF×NdotL is 0), so light order and summation are unchanged.
  - Split (diagnostic, before the change): `Graphics.shadow_quality=off` → p50/p95 233.68/244.33 ms. RT shadows are ~17 ms of the ~223 ms forward pass; the rest was per-light BRDF over pairs the conservative tile-sphere cull keeps (364 lights, 36 m radius).
  - Before ×3: 250.58/280.92, 244.09/255.01, 243.67/253.49 ms; forward GPU 224.2, 223.3, 223.3 ms; hash 0x709fdf1d9d9ff152.
  - After ×3: 150.79/157.93, 147.91/159.72, 147.35/157.96 ms; forward GPU 127.5, 127.6, 127.4 ms; hash 0x709fdf1d9d9ff152 every run. p50 −40%, p95 −38%. Criterion 2 for light_hall_bench is met (3 consecutive runs).
  - Fidelity: lighting_parity 0x350203be5257f128 and textured_parity 0x265fb1ac45c30181 match and PASSED. A first concurrent launch of both exited 1 early (the known abort), and the sequential relaunch passed. Hashes cover simulation, not pixels; the image-equality claim rests on the exact-zero argument, so a showcase clip sign-off is still owed under criterion 5.
  - grass_render (same shader through `grass_forward`) ×1: 125.73/129.86 ms, hash 0xdac13405ea41e3f8 — unchanged. This run overlapped the parity launches.
  - Remaining 128 ms forward is per-light shading of in-range pairs; tighter culling (3D clusters / sphere-vs-AABB) would be an algorithm change for the owner.
- 2026-11-09 — Steering run 4. Judged the light_hall_bench forward task from its report: kept; p50 −40% / p95 −38% over 3 runs, hash unchanged, parity hashes match. Criterion 2 met for light_hall_bench (clip sign-off under criterion 5 still owed). Trivial follow-up: the now-redundant `NdotL > 0.0` checks at forward_shading.slang:163/:166. Tighter light culling (clusters / sphere-vs-AABB) is parked for the batched architectural escalation with grass shading. Verdict: next task is the shared CPU VBD hotspot (vbd::gs_color_iter, broad_phase::pair_loop, contact_dual_update), measured on carousel_bench (largest), physics_stress, pyramid16k_cpu.
- 2026-11-09 — Task: CPU VBD Gauss-Seidel body step (`vbd::gs_color_iter` → `solver::step_colored_body`/`accumulate_contact`). **No net code change; tree reverted.**
  - Disassembly (mingw64 objdump on the Release Sandbox.exe; symbol values are section-relative, so add 0x140001000). In the frozen path of `step_colored_body`, every contact copies the 96-byte `frozen_jacobian` to the stack (3× ymm load/store + `vzeroupper`) before calling `accumulate_contact`. The cause is the `live ? contact_jacobian_of(...) : m_frozen_jacobians[ci]` prvalue conditional. GCC already splits the loop on `live_jacobians`. `accumulate_contact` does about 12 stores and 17 load-adds per call to `m_solve_state[body]`. That is the inherent per-row read-modify-write, not reloads caused by aliasing, so the "local accumulator" idea (about 33 floats, cannot stay in registers) was judged not worth doing and was not tried.
  - Before ×3 (same session; the machine was about 7–15% slower than the criterion-1 sweeps): carousel_bench 54.77/66.38, 56.30/68.72, 55.12/67.48; physics_stress 21.05/26.13, 20.85/24.61, 20.79/24.41; pyramid16k_cpu 89.70/1915.4, 90.29/1909.6, 91.01/1910.8. Hashes 0xe6d4e9bf9aeb2a33 / 0x455de8965ccf67bc / 0x828fe2ae85bc729f.
  - Tried: split the contact loop so the frozen path passes `m_frozen_jacobians[ci]` by reference (no copy). After ×3: carousel_bench 54.77/66.04, 56.09/67.47, 54.94/67.95; physics_stress 20.80/24.43, 21.46/25.53, 21.29/24.43; pyramid16k_cpu 90.99/1898.7, 92.90/1930.2, 91.52/1864.3. Hashes unchanged. Within noise, no p50 drop → reverted. The correctness tier was not run because no change remains.
  - Conclusion: per-contact call overhead and copies are not the bound. `gs_color_iter` self time is the solver arithmetic itself (row math, 3×3 outer products, Newton step inverses). Further gains need either math reordering or a SoA/SIMD rework of contact rows across bodies; both are outside the charter (results ordering / layout across systems) and should go to the batched escalation. Next CPU candidates: `vbd_cpu::broad_phase::pair_loop` (System.cpp:2010, 58.8 ms self in pyramid16k_cpu) and `vbd::contact_dual_update`.
- 2026-11-09 — Steering run 5. Judged the CPU VBD gs_color_iter task: refuted, reverted (frozen-Jacobian copy removal within noise on 3 scenarios ×3, hashes unchanged). Math reordering / SoA contact rows parked for the batched architectural escalation. Open trivial item: redundant `NdotL > 0.0` checks at forward_shading.slang:163/:166 still present. Verdict: next task is `vbd_cpu::broad_phase::pair_loop` (58.8 ms self in pyramid16k_cpu; also top-5 in physics_stress, render_stress, carousel_bench, pyramid_cpu).

- 2026-11-09 — Task: CPU VBD `vbd_cpu::broad_phase::pair_loop`. **Kept.**
  - Attribution: temporary sub-scopes (replay/sat/manifold/cache_lookup, since removed) on pyramid16k_cpu put about 59 ms p95 of the per-candidate loop in `pair_loop::replay`, the sleeping-pair replay through `contact_cache::for_each_pair`. SAT, manifold and lookup did not register. Every sleeping pair linearly scanned all unsorted cache keys (up to 4096).
  - Change: `contact_cache::prepare_pair_queries()` builds `m_tail_by_pair` once per broad-phase call: the unsorted handles, stable-sorted by (key_lo, key_hi). `for_each_pair` then uses `equal_range` on it instead of the scan. The stable sort keeps ascending handle order within a pair, so the emit order is identical. `add_scene_contacts_to_solver` calls it before the parallel loop and also reserves `pairs_bucket` per chunk. Files: `Engine/Engine/Source/Physics/VBD/ContactCache.cppm`, `Engine/Engine/Source/Physics/System.cpp`.
  - Before: same-session runs from 20:30–20:40Z on the unchanged tree. pyramid16k_cpu p50 92.90/91.52 ms (pair_loop self 68.4/67.2 ms); carousel_bench and physics_stress from the previous task's before runs: 54.77–56.30 and 20.79–21.05 ms. No fresh before ×3 was taken for this task.
  - After ×3: pyramid16k_cpu 81.77/2453.6, 81.33/1600.6, 81.51/1637.6 ms (pair_loop 52.8/52.1/52.7 ms); carousel_bench 46.20/59.31, 49.44/64.41, 52.73/64.18 ms; physics_stress 20.41/25.08, 20.47/25.44, 20.82/25.56 ms. All hashes unchanged (0x828fe2ae85bc729f, 0xe6d4e9bf9aeb2a33, 0x455de8965ccf67bc). pyramid16k_cpu p50 −11%, carousel_bench about −10% (noisy), physics_stress flat. pyramid16k_cpu's first after run had p95 2453 ms (the other two 1600–1638, against a baseline of 1596–1930), probably an outlier; watch it.
  - Correctness: all 13 parity_*_cpu, rollback_reference/replay and scene_reset_cpu PASSED with the ledger hashes.
  - Remaining replay cost (~52 ms) is the per-point callback (rotate_vector and pushes of large pending_point structs) plus the sorted-range lower_bound. Criterion 2 is not yet met for these scenarios. The NdotL cleanup is still open.
- 2026-11-09 — Steering run 6. Judged the pair_loop task from its report: kept; pyramid16k_cpu p50 −11%, carousel_bench ~−10% (noisy), physics_stress flat, all hashes unchanged, CPU parity/rollback/scene_reset_cpu pass. Weaker before-set (no fresh ×3); pyramid16k_cpu p95 outlier 2453 ms to recheck on the next measurement of it. Criterion 2 not yet met for any CPU VBD scenario. Verdict: switch to the untouched shared hotspot `geom_collect::collect_chunk` (60.6 ms self in grass_rigid_render); remaining replay callback cost (~52 ms) and `vbd::contact_dual_update` stay queued.

- 2026-11-10 — Task: geometry collector `geom_collect::collect_chunk` (grass_rigid_render). **Kept.**
  - Scene: 97,203 render entities per frame in 48 chunks of 2048; 96.8k are GPU-driven; one model, one mesh.
  - Attribution (temporary `steady_clock` counters, since removed; summed across workers per frame): chunk 77 ms; of that, the model-handle section (`valid()` + `pinned_model` + identity matrices) was 34 ms, append 10.5 ms, transform 3.5 ms. `resource::handle::valid()` does `std::atomic<std::shared_ptr>::load`, which in libstdc++ is a lock-bit RMW plus a refcount RMW on the single shared model slot, so every worker contends on the same cache line for every entity. The objdump of the lambda confirmed `valid()`, `pinned_model` and `append_entry` are out-of-line calls in the per-entity path; the future/shared_ptr destructor branches only fire for non-null skinned snapshots (not taken here).
  - Tried and reverted: a galloping `body_index_map` lookup with a per-chunk cursor. No change (p50 77.98, collect_chunk 61.1 ms): render ids are not ascending (about half of consecutive pairs are out of order) and the lookup costs about 10 ms in total.
  - Change: `pinned_model` checks the chunk's `resolved` map first and calls `handle.valid()`/`resolve()` only on a miss, returning null for invalid handles; the caller's separate `valid()` check is folded into a null check. Same model pointer, same skip behaviour, same entry order. File: `Engine/Engine/Source/Graphics/Renderers/GeometryCollector.cpp`.
  - Before ×3 (2026-11-09 21:18–21:24Z): grass_rigid_render 78.22/107.85, 77.07/104.50, 76.76/97.63 ms; collect_chunk 61.2/56.9/55.6 ms. render_stress 54.28/65.76, 50.84/65.28, 53.70/65.63 ms.
  - After ×3: grass_rigid_render 59.30/77.85, 60.75/75.94, 63.72/77.45 ms (p50 −18 to −24% against the same-session before, p95 −21 to −28%); collect_chunk 27.0/27.5/26.7 ms. Against the criterion-1 range (71.27–78.77): every run is ≥15% below the sweep means; run 3 (63.72) is not ≥15% below the single lowest baseline run (71.27). render_stress 50.01/63.94, 45.18/66.34, 43.30/60.44 ms. The collector is not its bound, so this is noise; run 2's p95 is 0.5 ms above the worst before run.
  - Hashes: grass_rigid_render 0x969384b052c1d56e and render_stress 0x2745a999bec5f71d on every run; lighting_parity 0x350203be5257f128 and textured_parity 0x265fb1ac45c30181 PASSED (textured_parity aborted early once and passed on relaunch).
  - Two render_stress launches hung: one stuck in `engine::render` mid-measure, the other in `sched::wait::init` before any frame (so before the collector ever ran). Symbolized frames are in swapchain/present and shader-compile code, not the collector. Both were relaunched and passed. Not investigated; watch for recurrence.
  - Also: removed the redundant `NdotL > 0.0` checks in `forward_shading.slang` `shade_surface`; they are dominated by the `continue` on `NdotL <= 0`. light_hall_bench ×1: 146.09/154.28 ms, hash 0x709fdf1d9d9ff152.
  - Remaining collect_chunk cost (about 27 ms summed): body lookup (about 10 ms, `flat_map` binary search over 97k ids), append (about 11 ms), and the per-entity handle id/find. A dense physics body index keyed by ECS slot would remove the lookup, but that is a cross-system move; it is parked for the batched escalation.
- 2026-11-10 — Steering run 7. Judged the geometry-collector task from its report: kept; grass_rigid_render p50 −18 to −24% / p95 −21 to −28% vs same-session before, collect_chunk 58→27 ms, all hashes unchanged, parity pass. Criterion 2 for grass_rigid_render is borderline (run 3 at 63.72 ms is not ≥15% below the lowest baseline run 71.27 ms), so it is not counted as met yet; recheck it on a later sweep. NdotL cleanup closed. render_stress hangs (present / shader compile) noted, not yet recurring work. Dense body index parked for the batched escalation. Verdict: next task is `vbd::contact_dual_update` (33.3 ms self in carousel_bench, 12.4 in physics_stress, also render_stress), the last untouched CPU VBD shared hotspot.
- 2026-11-10 — Task: CPU VBD `vbd::contact_dual_update` (`solver::update_dual`). **Kept.**
  - Cause, from source: for every active normal contact, each worker called the out-of-line `task::current_worker()` and did a read-modify-write on `per_worker_max`. That is a packed `inplace_vector<length, 64>`, so all workers wrote the same cache line (false sharing). Not confirmed by disassembly: no objdump was found on this shell's PATH, so the measurement below stands in for it.
  - Change: `update_dual` now runs `coarse_parallel` over 512-contact chunks. Each chunk keeps a local `length` max and writes it once into an `alignas(64)` per-chunk slot, and the slots are reduced after the loop. The per-contact math and order are unchanged; float `max` is exact and commutative. File: `Engine/Engine/Source/Physics/VBD/Solver.cpp`.
  - Before ×3 (2026-11-10 13:28–13:32Z): carousel_bench 46.84/59.43, 52.52/64.75, 52.30/65.98 (dual_update 33.1/37.9/39.8 ms); physics_stress 20.66/25.30, 20.75/25.11, 20.11/24.65 (13.2–13.7 ms); render_stress 51.66/67.24, 51.04/63.61, 49.99/62.06 (10.1–11.0 ms).
  - After ×3 (13:34–13:38Z): carousel_bench 44.13/57.18, 47.78/59.04, 48.28/84.75 (dual_update 12.5–16.4 ms); physics_stress 17.85/21.67, 18.51/22.03, 18.55/24.24 (5.0–5.2 ms); render_stress 42.52/57.92, 43.37/55.40, 42.85/58.59 (4.0–4.5 ms). Every hash was unchanged on every run (0xe6d4e9bf9aeb2a33, 0x455de8965ccf67bc, 0x2745a999bec5f71d).
  - p50 change: physics_stress −11%, render_stress −15%, carousel_bench −6 to −9% (noisy). carousel run 3 had a p95 outlier of 84.75 ms (p99 148 ms); runs 1–2 were below every before run.
  - Correctness: all 13 parity_*_cpu, rollback_reference/replay and scene_reset_cpu PASSED with the ledger hashes.
- 2026-11-10 — Steering run 8. Judged the contact_dual_update task from its report: kept; dual_update self 2.5× cheaper, p50 physics_stress −11%, render_stress −15% (same-session; baseline spread ~10%, not counted met), carousel_bench −6 to −9% with one p95 outlier (84.75 ms) to recheck. All hashes unchanged, CPU parity/rollback/scene_reset_cpu pass. No disassembly (objdump not on PATH). Criterion 2 not yet met for any CPU VBD scenario. Verdict: next task is the remaining `pair_loop::replay` per-point callback cost (~52 ms in pyramid16k_cpu: rotate_vector + pending_point pushes), measured on pyramid16k_cpu, carousel_bench, physics_stress ×3 before/after.
- 2026-11-10 — Task: CPU VBD `pair_loop` sleeping-pair replay (`contact_cache::for_each_pair` + callback). **No net code change; tree reverted.**
  - Attribution (temporary sub-scopes, since removed; pyramid16k_cpu ×1): replay ran on only 8 of the measured frames, with lookup (lower_bound + equal_range) ~52 ms and per-point callback ~60 ms self at p99, and 0 at p50. So replay drives the p95/p99 tail of pyramid16k_cpu, not its p50; the p50 `pair_loop` self (~52 ms) is the awake SAT/manifold path. The 2026-11-09 attribution ("59 ms p95 in replay") was a p95 figure.
  - Before (unchanged tree, ×1, 13:44Z): pyramid16k_cpu 82.07/1064.3 ms, pair_loop 51.9 ms, hash 0x828fe2ae85bc729f. Other before numbers are the previous task's after runs (13:34–13:38Z).
  - Tried: a hash index from (key_lo, key_hi) to each pair's run start in the sorted keys, built in `rebuild`, replacing the per-pair `lower_bound` (same handles, same emit order). After ×3: pyramid16k_cpu 97.17/1574.8 (first run after build, outlier), 79.48/1081.6, 79.91/1068.7 ms; pair_loop 57.3/47.5/49.0 ms; hash unchanged. ~3% on p50, within noise → reverted. carousel_bench and physics_stress were not measured, since replay is not on their p50 path either. The pending_point reserve/emplace candidate was not tried, because the callback, like the lookup, is only in tail frames.
  - Next CPU target for pyramid16k_cpu p50 is the awake narrow phase inside `pair_loop` (speculative_test / generate_shape_manifold / contact_cache::lookup), or `vbd::solve` (29.8 ms).
- 2026-11-10 — Steering run 9. Judged the replay task from its report: refuted, reverted (hash-index lookup ~3% p50, within noise; before was ×1). Corrected attribution: replay is tail-only (p95/p99), p50 `pair_loop` self is the awake narrow phase. Replay p95 trimming parked (does not move criterion 2's p50). Verdict: next task is `vbd::solve` / awake narrow phase in pyramid16k_cpu — attribute first with temporary sub-scopes (speculative_test / generate_shape_manifold / contact_cache::lookup vs solve), then attack the largest, ×3 before and after on pyramid16k_cpu, pyramid_cpu, physics_stress.
- 2026-11-10 — Task: awake narrow phase inside `pair_loop` (box–box SAT). **Kept.**
  - Attribution (temporary, since removed). Per-candidate `trace::scope_guard` sub-scopes overflowed the profiler frame ring: only 350 of 600 frames were kept and the new tags never reached the report. So `steady_clock` counters were used instead, summed across workers, pyramid16k_cpu ×1. Per broad-phase call, the chunk loop totalled ~175 ms: SAT `speculative_test` ~70 ms, `generate_shape_manifold` ~35 ms, per-point block ~62 ms (of which `contact_cache::lookup` ~38 ms). `vbd::solve` self is ~30 ms wall; the pair_loop figures are summed across workers, not wall time.
  - Cause, from source (no objdump used). `bounding_box::obb()` runs `mat3_cast(m_orientation)` on every call, and `sat_speculative`'s `project` lambda called `bb.obb()` per axis component. That is ~108 quaternion-to-matrix conversions per box pair, where 2 are needed.
  - Change: `sat_speculative` builds `obb1`/`obb2` and both half-extents once and projects onto those. Same inputs, same arithmetic order. File: `Engine/Engine/Source/Physics/Collision/NarrowPhaseCollisions.cppm`. `sat_penetration` has the same pattern and was left alone (not on this path).
  - Before ×3 (clean tree, 14:16–14:44Z): pyramid16k_cpu 88.03/1041.9, 96.49/1742.3 (first run after a build), 81.73/1086.9 ms; pair_loop self p50/p95 52.6/386, 63.6/434, 52.5/315 ms. pyramid_cpu 5.77/45.26, 6.51/44.57, 6.45/45.58. physics_stress 17.78/22.07, 17.74/22.07, 17.81/21.70.
  - After ×3 (14:48–14:59Z): pyramid16k_cpu 92.88/1494.6 (first run after a build), 81.69/1061.5, 81.59/1065.8 ms; pair_loop p50/p95 59.5/248, 53.3/187, 53.6/191 ms. pyramid_cpu 6.46/42.14, 6.34/40.70, 5.81/44.64. physics_stress 16.20/19.98, 16.44/20.71, 15.72/20.23.
  - Result: physics_stress p50 −8 to −11%, p95 −6 to −9%. pyramid16k_cpu p50 flat (81.6 vs 81.7 best), but pair_loop p95 −40 to −50%; the SAT cost is mostly in the tail frames that have many awake pairs. pyramid_cpu flat; p95 slightly better. Every hash unchanged (0x828fe2ae85bc729f, 0x1db83c6c0972dc68, 0x455de8965ccf67bc).
  - Correctness: all 13 parity_*_cpu, rollback_reference/replay and scene_reset_cpu PASSED with the ledger hashes.
  - pyramid16k_cpu p50 is still bound by the awake path as a whole. Remaining candidates: `contact_cache::lookup` returning a ~130-byte `optional<cached_lambda>` copy per point (~38 ms summed), `generate_manifold`'s repeated `obb()`/`face_vertices()` calls, and the same hoist in `sat_penetration`.
- 2026-11-10 — Steering run 10. Judged the box–box SAT hoist task from its report: kept; physics_stress p50 −8 to −11% (same-session, not ≥15% vs baseline), pyramid16k_cpu p50 flat but pair_loop p95 −40 to −50%, pyramid_cpu flat; all hashes unchanged, CPU parity/rollback/scene_reset_cpu pass. Criterion 2 still unmet for every CPU VBD scenario. Verdict: next task is `contact_cache::lookup`'s per-point ~130-byte `optional<cached_lambda>` copy (~38 ms summed across workers in pyramid16k_cpu), with the `sat_penetration` hoist allowed as a bit-identical rider only if it is on a measured path; ×3 before/after on pyramid16k_cpu, pyramid_cpu, physics_stress.
- 2026-11-10 — Task: `contact_cache::lookup` per-point copy in the awake narrow phase. **No net code change; tree reverted.**
  - Attribution (temporary `steady_clock` counters in `lookup`, since removed; pyramid16k_cpu ×1, summed across workers, two 240-call windows): ~43M calls per window; probe (`find_handle` + `stale`) 10.9 / 7.9 s, payload copy/swap 6.2 / 5.6 s. Timer overhead inflates both halves, but the copy looked like ~40% of the call, so the change was tried.
  - `sat_penetration` rider dropped: it has no callers in `Engine/Source`.
  - Before ×3 (clean tree, 15:22–15:32Z; a warm-up run after the build excluded): pyramid16k_cpu 83.63/1047.4, 81.12/1055.8, 84.75/1110.1 ms (pair_loop p50 53.7/54.4/54.4 ms); pyramid_cpu 6.09/43.37, 6.45/39.72, 6.10/43.66; physics_stress 15.32/18.94, 16.07/20.23, 16.30/20.19.
  - Tried: `lookup` returns `optional<cached_view>` (a pointer into `m_payloads` plus a swapped flag, with exact negate/swap accessors), so there is no 124-byte copy. After ×3 (15:43–15:50Z; a warm-up run excluded): pyramid16k_cpu 81.38/1004.5, 83.16/1021.2, 85.98/1158.8 (pair_loop 52.2/52.6/55.5); pyramid_cpu 6.14/44.68, 6.52/43.57, 6.14/45.52; physics_stress 15.81/20.06, 16.23/20.66, 16.00/19.87. All hashes unchanged (0x828fe2ae85bc729f, 0x1db83c6c0972dc68, 0x455de8965ccf67bc). The p50 change is within noise in every scenario → reverted. The correctness tier was not run because no change remains.
  - Conclusion: the in-loop timers overstated the copy. The out-of-order core hides it behind the probe's cache misses, which do not depend on it. The per-point block is bound by the hash probe (dependent random reads through `m_index`, `m_keys` and `m_payloads`) and by the `pending_point` push. A cache-friendlier layout (for example, key and payload co-located, or the index storing a key fingerprint) would be a data-layout change inside the cache; it is within one system, but it is a redesign, so it is left to steering.
- 2026-11-10 — Steering run 11. Judged the `contact_cache::lookup` copy task from its report: refuted, reverted (view-return within noise on pyramid16k_cpu/pyramid_cpu/physics_stress ×3, hashes unchanged); `sat_penetration` has no callers. Per-point cost is the dependent hash probe. Contact-cache layout redesign (co-located key+payload / fingerprinted index) parked: CPU VBD micro-work is now in diminishing returns (3 of the last 4 CPU VBD tasks refuted). Verdict: switch to the untouched GPU VBD path — `vbd_solve_iterations_stage` CPU self (16.2 ms physics_grow, 10.7 ms character_gpu_bench) and the `vbd_solve_iterations` / `vbd_narrow_phase` GPU passes — measured on physics_grow, character_gpu_bench, pyramid_gpu ×3 before/after.
- 2026-11-10 — Task: GPU VBD solve path (`vbd_solve_iterations_stage`). **No code change; attribution only.**
  - Correction to every earlier ledger reading of the "top 5 CPU self time" column: the per-tag table in the bench `.txt` also takes in GPU timestamp samples, because `RenderGraph.cpp` `read_profile_slots` calls `profile::ingest_gpu_sample` per pass. So `vbd_*_stage` rows in the CPU table are **GPU time summed per frame**, not CPU work. physics_grow: 2400 `vbd_solve_iterations_stage` samples over 600 frames (4 submits per frame) × EMA 3.69 ms ≈ 16.1 ms per-frame p50. The CPU recording of the pass is in `record_pass::body` (1.2 ms for every pass combined). The same applies to grass forward_pass in the earlier rows and to character_gpu_bench's 10.7 ms. So there is no CPU recording hotspot to cut; the cost is GPU solve time.
  - Run (unchanged binary, physics_grow ×1, 16:11Z): 31.56/47.56 ms, hash 0x1e4a89cda6843dc3, solve stage 16.3 ms/frame, narrow phase 3.9 ms/frame.
  - Per-dispatch split blocked: `Graphics.gpu_intra_pass_marks_enabled=true` is refused in scenario runs ("no system registered that setting in this run mode"), so the per-color / sweep / update_lambda `rec.mark` timings never reach the report. Offline SPIR-V was not obtained either: `slangc` (Vulkan SDK) fails on `vbd_solve_color.slang` alone, because the engine composes helper modules (`vbd_caps`, `vbd_shared`, …) into each program at runtime and `island_slots` etc. come from them.
  - Source read of the `vbd_solve_color` contact loop (`vbd_solve_color.slang:811`): one `rotate_vector` per contact side in the live-Jacobian path, frozen Jacobians otherwise, fixed-point gradient accumulation; no redundant quaternion→matrix work like the CPU SAT case. No bit-identical cut identified without per-dispatch evidence.
  - No change → no before/after sweeps and no correctness tier run.
  - To continue this hotspot, steering needs one of: (a) register the intra-pass-marks setting in scenario runs (a small engine change; within the charter as profiling infrastructure, but it touches settings registration), or (b) a way to dump the engine-composed SPIR-V per program.
- 2026-11-10 — Steering run 12. Judged the GPU VBD solve attribution task: no change, finding accepted (the `vbd_*_stage` rows are GPU time, confirmed against both `ingest_gpu_sample` sites). The hotspot cannot be split per dispatch until intra-pass marks work in scenario runs. Verdict: next task is the profiling-infrastructure fix (option a): make `Graphics.gpu_intra_pass_marks_enabled` settable in scenario runs, then use it on physics_grow to rank the per-dispatch GPU VBD cost.
- 2026-11-10 — Task: intra-pass marks in scenario runs (profiling infrastructure). **No code change.**
  - The setting already works in scenario runs. Steering run 12 and the GPU VBD attribution entry used the wrong key: `Graphics.gpu_intra_pass_marks_enabled` is the render-graph setter's name, not a setting. The real key is **`Graphics.gpu_profiling.intra_pass_marks=true`** (nested field in `gpu::context::data`, `Engine/Engine/Source/Gpu/Context.cppm:54`; default false).
  - Run: physics_grow with that override, run 20261110_161451. Override accepted, exit 0, PASSED, p50/p95 32.22/47.37 ms, hash 0x1e4a89cda6843dc3 (baseline). Marks appear as `vbd::solve::*` rows in `bench/physics_grow/20261110_161528.txt`.
  - Per-dispatch GPU VBD split (per-frame p50 sums, marks on, ×1). Solve stage 16.76 ms total: `color[0..4]` 1.85/1.80/1.68/1.65/1.53 (~8.5 ms), `island` 3.96, `update_lambda` 2.72, `color[5]` 0.81, `color[7]` 0.22, `color[6]` 0.10, `color[8..15]` ~1 µs each (empty dispatches), `island_levels` 0.03. Other stages: narrow phase 3.93, broad phase 1.69, build_coloring 1.29, build_adjacency 1.14, grid_build 1.10, post_stabilize 1.02, apply_restitution 0.71, collision_reset 0.60, build_active 0.40.
  - Ranking: (1) per-color solve colors 0–4 ~8.5 ms; (2) `island` 3.96; (3) narrow phase 3.93; (4) `update_lambda` 2.72. Single run; marks add one query each, so absolute numbers carry slight overhead.
- 2026-11-10 — Steering run 13. Judged the intra-pass-marks task: accepted, no code change; correct key is `Graphics.gpu_profiling.intra_pass_marks=true`. Per-dispatch ranking (×1, marks on) puts the per-color solve dispatches (colors 0–4, ~8.5 ms of 16.8 ms) first, then `island` 3.96, narrow phase 3.93, `update_lambda` 2.72. Verdict: next task is the GPU `vbd_solve_color` per-color dispatch — read its shader and compiled code, find a bit-identical ALU/memory/register cut, ×3 before/after on physics_grow, character_gpu_bench, pyramid_gpu with marks on for attribution and off for the p50/p95 numbers.
- 2026-11-10 — Task: GPU VBD per-color solve (`vbd_solve_color` `solve_body`, frozen-Jacobian path). **Kept.**
  - The runtime backend is **DX12**, so the compiled blob is DXIL, not SPIR-V. A temporary dump in `ShaderCompile.cpp` (since removed) wrote the blob, and `dxc -dumpbin` (Vulkan SDK) disassembled it. The DXIL showed three things:
    - the contact 3-row loop stayed a real loop, with `force`/`penalty`/`dirs` in `[3 x float]` allocas indexed by the loop counter;
    - `fx_add_vec3` kept a dynamic loop over the `[3 x i64]` fixed-point accumulators, so each row did a load/store round trip through local memory;
    - `j_ang` already loaded only one side (it branches on `is_a`), and unused `other_view` fields were already removed as dead code. So candidates (b), (c) and (d) were not needed and were not made.
  - Change: `[unroll]` on `fx_add_vec3`, plus `[unroll]` on the row loop with its two `continue`s folded into one `skipped` guard. With the `continue`s left in, dxc still emitted the loop. The DXIL afterwards has no i64 allocas and indexes the row values with constants. The arithmetic and its order are unchanged. File: `Engine/Resources/Shaders/Bodies/VBDPhysics/vbd_solve_color.slang`. The sweep and island entries include this file and get the same change.
  - Before ×3, marks off (16:21–16:27Z): physics_grow 32.06/48.02, 32.18/46.01, 31.76/46.88 (solve stage 16.83/16.82/16.33 ms); character_gpu_bench 24.64/28.43, 23.32/27.43, 22.66/26.60 (solve 11.49/10.30/10.55); pyramid_gpu 0.407/20.10, 0.329/20.86, 0.629/20.32. Marks on: physics_grow colors 0–4 = 1.79+1.73+1.59+1.60+1.46 = 8.18 ms.
  - After ×3 (16:30–16:36Z): physics_grow 27.39/36.05, 28.74/38.65, 29.22/38.76 (solve 13.79/14.08/14.13); character_gpu_bench 24.54/27.90, 24.11/27.10, 23.58/27.07 (solve 11.39/11.06/11.21); pyramid_gpu 0.376/17.93, 0.383/18.04, 0.337/18.49. Marks on: colors 0–4 = 1.58+1.45+1.35+1.35+1.23 = 6.97 ms (−15%).
  - Result:
    - physics_grow p50 −10%, p95 −18%, solve stage −15%. Against the criterion-1 baseline (31.0–32.0) it is not yet −15%.
    - character_gpu_bench is flat. Its solve stage does not drop, so its cost is probably in the joint/sweep paths, not in the contact rows.
    - pyramid_gpu p50 is flat, p95 −10%.
    - Every hash is unchanged (0x1e4a89cda6843dc3, 0x838486fe3dbffc2a, 0xf4b01fb73cd4afc7).
  - Correctness: scene_reset; rollback_reference/replay_gpu; rollback_impulse_reference/replay_gpu; rollback_reference/replay_stress; and all 15 parity_*_gpu PASSED with the ledger hashes.
  - The DXIL still has 197 `[3 x float]` and 29 `[9 x float]` allocas elsewhere (joint `dirs[3]` loops, mat3 helpers). These are the next candidate for the same fix, on the joint path that character_gpu_bench exercises.
- 2026-11-10 — Steering run 14. Judged the `vbd_solve_color` unroll task from its report: kept; physics_grow p50 −10% / p95 −18% (solve stage −15%), pyramid_gpu p95 −10% p50 flat, character_gpu_bench flat; all hashes unchanged, GPU rollback/parity/scene_reset pass. Criterion 2 still unmet for GPU VBD scenarios. Verdict: next task is the same local-array/unroll fix on the joint path (`dirs[3]` loops, mat3 helpers) that character_gpu_bench exercises — attribute character_gpu_bench with intra-pass marks first, ×3 before/after on character_gpu_bench, physics_grow, pyramid_gpu.

- 2026-11-10 — Task: GPU VBD joint path / constant-trip loops in `vbd_solve_color` and `vbd_curves` (character_gpu_bench). **Kept.**
  - Attribution (marks on, character_gpu_bench ×1, 16:46Z): solve stage 12.4 ms per frame; `island` 6.64, `color[0]` 2.99, `update_lambda` 1.41. The island entry runs the same `solve_body`, so the joint path is on the hot dispatch.
  - DXIL (temporary dump in `ShaderCompile.cpp`, since removed; `dxc -dumpbin`), `vbd_solve_islands`: 352 allocas and 575 GEPs indexed by a runtime value. The largest sources were:
    - the nested 3×3 loop in `geometric_stiffness_diagonal`, called per contact and per joint row;
    - the joint `dirs[k]`/`theta[k]`/`j.*[k]` row loops;
    - the fixed-point `fx_add_mat3`/decode loops over `int64_t[9]`;
    - the constant 3-trip loops in the coupled-joint curve helpers (`sample_spatial` init, `coupled_pseudo_inverse`, `coupled_frame_at`, `coupled_translation_lever`, `moving_point_in`, `moving_point_lever`).
  - Change: `[unroll]` on every one of those constant-trip loops. The ball soft/limit/drive `continue`s (solve and dual update) became `if` guards; `x <= 0 → continue` became `if (!(x <= 0))` so NaN behaves the same. Arithmetic and accumulation order are unchanged. Files: `Engine/Resources/Shaders/Bodies/VBDPhysics/vbd_solve_color.slang`, `Engine/Resources/Shaders/VBDPhysics/vbd_curves.slang`. After: islands 136 allocas and 149 runtime-indexed GEPs; solve_color 240→82 allocas. The remainder is data-indexed (`sample_spatial`'s `coordinates[coordinate]` and the `coupled.count` loops), which unrolling cannot remove.
  - Before ×3: the previous task's after runs (16:30–16:36Z, same tree without this change): character_gpu_bench 24.54/27.90, 24.11/27.10, 23.58/27.07 (solve 11.39/11.06/11.21 ms); physics_grow 27.39/36.05, 28.74/38.65, 29.22/38.76 (solve 13.79/14.08/14.13); pyramid_gpu 0.376/17.93, 0.383/18.04, 0.337/18.49.
  - After ×3, marks off (16:55–17:00Z, a warm-up run after the build excluded): character_gpu_bench 23.42/26.93, 22.99/25.78, 23.12/26.22 (solve 10.25/10.05/9.89); physics_grow 27.87/37.28, 24.74/37.07, 27.22/36.53 (solve 12.76/12.40/12.81); pyramid_gpu 0.453/17.88, 0.346/17.99, 0.347/18.04.
  - Result:
    - Solve stage −10% on character_gpu_bench and −9% on physics_grow, every run.
    - character_gpu_bench p50 −4% and p95 −4%; every after run is below every before run.
    - physics_grow p50 is noisy (−5% on average, one run +2%), and p95 is −2 to −4%.
    - pyramid_gpu is flat.
    - Kept on the solve-stage GPU time, which is the low-noise metric. Criterion 2 is still unmet for the GPU VBD scenarios.
    - Every hash is unchanged (0x838486fe3dbffc2a, 0x1e4a89cda6843dc3, 0xf4b01fb73cd4afc7).
  - Correctness: scene_reset, rollback_reference/replay_gpu, rollback_impulse_reference/replay_gpu, rollback_reference/replay_stress and all 15 parity_*_gpu PASSED with the ledger hashes.
  - `vbd_update_joint_lambda.slang` was not changed; it is not on the measured solve dispatches.- 2026-11-10 — Steering run 15. Judged the joint-path unroll task from its report: kept; GPU solve stage −10% character_gpu_bench / −9% physics_grow every run, character_gpu_bench p50 −4%, physics_grow p50 noisy ~−5%, pyramid_gpu flat; hashes unchanged, GPU rollback/parity/scene_reset pass. Before-set reused from the previous task's after runs (same tree minus change). Criterion 2 still unmet for GPU VBD scenarios. Verdict: next task is the GPU VBD narrow phase (3.93 ms/frame in physics_grow, 2.5 ms in grass_walk) — same DXIL alloca/runtime-indexed-loop inspection; after it, steering batches the parked architectural options into one escalation.

- 2026-11-10 — Task: GPU VBD narrow phase (`vbd_narrow_phase_stage`: box program `collision_narrow_phase` + `collision_narrow_phase_poly`). **No net code change; tree reverted.**
  - Attribution (marks on, physics_grow ×1, 17:12Z): narrow-phase stage 4.31 ms per frame p50. The stage records two dispatches; the box program carries the box–box SAT + clip path that physics_grow exercises.
  - DXIL (temporary dump in `ShaderCompile.cpp`, since removed; `dxc -dumpbin`): box program 135 allocas (52 `[3 x float]`, 30 `[4 x float]`, 21 `[9 x float]`, 12 `[9 x i32]`, 12 `[6 x float]`, 8 `[4 x i32]`); poly program 34.
  - Before ×3, marks off (17:14–17:20Z, a warm-up run after the build excluded): physics_grow 28.18/36.86, 28.12/37.49, 27.98/37.33 (narrow 4.15/3.69/4.13 ms); character_gpu_bench 25.23/28.52, 23.40/26.44, 22.90/25.86 (narrow 0.11–0.12); pyramid_gpu 0.381/18.28, 0.396/18.73, 0.355/17.97 (narrow 0.29–0.32).
  - Tried, bit-identical:
    - `[unroll]` on every constant-trip loop in the box path: `projected_radius`, the 3 SAT axis loops (`continue` → NaN-preserving `if (!(len < 1e-6))`), the 6-face loops in `find_best_face`/`classify_feature`, the 4-vertex loops in `build_clipped_face_contacts` (`break`/`continue` → guards), and `support_obb`;
    - exact select helpers replacing the runtime `axes[i]`/`he[i]`/`local[i]` indexing in `get_face_vertices`, `find_best_face` and `classify_feature`.
    - Box-program allocas 135 → 73. What remained was the data-indexed clip polygon (`ClipVertexPoly.verts[9]`) and the `candidates[]` arrays.
  - After ×3 (17:22–17:27Z, warm-up excluded): physics_grow 27.65/36.55, 27.15/36.93, 27.68/36.25 (narrow 3.99/3.78/3.85 ms); character_gpu_bench 23.92/27.24, 22.86/25.96, 22.67/25.60 (narrow 0.11–0.12); pyramid_gpu 0.349/18.02, 0.330/18.03, 0.438/17.73 (narrow 0.29–0.30). Every hash unchanged (0x1e4a89cda6843dc3, 0x838486fe3dbffc2a, 0xf4b01fb73cd4afc7).
  - Result: narrow-phase mean −3% (3.99 → 3.87 ms), with one after run (3.99) slower than the best before run (3.69); physics_grow p50 −2%; the other two flat. Fails the keep rule → reverted (shader back to HEAD, dump removed, rebuilt; physics_grow hash 0x1e4a89cda6843dc3 after the revert). The correctness tier was not run because no change remains.
  - Conclusion: in the narrow phase, unlike the solve, local-array traffic from fixed-length loops is not the bound. The remaining cost is probably the data-dependent clip loop over `verts[9]` (which unrolling cannot fix), per-lane divergence across shape-kind branches, and the warm-start lookup / contact stores. This stays unconfirmed without per-section marks inside the dispatch. Further work there would mean restructuring the clipper or the dispatch (one wave per pair, as the poly path does), which is an algorithm/dispatch change for the batched escalation.
- 2026-11-10 — Steering run 16. Judged the GPU narrow-phase task from its report: refuted, reverted (narrow phase −3%, one after run slower than best before; physics_grow p50 −2%; others flat), hash back to baseline after revert. Local micro-work is now in diminishing returns on CPU VBD (3 of 4 refuted), GPU narrow phase, and grass (refuted); the remaining large costs are all parked behind architectural options. Criterion 2 met only for light_hall_bench (grass_rigid_render borderline). Verdict: escalate the batched architectural options as planned in run 15.
