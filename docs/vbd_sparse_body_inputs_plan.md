# VBD GPU solver: sparse per-frame body inputs

## Problem

`gpu_solver::upload` writes every `vbd::body_state` (about 300 B) into the `vbd.body_input` upload channel each frame, and `vbd_apply_body_inputs` reads all of them. On a steady-state frame (`apply_all_body_inputs == 0`) the shader only consumes records whose `locked` or `reset_pending` flag is set: a handful of static bodies plus the envs resetting this frame. Everything else is read and discarded.

Measured on gpuchain18: `vbd_gpu::upload::body_write` 0.33 ms at 2048 envs and 0.68 ms at 4096 (15 MB into write-combined memory, chunked over the workers but on the main thread's critical path), and `vbd_gpu::upload::static_scan` 0.19 / 0.49 ms serial on the main thread (a `max_bodies` memset plus a strided scan of `locked` and `half_extents`).

## Change

Upload only the records the shader will merge, and fold the static scan into the same parallel pass.

- New upload channel `vbd.body_input_sparse` holding `1 + max_bodies` `uint32` indices (count first, same shape as `vbd.static_bodies`), bound as `body_input_index_data`. The existing `vbd.body_input` channel keeps its full size; on sparse frames only the listed slots are written, at their body index, so `body_input_data[bi]` still addresses the record for body `bi`.
- `gpu_solver::upload`, when `m_apply_all_body_inputs == 0`: one `coarse_parallel` pass over `bodies.first(m_body_count)` collects, per chunk and in ascending index order, the static list (`locked != 0`), the merge list (`locked != 0 || reset_pending != 0`) and the chunk's max half extent; the chunks are concatenated in order so `m_upload_static_bodies` and `m_grid_cell_size` are exactly what the serial scan produced. The merge list becomes `m_upload_body_input_indices`; the listed records are written into `m_body_input_channel.write_target()` at their slots with `host_write` at offset `bi * sizeof(body_state)`.
- When `m_apply_all_body_inputs != 0` (first frame, reseed, body-count change): the full `parallel_host_write` runs exactly as today and the sparse list is left empty; the static scan still runs (parallel form, same result).
- Stage `vbd_apply_body_inputs` keeps its dispatch shape. The shader branches on `pc.apply_all_body_inputs`: when set it behaves as today; when clear, thread `t` reads `body_input_index_data[1 + t]` for `t < body_input_index_data[0]` and applies the existing merge to that body. The dispatch size becomes `ceil_div(max(count, 1), workgroup_size)` on sparse frames.

## Why this is bit-identical

On sparse frames the shader today touches `body_data[bi]` only when `input.locked != 0 || input.reset_pending != 0`, using the freshly uploaded record; the sparse path uploads exactly those records and applies the same merge with the same push constants (`preserve_accel_weight`, `apply_all_body_inputs`). Records for other bodies are never read. Full frames are unchanged. `m_upload_static_bodies` and `m_grid_cell_size` are order-preserving reductions of the same fields, so the broad phase sees the same data. The CPU-side `bodies` vector is still built in full, so `build_motors`, joint rebuilds and the CPU solver are untouched.

## Gate

- Hash gate on both backends (Vulkan `AC3D5339…`, DX12 `F838905F…` at 409600 steps).
- Interleaved A/B against gpuchain19 at 4096 envs (the CPU-bound count), 15 workers.
