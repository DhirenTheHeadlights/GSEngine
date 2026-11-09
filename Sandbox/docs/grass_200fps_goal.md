# Goal: Grass scene at 200+ fps with real softbody blades

## Aim
Run the Grass scene at 200 fps or more without losing fidelity. Physics gets much cheaper by replacing the rigid-chain grass with a dedicated GPU grass solver:
- Every blade is a real deformable body: a vertex chain with mass, damping, bend stiffness and gravity, integrated each substep with vertex block descent.
- Blades flatten under feet and bodies, yield plastically, then slowly recover.
- **Two-way coupling:** blade contact forces are summed per rigid body and fed back into the main solver as external forces, so a walker feels the grass.

Physics debug drawing becomes cheap enough to leave on while testing, and it no longer needs to be on by default. Keep removing frame-time bottlenecks until the criteria hold.

## Starting point (from the previous cycle)
- `grass_render` p50 was 45–64 ms over 3 runs. Each of the 32,400 blades is 3 rigid boxes plus 3 ball joints as ECS entities, about 97k bodies, all going through VBD, the geometry collector and RT shadows.
- The collector costs about 26 ms. `physics_debug` reads back and re-uploads 97k snapshots and rebuilds a 97k-entry map every frame.
- The frame graph must keep `inline_all` resumes. The update graph uses `fan_out`.

## Pass criteria
1. **`grass_render` speed:** bench p50 ≤ 5.0 ms and p95 ≤ 8.0 ms, in 3 consecutive runs.
2. **`grass_walk` speed:** the same thresholds, in 3 consecutive runs. `grass_walk` is a new scenario where one character walks a fixed path across the field: GPU solver, real time, immediate present, the same warmup and frame counts as `grass_render`.
3. **Flattening:** `grass_walk` checks it with scenario expectations, so a failure fails the run.
   - While a foot or body is in contact, blades under it have a tip height ≤ 30% of their rest height.
   - Those blades recover to ≥ 90% of rest height within 5 s of contact ending.
   - The run logs the measured values.
4. **Two-way coupling:** `grass_walk` logs the summed grass force on the character.
   - It is non-zero while the character is in the field and zero outside it.
   - The character still finishes the path.
5. **Fidelity is unchanged:**
   - 32,400 blades at 0.15 m spacing, each 0.36 m tall.
   - Blades are visibly curved and tapered, and are lit, shadowed and TAA'd like other geometry.
   - Every graphics, quality and resolution setting stays at the scenario defaults. Nothing is turned off or lowered to reach the numbers.
   - The owner signs off a screenshot or clip. This one is an owner criterion.
6. **Determinism:**
   - `grass_render` and `grass_walk` each give one world-state hash across their 3 runs.
   - `rollback_reference_gpu` and `rollback_replay_gpu` still match each other.
   - `lighting_parity` still matches its documented hash.
   - Grass scenario hashes may be re-baselined once, and the new values are recorded in the ledger.
7. **Cheap debug drawing:** with `PhysicsDebug.enabled=true` set as an override, `physics_stress` p50 rises by ≤ 1.0 ms over debug off, judged on 3 runs of each.
8. **Builds:** the release build is clean, and `physics_stress`, `scene_reset`, `render_stress` and `grass_sleep_bench` all pass.

## Charter (no need to ask)
- **Grass solver:** a new engine system for the grass field, at `Engine/Engine/Source/Physics/Grass/*` with shaders under `Engine/Resources/Shaders/Bodies/Grass/*`. Blades are pure GPU buffers, not entities. One component describes a field: extent, spacing, blade shape, stiffness and yield.
  - Contact uses a spatial grid over the field against nearby body shapes, read on the GPU from the solver's body buffer, with no CPU round trip.
  - Blade-to-blade contact through the grid is allowed if it is needed for the look.
- **Coupling into the main solver:** add a per-body external-force input that the existing GPU solver path consumes (`vbd_apply_impulses` / predict) in `Physics/VBD/GpuSolver.*` and its shaders.
  - Accumulate in fixed point (the int64 pattern `vbd_solve_color` already uses) so the result is deterministic.
  - With the CPU solver, the grass is one-way only: it reads uploaded body transforms and gives no feedback.
  - Without a GPU device, the grass field is skipped (gated on `gpu_available`).
- **Grass rendering:** a renderer that draws blades straight from the simulation buffer, in `Graphics/Renderers/*` plus new shaders. It takes part in the depth prepass, shadows (raster and RT, through a BLAS/TLAS path or a documented equivalent) and TAA motion vectors.
- **Sandbox scene:** swap the Grass scene to the new field. Keep the rigid-chain workload as a separate "Grass Rigid" scene and point `grass_sleep_bench` at it, so the sleeping-body path keeps its coverage and its hash. Add the `grass_walk` scenario (`Sandbox/Source/Scenarios/*`, `Props/*`, `Worlds/*`) by following `docs/scenario_authoring.md`.
- **physics_debug:**
  - When the GPU solver is active, bind the solver's body buffer directly instead of reading snapshots back and re-uploading them.
  - Cache the shape instance lists and rebuild them only when collision components change.
  - Use `id_to_body_index` in place instead of copying it.
  - Default `enabled` to false.
- **Other bottlenecks** on the `grass_render` and `grass_walk` critical path are in scope: the geometry collector, the task graph's update-graph scheduling, render passes, the physics stages. Every change is verified against the hashes in criterion 6.
- **Process:**
  - Build and run scenarios freely. Turn diagnostics on with `settings` overrides only.
  - Experiments are reverted before a task closes, and every one is recorded in the ledger.
  - Judge performance on an idle machine with ≥ 3 runs, never on one run.

**Off limits without asking:**
- editing `settings.ini`;
- lowering any quality setting, resolution, blade count or density, or changing present mode, to hit a number;
- turning off TAA, shadows, clouds or GI;
- the frame graph's `inline_all` resume policy, or `gse.gpu_record` round semantics;
- expected hashes of non-grass scenarios;
- the CPU solver beyond reading transforms;
- network replication of grass state (grass is local and not replicated);
- commits.

## Steering order
1. Make `physics_debug` cheap and default it off (criterion 7). This is small and removes noise from every later measurement.
2. Build the grass solver and renderer with one-way contact, swap the Grass scene, move the rigid chain to "Grass Rigid", and re-baseline the grass hashes. Measure `grass_render`.
3. Add the `grass_walk` scenario with its flattening and recovery expectations, then add plasticity and recovery.
4. Add two-way coupling through the solver's external-force input, with determinism checks.
5. Profile the remaining critical path and cut bottlenecks until criteria 1–2 hold.
6. Do the fidelity check and get the owner's sign-off.

## Assumptions
- The machine `gse_run` uses is the owner's, and fps is judged there.
- Grass state is not part of rollback history. Two-way forces are a deterministic function of the current body state and the grass state, so rollback replay stays consistent as long as grass simulation is deterministic. If rollback scenarios diverge, steering escalates instead of snapshotting grass.

## Ledger
