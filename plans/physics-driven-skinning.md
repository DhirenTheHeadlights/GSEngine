# Physics-Driven Skinning — Scope

Import an authored skinned model, **auto-derive a rigid body per bone from the model's own
skeleton**, and drive those bodies from animation clips as **kinematic** bodies. The mesh
renders from the bodies. Physics cannot move them, so the character never falls — but it
lives in the physics world, so its collision volumes are its visible pose, exactly.

Traditional FPS character behaviour, with the physics engine as the transport for every pose.

Reference: commit `d883fc68` deleted the old skinning stack. This is not a revert — see §4.

---

## 1. Authority chain

```
                    proxy capsule (dynamic + motor_component)
                      · the ONLY thing that collides with the world
                      · gravity, ground, walls — the character "as a unit"
                                    │
                       world transform of the character
                                    │
                                    ▼
clip sample ──▶ local joint poses ──▶ FK ──▶ kinematic_target_component
                                                     │           per bone
                                                     ▼
                                        physics applies target
                                          · writes transform_component
                                          · derives velocity from the delta
                                                     │
                                                     ▼
                                        VBD solver (locked bodies)
                                          · resolve_collisions = false
                                          · no contacts, no cost
                                          · pure pose carriers
                                                     │
                                                     ▼
                                        body snapshot ──▶ skin palette ──▶ mesh
```

Animation is the **author**. Physics is the **transport**. Rendering reads only the bodies —
never the clip directly.

Every component in that chain has exactly one writer. Animation writes targets, physics writes
transforms, the renderer writes neither — see §9.8.

**Two collision tiers, and only one of them is on.** A single proxy capsule does all world
collision, so the character meets the ground and walls as a unit. The per-bone bodies are
animated but collision-disabled: they cannot collapse (locked), cannot fight each other
(no contacts), and cost nothing in broadphase. They exist to carry pose to the renderer and to
be *ready*.

That readiness is the point. Because render reads bodies, the ragdoll transition is three
flips and no new code:

| | animating | ragdoll |
| --- | --- | --- |
| bone bodies | `kinematic_body`, `resolve_collisions = false` | `dynamic_body`, `resolve_collisions = true` |
| bone joint_specs | inert (locked↔locked) | active — become the ragdoll constraints |
| proxy capsule | colliding, motor-driven | disabled |

The render path does not change at all. No separate hitbox system to keep in sync, and no way
for the visual pose and the collision pose to disagree.

Honest framing: for a character that will *never* react, this is more machinery than plain GPU
skinning needs. The payoff is that the ragdoll arrives free instead of as a second system.

---

## 2. Engine support — verified

The semantics this design needs already exist; the only physics change is the kinematic-target
split in §9.8, which is an ergonomics and dataflow fix rather than a missing capability.

`kinematic_body` already exists as a `motion_component::body` variant, and the semantics are
exactly right:

- **Upload is per-frame.** `update_vbd_gpu` rebuilds the entire `body_state` array from
  `transform_component` every frame ([System.cpp:843-880](Engine/Engine/Source/Physics/System.cpp:843)).
  Writing a bone's transform moves its body that frame.
- **Kinematic bodies are locked.** `locked = (dyn == nullptr)` — kinematic and static both
  upload with infinite mass. They collide and push, and the solver never displaces them.
- **Readback skips them.** The solver→`transform_component` writeback does `if (!dyn) continue`
  ([System.cpp:697](Engine/Engine/Source/Physics/System.cpp:697)), so animation's transform is
  never clobbered.

So the CPU owns the pose, the solver receives it, and nothing fights over it.

Other confirmations:

| Question | Answer |
| --- | --- |
| Disable collision per body? | **Yes, and it is a full opt-out.** `resolve_collisions = false` skips the body when building the collision-object list — no broadphase entry, no contacts — in both the CPU ([System.cpp:207](Engine/Engine/Source/Physics/System.cpp:207)) and GPU ([System.cpp:912](Engine/Engine/Source/Physics/System.cpp:912)) paths. Bone bodies cost nothing. |
| Collision layers/masks? | None exist. `resolve_collisions` is the only filter — which is sufficient here, since bone bodies want *all* collision off, not selective. |
| Character motor? | `motor_component` is already one: `velocity_drive_target`, `horizontal_only`, `requires_ground_contact`, `max_force`. Pairs with `is_airborne` / grounded bits. |
| Capsule collision? | Yes — full narrow-phase support, `bone_shape = collision_shape` variant of box/sphere/capsule |
| Body budget? | `vbd::limits.max_bodies = 5120` — comfortable for ~25 bones/char even at high character counts |
| Mass/inertia from shape? | `physics::mass_from_density` exists; inertia is no longer authored at all — the system derives the tensor and COM from the collision shape via `mass_properties_of` |
| Bone→GPU index for render? | `physics::data::id_to_body_index` is `[[= gse::shared]]`; `gpu_solver.snapshot_buffer()` is GPU-resident |

Bone bodies still occupy a solver slot and still round-trip their transform (the body array is
built from `motion_component`, independent of collision), which is exactly what the render path
needs — they are simply invisible to contact generation.

---

## 3. What this design deletes from the problem

Deriving bodies from **the model's own skeleton** rather than retargeting onto the hand-authored
18-bone humanoid removes the two hardest parts of the alternative:

- **No bind-pose reconciliation.** The bodies are fitted to the mesh's own bind pose, so the
  bind-fit transform is identity by construction. In a retargeting design this was the one
  genuinely risky part (physics rig proportions vs. mesh proportions), and it disappears.
- **No retarget map, no 65→18 weight collapse.** Bodies are 1:1 with kept joints.

And because bodies are 1:1 with joints, skinning collapses back to the textbook formula:

```
skin[j] = B_world(j) * inverseBind[j]
```

where `B_world(j)` is simply the physics body's world transform. The physics body array *is*
the global pose array. Nothing exotic.

Note the tradeoff this accepts: the character will not physically react while animating, and
the tuned locomotion rig (`humanoid_rig_default()`) is untouched and unrelated to this path.
The two can coexist — one is a simulated character, this is an animated one.

---

## 4. Why a straight revert of `d883fc68` is still wrong

Even though animation is back in scope, four things moved underneath the deleted code:

1. **The geometry pipeline is mesh-shader/meshlet based now.** `meshlet_geometry.slang` uses
   amplification + mesh shaders over `meshlets_buffer` / `meshlet_vertex_indices` /
   `vertices_buffer`. The old `skinned_geometry_pass.slang` was classic VS/PS. Reverting bolts
   a second, non-meshlet pipeline onto the renderer.
2. **Materials are a palette** (`material_palette_buffers`, `instance_data.material_index`) with
   a transparent/OIT path. The old skinned batches predate it.
3. **Systems are annotation-driven** (`[[= gse::system_run]]`, `[[= gse::system_state]]`,
   `shared_view<>` / `read<>` / `write<>`). `SkinComputeRenderer`'s old
   `struct system { struct data; static run/frame; }` shape no longer matches the ECS.
4. **`render_component` was reshaped** to fixed arrays of 16 model handles + tints + sizes.

Also, the old `skin_compute.slang` did a serial parent-chain walk on thread 0 to build global
poses. We do FK on the CPU (tens of joints — trivial) and the bodies then hold world transforms
directly, so that GPU pass does not come back.

### Animation: what returns, what stays dead

| Deleted | Verdict |
| --- | --- |
| `Clip` (+ `.gclip` asset, sampling) | **Return** — this is the pose source |
| `Skeleton` / `Joint` (`.gskel`) | **Return** — hierarchy + inverse binds needed for FK and skinning |
| `skinned_mesh` / `skinned_model` | **Return**, new `.gsmdl` v2 vertex format |
| `AnimationGraph`, `AnimationDsl`, `ControllerComponent`, `AnimationBindings` | **Stay dead** — a clip player is enough for this scope; revisit only if blending demands it |
| `skin_compute.slang` hierarchy walk | Stay dead — superseded by CPU FK + world-space bodies |
| `skinned_geometry_pass` / `skinned_depth_only` | Stay dead — see §6 |

---

## 5. The import step — "convert the model to its own phys obj"

Fully automatic, offline. Inputs: `.gskel` (hierarchy + bind matrices) and `.gsmdl` (vertices
with weights).

**Per joint:**
1. Collect vertices for which this joint is the dominant influence.
2. Transform them into the joint's bind-local space via `inverseBind[j]`.
3. Fit an oriented shape to that point cloud: elongated along the child direction → capsule;
   otherwise box or sphere.
4. Mass via `mass_from_density`; inertia needs nothing — the physics system derives the tensor from the fitted collision shape.

**Bone filtering.** The mixamo rig is 65 joints, ~40 of which are fingers. Do not make 65
bodies. Drop joints below a volume / dominant-vertex threshold and fold their weights into the
nearest kept ancestor. This is the auto-derived equivalent of a hand-authored retarget map:
fingers and toe-ends disappear on their own merits, and a different character with a different
rig needs no new authoring.

Weights referencing dropped joints get merged into the kept ancestor (summing weights that
collapse onto the same target) — the same merge machinery either design needed, now driven by
a threshold instead of a table.

**Joints between bodies.** Emit parent↔child `joint_spec` constraints during import. Between
two locked, collision-disabled bodies they are inert, so they cost nothing while the character
is animating — and they are exactly what's needed the moment a bone flips to dynamic. Free
ragdoll rigging.

**Proxy capsule.** Fit one upright capsule to the whole mesh in bind pose (radius from the
horizontal extent, height from the vertical). This is the character's world collider. Emit it
alongside the bones so a model imports as a complete, ready-to-spawn character.

**Sanity output:** per-bone vertex counts, weight sums ≈ 1, dropped-joint report, fitted shape
dimensions.

---

## 6. Runtime

### 6.1 Pose

The proxy capsule owns the character's world transform (gravity + `motor_component` drive).
Per frame: clip sample → local joint transforms → CPU FK rooted at the proxy's transform →
write `kinematic_target_component` on each bone entity. Tens of joints per character; not worth
a GPU pass.

Velocity is *not* the animation's problem. Physics derives it when applying the target (§9.4),
which matters both for limb-vs-prop pushing if a bone is ever flipped to collide, and for the
ragdoll handoff carrying momentum instead of starting from rest.

### 6.2 Skinning

Recommended: **pre-skin compute → deformed vertex buffer**, meshlet pipeline draws it unchanged.

Over skinning inline in the mesh shader:
- The character draws in depth prepass + forward + OIT (+ shadows). Pre-skinning pays once;
  inline pays per pass.
- **TAA motion vectors need previous-frame skinned positions.** `instance_data.prev_model_matrix`
  cannot express deformation. Double-buffering the deformed buffer gives this for free.
- Mesh vertex buffers already carry `acceleration_structure_build_input` — a deformed buffer is
  required for BLAS refit if the character is ever in ray tracing.
- Zero edits to `meshlet_geometry.slang` / `meshlet_depth_only.slang` / `meshlet_oit.slang`.

Cost: vertices × instances × 2. Fine for a handful of characters; revisit if counts grow.

### 6.3 Where the palette reads from

Read the palette from the **body snapshot**, uniformly, for every bone.

The tempting shortcut is to build the palette straight from the CPU FK poses (we have them).
Resist it: the moment one bone goes dynamic, its pose comes from the solver and you need a
per-bone merge. Reading bodies uniformly is one code path that is already ragdoll-correct.

On latency — for a *locked* body, solver output is identical to solver input, so a kinematic
bone's snapshot value is exactly what was uploaded. There is no divergence to worry about, only
which frame's upload gets read. Sequence the skin pass after the solver and read the latest
slot rather than the stale "safe" slot that `physics_debug::prepare` uses. If that turns out to
be awkward, kinematic bones can be sourced from the CPU pose with provably identical results.

### 6.4 Culling

`meshlet_bounds` are bind-pose and go stale under deformation. Build the per-instance AABB as
the union of the bones' world AABBs — physics already computes these (`world_aabb_of`).

---

## 7. Asset format

**`.gsmdl` v2** — bump `asset_format::version`. Per mesh:
- vertices: `position`, `normal`, `tex_coords`, `bone_slots[4]` (uint8), `bone_weights` (vec4f)
- `bone_slots` index the **kept-bone table** after filtering, not the original 65 joints
- per-bone: fitted `bone_shape`, mass, `inverseBind`, parent index, name
- material (unchanged; feeds the existing palette)

Because bones are derived from the model, the skeleton and the collision rig ship inside the
model asset. No sidecar, no `.gskel` needed at runtime — its data is baked in. `.gclip` stays
a separate asset.

---

## 8. Work breakdown

### Phase 0 — Kinematic targets (engine; independent of everything else)
`physics::kinematic_target_component` + target application and velocity derivation in
`physics::prepare`. Lands on its own, carries no dependency on the rest of this plan.

*Exit:* a kinematic box driven by a `kinematic_target_component` moves, and pushes a dynamic
crate with the right momentum. That second half is the part worth actually testing — it proves
the derived velocity, which is what the ragdoll handoff later depends on.

### Phase 1 — Rig derivation (offline; nothing renders) — **DONE**
`Tools/RigDerive/rig_derive.py`: `.gskel` + `.gsmdl` v1 → `.gsmdl` **v3**. Shape fitting from
skin weights, leaf-subtree pruning, weight merge, proxy capsule fit, validation dump, and a
read-back verify pass.

```
python Tools/RigDerive/rig_derive.py --target-mass 78
```

Result: **65 authored joints → 22 bones** (4 box, 15 capsule, 3 sphere), 43 dropped — every
finger joint folds into its hand, which absorbs their vertices (1435 → 6276). Zero weight-sum
errors, zero influence overflow, worst FK round-trip error 1.6e-07 m.

Four things that were not obvious going in:

- **The format is v3, not v2.** `Tools/BlenderExporter/exporter.py` already defines
  `GSMDL_VERSION = 2` for the PBR material block. Our baked asset is v1, whose material is a
  single string — the PBR block only exists at v2.
- **Mixamo bone-local space runs the bone along Z**, and the engine's `capsule_shape` is
  Y-aligned with no axis field. Fitting naively produced *zero* capsules — every limb came out
  a box. Fixed by baking an axis-permuting rotation into the body's bind frame, which is free
  because `skin[j] = B_world(body) · inverse(body_bind_world)` holds for *any* rigid body frame.
  The same mechanism centres the shape on its fitted centroid rather than the joint origin.
- **Prune by fitted size, not vertex count.** The mesh is dense at the hands: a finger joint
  dominates 194–506 vertices, more than Spine2 (288). A vertex-count threshold keeps all 30
  finger bones. `--min-extent 0.08` (metres) drops them cleanly while keeping neck and toes.
- **Summed shape volume overshoots by ~1.6×** because adjacent capsules overlap at joints —
  1000 kg/m³ gives a 124 kg character. `--target-mass` solves density backwards from a target;
  at 78 kg the distribution lands close to anthropometric tables (thigh 10.1 kg, shin 4.9,
  foot 1.1, upper arm 2.0, forearm 1.6).

The derived `.v3.gsmdl` is a build artifact, reproducible from the command above, and is not
committed.

*Exit met:* fitted rig inspectable as numbers before any GPU work exists.

### Phase 2 — Bodies, no mesh — **BUILT, BLOCKED ON BAKING**

Implemented and compiling: `clip_asset` (`.gclip` v2), `skinned_model` (`.gsmdl` v3 rig), the
`skeleton_instance_component` / `clip_player_component` pair, `animation::clip_player` (sample →
FK → kinematic targets), F7 spawn in the sandbox, and `runs_after`-free registration ordered
before physics in `Engine.cpp`.

**Open blocker.** `asset::get<clip_asset>(… "Clips/mixamo.com")` asserts `ID not found` because
neither asset is ever baked. Established by inspection:

- `out/build/…/Engine/Resources/` contains only `Fonts/` and `Textures/`, both dated before the
  change. `compile<T>()` calls `create_directories(baked_root)` unconditionally once past its
  two early returns, so the absent directory proves it returned early.
- The runtime manifest gives `mode = dev`, `root = <repo>`, so `resource_path()` is the repo's
  `Engine/Resources`, which does contain `Clips/mixamo.com.gclip`. The `!exists(source_root)`
  return is therefore not the one taken.
- That leaves `has_compile_path<T>` false — despite `font` carrying an identical annotation set
  (`source_dir` + `source_exts` + magic + version), `bake` overloads sitting in
  `export namespace gse`, and `:clip` being exported from `Graphics.cppm` exactly as
  `:font_compiler` is.

`static_assert(asset::has_compile_path<…>)` for both types now sits beside the pack in
`AssetTypes.cppm` — it converts this from a runtime missing-ID assert far from the cause into a
compile-time failure naming the type, and answers which of the concept's three clauses fails.

Note for whoever picks this up: `Engine/Resources/SkinnedModels/` holds both `character.gsmdl`
(v1) and `character.v3.gsmdl`. Once baking works the v1 file is scanned too and its `bake()`
correctly returns false on the version check, counting as a failure. Harmless, but it will show
up until the v1 source is retired — it is still the rig-derive input.

### Phase 2 exit criteria (unchanged)
Spawn the proxy capsule (dynamic + `motor_component`) and one kinematic, collision-disabled
entity per kept bone. Clip sampling + CPU FK rooted at the proxy, writing `transform_component`.
Render with the existing physics debug renderer.

*Exit:* an animated stick-figure of bone volumes that walks around the world on its proxy
capsule, is blocked by walls, stands on the ground, and cannot collapse. This validates the
entire authority chain with zero rendering work — and it is the phase most likely to surface
surprises.

Worth doing here while it is cheap: flip the bones to dynamic and confirm the ragdoll drops.
The joint_specs already exist, so this is a one-line test that de-risks Phase 4's payoff before
any mesh work is committed.

### Phase 3 — Skinned draw
`skinned_model` asset + loader, `skin_palette.slang` from the body snapshot,
`skin_deform.slang` → deformed vertex buffer, meshlet draw, per-instance AABB from bone AABBs.

*Exit:* the character renders skinned, driven end-to-end through physics.

### Phase 4 — Polish and payoff
TAA prev-buffer + motion vectors; depth prepass / OIT / shadow reuse; BLAS refit if needed.
Then the demonstration this design exists for: **flip bones to `dynamic_body` and watch the
ragdoll take over with no render-path change.**

Scoped in detail in `plans/skinning-phase4.md`.

---

## 9. Infrastructure

### 9.1 What already exists — not being built

Worth stating plainly, because it makes the new surface much smaller than it first appears:

| Capability | Provided by |
| --- | --- |
| Kinematic bodies, locked semantics, per-frame transform upload | `motion_component` + `update_vbd_gpu` |
| Per-body collision opt-out | `collision_component::resolve_collisions` |
| Character motor + grounded state | `motor_component`, `is_airborne`, grounded bits |
| Bone entity → GPU body index | `physics::data::id_to_body_index` (`[[= gse::shared]]`) |
| GPU-resident pose source | `vbd::gpu_solver::snapshot_buffer()` |
| Ragdoll constraints | `joint_spec` + the full `joint_config` variant |
| Mass / inertia from a fitted shape | `mass_from_density`; inertia and COM auto-derived from the shape (`mass_properties_of`) |
| Asset bake/load/handle framework | `asset_format::*` annotations, `asset::add_loader<T>` |
| GPU barriers between passes | auto-derived from pass resource usage |
| System ordering | scheduler derives it from declared component access |

### 9.2 New — asset layer

**`gse.graphics:clip`** (restore, sampling only). `.gclip` already exists in `Engine/Resources/Clips`
and the exporter already writes it. Needs: per-joint TRS tracks, duration, `sample(time)` →
local transforms. Everything else from the old animation stack stays deleted.

**`gse.graphics:skinned_mesh`** (restore, adapted). Skinned vertex format + GPU buffers.

**`gse.graphics:skinned_model`** (new `.gsmdl` v2). Carries meshes *and* the rig: kept-bone
hierarchy, inverse binds, fitted `bone_shape` per bone, and the proxy capsule. A model imports
as a complete character — no `.gskel` needed at runtime.

**Clip↔rig binding.** The clip is authored against the full 65-joint rig; the model keeps ~25
bones. Resolve by joint name at load into a remap table, ignoring clip joints with no body.

> **Constraint this imposes on filtering:** prune **leaf subtrees only**. If an interior joint
> were dropped, its rotation would still need to propagate to its descendants, forcing FK over
> the full authored hierarchy and a second parallel skeleton. Restricting removal to leaves
> keeps the kept-bone set a connected subtree containing the root, so FK is self-contained over
> exactly the bones that exist. Fingers and toe-ends are leaf chains, so this costs nothing in
> practice — and it is the difference between one skeleton and two.

### 9.3 New — components

**Physics-owned, general purpose:**

- `physics::kinematic_target_component` — `{ vec3<position> position; quat orientation; }`. The
  commanded pose of a kinematic body. See §9.8 for why this exists; it is not specific to
  characters, and moving platforms, doors, elevators and animated props all want it.

**Animation / render:**

- `skinned_render_component` — model handle, tint, visibility. Mirrors `render_component`.
- `skeleton_instance_component` — `proxy_id` + `bone_ids[max_bones]` + `bone_count`. The binding
  between the entity graph and the rig; fixed array for networkability.
- `clip_player_component` — clip handle, time, speed, looping, playing.

### 9.4 New — systems

**`gse::animation::clip_player`** — `[[= gse::system_state<"ClipPlayer">]]`, one
`[[= gse::system_run<>]]` entry taking `read<clip_player_component>`,
`read<skeleton_instance_component>`, `read<physics::transform_component>` (the proxy's pose, to
root the FK), `write<physics::kinematic_target_component>`. Advances time, samples the clip,
runs FK, writes each bone's target. It never writes `transform_component`.

**`physics::prepare`** gains target application (modification, §9.7).

> **The solver's kinematic contract — get this wrong and motion jitters.**
> `vbd_predict` *advances* locked bodies — `predicted_position = position + velocity * dt`, with
> the orientation rotated by `angular_velocity * dt` — and `vbd_finalize` commits it
> (`if (locked) { body.position = body.predicted_position; }`). That committed position is what
> lands in the snapshot, and the snapshot is what `physics_transform` patches into the instance
> buffer, so **it is what you see on screen**.
>
> Therefore, for a kinematic body:
> - `transform_component` = pose at the **start** of the step
> - `velocity` = the motion to be applied **over** the step
>
> Writing the target *into* `transform_component` and also supplying a velocity double-advances
> the body: it lands a full step past the target, then snaps back when the next target is
> applied — a two-position oscillation that looks like a sync bug rather than a physics one.

So `apply_kinematic_targets` keeps the start-of-step pose in
`data::kinematic_step_start` (seeded from the transform on first sight, mirroring how
`sleep_counters` is kept) and does:

```
transform = step_start                                    (the pose to advance FROM)
velocity  = (target - step_start) / dt                    (lands exactly on target)
step_start = target                                       (next step's start)
```

The solver then moves the body from `step_start` to precisely `target`. `transform_component`
trails the rendered pose by one step, which is what "start of step" means.

Deriving velocity this way is also why no separate first-frame snap is needed — `try_emplace`
seeds `step_start` from the current transform, so a body spawned at its initial target yields
zero velocity on frame one.

**`gse::renderer::skin`** — `system_run` to build the per-instance bone-index table (via
`id_to_body_index`), `system_frame` to dispatch the two compute passes.

**Ragdoll transition** — a `ragdoll_request` channel plus a small system that flips bone
`motion_component` to `dynamic_body`, sets `resolve_collisions = true`, removes the
`kinematic_target_component`, and disables the proxy. Channels already exist
(`ctx.read_channel<T>` / `channels.push`).

Velocity seeding is free here: because physics derived `current_velocity` / `angular_velocity`
from the target delta on every animated frame, the bones already carry the animation's momentum
at the instant of the flip. Without the target split this would have been a manual step, and an
easy one to forget — the ragdoll would drop from rest and only look wrong in motion.

### 9.5 New — shaders

- `skin_palette.slang` — body snapshot + per-instance bone-index table → `mat4` palette
- `skin_deform.slang` — vertices × palette → deformed vertex buffer

### 9.6 New — rig derivation tool

A standalone offline tool (`Tools/RigDerive/`) reading `.gskel` + `.gsmdl` v1 and writing
`.gsmdl` v2: shape fitting from skin weights, leaf-subtree pruning, weight merge, proxy capsule
fit, validation dump.

**Why not the Blender exporter.** `Tools/BlenderExporter/exporter.py` runs inside Blender against
a source rig, and no `.blend` / `.fbx` / `.glb` exists anywhere in the repo — only the baked
artifacts survived. Everything this phase computes is a pure function of data already in those
files, verified by parsing them:

| Input | Confirmed present |
| --- | --- |
| `character.gskel` | 65-joint hierarchy + local/inverse bind matrices |
| `character.gsmdl` v1 | 28,312 verts / 49,112 tris; position, normal, uv, 4 bone indices + weights; all weight sums valid; 52 of 65 joints actually influence the mesh |

Independently of the missing source art, this is the better split: no Blender in the iteration
loop, the derivation is testable on its own, and any model that can reach v1 gets rig derivation
regardless of what authored it. The exporter stays a thin "get mesh + skeleton out" step.

Format note: v1 stores a single material string; the PBR block only appears at version ≥ 2, so
the v1 reader path is `read_str()` then vertex data.

### 9.7 Modified

- `Physics/KinematicTargetComponent.cppm` — new component, but lands in the physics module
- `Physics/System.cpp` / `.cppm` — `prepare` gains target application + velocity derivation;
  declare `read<kinematic_target_component>` on the run signature
- `GeometryCollector.cppm` / `.cpp` — skinned batches (batching only; deform is separate)
- `Renderer.cppm` / `.cpp` — pass registration
- `Runtime/Engine.cpp` — `asset::add_loader<skinned_model>` / `<clip>`, and
  `register_systems<^^animation>` before `register_systems<^^physics>` (see §9.8)
- `Tools/BlenderExporter/exporter.py` — unchanged by this plan; rig derivation lives in its own
  tool (§9.6)

The physics change is small and self-contained, and lands independently of everything else here
— it is worth doing first and on its own, since moving platforms and doors benefit from it with
or without this feature.

### 9.8 How the pieces sequence

Per frame:

1. **input → proxy.** `motor_component::velocity_drive_target` set from input. The proxy is an
   ordinary dynamic body; gravity and world collision are the solver's job.
2. **clip_player.** Advances time, samples local joint poses, FK rooted at the proxy transform,
   writes `kinematic_target_component` for every bone entity.
3. **physics.** Applies targets → `transform_component` (+ derived velocity), then rebuilds the
   body array — proxy resolved normally, bones uploaded locked and skipped by collision.
4. **skin.** Palette from the body snapshot, deform into the vertex buffer.
5. **meshlet draw.** Unchanged.

#### Why the target component exists

The obvious shape — clip_player writes `transform_component` directly — makes two systems
writers of one component, and the scheduler resolves component conflicts by making a node depend
on **earlier-registered** nodes touching it
([Scheduler.cpp:243-281](Engine/Engine/Source/Ecs/Scheduler.cpp:243)). That is real ordering, but
it is *implicit in `Engine.cpp` registration order*, which is a poor place for load-bearing
semantics.

The deeper problem is that `transform_component` was doing double duty: physics' **output** for
dynamic bodies and its **input** for kinematic ones. Splitting the input out fixes the modelling
ambiguity, and the ordering question mostly dissolves with it:

| | writer | reader |
| --- | --- | --- |
| `kinematic_target_component` | clip_player (and any other driver) | physics |
| `transform_component` | physics | renderers, gameplay |

One writer per component, and a dependency chain that only runs one direction:
`driver → target → physics → transform → renderer`.

#### What registration order still costs

The scheduler derives the clip_player → physics edge from the RAW on
`kinematic_target_component`, so it is still resolved by registration order. But the consequence
is now bounded: **registering animation after physics costs one frame of animation latency, and
nothing else.** It cannot corrupt state, because the two systems no longer write the same
component.

Worth noting explicitly, since it is easy to assume the worst: this was *already* true before the
split, because the writes were disjoint per entity — on the GPU path physics skips non-dynamic
bodies entirely (`if (!dyn) continue`, [System.cpp:697](Engine/Engine/Source/Physics/System.cpp:697)),
and the CPU path writes kinematic transforms back with identical round-tripped values. The split
does not rescue correctness that was at risk; it removes the ambiguity that made the risk hard to
rule out, and makes the intended dataflow legible.

Register `register_systems<^^animation>` before `register_systems<^^physics>` in `Engine.cpp` to
get the good latency. Getting it wrong is a frame of lag, not a bug hunt.

> Accepted as-is: the ECS resolves *component-derived* edges by registration order, and that is
> fine here once no component has two writers. Deliberately not solved: there is still no way to
> express "run **before** X".
>
> What *was* added (Phase 0): `gse::runs_after<^^State>` / `runs_after_optional<^^State>`
> annotations, so a run phase can declare an ordering edge without taking a parameter it never
> reads. Previously the only way to say "run after X" was `shared_view<X>` in the signature,
> which forced a `(void)param;` when the ordering — not the data — was the point. Collected by
> `meta::order_deps_of` and appended in `append_fn_order_deps`, feeding the same
> `run_state_deps` / `optional_run_state_deps` that `shared_view` params feed.

---

## 10. Sharp edges

- **DX12 compute-written buffers.** Must be created with `.writable = true` and a stride, or
  `create_buffer` produces an SRV instead of a UAV and writes silently no-op — a bug class that
  previously presented as a GPU hang. Applies to the palette and deformed vertex buffers. See
  `dx12-backend-effort` notes.
- **Proxy/ragdoll overlap.** Disable the proxy capsule *before* enabling bone collision, or the
  bones spawn contacts inside the proxy and the ragdoll explodes on frame one.
- **Never write a target straight into `transform_component`.** See the contract box in §9.4 —
  the solver advances locked bodies itself, so setting the end pose *and* a velocity
  double-advances them. This presents as jitter between two positions, which reads like a
  frame-sync problem and sends you looking in the renderer. It is not; it is the physics
  contract.
- **Teleports still need care.** `step_start` seeding covers the first frame, but deliberately
  jumping a body a long distance in one step yields a correspondingly huge velocity, which will
  launch anything it touches (and the ragdoll, if flipped that instant). Clear the entity from
  `data::kinematic_step_start`, or re-add the component, to snap without velocity.
- **Schedule ordering.** The skin pass must observe post-integrate state; declare it through the
  access annotations rather than assuming pass order.
- **Shape fitting is heuristic.** Expect to hand-tune thresholds for the first character. The
  validation dump in Phase 1 is what makes that cheap.
- **`max_bones` fixed array.** Components use fixed arrays for networkability — pick 32, validate
  at bind time, do not grow dynamically.

---

## 11. Open questions

1. **Root motion.** The proxy owns world position, so the open part is narrower: do clips carry
   root displacement that should *drive* the proxy (animation-driven movement), or is the proxy
   driven purely by input via `motor_component` with clips supplying joint-local motion only?
   The latter is simpler and is the usual FPS answer.
2. **Character count.** One player model, or many NPCs? Drives whether the deformed vertex buffer
   needs a real pool/budget in Phase 3.
3. **Ray tracing.** Should the character appear in RT reflections/GI? If yes, BLAS refit moves
   from Phase 4 polish into Phase 3 design.
4. **Clip blending.** Is a single clip at a time enough for now, or is idle↔walk crossfade needed
   in this scope? Blending is cheap to add at the local-pose stage, but it decides whether any of
   the deleted animation-graph code is worth resurrecting.
