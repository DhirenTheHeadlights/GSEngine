# Render-graph bindless auto-barriers — plan

Status: **proposed** (not scheduled). Spun out of the Phase-3 headless GPU VBD bring-up
(2026-06-16), where a missing barrier on a bindlessly-written buffer caused a multi-day
TDR hang. This documents the footgun, why the graph cannot currently close it, and a
scoped design to make it close it.

## Problem

The render-graph cross-pass auto-barrier is **blind to bindless buffer/image writes**. A
compute pass that writes a buffer through a bindless descriptor, read by a later pass,
gets **no barrier** — a silent data race that manifests as a GPU hang or garbage, with
nothing at the call site to indicate the hazard was missed.

This bit the headless VBD solver: `vbd_finalize_stage` writes `body_buffer` via a bindless
dispatch, the next pass `vbd_state_copy_stage` reads it via `copy_buffer`, and no barrier
was inserted between them → the copy read mid-write → TDR. The fix was a hand-placed
`rec.barrier(gpu::barrier_scope::compute_to_transfer)` (`GpuSolver.cpp` `dispatch_compute`,
~line 1111). That fix is correct and idiomatic — but it is a landmine the next author can
step on, because nothing forces it.

## Background: the current barrier contract

`recording_context::note_touched` (`RenderGraph.cpp:198`) records a per-pass access
`{resource address, pipeline stages, access flags}` keyed by the resource's **address**.
At pass boundaries, `append_prev_pass_barriers` diffs these against `latest_writes` /
`reads_since_write` and emits the needed cross-pass barriers automatically.

`note_touched` is only ever called from operations where the graph holds the **concrete
resource object**, so it can take its address:

- `sample_image` → image, `shader_sampled_read`
- `copy_buffer` / `fill_buffer` → buffer, `transfer_read` / `transfer_write`
- `build_acceleration_structures` → AS
- `dispatch_indirect` / `draw_indirect` / `draw_mesh_tasks_indirect` / `bind_index` → buffer

A **bindless** dispatch is the gap. `dispatch<Entry>(pc, args, groups)`
(`RenderGraph.cppm:422`) only `push_data`s `args` and calls the raw `dispatch(x,y,z)`.
`args` is `binding_args<Pack>` — a reflected aggregate whose members are `bindless_slot` /
`combined_sampler_arg`, i.e. **pure `uint32` heap indices** (`PipelineBuilder.cppm:683`,
`binding_arg_type`). The buffer identity is deliberately discarded at `.slot()`. So the
dispatch has no resource address to hand `note_touched`, and the graph **cannot know** what
a compute shader reads or writes through the heap. This is inherent to the bindless model,
not an oversight.

Consequence — the de-facto contract today:

- **Graph owns:** image layout transitions + cross-pass barriers for resources it can name
  through typed APIs (images, copies, indirect/index buffers, acceleration structures).
- **Caller owns:** bindless compute-storage synchronization (`compute→compute`,
  `transfer→compute`, `compute→transfer`) via explicit `rec.barrier(scope)`.

This is engine-wide, not a VBD quirk — `BloomRenderer.cpp:226,257` carries
`compute_to_compute` between its iterative down/upsample dispatches, and
`RtShadowRenderer.cpp:181` carries `transfer_to_compute` before the trace. The manual
barrier is the established idiom; the goal of this ticket is to remove the *silent-failure*
class, not the idiom itself.

## Goal / non-goals

**Goal.** Make the graph auto-insert **cross-pass** barriers for bindless buffer/image
accesses, so the "a later pass reads a bindlessly-written resource with no barrier"
footgun (the one that caused the hang) becomes impossible. After this, the manual
`compute_to_transfer` in the VBD solver — and any other *cross-pass* manual barrier — can
be deleted.

**Non-goal (MVP).** Removing *intra-pass* manual barriers (between dispatches inside a
single pass). Those are visible, local, and authored deliberately. See Phase 2.

**Non-goal.** Changing the bindless model, `binding_args` push layout, or any shader.

## Why this splits into two phases

The VBD solve is a chain of **separate single-dispatch passes**
(`vbd_clear_state_buffers_stage` → … → `vbd_finalize_stage` → `vbd_state_copy_stage`, all
`in_chain<vbd_solve_chain>()`). The manual barriers fall into two distinct classes:

- **Cross-pass** (e.g. `finalize` pass writes `body_buffer` → `state_copy` pass reads it):
  handled by `append_prev_pass_barriers` **once the write is recorded**. This is the
  exact case that hung. **MVP target.**
- **Intra-pass** (the `compute_to_compute` barriers inside `vbd_solve_iterations_stage` /
  `vbd_apply_restitution_stage`, `GpuSolver.cpp:998–1088`; Bloom's down/upsample chain):
  between dispatches within one pass. `append_prev_pass_barriers` runs at pass boundaries
  only, so these are out of MVP scope. **Phase 2.**

## Design — MVP: cross-pass bindless auto-barriers

Make `dispatch<Entry>` / `push_bindings<Entry>` call `note_touched` for each bound
resource, with the right access. Two pieces:

### 1. Recover the resource address from a slot (runtime)

`binding_args` carries only slots; the address must be recovered. Preferred: a
**slot → resource-address registry** on the device. Slots are already device-allocated
(`allocate_buffer_slot` / `allocate_image_slot`, `Device.cpp:602/606`) and the bindless
heap already maps slot → descriptor (`build_bindless_mappings`). Extend that mapping to
retain the resource address when a buffer/image is registered into the heap, and invalidate
it on free.

- **Pro:** zero call-site churn; `binding_args` stays a tight uint payload.
- **Con:** slot recycling — the registry must be updated on (re)registration and cleared on
  free, or a stale entry yields a wrong/missing barrier. This is the main correctness risk.

(Rejected alternative: thread `buffer*`s alongside slots into a parallel ref structure.
Pervasive change to every binding init site and bloats the dispatch path. Only revisit if
the registry's lifetime story proves untenable.)

### 2. Derive access intent from the binding pack (compile time)

`dispatch<Entry>` is already templated on `Entry` and uses `entry_bindings_pack_t<Entry>`.
A consteval walk over the pack yields, per binding, the `{access flags, resource type}`
from its descriptor type:

- `ssbo` → `shader_storage_read | shader_storage_write`
- `ssbo_readonly` → `shader_storage_read`
- `combined_image_sampler` / sampled image → `shader_sampled_read`
- storage image → read/write per its qualifier

Stage is `compute_shader` for a dispatch. At record time, zip each binding's compile-time
access with its runtime slot value (read from `args` by reflected member), look the slot up
in the registry, and `note_touched(addr, compute_shader, access)`. The existing
`append_prev_pass_barriers` then emits the cross-pass barrier with no further work.

### What the MVP buys

- The `finalize → state_copy` hazard auto-resolves: `finalize`'s bindless write lands in
  `latest_writes`; `state_copy`'s `copy_buffer` already note_touches `body_buffer` as
  `transfer_read`; the diff emits `compute→transfer`. **Delete the manual barrier.**
- Any future "compute writes a bindless buffer, a later pass reads it" is covered
  graph-wide — the silent-hang class is gone.

## Design — Phase 2 (optional): intra-pass hazard tracking

Track per-dispatch accesses **within** a pass and auto-insert a barrier when dispatch _N_
reads/writes a resource an earlier dispatch in the same pass wrote. This removes the
iterative `compute_to_compute` barriers (VBD solve loop, Bloom). Higher complexity (mid-pass
barrier insertion, hazard bookkeeping per dispatch) for lower value (these barriers are
local and visible). **Defer** unless intra-pass races become a recurring source of bugs.

## What stays manual either way

- **Intra-pass** barriers until Phase 2 ships.
- **Conservative-vs-targeted tradeoff:** auto-barriers derived from coarse access intent
  may over-synchronize (a full `compute_to_compute` where a narrower scope would do). If a
  hot pass regresses, an explicit barrier + an opt-out on that dispatch is the escape hatch.
  Measure before assuming the auto path is free.

## Risks

- **Registry lifetime / slot recycling** — the dominant correctness risk; a stale slot →
  address entry produces a wrong or missing barrier (i.e. the same silent failure, harder
  to spot). Needs a tight invalidation story and a debug assert (slot in registry ⇔ live).
- **Over-barriering** — possible throughput regression on bindless-heavy passes; keep the
  manual `rec.barrier` + a per-dispatch opt-out as the escape hatch.
- **Determinism** — barrier placement must not depend on hash-map iteration order; keep
  emission deterministic (stable per-binding order from the pack).

## Acceptance / validation

- `--physics-parity --use-gpu-solver` stays green (boxes fall, run-to-run deterministic,
  CPU↔GPU within tolerance) **with the manual `compute_to_transfer` deleted**.
- Locomotion smoke trials 2–5 bit-identical (`state_hash` unchanged).
- A focused test: a two-pass graph (compute writes a bindless buffer → second pass reads
  it) hangs/validation-errors **without** the feature and passes **with** it.
- Bloom / RT-shadow render paths unchanged visually (their intra-pass barriers stay).

## Effort & sequencing

- **MVP** — registry + `dispatch<Entry>` note_touched + delete the VBD cross-pass barrier.
  Medium: the consteval pack walk is in-wheelhouse; the registry lifetime is the real work.
- **Phase 2** — intra-pass tracking. Larger, optional, separable.
- Independent of Phase-3 locomotion work; schedule when the RHI is otherwise quiet. The
  manual barrier is correct in the meantime — this is a footgun-removal, not a bugfix.

Also worth bundling (separate small item, same neighborhood): a **build-time
`push_data_size ≤ maxPushDataSize` assert** in the pipeline/bindless-mapping builder. The
companion footgun in the same bring-up was a silent push-data overflow (descriptor indices
+ push constants > 256 B); a compile/load-time assert would make it loud.

## References

- `Engine/Engine/Source/Gpu/Graph/RenderGraph.cpp` — `note_touched:198`,
  `append_prev_pass_barriers`, `copy_buffer`/`fill_buffer` note_touched sites.
- `Engine/Engine/Source/Gpu/Graph/RenderGraph.cppm:422` — `dispatch<Entry>`.
- `Engine/Engine/Source/Gpu/Shader/PipelineBuilder.cppm:33,674` — `binding_args_aggregate`,
  `binding_arg_type` (`bindless_slot` / `combined_sampler_arg`).
- `Engine/Engine/Source/Gpu/Device/Device.cpp:602,606` — slot allocation.
- `Engine/Engine/Source/Physics/VBD/GpuSolver.cpp:1110` — the manual `compute_to_transfer`
  the MVP retires; `:998–1088` — the intra-pass barriers Phase 2 would retire.
- `Engine/Engine/Source/Graphics/Renderers/BloomRenderer.cpp:226,257`,
  `RtShadowRenderer.cpp:181` — the same idiom in renderers.
- Memory: `gpu-bindless-render-graph-gotchas` (the bring-up that surfaced this).
