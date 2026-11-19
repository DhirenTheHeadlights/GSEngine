# VBD GPU dynamic capacities

Supersedes `docs/vbd_limits_runtime_plan.md` (written 2026-09-08, deleted from the tree on
2026-09-15). This note now records what landed and what is still blocked.

## What landed (2026-10-15)

Capacities are derived from the scene and there are no capacity settings.

- `vbd_capacities` (`Physics/VBD/Constraints.cppm`) is built by one function, `capacities_for`,
  from a `vbd_scene_counts`. Every primary is the next power of two above the measured count with a
  small floor, clamped to `capacity_index_ceiling` (2^20, the restitution sort key's index width).
  The four derived members are still default member initializers reading the primaries, so the
  single designated-init expression in `capacities_for` is what keeps them consistent — **do not
  assign a capacity field after construction**, build a fresh struct instead.
- `gpu_solver::upload` merges the payload's counts into `m_scene_counts` and, while the solver has
  not been built, re-derives `m_capacities` from them. Host staging in the same call then follows
  those sizes, so staging and the device agree.
- `gpu_solver::initialize_compute` no longer takes capacities and no longer touches the device: it
  records the readback policy and the trace marks. `gpu_solver::ensure_capacity`, called from the
  frame tier after `upload` and before `commit_upload`, emits the Slang constants, builds the ~37
  compute pipelines and creates the buffers. Both the physics system and the shadow-step harness
  call it.
- `gpu_solver_driving` (device present) gates the drive path — the update tier that builds the
  upload, and the frame tier that builds the buffers. `gpu_solver_active` (buffers exist) still
  gates every consumer of solver data. Gating the drive path on the buffers would have deadlocked:
  nothing would ever build the buffers that the gate waits for.
- `max_islands` is now `max_bodies`. Islands are at most one per jointed body, so the old
  1024-island ceiling — phase 3 of the previous plan — is gone, and the island buffer costs
  `1 + 2 * max_bodies + max_bodies` uints to get rid of it.
- `ring_max_bodies` and `ring_max_contacts` are the body and contact capacities, so the rollback
  ring no longer silently switches itself off for a scene larger than its slots.

A small scene is now much cheaper than it was: capacities used to be fixed at 20480 bodies and
262144 contacts for every run, and contacts dominate solver memory.

## What is blocked: growth after the first step

Capacities are chosen once, on the first step that has bodies, and cannot change afterwards. A
scene that grows past them asserts in `gpu_solver::upload` with a message naming the quantity, the
size the solver chose and the size the scene now needs. Because sizing is automatic, a restart now
fixes that; before this change a restart only helped if you also guessed a setting.

Growing at runtime needs the old buffers and pipelines released, and **the engine has no way to
release either above the backend seam**:

- `gpu::device` (`Gpu/Device/Device.cppm`) has `retire` for semaphores, fences and query pools
  only. There is no `retire(handle<buffer>)`, no `retire(handle<shader_object>)` and no
  `retire(handle<pipeline_layout>)`, so nothing in the engine has ever destroyed a buffer before
  device teardown.
- `gse::vulkan::device` does have `retire(handle<buffer>)` and a frame-deferred `resource_arena`
  that already covers shader objects and pipeline layouts generically, so the Vulkan half is small.
- `gse::dx12::device` has no per-resource release at all: buffers go into a flat
  `m_owned_buffers` vector with no handle association, and `collect_garbage()` is an empty
  function. Buffer retirement there means associating resources with handles, a deferred-release
  queue keyed by frame, and freeing the bindless slot and its descriptor.

`gpu::readback_channel` and `gpu::upload_channel` are the same problem one layer up: reassigning
one leaks its staging buffers, because `gpu::buffer` holds a handle and has no owning destructor.
The solver holds 7 readback channels and 17 upload channels.

So growth is gated on DX12 resource retirement, not on the solver. The sequence, when it is worth
funding:

1. `retire(handle<buffer>)` on the frontend device and both backends, plus the DX12 deferred-release
   queue that makes it real.
2. `retire(handle<shader_object>)` and `retire(handle<pipeline_layout>)`, which the Vulkan arena
   already supports.
3. `release(device&)` on both channel types.
4. A `release_gpu` on `gpu_solver` that retires everything `create_buffers` made, then
   `ensure_capacity` grows instead of refusing: re-derive, rebuild, re-create, and force a reseed.
   The reseed path exists — physics holds the CPU-authoritative body state, `refresh_joints`
   recomputes joint duals and `c0`, and `m_body_buffers_seeded` forces `apply_all_body_inputs`.
   Warm-start λ and contact history are lost for the growth tick; convergence dips, nothing
   diverges.

Growth must be monotone and hysteretic: each event re-emits the Slang constants and rebuilds every
pipeline, and `compile_compute_spirv` has no cache and a global mutex, so a scene oscillating
around a boundary would be unusable.

## The cheaper alternative, and why it was not taken

Capacities reach the shaders as generated Slang literals (`emit_slang_constants(m_capacities)` feeding
`build_compute_program`'s `runtime_constants`, whose only caller is the solver). Moving them into
`solver_config`, which 14 of the 19 capacity-using shaders already bind, would make a capacity change
cost buffer recreation only — no pipeline rebuild — and would delete that codegen path outright.

It was rejected because the cost lands in the hottest shaders: `h % grid_table_size` folds to a mask
today and becomes an integer division in the broad-phase inner loop, `ELEMS_PER_THREAD` in
`collision_build_coloring.slang` and the restitution bit widths in `vbd_apply_restitution.slang` stop
being compile-time, and 8 shaders would gain a binding they do not need. It remains the better system
on paper if growth events ever become frequent enough to matter.

## Gate

`Tools/run-frozen-probe.sh` for behaviour — never a state hash alone, see
`gpu-async-control-loop-latency`. Capacities changed for every scene, so the generated shader source
and therefore the state hash moved on both backends; refs had to be re-taken.
