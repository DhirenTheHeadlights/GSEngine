# Arbitrary collision shapes — scope

Status: scoped, not started. Written 2026-08-13, immediately after the shape-derived inertia tensor landed (`docs/solver_plan.md` § inertia). Read that section first — this document assumes it.

## Decisions taken

| Question | Decision |
|---|---|
| Center of mass | **Real COM offset carried by the solver.** Not asset-baked-centered. |
| GPU non-box shapes | **Fix it** — shape kind + params on the GPU body, branch the narrow phase. |
| Hull rung scope | **Reserve the compound seam**, ship one hull per body in phase one. |
| Sequencing | COM offset first and alone → GPU shape kind → convex hull → decomposition deferred. |
| Gyroscopic term | Still dropped. Unrelated to this program; see solver_plan. |

Not decided: whether convex decomposition is ever built, and what the hull vertex cap is (64 is the working assumption and the number the rest of this document is costed against).

## Three findings that gate the program

**1. The GPU narrow phase is box-only, and this is a live defect.** `vbd::body_state` (`Constraints.cppm:196`) carries `half_extents` and nothing else describing geometry — no shape kind, no radius. `collision_narrow_phase.slang:928` reads `ba.half_extents` / `bb.half_extents` and runs OBB-vs-OBB SAT unconditionally, and nothing filters non-box bodies out of the upload (`build_body_states` uploads every body). Since `bounding_box` sets half_extents to `(r,r,r)` for a sphere and `(r, h+r, r)` for a capsule, **on the GPU backend a sphere collides as a cube of side 2r and a capsule as a box.** The CPU path is shape-correct — it dispatches on the shape variant through `generate_shape_manifold` (`System.cpp:594`). This has been invisible because the entire parity harness is cubes, and it is not in the recorded GPU gap ledger, which covers sleeping, readback lag, `max_contacts`, and the warm-start scan.

Narrower than it first looks: `vbd_finalize.slang:46` also reads `half_extents`, but only to refresh the broadphase AABB, and an OBB-derived AABB is *conservative* for a sphere or capsule. Only the narrow phase produces wrong contacts.

**2. The diagonal tensor is a ceiling, not a choice.** `principal_moments` returns `vec3<inertia>` and that is exactly right for box, sphere and capsule because their principal axes coincide with the body frame by construction. A hull's do not. The representation has to become a full symmetric body-space tensor (or diagonal plus a principal-axis quat; the full tensor is simpler and the downstream plumbing is already `mat3`).

**3. Origin-is-COM is assumed throughout.** `body_state::position` is simultaneously the entity's transform position, the integration variable, and the rotation center that every lever arm is measured from. Box, sphere and capsule centroids are the origin by construction, so this has never been tested. It is the deepest change in the program and the reason COM is sequenced first.

## Rung 1 — COM offset — IMPLEMENTED 2026-08-13, gate not yet run

**The invariant that made this cheap: `body_state::position` *is* the center of mass.** The transform component keeps describing the origin; `com_from_origin` / `origin_from_com` (`MotionComponent.cppm`) are the only two places the conversion is spelled out. Choosing COM for the integration variable meant the entire solver interior — all eleven `rotate_vector(orientation, local_anchor)` sites, the contact lever arms at `System.cpp:646-647`, the sleeping-pair replay at `:564/:567`, `Solver.cppm:881/893` — needed **no edits at all**, because those sites were already measuring from `body_state::position` and that is now the correct origin for a lever arm. The conversion lives entirely at the boundaries.

What actually changed:

| Site | Change |
|---|---|
| `BoundingBox.cppm` | `principal_moments` → `mass_properties_of`, returning `{ moments, centroid }`. One variant match yields both, which is also the shape the hull tetrahedron integral wants. Every current shape declares `.centroid = {}` explicitly. |
| `Constraints.cppm` | `body_state` gains `com_local`. The Slang mirror is reflection-generated from the C++ struct (`[[= shaders::shader_struct]]`), so the layout propagates automatically — the hand-sync risk that made this look dangerous does not exist. |
| `System.cpp` `build_body_states` | Restructured: per-body `mass_properties` are computed **before** the body-build loop (order-matches fast path, map fallback), so loop 1 can set `position` to the COM directly instead of loop 2 fixing it up afterwards. Loop 2 is back to AABBs only, and derives the shape center with `origin_from_com`. The separate `is_rotatable` post-pass is gone — loop 1 has the tensor in hand. |
| `System.cpp` `update_vbd` | `body_moments` → `body_props`; the substep body build sets the COM and carries `com_local`. |
| `System.cpp` ×3 writebacks + `query_transform` | `origin_from_com` on the way out. |
| `MotionComponent.cppm` `interpolated_transform` | Now back-interpolates the COM, rotates, then recovers the origin — the two stopped being separable. Also moved out of the interface body to match the file's convention. |
| `vbd_shared.slang` | `body_origin` helper. |
| `collision_narrow_phase.slang` | Splits `center_*` (COM, for lever arms) from `shape_center_*` (origin, for SAT and contact generation). These were the same variable; that conflation was the whole bug. |
| `vbd_finalize.slang`, `physics_instance_transform.slang`, `physics_debug_instanced.slang` | AABB refresh, render interpolation, and debug shape rendering all need the origin. |

**Acceptance gate, not yet run:** every shape in the tree has a zero centroid, and `rotate_vector(q, 0)` is exactly zero in IEEE, so every conversion is a bit-exact no-op. The parity hashes must not move — `pyr50 0xbbcd…`, `pile 0xf27e…`. A moved hash means a conversion has the wrong sign or the wrong space, and with nothing else in flight the failure is localized to this rung.

**Two sites deliberately left incomplete, to close in rung 3 when a nonzero centroid first exists:**

- `apply_kinematic_targets` derives `current_velocity` from origin deltas, not COM deltas. `motion_component::current_velocity` is defined by this rung as the **COM** velocity (it is what impulses act on and what the solver produces), so a *rotating* kinematic body with an offset COM would get a slightly wrong velocity. Fixing it needs `read<collision_component>` added to `prepare`'s ECS access, which is not worth widening a system's access set for a value that is provably zero today.
- The two `interpolated_transform` callers (`GeometryCollector.cpp`, `SkinRenderer.cpp`) pass an explicit `vec3<displacement>{}`. The parameter is deliberately **not defaulted** so these sites are greppable rather than silently wrong later; neither renderer has shape access today.

## Rung 2 — GPU shape kind — IMPLEMENTED 2026-08-13, gate not yet run

The GPU narrow phase now dispatches on shape exactly as the CPU does. `body_state` gains `shape_kind` and `shape_params`; `vbd_limits` gains `shape_box/sphere/capsule` (matching the `collision_shape` variant order, so `cc.shape.index()` maps straight across) and the four feature-index constants.

**`shape_params` exists specifically to avoid a derived quantity.** Radius and half-height are recoverable from `half_extents` — `bounding_box` builds a capsule's as `(r, h+r, r)` — but `(h + r) - r` is not bit-exact in float, and a sub-ULP half-height difference between backends is exactly the class of divergence this project spends its time hunting. The params are stored as authored instead. `half_extents` keeps its existing job: the box case and the conservative AABB that `vbd_finalize` refreshes.

The Slang port mirrors `NarrowPhaseCollisions.cppm` function for function — `closest_point_on_segment`, `segment_segment_closest_params`, `query_obb`, `segment_obb_query` (same three refinement iterations plus both endpoints), `classify_box_face`, `classify_capsule_feature` with the same 0.01 cap threshold — and reproduces the canonicalization exactly: order by `shape_kind`, dispatch on the ordered pair, negate the normal on unswap, and swap `position_on_a`/`position_on_b` plus `feature_hi`/`feature_lo` on the manifold. That last swap is the CPU's `std::swap` of all four feature component pairs, which is precisely a hi/lo word swap in the packed representation.

**Box-box is untouched by construction:** the kernel branches on `kind_a == shape_box && kind_b == shape_box` *before* any of the new machinery and calls `sat_speculative` / `generate_contacts` with plain locals, exactly as before. Cube scenes must not move — that is the regression gate. Sphere and capsule scenes have no prior GPU baseline to preserve; they were wrong before.

### DX12 device hang — fixed 2026-08-13, same day

The first version of this rung hung the GPU on DX12 (`DEVICE_HUNG`, DRED naming `vbd_narrow_phase_stage`'s indirect dispatch) while Vulkan ran the same Slang fine. Two mistakes, one of which was the real damage:

**The damage: box-only scenes were routed through the new machinery for no benefit.** Pyramid, mound, drop — every existing scene and every baseline is boxes, and the first version funnelled all of them through `shape_speculative` / `shape_contacts` even though those immediately forwarded to the original functions. A bug in the new code therefore took down scenes that had nothing to do with shapes. The kernel now branches to the original path first, so box-only scenes cannot be affected by anything in this rung — which is what the gate assumed all along.

**The likely mechanism: a by-value struct containing an array, dynamically indexed.** The first version introduced a `ShapeInfo { uint; float3 ×3; float3 axes[3]; }` passed by value everywhere, plus `query_obb` indexing both a local `float local[3]` and `s.axes[min_axis]` with runtime indices — and `segment_obb_query` calls `query_obb` six times. DXC must materialise those into indexable scratch per call; SPIR-V tolerates the same source. Every other function in this file takes `(center, he, axes[3])` as plain parameters — `sat_speculative`, `generate_contacts`, `projected_radius`, `support_obb` — and those have always worked on both backends. The new code now follows that convention: no struct-with-array by value, no struct-with-array assignment for the lo/hi swap, `float3 local` instead of `float local[3]`, and the min-axis selection unrolled into three explicit branches instead of a dynamic index. The one data-bound loop (the manifold swap) is now bounded by a constant 4 with a `break`.

**Not proven, and worth saying:** the exact faulting construct was not isolated — the fix removes the whole class rather than bisecting within it. Two facts were established along the way and are worth keeping: the pyramid hanging *rules out* the sphere/capsule math, since that scene never reaches it; and `warm_start_lookup`'s `for (uint k = 0; k < count; k++)` cannot be the spinner despite looking like the obvious candidate, because it breaks on `slot >= max_contact_adjacency` and so terminates immediately even on a garbage count.

**`ParityShapes` closes the coverage gap** — added the same day, since the harness structurally could not catch the original defect and could not have confirmed the fix. Sixteen bodies in six well-separated groups, one per shape pair, so a divergence localizes to a single narrow-phase path. Details and the reasoning behind the geometry are in `docs/solver_plan.md`; the short version is that every group is a stable cradle or flat rest rather than a balanced stack, because a baseline scene must not amplify ULP noise into a false alarm, and the box-capsule support is widened so the capsule endpoints do not land on the support's face edges. New builders in `EntityBuilders.cppm`: `capsule`, `static_sphere`, `static_capsule`, all returning the renamed `collider_archetype` (was `static_collider_archetype`, which had no callers) since a physics-only body needs no render primitive — there is no capsule render spec in the engine.

The gate entry is `report`/`report` for now. It runs and prints, so a regression is visible, but it asserts nothing: promoting it to `require` before a first round would be encoding a number nobody has measured.

## Rung 3 — convex hull

**3a — full body tensor. IMPLEMENTED 2026-08-13.** `mass_properties` now carries `mat3<inverse_inertia> inv_inertia_body` instead of `vec3<inertia> moments`, and `inv_inertial_tensor` is reduced to `R · I⁻¹ · Rᵀ` with no diagonal assumption.

**Storing the *inverse* rather than the tensor is what keeps this bit-exact, and it is not a detail.** A general 3×3 inverse of an exactly diagonal matrix is not bit-identical to per-element reciprocals — the cofactor form computes `(d₁d₂)/(d₀d₁d₂)`, which is not `1/d₀` in float. Inverting at the source instead lets each shape use the form its own structure warrants: box, sphere and capsule build the guarded diagonal reciprocal via `inverse_diagonal_inertia` exactly as before, and a hull will do one general inverse of its integrated tensor. The operations and their order are unchanged for every shape that exists, so the parity hashes must not move.

**3f — box/hull unification. PARKED 2026-08-13, decided but deliberately deferred. Do not drop this.**

The decision stands: a box is a convex hull, and representing it as a 6-face / 12-edge / 8-vertex polytope so the bespoke box routines can be deleted is the correct end state. It is parked, not cancelled.

Why it is parked, and the rule it earned: the DX12 hang below happened *because* box-only scenes were routed through new, unproven code. Unification does that deliberately and permanently — it puts every baseline the solver's stability was bought with (`pyr50 0xbbcd…`, `pile 0xf27e…`, stress, the whole ladder) onto a rewritten path. Doing that while anything else is in flight makes a moved hash unattributable.

Conditions for doing it, all of them:

1. Land it **alone** — no other physics change in the tree, so a moved hash has exactly one candidate cause.
2. Run the gate round **before** it, so the pre-unification hashes are freshly measured rather than assumed.
3. Expect box-box to stop being *structurally guaranteed* and become *something to verify* — the acceptance bar is bit-identical hashes on both backends, not "close enough".
4. Keep the old box routines in the tree until the round is green, so reverting is a one-line branch flip rather than a re-implementation.
5. Do the CPU and GPU sides as separate rounds, not one change — the hang showed that a construct can be fine on SPIR-V and fatal on DXIL, and that class is only findable when one backend at a time moves.

**3b — hull geometry. IMPLEMENTED 2026-08-13, UNBUILT AND UNVERIFIED.** `Physics/Collision/ConvexHull.cppm` (new partition, picked up by the existing recursive glob): the `convex_hull` indexed-polytope type (vertices, face planes, face loops, edge list), an incremental hull builder with coplanar-triangle merging, the tetrahedron mass integral, and `hull_support` / `hull_half_extents`.

**The mass integral stays in the unit system end to end.** The tetrahedron formulation runs through length⁴ and length⁵ intermediates that have no engine-named tag — that is not a reason to drop to floats. `gse` quantity dimensions are generic template parameters, so those powers are spelled with `decltype` (`hull_quartic`, `hull_quintic`) and the whole integral accumulates in quantities, landing on `inertia` via a unit density. Stripping units in intermediate math is banned outright: the dimensional check is worth most exactly where the algebra is hardest.

**Coplanar merging is not cosmetic**: without it a box hull is 12 triangles rather than 6 quads, which would give worse manifolds and would break the box-as-hull equivalence check below.

**Nothing constructs a hull yet, which is what makes this safe to land unverified** — it is purely additive, and no existing path can reach it.

**3d — hull narrow phase. IMPLEMENTED 2026-08-13, UNBUILT. One required piece deliberately NOT done — see the guard below.**

`Physics/Collision/HullCollision.cppm` (new partition) holds the polytope layer: a `polytope` span-view, an allocation-free `box_polytope` adapter, SAT over faces-A / faces-B / edge×edge, Sutherland-Hodgman reference-face clipping, and point/segment queries. `hull_shape { uint32 index }` is variant alternative 3, so canonicalisation by variant index always puts hull on the `hi` side — which is why only four new pairs exist (box-hull, sphere-hull, capsule-hull, hull-hull) rather than eight. Both dispatches in `NarrowPhaseCollisions.cppm` gained those branches; `shape_data` gained a `const convex_hull*` resolved by the caller, and `resolve_hull` maps the index through `body_build_view::hulls` / `data::hulls`.

**Box-box is still untouched** — the box is only expressed as a polytope on hull-involving pairs. That is the parked 3f, not this.

**REQUIRED BEFORE ANY HULL IS SPAWNED — not done:** a hull body reaching the GPU solver gets `shape_kind == 3`, which no shader branch matches, so `shape_speculative` falls through to its capsule-capsule tail and produces garbage. This is exactly the silent-wrongness class that hid the box-only defect. The guard belongs in the GPU path specifically (`update_vbd_gpu`), not in `build_body_states`, because the CPU shadow step shares that function and must keep working for hulls. Nothing constructs a hull yet, so nothing is currently broken — but this must land before the first hull spawn, and before `data::hulls` is ever populated.

**Closed since:** the GPU guard now asserts `data::hulls.empty()` in `update_vbd_gpu` — O(1), fires the moment a hull is registered while the GPU solver is on, and conservative in the safe direction. `build_body_states` loop 2 has its hull branch (AABB + `bounding_box(tc, hull)`), both `build_body_states` call sites pass `.hulls`, and `volume_of` / `mass_from_density` take a hull pointer.

**Broadphase closed too.** `world_aabb_of` takes a `const convex_hull*` and has its hull branch; the resolve happens **once** in `collect_collision_objects` and is stored on `collision_pair::hull`, so the pair-set validator and the `shape_data` handed to the narrow phase both read it without re-resolving. That last one was a latent hole of its own — `shape_data::hull` existed but was never populated, so every hull pair would have bailed at its null check even with a correct AABB. `SkinRenderer.cpp` passes `nullptr`; bones are never hulls.

**Hulls can be authored now.** Scene setup cannot reach `physics::data` — `scene` exposes a `registry&` but the scheduler is private to `engine`, so there is no path from a `setup_fn` to system state. Hulls are therefore authored as *data*: a `hull_definition` component carries the point cloud, and `integrate` interns it on first sight — builds the hull, appends to `d.hulls`, rewrites the entity's `collision_component::shape` to a `hull_shape` with the resulting index, and flags the definition consumed. That puts hull construction in the system that owns hulls, needs no new access plumbing, and is the same shape an asset-backed path would take later (the asset would supply the points instead of the component).

**The box-as-hull equivalence group is live** in `ParityShapes` at x = -21, mirroring the box-box group at x = -15 exactly: same `unit_box` support, same 1.55 drop height, same size. The only difference is that one body is authored as a `box_shape` and the other as eight corner points fed through `build_convex_hull`. **They should behave identically** — that is the whole point, and any divergence between those two groups is a defect in the hull path, localized without interpretation. It exercises the builder, the coplanar merge (a box hull must come out as 6 quads, not 12 triangles, or the manifolds differ), the mass integral, the polytope SAT and the clip, all against a baseline that is already trusted.

**The first defect that group caught: degenerate contact features. FIXED AND MEASURED 2026-08-13.** The hull twin crept laterally forever at a constant −1.22e-4 m/frame in x while the box twin held x = −15.000000 exactly.

**Result, `parity_shapes_cpu` 300 frames, hash 0x62b5b16419747d67 → 0x5429bf86de4b5bf7.** The per-frame rate is now **exactly** +0.000e+00 in both x and z from frame 40 to 299 — not small, zero — and the hull's position is bit-frozen from frame 17 onward with velocity exactly zero from frame 8. Rest height went from 0.05 mm off its twin to **exact**: 1.499500 against 1.499500, worst |dy| 1.19e-7, one float ULP at that magnitude. The y-drop over the run matches the box twin digit for digit (−4.369e-02 both). Total lateral displacement fell from 4.5 cm to 1.0 mm.

**The residual 1 mm impact kick is also CLOSED, 2026-08-14 — and the equivalence pair is now BIT-IDENTICAL.** Worst |dx|, |dy| and |dz| against the twin are all exactly 0.000000e+00 across all 300 frames, transient included. Hash 0x5429bf86de4b5bf7 → 0x888e95dd1c3fba1e, stable over four runs. Baseline recorded (p50 0.198 ms, p95 0.390 ms, p99 0.539 ms — p95/p99 both *better* than the 0.456/0.574 measured before the fix; the 44–118 ms spikes seen mid-investigation were environmental).

**The mechanism, measured rather than reasoned.** Three plausible hypotheses died on data first — a manifold collapse to one support point (killed on magnitude: single-corner torque predicts 3.36, measured 0.138), a COM offset from `integrate_hull` (killed because the body settles at quaternion identity), and the hull builder (killed by replicating it: 8 vertices, 6 quads, bottom-face centroid exactly (0,−0.5,0), correct winding). The answer only came from a CPU contact trace, and it was not in the clip path at all — it was **the fallback**.

During the speculative approach the gap is ~6 mm, so the reference-plane depth filter rejects every clipped vertex and **both** paths fall back to a single support point. The box's `support_obb` sums `sign(dot)·half·axis`; on a perfectly axis-aligned box every top vertex ties at `dot = 0`, the x and z terms vanish, and it lands on the face **centre** — zero lever arm. The hull's `polytope_support` took the first strictly-greater vertex and landed on a **corner**: `ra (−0.5,−0.5,−0.5)`. τ = r × F for that corner is `(+,0,−)`, matching the measured `avel (+0.0977, ~0, −0.0977)` exactly.

**The worse half was the feature key.** The fallback built it from *face* indices tagged `feature_type::vertex`, so the key stayed constant while the chosen support vertex flipped corners between frames. The contact cache matched on it and restored the previous corner's sticking anchor: `ra (+0.5,−0.5,+0.5)` against `rb (−0.5,+0.5,−0.5)`, tangential `c0` of **0.999 m**, an 850 N friction impulse. A stable key naming unstable geometry — the exact inverse of the frame-stability invariant the clip path satisfies. **Any future fallback must satisfy that invariant too.**

Fix: the fallback now uses `face_centroid` of each best-aligned face and labels it `face/face`, which is byte-identical to the box's fallback feature (`0x203ffff0202ffff`). Centred, so no lever arm; keyed on face indices, which are fixed hull data. The hull and box now emit the *same contact constraint* — same key, same lambda (−9798.83496094 N), same penalty, same anchors — which is why the trajectories agree bit-for-bit. `polytope_support` was the fallback's only caller and is deleted.

**Note the box's zero torque was luck, not design** — a degenerate coincidence of the OBB support formula under perfect axis alignment. Tilt the box and it picks a corner too. This was never "hull wrong, box right"; it is a latent sharp edge the hull happened to step on first, and it is still live for any non-axis-aligned box-box fallback.

**Watch item:** when the clip fails because the faces genuinely do not overlap in projection (a glancing edge-on approach) rather than because they are separated along the normal, two face centroids can sit laterally apart. The old arbitrary support vertex was no better there, and SAT has already established these are the two most normal-aligned faces — but that is the case to check if a non-flat scene misbehaves.

**What this buys 3f.** Condition 3 required box-box to be *verified* bit-identical rather than structurally assumed, and the measurement before this fix said the two paths were not. They now are, on the one geometry where they can be compared directly. The correctness objection to unification is answered.

**The cost objection is now addressed too, 2026-08-14.** `polytope_sat` had two independent costs: `polytope_extreme` called `world_vertex` — a quaternion rotation — for every vertex on *every* axis test, and the edge loop rebuilt `eb` and `normalize(ea)` inside the *inner* loop, so edge directions were recomputed Ea×Eb times. `build_polytope_frame` now transforms each body's vertices once per pair into a `polytope_frame` and `span_extreme` reduces to a dot-product loop over cached positions. `push_unique_axis` drops any axis parallel to one already accepted (|dot| > 1 − 1e-6); a box's 6 face normals are 3 unique axes negated and its 12 edges are 3 unique directions, so deduping the *directions before crossing* turns 12×12 into 3×3. **Box-box through the generic path: 156 axis tests → 15, exactly what `sat_speculative` does.**

**This is provably behaviour-preserving, and it was verified as such.** Overlap is invariant under axis negation — negating swaps and negates the extents, leaving `min(hi_a − lo_b, hi_b − lo_a)` unchanged — and the final sign is fixed by the `pose_b.center − pose_a.center` flip. Dedup keeps first-seen representatives, and equal-overlap duplicates could never displace the first anyway because the comparison is strict `<`. Parallel edges produce parallel crosses, so no dropped pair contributes an axis the survivors do not. `parity_shapes_cpu` hash did not move.

**New scenario `parity_hull_pile_cpu` (scene `ParityHullPile`), baseline recorded, hash 0x71483295e75b6b87 stable ×4.** A line-for-line mirror of `parity_pile_scene_setup` with `box_as_hull` swapped for `box` — 216 bodies, 6×6×6, 0.75 m spacing, same floor — so the two scenes differ *only* in authoring and the pair reads directly as what unification costs. **CPU only and it must stay that way** until 3e: `assert(d.hulls.empty())` in `update_vbd_gpu` means a `_gpu` twin would fire on frame one.

**Measured cost of the generic path: ~1.13× the bespoke box path** (20 frames, p50 2.413 ms vs 2.137 ms). **Only the 20-frame window supports that number.** At 30 frames it reads 3.005 vs 4.276 and at 60 it reads 5.162 vs 8.654 — the comparison *inverts*, not because hulls get faster but because **the hull pile falls asleep earlier than the box pile**, so longer windows compare different amounts of active work rather than two paths doing the same work.

**Two things that scenario exposed, both open.** Its 300-frame p50 is 0.078 ms because the median frame is a sleeping one, so it is a weak *performance* gate as configured — its real value is the hash, which is the first thing in the harness that would catch a hull narrow-phase regression at all. And the differing sleep timing between the two piles is itself an unexplained divergence: same geometry, same layout, same solver. It may be benign, or it may be the same class of thing the 1 mm turned out to be. Not investigated.

**Still not measured: the before/after for the SAT optimization itself.** It landed before the scenario existed, so the 1.13× is hull-path-with-dedup against the box path, not dedup against no-dedup. "156 → 15" remains arithmetic read off the code, not a benchmark. Getting the real number needs stash → build → record → restore → build, and `parity_hull_pile_cpu` at a ~20-frame window is the scene that would show it. Rest height was already right to 0.05 mm, so mass properties, tensor, COM and contact depth were all fine — the defect was entirely in the manifold. `polytope_manifold` gave **every point in a manifold the same `feature_id`**: `.index_a`/`.index_b` were the reference and incident face indices, both loop-invariant, and no per-vertex side tags existed. Four physically distinct corners therefore collapsed onto one contact-cache key, so they overwrote each other's cached lambda in turn and all four contended for a single sticking anchor — a frame-constant lateral bias that never decays. That also explains why the reference-face-churn fix moved the impact transient 4.5× but left the creep untouched: the features were degenerate no matter which face was reference.

The fix is the hull analogue of what `generate_manifold` already does for boxes. The Sutherland-Hodgman loop now carries provenance per output vertex — `hull_clip_vertex` holds a `point` plus two `hull_clip_sides` sets recording which reference-face edge planes clipped it and which incident-face edges it lies on — and `clip_vertex_feature` turns that into a distinct `feature_id`, with `feature_type` derived from the side count (0 → face, 1 → edge, 2 → vertex) exactly as `feature_type_from_sides` does. Candidates are then sorted by packed feature and quantized tangent-plane coordinates, deduped by feature, and the first four taken.

**Frame stability, not just uniqueness, is the requirement** — a key that changes for the same physical corner next frame misses the warm start just as badly. Both index spaces are fixed hull data: the reference/incident face indices, and the local edge ordinals within each face loop. At the 64-vertex cap a face has at most 64 boundary edges, so a side ordinal can never collide with the `0xFF` `feature_side_none` sentinel.

**Two threshold changes ride along, and they are load-bearing rather than cosmetic here, because both bodies in this group are the same `unit_box`.** The incident face's corners land *exactly* on the reference face's edge planes, so the old exact `d <= 0` clip predicate was a knife-edge: a corner at `d = +1e-7` was dropped and replaced by a spurious intersection point, and the polygon's composition flickered frame to frame on float noise alone. The clip now uses the box path's `1e-4 m` band for both the inside test and the on-plane side tagging, which retains all four corners deterministically and tags each with the two reference edges it sits on. The reference-plane depth filter moved from `> 0` to the same band for the same reason. A zero-length guard on the reference edge's clip-plane axis was added alongside; the box path has always had one and the hull path was one collinear face-loop pair away from a NaN contact normal.

`hull_clip_sides` / `add_clip_side` / `shared_clip_sides` / `clip_sides_type` duplicate `active_sides` / `add_side` / `shared_sides` / `feature_type_from_sides` from `NarrowPhaseCollisions.cppm`. That is deliberate: hoisting them into `:contact_manifold` would edit the box partition, and box hashes are the gate for everything. 3f deletes the box copy anyway.

**Still outstanding:** `PhysicsDebugRenderer.cpp` will not draw hulls.

**3c — generation from a mesh. NOT STARTED.** The narrow phase (hull-hull, hull-box, hull-sphere, hull-capsule) is the remaining CPU work, and the design that keeps it small is a *polytope descriptor*: one SAT + clip routine over (faces, edges, vertices) plus point-vs-descriptor and segment-vs-descriptor queries, with a box expressed as a descriptor **only on hull-involving pairs**. Box-box keeps its own path untouched — that is 3f's job, and 3f is parked.

**The verification story to build first, before trusting any of it:** author a unit box *as a hull* and run it against a real box through the hull path. The results should match the box-box path. That is the 3f equivalence test performed as a comparison rather than a replacement, it costs one `ParityShapes` group, and it validates the builder, the coplanar merge, the mass integral and the narrow phase in one shot.

**3e — GPU hull support. DEFERRED, and hulls must not reach the GPU solver until it exists.** A hull is variable-size data and cannot live in the fixed-size `body_state`; it needs vertex/face/edge buffers plus per-body offset and count. Deferring is also the right call on timing: the DX12 transfer-copy investigation is live, and today's hang is the case study for why CPU and GPU should not move in the same round. Until 3e lands, a hull body reaching the GPU narrow phase would fall through the `shape_kind` chain into the capsule branch and produce garbage — that guard is required as part of 3d, not optional.

The remaining pieces of rung 3 are below.

**Storage and generation.** QuickHull over the source mesh at asset load or bake time, capped at 64 vertices. Deterministic because it is a load-time step, which matters given the determinism campaign. The hull is a fourth alternative in `collision_shape`.

**Why 64 is the cap, precisely.** `feature_id` (`ContactManifold.cppm:19`) stores `index_a`/`index_b` as `uint8`, but `feature_type` separates vertex, edge and face into three independent 256-spaces. A 64-vertex convex polytope has F ≈ 2V-4 = 124 and E = V+F-2 = 186 by Euler — all three fit under 256. **At a 64-vertex cap the existing warm-start key needs no change whatsoever.** Above it, the key has to widen and every cached lambda re-keys.

**Why a cap is mandatory at all.** Hull-hull SAT is O(Fa·Fb + Ea·Eb) and the edge-cross term dominates: two 186-edge hulls is ~35k axis tests per pair. This is why Bullet caps at 64 and PhysX at 255. Left uncapped it will not be a performance regression, it will be a wall.

**Narrow phase.** Either hull SAT with face clipping (keeps the current 4-point clipped-manifold and feature-key structure, which the warm starting and sticking anchors depend on) or GJK/EPA plus separate manifold generation. **Prefer SAT + clipping** — it is the shape of what already exists, and the solver's stability record is built on stable feature IDs. GJK/EPA returns one deepest point per call and still needs clipping bolted on for a manifold, so it buys less than it looks.

**Mass properties.** The tetrahedron-decomposition integral over hull faces yields volume, centroid and the full inertia tensor in one pass. The centroid becomes the body's COM directly — rung 1 is what makes that usable. `principal_moments` becomes the diagonal special case of a general body-tensor function.

**The reserved compound seam.** Ship one hull per body, but build it so decomposition is additive:

- The narrow-phase entry takes **(shape, local transform)** pairs, with the transform always identity in this rung.
- The sub-shape index fits in `feature_id` without widening the key: `feature_type` needs 2 of its 8 bits, so there are 6 spare bits on each side of `pack_feature` — **64 sub-shapes per body inside the existing `uint64`.** *Correction 2026-08-13: reserving them early buys nothing.* `pack_feature_component` shifts the type byte whole, so redefining that byte later as `feature_type | (sub_shape << 2)` leaves every existing key byte-identical while `sub_shape` is 0. There is no re-key cost to deferring, and speculative fields nothing writes are worse than none. Add them when a compound first exists.
- Broadphase keeps a per-body AABB in this rung, but the AABB source becomes "union over sub-shapes" rather than "the one shape".

**GPU.** A hull is variable-size data, so it cannot live inside the fixed-size `body_state` the way a sphere radius can. It needs a separate vertex/face/edge buffer with per-body offset and count, and the GPU narrow phase becomes a loop over variable geometry with unbounded register and LDS pressure. This is materially harder than rung 2 and should be scoped separately once the CPU hull rung is real.

## Rung 4 — convex decomposition, deferred

V-HACD or CoACD offline, producing a compound of hulls. It is strictly hull-plus-container: no new collision algorithm beyond rung 3, but the container, the contact budget, and an asset-pipeline stage with per-asset tuning.

Two things to price when it comes up:

- **Contact budget.** 16 pieces against 16 pieces is 256 candidate sub-pairs. The GPU's `max_contacts` is 262,144 and the warm-start lookup is already flagged as quadratic-shaped at scale, so a decomposed world is a different scale regime, not the same one with more shapes.
- **Overlapping pieces break the mass sum.** V-HACD pieces overlap slightly, so parallel-axis summing their tensors double-counts mass in the overlaps. The usual fix is normalizing to an authored target mass — which puts a caveat back on the "nothing authors mass properties" property just won.

## What this program does not cover

Static triangle meshes for level geometry. That is a separate and much cheaper problem — static bodies have zero `inv_inertia` so no mass-property work at all, and convex-vs-triangle never touches the dynamic path. If level collision is the actual need, it should be scoped on its own rather than waited on behind this.
