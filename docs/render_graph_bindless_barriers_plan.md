# Render-graph bindless auto-barriers — plan

Status: **zero manual barriers — `rec.barrier` and `gpu::barrier_scope` are deleted (2026-07-28).**
Spun out of the Phase-3 headless GPU VBD bring-up (2026-06-16), where a missing barrier on a
bindlessly-written buffer caused a multi-day TDR hang. Both the cross-pass path (slot registry +
consteval access walk) and the intra-pass path (per-pass last-accessor diff at `note_touched`)
are implemented, so barrier correctness is now automatic with no author-facing escape hatch.
`recording_context::pipeline_barrier` survives as the internal primitive both paths emit through,
and as the explicit sync inside `build_blas_in_place` / `build_tlas_in_place` / `transition_image_to`.

An earlier revision of this doc claimed P1 v1 had landed. It had not — no intra-pass tracker
existed in any ref until 2026-07-28. Treat the statuses below as describing the current tree.

## Problem

The render-graph cross-pass auto-barrier was **blind to bindless buffer/image writes**. A
compute pass that writes a buffer through a bindless descriptor, read by a later pass, got
**no barrier** — a silent data race that manifests as a GPU hang or garbage, with nothing
at the call site to indicate the hazard was missed.

This bit the headless VBD solver: `vbd_finalize_stage` writes `body_buffer` via a bindless
dispatch, the next pass `vbd_state_copy_stage` reads it via `copy_buffer`, and no barrier
was inserted between them → the copy read mid-write → TDR. The stopgap was a hand-placed
`rec.barrier(gpu::barrier_scope::compute_to_transfer)`. That is correct and idiomatic — but
it is a landmine the next author can step on, because nothing forces it.

## Current state — what is already built

The cross-pass gap is now closed. The pieces:

1. **Slot → resource registry.** `device::set_slot_resource` / `resource_for_slot`, backed
   by `m_slot_resources` (`Device.cpp:660-694`). **Coverage caveat (important):** the registry
   is populated *only* by the `create_buffer` / `create_image` bindless path
   (`Device.cpp:660,677-678`). The **manual slot path** — `allocate_image_slot()` /
   `allocate_buffer_slot()` + `write_sampled_image()` / `write_storage_buffer()`
   (`Device.cpp:701,717,709`) — **never calls `set_slot_resource`**, so those resources are
   absent from the registry and `resource_for_slot` returns a null `resource_ref`. TAA
   (`hdr_color`) and Bloom (`hdr_view`, the mip chain) bind through this path, so they are
   **currently invisible to the auto-barrier tracker** and correctly fall through the
   `if (ref.ptr)` guard. Closing this is the true prerequisite for both P0 and P3 (see below).
   Invalidated by `free_slot_resources` (`TransientPool.cpp:221`).
2. **Consteval access walk.** `register_one_bindless<T>` (`RecordingContext.cppm:302`)
   derives `{resource, access}` for one binding from `descriptor_type_v<T>` +
   `descriptor_access_v<T>`; `register_bindless_usage<Entry>` folds it over
   `entry_bindings_pack_t<Entry>` in stable pack order.
3. **Wired into the record path.** `dispatch<Entry>` (compute) and `push_bindings<Entry>`
   (graphics: vertex|fragment|mesh|task) both call `register_bindless_usage`, which feeds the
   existing `note_touched → append_prev_pass_barriers` engine and does DX12 image layout
   transitions via `transition_image_for_binding`.

So a bindless write now lands in `latest_writes`, and any later pass that touches the same
resource (through a typed API *or* another bindless access) gets an automatic cross-pass
barrier. The `finalize → state_copy` hazard that hung is now covered by construction.

## How good can this get? (the target bar)

**Provably-optimal barriers are infeasible.** Worth stating plainly so the implementation
aims at the right target and we don't chase a bar that doesn't exist. "Optimal" fails on two
independent axes:

- **Precision of the hazard set.** A barrier is really about a `(resource region, access)`
  pair. Minimal barriers need the exact memory footprint of each dispatch — but a compute
  shader indexes an SSBO *dynamically*, so the touched byte range is **data-dependent and
  not statically decidable**. Any static system must over-approximate. We can know the bound
  *resource*, the *access type* (read vs read-write, from the descriptor), the *stages*, and
  — in this engine — the *mip/slice* (mips are separate image objects). We cannot know the
  exact byte range of a buffer write without programmer annotation. So "provably no redundant
  barrier" is impossible: two dispatches writing disjoint halves of one buffer look like a
  WAW hazard.
- **Placement given the hazard set.** Even with a perfect hazard graph, placing barriers to
  minimize GPU stalls (hoist/sink, split-barriers, pass reordering, queue assignment) is a
  resource-constrained scheduling problem — NP-hard. Exact optimality is out; good heuristics
  get near it.

**But optimal is the wrong bar.** The right bar is *"correctness is automatic, and precision
is ≥ what a careful author hand-writes."* That is very achievable, because the hand-written
barriers already sit far below optimal — `rec.barrier(compute_to_compute)` is a *blanket*
memory barrier (no resource, no range, whole stage). The automatic path operates at
**resource + access + stage** granularity — strictly finer than the manual code — and gets
**per-mip for free** (mips are distinct resources), while being impossible to forget.

```
coarsest ──────────────────────────────────────────────► optimal
  blanket        resource+access      per-mip /            exact byte
  compute_to_    +stage (targeted     subresource          range / proven
  compute        buffer_barrier)                            disjoint
  ▲                    ▲                    ▲                    ▲
  manual today    automatic path       free here (mips =    needs annotation
  (a landmine)    (the target)         separate images)     or undecidable
```

The only thing beyond the automatic bar needs *semantic knowledge the type system cannot
derive* — "these two writes are disjoint," or "already synced two sub-steps ago." A human
occasionally knows those; the compiler cannot prove them.

**No opt-out.** An earlier revision argued for keeping `rec.barrier` as a performance knob.
That was rejected: a blanket `compute_to_compute` is strictly *coarser* than what the tracker
emits, so it was never an optimization — only a way to forget a barrier and get a silent TDR.
The knob, if one is ever needed, should be a per-dispatch range/disjointness annotation that
makes the tracker emit *less*, not a hand-placed barrier that makes it emit more.

## Coverage: cross-pass and intra-pass

`note_touched` accumulates per pass and flushes at the pass boundary (`finalize_pass`) for the
cross-pass path, and additionally diffs each access against a per-pass last-accessor map
(`m_last_access`) for the intra-pass path. Both feed off the same `{resource, stage, access}`
tuple, so every access route — bindless `dispatch`/`push_bindings`, `copy_buffer`,
`fill_buffer`, `dispatch_indirect`, `sample_image`, `build_acceleration_structure` — is covered
by construction.

- **Cross-pass** (`append_prev_pass_barriers`, `RenderGraph.cpp`): per-queue `latest_writes` +
  `reads_since_write`, emitting targeted buffer/image/memory barriers at pass boundaries.
- **Intra-pass** (`recording_context::emit_intra_pass_barrier`): on a re-access of a resource
  where either side writes (RAW/WAW/WAR), emits one targeted barrier before the command and
  replaces the entry; read-after-read accumulates stages/access instead.

## Ranked plan to reach zero

**P-reg — Registry completeness (prerequisite for P0 and P3).** Route the manual slot path
(`allocate_*_slot` + `write_sampled_image` / `write_storage_buffer` /
`write_acceleration_structure`) through `set_slot_resource`, so every bindlessly-bound
resource is in the registry — not just those created via the `create_buffer` / `create_image`
bindless path. Until this lands, the tracker is blind to TAA/Bloom/manual-slot resources.
Watch slot recycling: a freed-and-rebound slot must be re-registered (or its entry cleared),
or `resource_for_slot` returns a stale resource — a wrong barrier, the same silent class,
harder to spot.

  *Status (landed — images + storage buffers).* Images register in the agnostic
  `write_sampled_image` (identity = `img.handle()`, aspects from the format; no call-site
  churn) — covers TAA, Bloom, OIT, Tonemap, LightCulling. Storage buffers register in
  `write_storage_buffer`, whose agnostic signature now takes `const buffer&` (identity =
  `buf.handle()`, unifying with the typed path; backend vtable unchanged); the two call sites
  (PhysicsTransform, RtShadow) pass the buffer object. **AS is deferred to P2** — it has no
  consumer until `register_one_bindless` learns an acceleration-structure branch, and wiring
  it needs the AS object threaded through `write_acceleration_structure` (call sites currently
  pass a raw address). Recycling is safe as-is: view slots are allocated once then rewritten
  every frame, so steady state is assignment-only (no resize), and renderer device access is
  serialized by the ECS schedule (all take `shared_view<gpu::context::data>`); a freed slot is
  never bound by any shader, so free-time clearing stays unnecessary.

**P0 — Reap the cross-pass win already paid for. DONE 2026-07-28.** All 35 `rec.barrier` call
sites are deleted, along with `recording_context::barrier` and `gpu::barrier_scope`. The
cross-pass ones (first op of a fresh `co_await gpu::pass`) are covered by
`append_prev_pass_barriers`; the intra-pass ones by P1. `compute_to_indirect` is covered —
`dispatch_indirect` / `draw_indirect` / `draw_mesh_tasks_indirect` all `note_touched` the
indirect buffer with `draw_indirect` + `indirect_command_read`, so it participates in both
paths. The AS→shader barrier (GiProbe) duplicated the post-barrier that
`build_tlas_in_place` already emits.

**P1 — Intra-pass hazard tracking (the one real feature). LANDED 2026-07-28.** Implemented at
the `note_touched` chokepoint (`RecordingContext.cpp`) rather than per-dispatch, so it covers
every access path uniformly. A per-pass `m_last_access` map (`const void*` → `{stages, access}`,
moved with the context like `m_touched`, fresh per pass) records the last access of each
resource; on a re-access where either side writes, it emits one targeted `buffer_barrier` /
`image_barrier` / `memory_barrier` (by `resource_ref::type`) before the command. Image barriers
carry `prev_state = next_state = m_image_states[ptr].current`, which is the pre-transition state
because `note_touched` runs before `transition_image_for_binding` — so the tracker emits pure
sync and layout management stays with the existing transition path. Lookup is by pointer and at
most one barrier is emitted per access, so emission order is command order, not hash-map order.

  Two supporting fixes were required and are part of this change:

  - **`push_bindings` stage attribution.** It hardcoded `vertex|fragment|mesh|task`, but the VBD
    solver uses it for *compute* dispatches (`push_bindings<E>` + `dispatch_indirect`). Every
    compute access in that loop was therefore recorded under graphics stages — wrong for the
    cross-pass path too, since `srcStage = fragment_shader` does not order a compute write.
    `bind()` now records `shader_program::is_compute()` and `bound_shader_stages()` returns
    `compute_shader` accordingly.
  - **DX12 dropped same-state write hazards.** `cmd_pipeline_barrier` skipped any buffer/image
    barrier whose `before == after`, so a UAV→UAV hazard — the common compute RAW/WAW — produced
    *no* barrier at all, while the manual `rec.barrier` path (a `memory_barrier`) correctly
    became a global UAV barrier. Same-state write hazards on an `UNORDERED_ACCESS` resource now
    emit a per-resource `D3D12_RESOURCE_BARRIER_TYPE_UAV`. This is a standalone DX12 correctness
    fix; it applies to the cross-pass path as well.

  **Intentional scope limit:** the intra-pass tracker skips hazards whose *destination* is a
  graphics stage, so repeated `push_bindings` across draws in one render pass do not emit a
  barrier per draw. Graphics-after-graphics intra-pass hazards on bindless storage resources
  (e.g. the meshlet OIT accumulation path) are therefore still the shader's responsibility.
  Compute-after-graphics is covered, as is everything at a pass boundary.

**P2 — Close descriptor-type coverage in `register_one_bindless`.** Acceleration structures
are still unhandled (only image/buffer), *and* AS resources are never `set_slot_resource`'d.
This is **no longer a correctness gap**: `build_tlas_in_place` / `build_blas_in_place` end with
an explicit `rec.pipeline_barrier` post-barrier (AS build write → `acceleration_structure_read`
in compute|fragment), which is what the deleted GiProbe `acceleration_structure_to_shader`
barrier was duplicating. Closing P2 would let the tracker derive that instead of the builders
hardcoding it, and would make an AS bound-but-never-built detectable. Note: **descriptor arrays
are correctly skipped, not a gap** — the only `count > 1` binding is `sampler2d_array` = the
global bindless texture table (`bindless_texture_capacity`); per-texture barriers on the global
table would be nonsense.

**P3 — Kill the silent-failure tail (strictly after P-reg).** `register_one_bindless` silently
no-ops when `resource_for_slot` returns null (`if (ref.ptr)`) — the *same* silent-hang class
relocated: a bound-but-unregistered slot yields no barrier and no error. The obvious debug
assert ("a *valid* (`!= invalid_index`) storage/image slot must resolve to a live resource")
is **only correct once P-reg makes the registry complete** — attempted before it, it
false-positives on every manual-slot bind (TAA `hdr_color` crashed on exactly this). Sequence
it strictly after P-reg, and pair it with the slot-recycling invalidation story (slot ∈
registry ⇔ live). **This is the dominant correctness risk.**

  *Discovered during the early attempt:* `capture_stacktrace` (`Stacktrace.cpp:58`, via
  `std::to_string(stacktrace_entry)`) faults while formatting an assertion that fires on a
  worker thread — so a failed `gse::assert` in the record path surfaces as a raw
  `0xc0000005` AV *instead of* the assertion message, swallowing the diagnostic. Worth
  hardening independently, since it masks every worker-thread assert, not just this one.

**P4 — host→compute (`RtShadowRenderer.cpp`). RESOLVED — the barrier was never needed.** Host
writes are not GPU ops the graph sees, but neither backend requires an explicit dependency for
them: Vulkan's queue-submit performs an implicit host→device memory-domain operation for writes
made before the submit (which is when every `buffer::host_write` here runs), and the DX12
backend already discards `host_write` buffer barriers outright
(`cmd_pipeline_barrier`, `Dx12/Device.cpp`). The `memory_barrier` form of the deleted
`host_to_compute` only ever reached DX12 as a *global UAV barrier* — which the intra-pass
tracker now emits per-resource, and only where a real hazard exists.

  Note the gap this leaves for `append_host_dirty_barriers`, which is the belt-and-braces path
  for the typed API: it keys off `resource_ref::host_buffer`, and the bindless slot registry
  never populates that field — `create_buffer` cannot (its local `buffer` is about to be moved
  out, so its address would dangle) and `write_storage_buffer` currently does not. So a
  host-written *bindless* buffer is invisible to that pass. Correct today for the reason above;
  worth closing if a backend ever needs a real host barrier.

## Risks

- **Registry lifetime / slot recycling** — the dominant correctness risk; a stale slot →
  address entry produces a wrong or missing barrier (the same silent failure, harder to
  spot). Needs tight invalidation and the P3 debug assert (slot in registry ⇔ live).
- **Over-barriering** — a `read_write` pack member is marked written every dispatch, so a
  shared rw buffer barriers on every access. Safe, possibly slower than a hand-tuned scope, and
  there is no longer an opt-out. Mitigation is per-`Entry` access precision (a member the shader
  only reads should be `read`, not `read_write`), not a hand-placed barrier. Measure hot passes
  before assuming the auto path is free.
- **Determinism** — barrier placement must not depend on hash-map iteration order. The
  intra-pass tracker is a keyed lookup emitting at most one barrier per access, so its order is
  command order; the cross-pass path must keep the stable per-binding pack order.

## Acceptance / validation

Nothing below has been run yet — this change is unverified.

- `--physics-parity --use-gpu-solver` stays green (boxes fall, run-to-run deterministic,
  CPU↔GPU within tolerance) with the manual barriers deleted. **This is the real test:** the
  tracker is not additive, it *replaces* the manual barriers, so a missed hazard surfaces as
  non-determinism or a TDR rather than as a slowdown.
- Locomotion smoke trials 2–5 bit-identical (`state_hash` unchanged).
- Bloom / RT-shadow / GI-probe render paths unchanged visually — Bloom's mip ping-pong is the
  one image-resource intra-pass hazard in the tree and the only exercise of the image branch.
- DX12: physics no longer freezes/falls-through (the UAV-barrier fix is the first time a
  compute UAV→UAV hazard produces any barrier at all on that backend).
- P3: the debug assert fires on a deliberately-unregistered bound slot and never on a
  legitimately-unbound (`invalid_index`) one.

## References

- `Engine/Engine/Source/GpuRecord/RecordingContext.cppm` — `register_one_bindless`,
  `register_bindless_usage`, compute `dispatch`, graphics `push_bindings`, `access_track` /
  `m_last_access`.
- `Engine/Engine/Source/GpuRecord/RecordingContext.cpp` — `note_touched`,
  `emit_intra_pass_barrier`, `bound_shader_stages`, `transition_image_for_binding`.
- `Engine/Engine/Source/Gpu/Graph/RenderGraph.cpp` —
  `append_prev_pass_barriers`, `append_barrier_for_resource`, `append_host_dirty_barriers`.
- `Engine/Engine/Source/Dx12/Device.cpp` — `cmd_pipeline_barrier`
  (`unordered_hazard_between`, the same-state UAV-barrier path).
- `Engine/Engine/Source/Gpu/Device/Device.cpp:660,683,693` — `set_slot_resource` /
  `resource_for_slot`; `:466` — `m_slot_resources`.
- `Engine/Engine/Source/Gpu/Graph/TransientPool.cpp:221` — `free_slot_resources`.
- `Engine/Engine/Source/GpuBackend/Enums.cppm:19,24` — `descriptor_access`,
  `descriptor_type`; `Core.cppm:49` — `bindless_slot::invalid_index`.
- `Engine/Engine/Source/Gpu/Shader/ShaderCodegen.cppm:594,622,632` — `descriptor_type_of`,
  `descriptor_count_of`, `descriptor_access_of`.
- `Engine/Engine/Source/Physics/VBD/GpuSolver.cpp` — the solve loop that carried 30 of the 35
  deleted barriers.
- `Engine/Engine/Source/Graphics/Renderers/BloomRenderer.cpp`, `RtShadowRenderer.cpp`,
  `GiProbeRenderer.cpp`, `CullComputeRenderer.cpp` — the same idiom in renderers.
- `Engine/Engine/Source/GpuRecord/AccelerationStructure.cpp` — `build_blas_in_place` /
  `build_tlas_in_place`, which keep explicit `rec.pipeline_barrier` pre/post sync.
- Memory: `gpu-bindless-render-graph-gotchas` (the bring-up that surfaced this).
