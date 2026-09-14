# VBD GPU solver: slim per-frame joint drive inputs

## Problem

Every joint drive or muscle write bumps `physics::data::joints_generation`, which the GPU upload system treats as a structural change. With drives enabled (every locomotion trainer tick) the frame does, serially on the CPU chain:

1. `copy_joints_with_inputs`: copy every `joint_definition` (34k at 2048 envs) and re-apply drive/muscle components.
2. `build_joint_constraints`: resolve both bodies per joint and write a full `vbd::joint_constraint` (~240 B) per joint.
3. `gpu_solver::upload` joint rebuild: slot loop, topology key, 8 MB copy into `m_upload_joints`.
4. `commit_upload`: 8 MB host write into the joint upload channel.
5. GPU: `vbd_apply_joint_inputs` reads the 8 MB input, keeps the resident duals, writes 8 MB back.

Measured at 2048 envs (gpuchain14): `build_joints` 1.2 ms, `joint_rebuild` 0.7 ms, `joint_write` 0.4 ms, about 2.3 ms of a 12.5 ms frame. Only the drive fields (`drive_target`, `drive_stiffness`, `drive_damping`, `drive_max_torque`) and `activation` change between frames.

## Change

Split the generation counter and upload only what changed.

- `physics::data` gains `joint_inputs_generation`. `physics::prepare` bumps it (instead of `joints_generation`) when a drive is enabled or a muscle activation changed. Structural edits (`create_joint`, `remove_joint`, spec re-resolve, `clear_runtime_state`) keep bumping `joints_generation`.
- New `vbd::joint_drive_input` shader struct: `drive_target`, `activation`, `drive_stiffness`, `drive_damping`, `drive_max_torque`. One record per resident joint slot.
- `build_joint_constraints` optionally reports the compaction slot per definition (`unresolved` for dropped joints). `gpu_upload::data` keeps that slot vector from the last full build.
- `gpu_upload::run`:
  - `refresh_joints` (structural: `joints_generation`, body count, joint count, `plan.reset`) → full path exactly as today, and the slot vector is refreshed.
  - otherwise, if `joint_inputs_generation` moved → build `joint_drive_input[slot]` in parallel from the definition plus the drive/muscle components (same semantics as `copy_joints_with_inputs`: muscle overrides `activation`; enabled drive copies its four fields; disabled drive zeroes `drive_stiffness` only) and pass it as `solver_upload::joint_inputs`. The full `joints` span is empty on these frames.
  - If a full build reported any rest orientation initialisation, the next frame is forced full as well, so no slim frame ever runs against a definition whose rest orientation is still pending.
- `gpu_solver`:
  - keeps `m_joint_slots` (input index → resident slot) from the last rebuild.
  - `upload`: when `payload.joint_inputs` is non-empty and joints are resident with the same count, scatter the records through `m_joint_slots` into `m_upload_joint_inputs` and mark the slim stage. If the joints are not resident (should be unreachable: every reseed trigger is visible to `gpu_upload::run`) log once and skip the stage.
  - `commit_upload`: `parallel_host_write` of the records into a new `vbd.joint_drive_inputs` upload channel (`max_joints * sizeof(joint_drive_input)`).
  - New binding `joint_drive_input_data`, new stage `vbd_apply_joint_drive_inputs_stage` dispatched over `joint_workgroups` at the same chain position as `vbd_apply_joint_inputs` (after `clear_state_buffers`). It writes the five fields into `joint_data[ji]` and leaves everything else resident.
- Shader `Bodies/VBDPhysics/vbd_apply_joint_drive_inputs.slang`: read record `ji`, write the five fields of `joint_data[ji]`.

## Why this is bit-identical

The resident non-drive fields on the device are exactly the values the full path uploaded last (they can only change through a `joints_generation` bump, which forces the full path). The drive fields are written from the same floats the full path would have produced. The duals stay resident in both paths. `warm_start_joint` and `compute_joint_c0` run only on non-merge full uploads, unchanged. Stage order is unchanged.

## Gate

- Hash gate on both backends (Vulkan `AC3D5339…`, DX12 `F838905F…` at 409600 steps).
- Interleaved A/B against gpuchain14 at 2048 envs, 15 workers, 100 updates per arm.
- CPU solver path untouched (`ShadowStep` still uses `copy_joints_with_inputs` + `build_joint_constraints`).
