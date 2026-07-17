# Render-graph bindless auto-barriers — plan

Status: **cross-pass MVP landed; tracking the path to zero manual barriers.** Spun out of
the Phase-3 headless GPU VBD bring-up (2026-06-16), where a missing barrier on a
bindlessly-written buffer caused a multi-day TDR hang. The original "proposed" design has
since been implemented (the slot registry + consteval access walk, wired into both compute
`dispatch` and graphics `push_bindings`). This doc now records what exists, how good the
automatic path can realistically get, and the ranked work remaining to retire the manual
`rec.barrier` calls as a *correctness* requirement.

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

**Opt-out philosophy.** `rec.barrier` (and a future per-dispatch range/skip annotation) stays
in the API — not as correctness you can forget, but as a **performance knob** for the rare
hot pass where the author has knowledge the tracker lacks. This mirrors every production
render graph (Frostbite FrameGraph, RGL): full automation with a manual override. Success =
every *functional* barrier is automatic; every surviving `rec.barrier` is a deliberate,
measured optimization.

## The two classes of remaining manual barriers

`note_touched` accumulates per pass and flushes at the pass boundary (`finalize_pass`), so
the automatic path is **cross-pass only**. The `co_await gpu::pass<Stage>(ctx)` idiom makes
each stage a separate pass — often one dispatch each — so the split is:

- **Cross-pass** (one dispatch per `co_await pass`; the `rec.barrier` is the first op of the
  new pass, guarding against the previous pass): the VBD collision stages
  (`GpuSolver.cpp:1124,1136,1148,…`) and the `finalize → state_copy` transfer barrier.
  **Covered today** by `append_prev_pass_barriers` — redundant, deletable after verification.
- **Intra-pass** (multiple dispatches on one `rec`): the VBD solve-iteration loop
  (`GpuSolver.cpp:1261-1312`) and Bloom's down/up mip loops (`BloomRenderer.cpp:243,275`).
  `append_prev_pass_barriers` runs at pass boundaries only, so these are **not covered**.

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

**P0 — Reap the cross-pass win already paid for.** Verify, then delete the cross-pass manual
barriers **for passes whose resources are all registry-tracked**. Passes that bind through
the manual slot path (TAA, Bloom) keep their barriers until P-reg lands. No new code; gated on
the validation harness below.

  *Status (VBD cross-pass done — 11 barriers removed).* First tranche (8, **verified** via
  `--physics-parity --use-gpu-solver` determinism): the cross-pass `compute_to_compute`
  barriers that were each the first op of a fresh `co_await gpu::pass` guarding a single
  `dispatch<Entry>` (grid_build, broad_phase, prepare_indirect, prepare_contact_indirect,
  build_adjacency, build_coloring, prepare_color_indirect, predict). Second tranche (3, deleted
  after that verify — re-verify pending): narrow_phase's `compute_to_compute`, freeze_jacobians'
  `compute_to_compute`, and the `compute_to_transfer` at `vbd_state_copy_stage` — the original
  footgun (finalize writes `body_buffer` bindlessly → state_copy reads it via `copy_buffer`; now
  auto-covered because `body_buffer` is registered). All buffers involved are
  `create_buffer(.bindless)`-registered, so `append_prev_pass_barriers` emits the equivalent
  targeted barrier at each pass boundary. **Kept** (all intra-pass or unconfirmed): the
  solve-iteration loop, restitution, post-stabilize, the finalize dispatch→copy sequence, and
  the 3 `compute_to_indirect` barriers (indirect-buffer tracking not yet confirmed). The
  remaining survivors are essentially all **intra-pass → P1 territory**.

**P1 — Intra-pass hazard tracking (the one real feature).** `register_one_bindless` already
computes the exact `{resource, stage, access}` tuple per dispatch; today it only forwards it
to the pass-level accumulator. Add a per-pass "last accessor" map; before recording each
dispatch's accesses, diff against it and emit a **targeted** buffer/image barrier on a
RAW/WAW/WAR, then update the map. It is a diff-and-emit step on data already in hand, not new
analysis. Targeted per-resource barriers can *beat* the blanket `compute_to_compute` (e.g.
Bloom's true mip ping-pong RAW) when per-`Entry` packs carry accurate read/read_write
qualifiers. Emit in stable pack order — never hash-map order — to preserve GPU determinism.

  *Status (v1 landed — buffers only, additive, unverified).* Implemented at the `note_touched`
  chokepoint (`RecordingContext.cpp`) rather than per-dispatch, so it covers every access path
  uniformly — bindless dispatch, `copy_buffer`, `fill_buffer`, `dispatch_indirect`. A per-pass
  `m_intra_access` map (`const void*` → `{stages, access}`, moved with the context like
  `m_touched`, fresh per pass) records the last access of each buffer; on a re-access where
  either side writes (RAW/WAW/WAR), it emits one targeted `buffer_barrier` before the command
  and replaces the entry; read-after-read accumulates. **Scoped to `resource_type::buffer`**
  (retires the VBD solve-iteration loop, the bulk); images need `prev/next_state` layout
  handling and stay on manual barriers (Bloom) for a v2. The change is **purely additive** — it
  adds barriers, removes none — so it cannot cause a *missing*-barrier hang. v1 verified working
  (full game run, additive phase).

  *Follow-up (redundant-barrier cleanup — pending re-verify).* With v1 proven, deleted all 14
  now-redundant `compute_to_compute` barriers in the VBD solver (`GpuSolver.cpp`: the
  solve-iteration, restitution, and post-stabilize loops, plus the solve pass's cross-pass
  first-op guard). The intra-pass ones are covered by the v1 tracker; the one cross-pass guard
  is covered by the already-verified cross-pass path. **This is the real test of v1's
  sufficiency** — v1 only proved the auto barriers don't *break* anything (they were additive);
  removing the manual ones proves they *suffice*. **Kept**: the 3 `compute_to_indirect` (indirect
  dimension, unconfirmed) and the finalize `compute_to_transfer`/`transfer_to_compute` sequence
  (buffer hazards the tracker likely covers, but held for a separate round). A missed hazard now
  surfaces as non-determinism or a TDR → `git checkout GpuSolver.cpp` restores the barriers.

**P2 — Close descriptor-type coverage in `register_one_bindless`.** Acceleration structures
are unhandled (only image/buffer), *and* AS resources are never `set_slot_resource`'d (only
buffers + storage/sampled images are). Covering the GiProbe AS→shader barrier needs both a
registry hook at AS registration and an `acceleration_structure` branch here (access =
shader read). Note: **descriptor arrays are correctly skipped, not a gap** — the only
`count > 1` binding is `sampler2d_array` = the global bindless texture table
(`bindless_texture_capacity`); per-texture barriers on the global table would be nonsense.

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

**P4 — host→compute (`RtShadowRenderer.cpp:252`).** Host writes are not GPU ops the graph
sees. Either a `note_host_write` on the map/unmap path or leave it as the one sanctioned
manual barrier. Tiny.

## Phase 2 note — why intra-pass was deferred originally

Higher complexity (mid-pass barrier insertion, per-dispatch bookkeeping) for barriers that
are local and visible. With the cross-pass foundation now proven, P1 is the natural next
lever and the bulk of the remaining `rec.barrier` calls. It reuses the existing per-binding
derivation, so the marginal cost is the intra-pass diff + a targeted-barrier builder.

## Risks

- **Registry lifetime / slot recycling** — the dominant correctness risk; a stale slot →
  address entry produces a wrong or missing barrier (the same silent failure, harder to
  spot). Needs tight invalidation and the P3 debug assert (slot in registry ⇔ live).
- **Over-barriering** — a `read_write` pack member is marked written every dispatch, so a
  shared rw buffer barriers on every boundary. Safe, possibly slower than a hand-tuned scope.
  Mitigation: per-`Entry` access precision + keep the `rec.barrier` opt-out. Measure hot
  passes before assuming the auto path is free.
- **Determinism** — barrier placement must not depend on hash-map iteration order; keep
  emission deterministic (stable per-binding order from the pack).

## Acceptance / validation

- `--physics-parity --use-gpu-solver` stays green (boxes fall, run-to-run deterministic,
  CPU↔GPU within tolerance) **with the cross-pass manual barriers deleted** (P0).
- Locomotion smoke trials 2–5 bit-identical (`state_hash` unchanged).
- A focused test: a two-pass graph (compute writes a bindless buffer → second pass reads it)
  hangs/validation-errors **without** the feature and passes **with** it.
- Bloom / RT-shadow render paths unchanged visually.
- P3: the debug assert fires on a deliberately-unregistered bound slot and never on a
  legitimately-unbound (`invalid_index`) one.

## References

- `Engine/Engine/Source/GpuRecord/RecordingContext.cppm:302` — `register_one_bindless`;
  `:334` — `register_bindless_usage`; `:342,350` — compute `dispatch`; `:357,364` —
  graphics `push_bindings`.
- `Engine/Engine/Source/Gpu/Graph/RenderGraph.cpp` — `note_touched`,
  `append_prev_pass_barriers`, `append_barrier_for_resource`.
- `Engine/Engine/Source/Gpu/Device/Device.cpp:660,683,693` — `set_slot_resource` /
  `resource_for_slot`; `:466` — `m_slot_resources`.
- `Engine/Engine/Source/Gpu/Graph/TransientPool.cpp:221` — `free_slot_resources`.
- `Engine/Engine/Source/GpuBackend/Enums.cppm:19,24` — `descriptor_access`,
  `descriptor_type`; `Core.cppm:49` — `bindless_slot::invalid_index`.
- `Engine/Engine/Source/Gpu/Shader/ShaderCodegen.cppm:594,622,632` — `descriptor_type_of`,
  `descriptor_count_of`, `descriptor_access_of`.
- `Engine/Engine/Source/Physics/VBD/GpuSolver.cpp:1124…` — cross-pass barriers (P0 retires);
  `:1261-1312` — the intra-pass solve loop (P1 retires).
- `Engine/Engine/Source/Graphics/Renderers/BloomRenderer.cpp:243,275`,
  `RtShadowRenderer.cpp:252`, `GiProbeRenderer.cpp:174` — the same idiom in renderers.
- Memory: `gpu-bindless-render-graph-gotchas` (the bring-up that surfaced this).
