# Physics Module Review

Scope: `Engine/Engine/Source/Physics/**` — the two solvers (`vbd::solver`, `vbd::gpu_solver`), the shared constraint structs, and `physics::data` / `prepare` / `integrate` / `frame` that tie them together. Reviewed against `docs/CODE_REVIEW_GUIDE.md` and `docs/STYLEGUIDE.md`.

Nothing here has been compiled. Verification was static inspection only, per the guide's Verification section. No code was changed.

Where a finding overlaps `docs/solver_plan.md`, that is called out — the readback-ownership items are the near-term expression of the graph-owned readback channel design already agreed there, not a competing proposal.

---

## The architecture read

The messiness has one root: **the split is backend-first when the structure is stage-first.**

Both backends run the same pipeline:

```
ECS -> body_state[] -> joint/motor/impulse constraints -> solve -> body_state[] -> ECS
```

Only `solve` genuinely differs. Everything on either side of it is the same work. But the code splits at the *tick*, into `update_vbd` (330 lines) and `update_vbd_gpu` (410 lines), each re-deriving the whole pipeline. The consequences are all downstream of that one decision:

- The joint-constraint build is written twice, **26 fields, character-identical** (`System.cpp:1391` and `System.cpp:1678`). The motor build is written twice, 5 fields, identical (`System.cpp:1356`, `System.cpp:1654`). Adding a field to `joint_constraint` requires two edits and nothing catches the miss.
- `build_body_states` exists and is used by the GPU path and the shadow step — and the CPU path hand-rolls a *different subset* of it inline (`System.cpp:1546-1595`).
- `body_state` is presented as the shared representation but is only half-shared: `shape_kind`, `shape_params`, `half_extents`, `aabb_min`, `aabb_max`, `reset_pending` and `accel_weight` are populated on the GPU path and left at zero on the CPU path, which keeps `accel_weight` in a parallel `solver::m_accel_weight` instead. The struct says "one representation"; the code has two.
- `id_to_body_index` is a member of the shared physics state that **only the GPU path writes** — so three exported queries silently return the wrong answer whenever the CPU backend is live (finding 2).

The second root is that **"which backend is live" has no owner.** `use_gpu_solver` is read raw in seven places, each pairing it with different extra conditions (`gpu_s`, `gpu_buffers_created`, `compute_initialized()`, `buffers_created() && body_count() > 0`). `d.gpu_buffers_created` is a third copy of a fact `gpu_solver` already owns. Nothing reconciles them, which is why flipping the setting at runtime stops the simulation with no diagnostic (finding 1).

The third is that **`gpu_solver` is published `[[= gse::shared]]` in full**, so every system in the engine can reach its device buffers and upload state when no consumer needs more than a snapshot span and a body count (finding 6).

*Revision note:* findings 3 and 6 were first written against a tree that predated the graph-owned readback channel migration, which landed the same day. Both are corrected below; the live hazards they described are gone.

The proposed shape is in [Proposed structure](#proposed-structure) at the end.

---

## Status

**All seven steps of the proposed structure are applied** (unbuilt at the time of writing), except the `gpu_solver` projection, which is refused with reasons below.

**Step 7.** `data::joints` is now an `id_mapped_collection<joint_definition>` keyed by the entity that owns the `joint_spec`; `joint_handle` and the parallel `joint_handles_by_entity` map are gone, and with them the four `>= d.joints.size()` bounds guards that were hardening the broken index representation. `create_joint` and `remove_joint` take a `gse::id` instead of a shifting index, so the handle-invalidation defect is unspellable rather than merely unreached.

`add_scene_contacts_to_solver` lost three parameters: `bool update_scene_state` and the `write<collision_result_component>*` pointer, both of which its single caller always filled the same way, and `write<motion_component>&`, which became unused once `pending_pair_meta` started carrying the body indices that were in scope at its construction all along. That removes two `motion.find` calls plus pointer arithmetic per contact pair from the emit loop. `build_motor_constraints` now takes its airborne index from the `id_to_body_index` lookup it already performs rather than from a second, pointer-derived one.

Two pointer-subtraction sites remain deliberately, in the GPU and CPU impulse loops. Both run *before* the body build, so `id_to_body_index` still holds the previous tick's mapping there; the motion-array index is the honest source at that point. Substituting the map would introduce a stale-index bug in exchange for cosmetics.

**Steps 5 and 6.** `gpu_solver_stats` deleted outright — it was pushed, stored into `data::gpu_stats`, and read by nothing. `gpu_body_index_map` deleted; `GeometryCollector` reads `phys_s.id_to_body_index` through a shared view instead of rebuilding an `unordered_map` from channel entries every frame. `gpu_upload_payload` became `vbd::solver_upload`, living next to the solver whose input contract it is: `gpu_solver::upload` takes it whole, so the nine positional arguments (four same-typed spans, two adjacent bools) are gone, and the three vectors are now moved into the channel instead of deep-copied every GPU tick. `gpu_solver_frame_info` is derived once in `gpu_solver_frame_info_of` rather than spelled out at two sites. The GPU diagnostics block moved out of `update_vbd_gpu` into `physics::gpu_diagnostics`, a system with its own state and an `enabled` setting; `set_color_launch_hint` stays in the tick, because it is a production input derived from `diagnostics()`, not a diagnostic.

### Finding 13, deferred: the remaining allocations want a measurement first

`add_scene_contacts_to_solver` builds `std::vector<std::vector<pending_pair_meta>>` and `std::vector<std::vector<pending_point>>` sized by chunk count on every call — once per substep. `build_pair_set` does the same with `per_chunk_candidates`, but only on a pair rebuild, so it is the colder of the two. The inner buckets do not allocate until first push, so the real cost is their growth reallocation, not the outer construction.

Hoisting them is not free, and that is why it is not done here:

- `pending_pair_meta` and `pending_point` are function-local types. Persistent scratch means promoting both to namespace scope and putting an aggregate on `physics::data`, plus a parameter on the signature — two new exported types and a new state member to remove an unmeasured cost.
- `frame_arena` is the wrong tool: the buckets are filled on worker threads and consumed on the main thread, which is the cross-thread free recorded as the multi-GB leak in `frame-arena-cross-thread-leak`.
- A flat single-vector rewrite with a counting pre-pass avoids the new types but is an algorithm change to the broad phase, which is gate-relevant.

The guide's own rule applies: verify the actual call frequency rather than assuming, and do not add machinery for a cost that has not been shown. The instrumentation already exists — `vbd_cpu::broad_phase::pair_loop` and `::emit` are live trace scopes. The right sequence is to read those in a CPU-solver profile, and hoist only if the allocation shows above the pair test and narrow phase around it. Doing it blind would be a state-and-types increase justified by nothing.

### Finding 6, refused: the `gpu_solver` projection is not worth building

The proposal was to stop publishing `gpu_solver` and publish a small read-only projection. Working through the consumers, it does not pay:

- The union actually needed through `shared_view` is ten accessors — `buffers_created`, `body_count`, `retired_generation`, `solver_cfg`, `diagnostics`, `reseeding`, and four readback spans — because `ContactTrace` alone uses seven of them, including `read_contact_dump`, `read_grounded` and `read_narrow_phase_debug`. A projection wide enough for the diagnostic systems is not a narrowing.
- The four readback accessors return spans into the graph channel's staging ring. Storing those spans in shared state would be strictly *worse* than the current method calls: `latest()` resolves at the point of use, whereas a stored span goes stale the moment three frames pass without a physics tick — and the style guide's rule against retaining raw pointers from a shared view exists for exactly that. Making it safe means copying the body array (~1.3 MB) every tick.
- What remains — carrying only the scalars in a projection while the spans stay method calls — splits every consumer across two access routes to buy nothing, and fails the guide's own test that a prevention must remove more complexity than it introduces.

The narrowing that *would* pay is the one the readback-channel migration already delivered: the accessors are non-blocking and cannot serve an unretired version. What is left is a wide read-only surface with no live defect behind it. Recorded as accepted rather than fixed.

**Steps 1-3** (applied earlier in the same pass):

| Finding | State |
|---|---|
| 1 — runtime `use_gpu_solver` flip stops physics | Fixed in the physics module; settings-layer prevention still open (see below) |
| 2 — `id_to_body_index` GPU-only | Fixed; `update_vbd` now publishes it |
| 3 — ungated snapshot read | Retracted; resolved by the readback-channel migration |
| 4 — duplicated tick pipeline | Fixed; constraint builds and body build all unified |
| 5 — three default layers | Fixed; `default_solver_config` deleted |
| 6 — `mutable` + blocking `const` | Resolved by the readback-channel migration; scope corrected to the `[[= gse::shared]]` surface |
| 7 — `remove_joint` invalidates handles | Fixed; `id_mapped_collection` keyed by owner id |
| 8 — write-only `gpu_solver_stats` channel | Deleted |
| 9 — payload copies + 9-arg `upload` | Fixed; vectors moved, payload passed whole as `vbd::solver_upload` |
| 10 — diagnostics in the production tick | Fixed; moved to `physics::gpu_diagnostics` |
| 11 — index map published three ways | Fixed; channel deleted, consumers read the shared member |
| 12 — `gpu_solver_frame_info` derived twice | Fixed; one `gpu_solver_frame_info_of` |
| 13 — per-substep allocation | Half fixed; `bodies` and `result_bodies` hoisted. The remaining half is **deliberately deferred pending measurement** — see below |
| 15 — index by pointer subtraction | Mostly fixed; two sites kept with reasons above |
| 16 — dead parameters on the broad phase | Fixed; three parameters removed |
| 14 — `gpu_buffers_created` third copy | Deleted |
| 18 — transform-less phantom body | Fixed |

What landed: `build_joint_constraints`, `build_motor_constraints`, `build_mass_properties`, `build_body_states` and `build_body_bounds` each have one home and both backends call them; `gpu_solver_active(const data&)` / `gpu_solver_active(shared_view<data>)` is the single authority for the live backend and all six consumers route through it; `integrate` falls back to the CPU solver with a one-time error log instead of silently doing nothing; `default_solver_config()` is gone and `solver_config_from_settings` builds the config directly; `shadow_step` now takes `phys.vbd_solver.config()` — the live CPU config — so the parity instrument measures the solver it is comparing against; the two disagreeing `solver_config` member defaults (`iterations`, `velocity_sleep_threshold`) now match the settings defaults.

The body-build unification needed three prerequisites, all applied:

- **Mass properties hoisted onto `body_build_view`** as `mass_props`, filled by a new `build_mass_properties`. Without this the CPU would recompute them every substep instead of once per tick. The CPU's old serial loop is replaced by the parallel one both paths now share.
- **The AABB / shape-params pass split into `build_body_bounds`.** Only the GPU broad phase reads `shape_kind`, `shape_params`, `half_extents`, `aabb_min` and `aabb_max` — verified by search — so the CPU simply does not call it rather than paying for it every substep behind a flag.
- **The transform-less phantom fixed** (finding 18), which is what made the shared builder behaviour-preserving for the CPU.

Three smaller things fell out while doing it:

- `body_build_view::body_airborne` was never read by the function that took it. Deleted; all three call sites stopped filling it.
- `build_body_states` inserted a `{id{}, 0}` entry into `id_to_body_index` for every transform-less body, because the staging vector was index-assigned in parallel and never filtered. Now filtered on `has_transform`. This matters more than it did, since `update_vbd` now publishes that map.
- `motion_component::reset_pending` has **no writer anywhere in the repo** — it is read at `System.cpp:1143` and cleared at `System.cpp:1392`, and nothing ever sets it. So it is always zero, the read is a dead branch, and the clear is a no-op. Recorded under Low; not removed, since the field looks like it was meant to carry teleport intent and deleting it is a design decision rather than a cleanup.

**The `restart_required` latch is applied.** The annotation now means what its name says, for every setting carrying it.

`save::registry` gained a staged layer — `stage_value` / `clear_staged` over a `m_staged` doc, mirroring the existing `m_overrides` shape. `save_to_file` overlays it *after* the live-state serialization and the session-override restore, so a staged value reaches the user ini while live state is untouched; the next boot loads it normally. `panel_state::apply_all` stages restart-required fields instead of calling `push_change`, and clears the staged entry when a field is edited back to its live value. `discard_all` now takes the registry and drops staged entries, so Discard cannot leave a change that still fires next boot. The hot-reload fast path is skipped for restart-required fields, which would otherwise have pushed them live before Apply was ever pressed.

The footer already reads correctly with no change: a staged field stays `modified` (live value did not move), so the status shows "N unsaved change(s) - Restart required" and the Restart button appears.

The physics fallback stays, because the latch closes the settings-panel path but not `set_override`, `pin`, or a command-line assignment — all of which still write live state through `apply_one_key`. The log line now says which is which.

---

## Critical

### 1. Flipping `use_gpu_solver` at runtime stops physics with no diagnostic

`System.cppm:113`, `System.cpp:838`, `System.cpp:1011`, `System.cpp:1036`

**Impact.** The world freezes completely — no integration, no collision, no log line, no assert. It reads as a hang in whatever the user was testing, and the cause is a settings toggle three menus away.

**Mechanism.** `use_gpu_solver` carries `settings::restart_required`, but that annotation is presentational: `settings::panel_state::apply_all` (`Graphics/2D/Gui/Settings.cppm:488`) still calls `field.push_change(...)` and only sets `restart_pending_applied` to drive a banner. So the bool does flip live. `init` is the only place that creates GPU resources and it ran under the old value (`System.cpp:842`), leaving `d.gpu_buffers_created == false`. `integrate` then takes the GPU branch (`System.cpp:1011`) and `update_vbd_gpu` returns at its first line (`System.cpp:1036`). Every subsequent tick does nothing.

**Resolution.** Make the live backend a derived, single-authority fact rather than a raw setting read. Either lazily initialise the GPU solver on first GPU-branch entry, or — if a restart genuinely is required — have the physics system detect `use_gpu_solver && !d.gpu_solver.buffers_created()`, log once at error level, and continue on the CPU solver rather than doing nothing. The `if (!d.gpu_buffers_created) return;` guard is the shape the guide warns about: a silent early return that converts a broken invariant into a missing feature reported nowhere.

**Prevention.** Recurring class — `restart_required` is on four other settings (`Gpu/Context.cppm:33,39`, `GpuBackend/DeviceSettings.cppm:23`, `ShadowStep.cppm:44`) and none of them can be assumed to be latched either. The proportionate guardrail is at the settings layer: `restart_required` should suppress the live `push_change` and stage the value for the next boot, so the annotation means what its name says for every current and future user. That is one change in `apply_all` and removes the whole class.

### 2. `id_to_body_index` is GPU-only, so three exported queries are silently wrong on the CPU backend

`System.cppm:293`, `System.cpp:1494`, `System.cpp:168-197`

**Impact.** With `use_gpu_solver` off, `query_transform` returns `nullopt` for every entity, `is_airborne` returns `true` for every entity, and `is_sleeping` returns `false` for every entity. None of them fails; they return the "not found" answer, which for `is_airborne` is indistinguishable from a correct negative. Any gameplay code that gates on grounded state gets the wrong answer on one backend and the right answer on the other.

**Mechanism.** `update_vbd_gpu` writes `d.id_to_body_index` through `build_body_states` (`System.cpp:1285`). `update_vbd` builds a *function-local* `id_to_body_index` (`System.cpp:1494`) with identical contents and never publishes it, so the member stays as `clear_runtime_state` left it — empty. The three query functions all begin with `d.id_to_body_index.find(...)` and take their absence branch every time.

This is the paired-derivation defect in its canonical form: the fact is derived twice, only one copy is published, and the reader cannot tell an empty map from an absent entity.

**Resolution.** The map is `motion_owners[i] -> i` on both paths — it is not backend-specific and should not be built twice. Have `update_vbd` call `build_body_states` (which it should anyway, per finding 4) so the member is written on both paths. Then `gpu_body_index_map` (finding 11) collapses into it too.

Better still: because the value is always the motion-component index, the map is derivable from the component array's own owner list and need not be stored at all. A `body_index_of(shared_view<data>, id)` that consults the motion component removes the state and the divergence together — the guide's "remove the state, not the branch".

**Prevention.** The strongest available guardrail is the one in finding 4: if there is only one body-state build, there is only one place the map can be written. This is a one-off consequence of the duplicated pipeline, not an independent error class.

*Note:* `is_airborne`, `is_sleeping` and `remove_joint` have no call sites anywhere in the repo; `query_transform` has one (`Sandbox/.../OrbitCamera.cpp:130`), which happens to fall back to the transform component and so masks the defect today. That is why this has not been noticed, not a reason to leave it.

### 3. ~~`physics_debug::prepare` host-reads the solver snapshot with no readiness gate~~ — RESOLVED before this review landed

**Retracted.** Written against a tree that predated the graph-owned readback channel migration, which landed the same day (`memory: gpu-graph-owned-readback-channels`, `docs/solver_plan.md` "Graph-owned readback channels"). In the current tree `snapshot_buffer()`, `retired_snapshot_slot()`, `snapshot_body_count()`, `wait_for_slot()` and `slot_ready()` are all deleted, and `PhysicsDebugRenderer.cpp:403` reads through `ps.gpu_solver.read_body_states()` — the channel's `latest()`, which never serves age < 2 and never blocks. The ungated read and the mis-sized read are both gone. The `docs/GRAPHICS_REVIEW.md:1753` sizing finding is closed with them, since count and data now come from the served version's published size.

What survives is only the weaker structural point, folded into finding 6 below: `gpu_solver` is still `[[= gse::shared]]`, so `body_buffer(slot)` remains reachable from any `shared_view<physics::data>`. That is now a boundary worth tightening rather than a live hazard.

---

## High

### 4. The tick pipeline is duplicated per backend

`System.cpp:1035` (`update_vbd_gpu`) and `System.cpp:1449` (`update_vbd`)

**Impact.** Every change to the shared representation has to be made twice, and the compiler cannot tell you when you make it once. The two copies have *already* diverged: the CPU build omits `shape_kind`, `shape_params`, `half_extents`, `aabb_min`, `aabb_max`, `accel_weight` and `reset_pending`; the GPU build omits the CPU's no-transform placeholder body (`.locked = 1u` at `System.cpp:1555`) in favour of a `has_transform` sidecar. Finding 2 is a direct consequence.

**Mechanism.** The split is at the tick rather than at the solve. Concretely duplicated:

| Work | GPU | CPU |
|---|---|---|
| `joint_constraint` build (26 fields, identical) | `1391-1419` | `1678-1706` |
| `velocity_motor_constraint` build (identical) | `1356-1363` | `1654-1661` |
| rest-orientation seeding | `1385-1388` | `1672-1675` |
| body-state build | `build_body_states` | inline `1546-1595` |
| motor ground/sleep gating | `1335-1354` | `1633-1652` |
| impulse -> sleep-counter reset | `1247-1258` | `1459-1470` |
| transform/velocity writeback | `1063-1095` | `1745-1774` |

**Resolution.** Extract the shared halves and let both drivers call them: `build_joint_constraints`, `build_motor_constraints`, `build_impulse_constraints`, `scatter_body_states`, and `build_body_states` for both (the CPU path simply does not read the AABB fields — it has its own broad phase). Each of the two drivers then shrinks to the part that is genuinely different: the CPU's substep loop with CPU broad/narrow phase, and the GPU's upload/readback marshalling. That is the refactor the guide's "Would a larger refactor leave fewer concepts and fewer special cases?" is asking for.

**Prevention.** Once there is one build site per constraint type, a new field is either set there or set nowhere — the compiler still will not catch a missing field, but there is only one place to look. A stronger guardrail is available if wanted: `joint_definition` and `joint_constraint` carry the same 26 fields under the same names, so the copy is reflection-derivable (`gse.meta` member walk) rather than hand-written. That removes the class entirely and is proportionate given the field count.

### 5. Three disagreeing default layers for `solver_config`, and the parity instrument reads the wrong one

`Solver.cppm:24-58`, `System.cpp:199-215`, `System.cppm:114-274`, `ShadowStep.cpp:411-414`

**Impact.** The shadow step is described in its own settings text as a parity instrument — "a residual is a defect rather than chaos". It is not measuring the solver that is running. It compares the GPU against a CPU solver configured from settings, using a GPU config built from hardcoded defaults. Change `Physics.collision_margin`, `solver_alpha`, `solver_beta`, `solver_gamma`, `penalty_min/max`, `stick_threshold`, either sleep threshold, or either speculative margin, and the residual it prints is the difference between two different simulations. Given how much of `solver_plan.md` rests on shadow-step residuals, a wrong reading here is expensive.

**Mechanism.** The same value has three homes that can disagree, and one already does:

| Field | `solver_config` member default | `default_solver_config()` | `physics::data` setting |
|---|---|---|---|
| `iterations` | 4 | 15 | 15 |
| `velocity_sleep_threshold` | 0.001 m/s | 0.05 m/s | 0.05 m/s |

`solver_config_from_settings` (`System.cpp:217`) starts from `default_solver_config()` and then overwrites every field it sets, so on the production path `default_solver_config` is entirely inert — every value it names is either immediately replaced or identical to the member default it restates. Its only live caller is `ShadowStep.cpp:411`, which overrides just `iterations`, `use_jacobi` and `jacobi_omega` and inherits the stale rest.

**Resolution.** Delete `default_solver_config()`. Values a setting owns get their default only on the `data` member; values no setting owns keep the `solver_config` member default. Fix `velocity_sleep_threshold` while doing it. Have `shadow_step::run` call `solver_config_from_settings` so the instrument measures the configured solver.

**Prevention.** Local — but worth recording in `solver_plan.md`'s rules section, because a residual measured under the old code is not comparable to one measured after, and the ledger there should say so.

### 6. `gpu_solver` is published `[[= gse::shared]]` in full

`System.cppm:301`, `GpuSolver.cppm:147`

**Corrected scope.** As first written this finding was about `mutable std::uint64_t m_waited_generation` letting `const` readers block on a GPU fence. That is resolved — the graph-owned readback channel migration deleted `wait_for_slot`, `slot_ready` and the `mutable` member, and the `const` readers are now genuinely non-blocking. See the retraction under finding 3.

**Impact, as it now stands.** Publishing the whole solver object exposes `body_buffer(slot)`, `render_snapshot_slot()`, `dispatch_generation()` and the upload-side state to every system in the engine. Nothing currently misuses them, but the surface is far wider than any consumer needs, and it is the surface that produced the observation-layer race recorded in `gpu-vbd-stacking-explodes` and the ungated read that finding 3 caught before the migration removed it.

**Resolution.** Publish the read-only projection consumers actually use — `body_count()`, the latest retired snapshot span, `diagnostics()` — as a small shared struct, and keep the solver object private to the physics system. Consumers today are `PhysicsDebugRenderer`, `ContactTrace`, `CpuContactTrace` and `ShadowStep`; none needs a raw buffer.

**Prevention.** An ownership boundary, not a check — the unsafe read becomes unspellable rather than discouraged. Lower priority than it was: with the channel API guaranteeing age >= 2 by construction, the remaining exposure is a maintenance surface rather than a live defect.

### 7. `remove_joint` invalidates every handle above it

`System.cpp:161`, `System.cppm:294`

**Impact.** After one removal, every `joint_handle` in `d.joint_handles_by_entity` above the removed index points at the wrong joint. `prepare` then writes muscle activation and drive targets into unrelated joints (`System.cpp:918`, `System.cpp:933`), and re-resolving a `joint_spec` overwrites a different joint's definition (`System.cpp:900`). Latent today — `remove_joint` has no call sites — but it is exported API that cannot be called correctly.

**Mechanism.** `create_joint` returns `d.joints.size()` as the handle, i.e. a dense index; `remove_joint` does `d.joints.erase(begin + handle)`, shifting every later element down. `d.joint_handles_by_entity` is a hand-rolled parallel ID-to-index map over that vector, with no fixup.

**Resolution.** This is verbatim the guide's mandatory question: "Does contiguous storage maintain a parallel ID-to-index map or hand-roll swap removal? Use `id_mapped_collection` unless the dense indices are themselves stable identities stored outside the collection." They are not stable here. Replace `std::vector<joint_definition>` + `std::flat_map<id, joint_handle>` with `id_mapped_collection<joint_definition>` and drop `joint_handle` for `gse::id`. The bounds guards at `System.cpp:162`, `899`, `915` and `930` all disappear with it — they are hardening a representation that should not exist.

**Prevention.** The mandatory question is the guardrail and it exists; this got in because the map and the vector were added in separate changes. Worth a targeted sweep for the same pairing elsewhere in the module.

### 18. A body with no transform becomes a free-falling phantom in the GPU solver

`System.cpp:259`, `System.cpp:331`, `System.cpp:1272` — found while attempting the `build_body_states` adoption in step 1

**Impact.** An entity carrying a `motion_component` but no `transform_component` is uploaded to the GPU solver as an unlocked, gravity-affected, 1 kg body at the world origin. It participates in the grid, the broad phase and the solve, and it falls forever. Nothing reports it. Latent — I have not found a scene that constructs that pairing — but nothing in the module prevents one, and the failure is silent and off-screen.

**Mechanism.** `build_body_states` does `bodies.assign(body_count, vbd::body_state{})` and then `return`s early from the per-body lambda when the entity has no transform, leaving the default. `body_state`'s defaults are `mass = kilograms(1.f)`, `locked = 0`, `update_orientation = 1`, `affected_by_gravity = 1` — a valid dynamic body, not an inert one. `has_transform[i]` records the miss, but its only consumer clears `reset_pending`; the full `bodies` vector including the phantoms is what gets uploaded.

The CPU path never had this: its inline build writes `.locked = 1u, .update_orientation = 0u, .affected_by_gravity = 0u` for the same case (`System.cpp:1555` before this pass). The two builds disagreed, and the GPU one took the unsafe reading — which is why this is finding 4 wearing a different hat, and why it blocks the CPU adopting the shared builder.

**Resolution — applied.** `build_body_states` now writes the locked placeholder (`locked = 1`, `update_orientation = 0`, `affected_by_gravity = 0`) for transform-less bodies, matching what the CPU path always did. On the GPU such a body is now classified static, and because `build_body_bounds` skips it its AABB stays inverted (`min = +1e30`, `max = -1e30`), so it cannot pair with anything. It occupies a body slot and is otherwise inert.

**Prevention — still open.** The real guardrail is that `body_state{}` should not default to a valid dynamic body. `locked = 1` as the *member* default makes the value-initialised state inert, so every future partial build fails safe instead of spawning a phantom. Not done here: it needs a pass over every site that value-initialises a `body_state` and relies on the current default, including the shader-side struct, which is codegen'd from the C++ one. Worth doing deliberately.

---

## Medium

### 8. `gpu_solver_stats` is a write-only channel round-trip

`System.cppm:90`, `283`, `360`, `396`; `System.cpp:145`, `946`, `1813`

`frame` pushes it, `prepare` reads it into `d.gpu_stats`, and `d.gpu_stats` is never read by anything — verified across the whole repo. Both ends are the *same system* with the same `data&`, so the channel is not crossing an ownership boundary either. That is a message type, two channel parameters, a state member and a reset line, all in service of a value with zero consumers. Delete the type and all five references. Both fields it carries (`active`, `motor_count`) are directly readable from `d.gpu_solver` if a consumer ever appears.

### 9. `gpu_upload_payload` deep-copies three vectors per tick, then is destructured into a 9-argument call

`System.cpp:1429-1439`, `System.cpp:1792-1802`, `GpuSolver.cppm:93`

`.bodies = bodies` and `.motors = motors`, `.joints = gpu_joints` copy (`.impulses` correctly moves). At `limits.max_bodies = 20480` and `sizeof(body_state)` around 250 bytes, that is a multi-megabyte per-tick copy for a same-system handoff; the only reason `bodies` cannot move is that its `.size()` is read two lines later.

The payload is then unpacked into `upload(bodies, motors, joints, impulses, cfg, dt, steps, refresh_joints, force_reseed)` — nine positional arguments, four same-typed spans, and two adjacent bools. This is the guide's Parameter Objects section describing itself: the aggregate that should be passed already exists, is already built, and is thrown away one line before the call. Take `const gpu_upload_payload&` and both problems close.

`update_vbd` (8 parameters) and `update_vbd_gpu` (12) want the same treatment.

### 10. GPU diagnostics aggregation and logging sit in the production tick

`System.cpp:1098-1153`

Fifty-five lines of peak tracking, counter accumulation and `log::println` formatting run inside `update_vbd_gpu` on the hot path, mixed with the readback that actually drives the sim. `d.gpu_diag_peak`, `gpu_conflict_total`, `gpu_stale_read_total`, `gpu_stale_check_total`, `gpu_stale_first_frame` and `gpu_diag_frames` are all diagnostic state living in production system state.

The module already has the right pattern next door — `ContactTrace` and `CpuContactTrace` are separate systems reading `shared_view<physics::data>`. Move this to a `gpu_solver_diagnostics` system in the same module with the same shape. Note that `set_color_launch_hint` (`System.cpp:1099`) is *not* diagnostics — it is a production input derived from `diagnostics()` per `solver_plan.md:890` and must stay in the tick.

### 11. `id_to_body_index` is published three ways

`System.cppm:86` / `293`, `System.cpp:1046-1053`, `GeometryCollector.cpp:166-174`, `PhysicsDebugRenderer.cpp:412`

The same mapping exists as a `[[= gse::shared]] std::flat_map` member, as a `gpu_body_index_map` channel message rebuilt into a `std::vector<std::pair<...>>` and pushed twice per tick, and as an `std::unordered_map` that `GeometryCollector` reconstructs from that channel every frame. `PhysicsDebugRenderer` reads the shared member directly, so both delivery mechanisms are live simultaneously.

With finding 2 resolved the member is correct on both backends, and the channel and the per-frame `unordered_map` rebuild can both go. That also removes a per-frame allocation and hash-map build from the render path, which the guide's runtime-cost pass calls out specifically.

### 12. `gpu_solver_frame_info` is derived at two sites and carries a raw buffer pointer through a channel

`System.cpp:1159-1164`, `System.cpp:1818-1823`, `System.cppm:102`

The four-field construction — slot selection, `body_count`, `sizeof(body_state)`, `offset_of(^^body_state::position)` — is written out twice, verbatim, in two functions. Duplicated derivation: only one will be updated.

It also answers the guide's mandatory question "Does any deferred callback, task, or channel payload retain a raw pointer or reference obtained from a shared view?" with yes — `const gpu::buffer* snapshot` points into `d.gpu_solver.m_frames[slot].body_buffer` and is consumed next frame by `PhysicsTransformRenderer`. It survives because `data` is not reseated, which is an invariant nothing states. Publish the slot index and let the consumer resolve through the physics view, or move the whole thing onto the graph-owned readback channel per `solver_plan.md:857`.

### 13. Per-substep heap allocation and three full body-array copies in the CPU tick

`System.cpp:1539`, `System.cpp:1734`, `Solver.cppm:255`, plus `end_frame`

Inside `for (int step = 0; step < total_substeps; ++step)`: `std::vector<vbd::body_state> bodies` is constructed and destroyed each substep, `begin_frame` copies it into `m_bodies` via `assign`, and `end_frame` copies back out into a second per-substep `std::vector<body_state> result_bodies`. That is two allocations and three full copies of an N x ~250-byte array per substep, per tick.

The solver already owns `m_bodies`. Hoist both vectors out of the loop, and consider having the caller write into a span the solver exposes rather than round-tripping through two temporaries.

### 14. `d.gpu_buffers_created` is a third copy of a fact `gpu_solver` owns

`System.cppm:276`, `System.cpp:844`, `System.cpp:1036`

Written once in `init` from `d.gpu_solver.buffers_created()`, read once in `update_vbd_gpu`, and never refreshed. It cannot be right after any state change that `buffers_created()` reflects — which is what makes finding 1 silent. Ask the solver.

### 15. Body index recovered by pointer subtraction from the component array, six times

`System.cpp:797`, `805`, `1253`, `1342`, `1466`, `1640`

`motion.find(eid)` followed by `mc - motion.data()` to recover an index, each guarded by a bounds check against `body_airborne.size()`. The information is already available: `collision_pair` carries `body_index` (`System.cppm:310`), and `id_to_body_index` carries it for the motor and impulse loops. `pending_pair_meta` deliberately stores owners and then re-derives the index by pointer arithmetic when both indices were in scope at construction (`System.cpp:598-599`).

Underneath this is an unstated invariant: motion-component index and solver body index are the same number. Both paths do currently build the map as `i -> i`, but nothing enforces it, and `body_airborne` / `body_sleeping` are sized by `motion.size()` while being indexed by both meanings interchangeably. Carry the index on `pending_pair_meta` and delete the pointer arithmetic; if the two indices are meant to be the same thing, say so once instead of six times.

### 16. `add_scene_contacts_to_solver` has two dead parameters

`System.cppm:453`, `System.cpp:555`, single call site at `System.cpp:1619`

`bool update_scene_state` is passed `true` and `write<collision_result_component>* results` is passed `&results` by the only caller. The guide is decidable here: "A pointer parameter is a claim that absence is meaningful. When every call site passes a known address the claim is false and the null checks behind it are dead; take a reference." Both parameters guard branches that cannot fire (`System.cpp:790`, `812`, `830`). Take the reference, drop the bool, delete the branches.

### 17. `System.cpp` is 1824 lines holding six unrelated responsibilities

Joint authoring (`make_joint_definition`), body-state construction, broad phase, contact/manifold glue, both tick drivers, GPU upload marshalling, GPU diagnostics logging, and the frame dispatch all share one file. The style guide's file-organisation rule is written for classes but the review guide's architecture pass reaches the same conclusion. Splitting along the seams finding 4 introduces — `BodyBuild`, `BroadPhase`, `GpuBridge`, `System` — makes the backend-shared code visibly shared rather than incidentally adjacent.

---

## Low

- **`body_state` serves three ABIs at once** (`Constraints.cppm:183`): GPU shader layout, CPU solver working state, and render snapshot. `accel_weight` is the clearest casualty — the GPU writes it and `ContactTrace`/`ShadowStep` read it, while the CPU solver keeps the real value in `solver::m_accel_weight` (`Solver.cppm:195`) and leaves the struct field at zero, so `CpuContactTrace` prints a constant 0. Either the CPU solver writes the field or the field is documented as GPU-only; today it is neither.
- **`build_body_states` inserts a bogus entry** (`System.cpp:262`, `334`, `374`): `id_to_body_index_staging` is default-constructed to `body_count` and written only for bodies that have a transform, so bodies without one contribute a `{id{}, 0}` pair to the map. Benign — nothing looks up a default `id` — but the map's size no longer means what it appears to. Build the staging vector by `push_back` instead of index assignment.
- **`clear_runtime_state` resets the CPU solver through two degenerate frame calls** (`System.cpp:146-151`): `begin_frame({}, {}, cache)` plus `seed_previous_velocities({})`. `seed_previous_velocities` has no other caller, and `m_prev_velocity` is self-managing anyway (`Solver.cppm:443`). The solver wants a `reset()`; these two calls are a reset spelled in the vocabulary of a step.
- **`frame` and `prepare` index channel element `[0]`** (`System.cpp:947`, `1790`) and silently drop any others. Single-producer today, so unreachable — but if the channel stays after finding 8 and finding 9, assert the count rather than implying a multiplicity that is not handled.
- **`physics_substeps` reaches the two backends by different arithmetic**: the CPU computes `sub_dt = constant_update_time / substeps` and loops `steps * substeps` (`System.cpp:1452-1454`, `1535`), while the GPU sends `dt * steps` with `steps * substeps` and re-divides internally (`System.cpp:1435-1436`, `GpuSolver.cpp:938`). `constant_update_time` and `fixed_dt` are the same value (`SystemClock.cppm:248`, `253`), so this agrees today — but two spellings of one clock and two places that divide by substeps is a parity hazard in a module whose whole campaign is parity. Pick one accessor and one division site.

---

## Proposed structure

Sequenced so each step is independently landable and each one makes the next smaller.

1. **Extract the shared pipeline halves** (finding 4). `build_joint_constraints`, `build_motor_constraints`, `build_impulse_constraints`, `scatter_body_states`; `update_vbd` adopts `build_body_states`. Both drivers shrink to their genuinely different middles. This is the change everything else gets easier after, and it closes finding 2 as a side effect.
2. **One authority for the live backend** (findings 1, 14). Delete `d.gpu_buffers_created`; derive from `d.gpu_solver`. Route the seven consumers through one accessor. Fix `restart_required` at the settings layer so the toggle cannot half-apply.
3. **Collapse the defaults** (finding 5). Delete `default_solver_config()`; `shadow_step` uses `solver_config_from_settings`; fix `velocity_sleep_threshold`.
4. **Narrow the `gpu_solver` boundary** (findings 6, 12). Stop publishing the solver object; publish a small read-only projection instead, and let `PhysicsDebugRenderer`, `ContactTrace`, `CpuContactTrace` and `ShadowStep` consume it. Now a maintenance-surface change rather than a hazard fix, so it can sit behind steps 5-7 if scheduling demands.
5. **Delete the ceremony** (findings 8, 9, 11). `gpu_solver_stats` and `gpu_body_index_map` go; `gpu_upload_payload` moves its vectors and is passed whole to `upload`.
6. **Diagnostics to their own system** (finding 10), keeping `set_color_launch_hint` in the tick.
7. **`id_mapped_collection` for joints** (finding 7), and the parameter/allocation cleanups (findings 13, 15, 16).

Steps 1-3 are where the "messy" feeling actually lives. Steps 4-6 are what make the module's ownership legible to everything outside it.
