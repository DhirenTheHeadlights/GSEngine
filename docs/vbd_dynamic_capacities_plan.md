# VBD GPU dynamic capacities

Supersedes `docs/vbd_limits_runtime_plan.md` (written 2026-09-08, deleted from the tree on
2026-09-15). That note's recommended design — "route A" — has since **landed**, which is presumably
why it was deleted. This plan re-scopes what is left.

## What already works

Route A of the prior plan is in the tree today:

- `vbd_capacities` (`Physics/VBD/Constraints.cppm:10-25`) is split out of `vbd_limits` as a plain
  struct. It holds `max_bodies`, `max_contacts`, `max_collision_pairs`, `max_joints`, `max_islands`,
  `max_impulses`, `max_motors`, `grid_table_size`, the two rollback-ring caps, and the four derived
  members.
- `gpu_solver` carries a runtime instance, `vbd_capacities m_capacities{}` (`GpuSolver.cppm:538`).
  Every buffer size, `reserve`/`assign`, dispatch computation and assert reads `m_capacities.`, not a
  `constexpr`.
- The runtime constant-emission path exists and is wired: `shaders::emit_slang_constants(m_capacities)`
  (`GpuSolver.cpp:1412`) feeds `gpu::build_compute_program(..., capacity_constants)` for all ~25
  pipelines.
- Settings exist and are honoured: `Physics.gpu_max_bodies` … `gpu_ring_max_contacts`
  (`System.cppm:290-364`), mapped by `capacities_from_settings` (`System.cpp:298`).
- The prior plan's fourth blocker is resolved by the split itself. `max_colors` stayed in
  `vbd_limits`, which is still a `constexpr shader_constant_block`, so the `groupshared` arrays in
  `collision_build_coloring.slang` and the `std::array<…, max_colors>` members remain compile-time.
  Same for `workgroup_size` / `adjacency_workgroup_size` behind `gpu::threads<>`.

**So "runtime-configurable" is done. "Dynamic" is not.** The values still come from hand-set settings
that the user has to guess, and every overflow is a hard assert.

## The mechanism, now settled

The prior plan answers the open question this document previously carried. Capacities reach the
shaders as **generated Slang literals**, not specialization constants:
`emit_slang_constants<T>()` (`Gpu/Shader/ShaderCodegen.cppm:363`) reflects over the members of `T`
and emits `public static const uint <name> = <literal>;`; the no-argument overload uses `T{}` and the
`const T&` overload (`:368`) uses the live instance. That string is appended to the generated wrapper
source and compiled at pipeline-build time.

The consequence sets the cost of everything below: **changing a capacity means recompiling every
compute pipeline**, not just re-creating buffers. Order of seconds, not microseconds. Spec constants
(route B in the prior plan) would avoid the recompile but have zero call sites in the tree, need the
DX12 path proven, and buy nothing else — still not worth it.

## What is actually left

### Problem 1 — the capacities do not come from the scene

`physics::init` (`System.cpp:1162`) calls `initialize_compute` with `capacities_from_settings(d)`,
and `initialize_compute` calls `create_buffers` exactly once (`GpuSolver.cpp:1462`). `physics::init`
runs at **engine** init, before any scene is loaded, so the body/joint/island/motor counts are not
knowable at the point the sizes are chosen. That is the structural reason this is settings-driven and
not scene-driven — not an oversight.

Measured on the HumanoidLocomotion training scene (16 joints, 1 island, ~12 bodies per humanoid):

| envs | outcome |
|---|---|
| 512 | fits stock defaults |
| 1024 | `joint count 16384 exceeds max_joints 8192` (`GpuSolver.cpp:823`) |
| 1024 | `island count 1024 exceeds max_islands 512` (`GpuSolver.cpp:1107`) |

The user experience is "raise four settings by hand, restart, guess again". Worse, the
`settings::range<>` annotations are wider than the real limits, so a plausible value passes the range
check and then asserts at startup — `gpu_max_contacts` advertises 4194304 but anything above 262144
dies at the sort-key assert (`GpuSolver.cpp:1401`).

### Problem 2 — overflow is fatal, not a growth event

Eight sites assert instead of growing: `GpuSolver.cpp:814` (bodies), `:815` (motors), `:819`
(impulses), `:823` (joints), `:1107` (islands), and `GpuUpload.cpp:165, 201, 218`. The rollback ring
is worse — an oversized scene silently disables it (`GpuSolverRing.cpp:95-98`) rather than asserting.

### Problem 3 — the 1024-island ceiling

`initialize_compute` asserts that the restitution sort key packs 18-bit body indices, 18-bit contact
indices and a 10-bit island slot (`GpuSolver.cpp:1401`):

- bodies ≤ 262144
- contacts ≤ 262144 — already the default, so that one can never grow at all
- **islands ≤ 1024**

Islands are one per humanoid, so **1024 envs is a hard ceiling** for this scene no matter how good
the sizing is.

## Proposed design

### Phase 1 — defer solver init and size from the first scene

This is the whole of "dynamic like the CPU" for the case that actually matters, because the trainer
knows its env count before it builds the scene.

1. Split `initialize_compute` into pipeline-independent setup and the capacity-dependent part
   (constant emission, pipeline build, `create_buffers`). Do not run the second part in
   `physics::init`.
2. Run it lazily at the first step that has bodies, deriving capacities from the real counts plus a
   margin (1.5x, rounded to a power of two), clamped to the hard limits above.
3. Settings become an **override**, not the allocation size: `0` means auto, non-zero pins the value.
   Keep `restart_required{}` on them, and tighten the `settings::range<>` bounds to the real limits so
   the range check stops accepting values that assert.
4. Fix the derived members together (`max_contact_adjacency`, `max_joint_adjacency`,
   `max_grounded_uints`, `grid_max_entries`) — they are defined in terms of the primaries at
   `Constraints.cppm:21-24` and must be recomputed as a unit.
5. Fold the rollback-ring caps into the same derivation so the ring stops silently switching itself
   off.

A 256-env run then allocates for 256 envs rather than the fixed 20480 bodies, which also cuts memory
and readback traffic. The one-time cost is that shader compilation moves from engine init to the
first populated step.

### Phase 2 — grow on overflow

Only worth doing after phase 1, and only for scenes that change size mid-run.

1. Detect overflow CPU-side *before* dispatch. All the counts are known at upload time, so this needs
   no readback.
2. Between frames — never inside a recorded `vbd_solve_chain` — re-emit the constants, rebuild the
   pipelines, re-create the buffers and the readback channels (`GpuSolver.cpp:791-796`), and re-seed
   device-local state. `m_body_buffers_seeded` already forces `apply_all_body_inputs`, so most of the
   re-seed path exists.
3. Growth must be monotone and hysteretic. With a pipeline recompile behind each event, a scene
   oscillating around a boundary would be unusable otherwise.

The resident per-frame state is the real work here: joint duals, warm-start λ, contact history and
sleep counters all need copy-and-migrate, and any of them dropped silently changes solver behaviour
rather than crashing. The prior plan rejected this route outright for that reason. Phase 1 makes it
unnecessary for the trainer; treat phase 2 as optional and justify it with a scene that needs it.

### Phase 3 — lift the 1024-island ceiling

Repack the restitution sort key, cheapest first:

- widen to a 64-bit key — simplest, costs sort bandwidth;
- steal bits from the contact index — a scene with 1024+ islands is unlikely to need the full contact
  range;
- sort per island, removing the island slot from the key entirely.

This is a `.slang` change and must be gated on **both Vulkan and DX12** per
`dx12-partial-struct-loads-change-hash`.

## Risks

- Any capacity change alters the generated shader source, so the state hash moves on both backends.
  Phase 1 changes allocation sizes for every scene, so expect refs to move and budget a 3-seed parity
  run.
- Deferring pipeline creation to the first step moves a multi-second cost into the first frame.
  Measure it; a warmup hitch may need hiding.
- `ShadowStep.cpp:397` calls `initialize_compute(ctx, *gpu_s, {})` with default capacities and must
  keep working under whatever split phase 1 introduces.

## Gate

`Tools/run-frozen-probe.sh` for behaviour — never a state hash alone, see
`gpu-async-control-loop-latency` — plus a scene that crosses a growth boundary mid-run if phase 2 is
built. Success for phase 1 is that 1024 envs starts with no hand-set `Physics.gpu_max_*` at all, and
that 256 envs still scores what it scores today.
